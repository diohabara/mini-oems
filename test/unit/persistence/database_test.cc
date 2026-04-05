#include "core/persistence/database.h"

#include <gtest/gtest.h>

namespace oems::persistence {
namespace {

class DatabaseTest : public ::testing::Test {
 protected:
  Database OpenDb() {
    auto db = Database::Open(":memory:");
    EXPECT_TRUE(db.has_value());
    EXPECT_TRUE(db->Migrate().has_value());
    return std::move(*db);
  }
};

TEST_F(DatabaseTest, OpenInMemorySucceeds) {
  auto db = Database::Open(":memory:");
  EXPECT_TRUE(db.has_value());
}

TEST_F(DatabaseTest, MigrateIdempotent) {
  auto db = OpenDb();
  EXPECT_TRUE(db.Migrate().has_value());
  EXPECT_TRUE(db.Migrate().has_value());
}

TEST_F(DatabaseTest, SaveAndLoadOrder) {
  auto db = OpenDb();
  order::Order o{
      .internal_id = 42,
      .client_order_id = "CL1",
      .symbol = Symbol{"AAPL"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = 10000,
      .order_qty = 100,
      .filled_qty = 30,
      .remaining_qty = 70,
      .status = OrderStatus::kPartiallyFilled,
      .created_at = Now(),
      .updated_at = Now(),
  };
  ASSERT_TRUE(db.SaveOrder(o).has_value());
  auto loaded = db.LoadOrder(42);
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->internal_id, 42U);
  EXPECT_EQ(loaded->client_order_id, "CL1");
  EXPECT_EQ(loaded->symbol.value, "AAPL");
  EXPECT_EQ(loaded->side, Side::kBuy);
  EXPECT_EQ(loaded->type, OrderType::kLimit);
  EXPECT_EQ(loaded->price, 10000);
  EXPECT_EQ(loaded->order_qty, 100);
  EXPECT_EQ(loaded->filled_qty, 30);
  EXPECT_EQ(loaded->remaining_qty, 70);
  EXPECT_EQ(loaded->status, OrderStatus::kPartiallyFilled);
}

TEST_F(DatabaseTest, SaveOrderUpdatesExisting) {
  auto db = OpenDb();
  order::Order o{.internal_id = 1, .order_qty = 100, .remaining_qty = 100};
  ASSERT_TRUE(db.SaveOrder(o).has_value());
  o.remaining_qty = 50;
  o.filled_qty = 50;
  o.status = OrderStatus::kPartiallyFilled;
  ASSERT_TRUE(db.SaveOrder(o).has_value());
  auto loaded = db.LoadOrder(1);
  ASSERT_TRUE(loaded.has_value());
  EXPECT_EQ(loaded->remaining_qty, 50);
  EXPECT_EQ(loaded->filled_qty, 50);
}

TEST_F(DatabaseTest, LoadOrderNotFound) {
  auto db = OpenDb();
  auto result = db.LoadOrder(999);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

TEST_F(DatabaseTest, LoadOrdersReturnsAll) {
  auto db = OpenDb();
  for (OrderId i = 1; i <= 5; ++i) {
    order::Order o{.internal_id = i, .order_qty = 100, .remaining_qty = 100};
    ASSERT_TRUE(db.SaveOrder(o).has_value());
  }
  auto all = db.LoadOrders();
  ASSERT_TRUE(all.has_value());
  EXPECT_EQ(all->size(), 5U);
  EXPECT_EQ((*all)[0].internal_id, 1U);
  EXPECT_EQ((*all)[4].internal_id, 5U);
}

TEST_F(DatabaseTest, AppendAndLoadEvents) {
  auto db = OpenDb();
  order::OrderEvent ev1{.event_id = 1,
                        .order_id = 10,
                        .type = order::EventType::kAccepted,
                        .timestamp = Now(),
                        .detail = "ok"};
  order::OrderEvent ev2{.event_id = 2,
                        .order_id = 10,
                        .type = order::EventType::kFill,
                        .timestamp = Now(),
                        .detail = "qty=100"};
  order::OrderEvent ev3{.event_id = 3,
                        .order_id = 11,
                        .type = order::EventType::kAccepted,
                        .timestamp = Now(),
                        .detail = "other"};
  ASSERT_TRUE(db.AppendEvent(ev1).has_value());
  ASSERT_TRUE(db.AppendEvent(ev2).has_value());
  ASSERT_TRUE(db.AppendEvent(ev3).has_value());

  auto events_10 = db.LoadEvents(10);
  ASSERT_TRUE(events_10.has_value());
  EXPECT_EQ(events_10->size(), 2U);
  EXPECT_EQ((*events_10)[0].event_id, 1U);
  EXPECT_EQ((*events_10)[1].event_id, 2U);

  auto all = db.LoadAllEvents();
  ASSERT_TRUE(all.has_value());
  EXPECT_EQ(all->size(), 3U);
}

TEST_F(DatabaseTest, SaveAndLoadExecution) {
  auto db = OpenDb();
  matching::Fill fill{
      .execution_id = 7,
      .symbol = Symbol{"AAPL"},
      .aggressive_order_id = 10,
      .passive_order_id = 20,
      .aggressive_side = Side::kBuy,
      .price = 10050,
      .quantity = 50,
      .timestamp = Now(),
  };
  ASSERT_TRUE(db.SaveExecution(fill).has_value());
  auto all = db.LoadExecutions();
  ASSERT_TRUE(all.has_value());
  ASSERT_EQ(all->size(), 1U);
  EXPECT_EQ((*all)[0].execution_id, 7U);
  EXPECT_EQ((*all)[0].symbol.value, "AAPL");
  EXPECT_EQ((*all)[0].price, 10050);
  EXPECT_EQ((*all)[0].quantity, 50);
}

TEST_F(DatabaseTest, AuditLogAppend) {
  auto db = OpenDb();
  EXPECT_TRUE(db.AppendAudit("order", "submit AAPL buy 100@10000").has_value());
  EXPECT_TRUE(db.AppendAudit("session", "logon from CPTY1").has_value());
}

TEST_F(DatabaseTest, ServiceStateKeyValue) {
  auto db = OpenDb();
  ASSERT_TRUE(db.SaveServiceState("last_seq", "42").has_value());
  auto v = db.LoadServiceState("last_seq");
  ASSERT_TRUE(v.has_value());
  ASSERT_TRUE(v->has_value());
  EXPECT_EQ(**v, "42");
}

TEST_F(DatabaseTest, ServiceStateMissingKey) {
  auto db = OpenDb();
  auto v = db.LoadServiceState("nope");
  ASSERT_TRUE(v.has_value());
  EXPECT_FALSE(v->has_value());
}

TEST_F(DatabaseTest, ServiceStateUpsert) {
  auto db = OpenDb();
  ASSERT_TRUE(db.SaveServiceState("k", "v1").has_value());
  ASSERT_TRUE(db.SaveServiceState("k", "v2").has_value());
  auto v = db.LoadServiceState("k");
  ASSERT_TRUE(v.has_value() && v->has_value());
  EXPECT_EQ(**v, "v2");
}

TEST_F(DatabaseTest, MoveConstructor) {
  auto db1 = OpenDb();
  order::Order o{.internal_id = 1, .order_qty = 10, .remaining_qty = 10};
  ASSERT_TRUE(db1.SaveOrder(o).has_value());
  Database db2 = std::move(db1);
  auto loaded = db2.LoadOrder(1);
  EXPECT_TRUE(loaded.has_value());
}

}  // namespace
}  // namespace oems::persistence
