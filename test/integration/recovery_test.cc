#include <gtest/gtest.h>

#include "core/persistence/database.h"

namespace oems::integration {
namespace {

TEST(RecoveryIntegration, SaveAndReloadOrdersAndEvents) {
  // Phase 1: write some orders and events.
  {
    auto db = persistence::Database::Open(":memory:");
    // ":memory:" database is bound to this connection, so we need a file
    // on disk to truly simulate restart.  Use a temporary file.
  }
  std::string path = "/tmp/oems-recovery-test.db";
  std::remove(path.c_str());

  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    ASSERT_TRUE(db->Migrate().has_value());
    order::Order o1{.internal_id = 1,
                    .client_order_id = "A",
                    .symbol = Symbol{"AAPL"},
                    .side = Side::kBuy,
                    .type = OrderType::kLimit,
                    .price = 10000,
                    .order_qty = 100,
                    .remaining_qty = 100,
                    .status = OrderStatus::kAccepted,
                    .created_at = Now(),
                    .updated_at = Now()};
    order::Order o2{.internal_id = 2,
                    .client_order_id = "B",
                    .symbol = Symbol{"AAPL"},
                    .side = Side::kSell,
                    .type = OrderType::kLimit,
                    .price = 10100,
                    .order_qty = 50,
                    .remaining_qty = 50,
                    .status = OrderStatus::kAccepted,
                    .created_at = Now(),
                    .updated_at = Now()};
    ASSERT_TRUE(db->SaveOrder(o1).has_value());
    ASSERT_TRUE(db->SaveOrder(o2).has_value());
    ASSERT_TRUE(db->AppendEvent(order::OrderEvent{.event_id = 1,
                                                  .order_id = 1,
                                                  .type = order::EventType::kAccepted,
                                                  .timestamp = Now(),
                                                  .detail = "ok"})
                    .has_value());
  }

  // Phase 2: simulate restart by opening the file again.
  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    // Schema is preserved across connections.
    auto orders = db->LoadOrders();
    ASSERT_TRUE(orders.has_value());
    EXPECT_EQ(orders->size(), 2U);
    auto events = db->LoadAllEvents();
    ASSERT_TRUE(events.has_value());
    EXPECT_EQ(events->size(), 1U);
  }

  std::remove(path.c_str());
}

TEST(RecoveryIntegration, ServiceStatePersists) {
  std::string path = "/tmp/oems-svc-state-test.db";
  std::remove(path.c_str());

  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    ASSERT_TRUE(db->Migrate().has_value());
    ASSERT_TRUE(db->SaveServiceState("last_seq_num", "123").has_value());
  }
  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    auto v = db->LoadServiceState("last_seq_num");
    ASSERT_TRUE(v.has_value() && v->has_value());
    EXPECT_EQ(**v, "123");
  }
  std::remove(path.c_str());
}

}  // namespace
}  // namespace oems::integration
