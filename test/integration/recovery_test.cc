#include <gtest/gtest.h>

#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "core/persistence/database.h"
#include "core/risk/risk_manager.h"

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

TEST(RecoveryIntegration, OrderManagerPersistsAndRestoresRuntimeState) {
  std::string path = "/tmp/oems-order-manager-recovery.db";
  std::remove(path.c_str());

  OrderId sell_id = 0;

  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    ASSERT_TRUE(db->Migrate().has_value());

    risk::RiskManager risk;
    matching::MatchingEngine engine;
    order::OrderManager om(risk, engine, &*db);

    auto sell = om.SubmitOrder(order::NewOrderRequest{.client_order_id = "S1",
                                                      .symbol = Symbol{"AAPL"},
                                                      .side = Side::kSell,
                                                      .type = OrderType::kLimit,
                                                      .price = 10000,
                                                      .quantity = 100});
    ASSERT_TRUE(sell.has_value());
    sell_id = sell->internal_id;

    auto buy = om.SubmitOrder(order::NewOrderRequest{.client_order_id = "B1",
                                                     .symbol = Symbol{"AAPL"},
                                                     .side = Side::kBuy,
                                                     .type = OrderType::kLimit,
                                                     .price = 10000,
                                                     .quantity = 40});
    ASSERT_TRUE(buy.has_value());
    EXPECT_EQ(buy->status, OrderStatus::kFilled);
  }

  {
    auto db = persistence::Database::Open(path);
    ASSERT_TRUE(db.has_value());
    ASSERT_TRUE(db->Migrate().has_value());

    risk::RiskManager risk;
    matching::MatchingEngine engine;
    order::OrderManager om(risk, engine, &*db);
    ASSERT_TRUE(om.RestoreFromDatabase().has_value());

    auto restored_sell = om.GetOrder(sell_id);
    ASSERT_TRUE(restored_sell.has_value());
    EXPECT_EQ(restored_sell->status, OrderStatus::kPartiallyFilled);
    EXPECT_EQ(restored_sell->filled_qty, 40);
    EXPECT_EQ(restored_sell->remaining_qty, 60);

    auto restored_events = om.GetEvents(sell_id);
    EXPECT_FALSE(restored_events.empty());

    auto fills = om.GetAllExecutions();
    ASSERT_EQ(fills.size(), 1U);
    EXPECT_EQ(fills[0].quantity, 40);

    auto crossed = om.SubmitOrder(order::NewOrderRequest{.client_order_id = "B2",
                                                         .symbol = Symbol{"AAPL"},
                                                         .side = Side::kBuy,
                                                         .type = OrderType::kLimit,
                                                         .price = 10000,
                                                         .quantity = 60});
    ASSERT_TRUE(crossed.has_value());
    EXPECT_EQ(crossed->status, OrderStatus::kFilled);

    auto completed_sell = om.GetOrder(sell_id);
    ASSERT_TRUE(completed_sell.has_value());
    EXPECT_EQ(completed_sell->status, OrderStatus::kFilled);
  }

  std::remove(path.c_str());
}

}  // namespace
}  // namespace oems::integration
