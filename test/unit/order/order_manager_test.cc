#include "core/order/order_manager.h"

#include <gtest/gtest.h>

namespace oems::order {
namespace {

class OrderManagerTest : public ::testing::Test {
 protected:
  risk::RiskManager risk_;
  matching::MatchingEngine engine_;
  OrderManager om_{risk_, engine_};

  NewOrderRequest MakeReq(Side side = Side::kBuy, Quantity qty = 100, Price price = 10000,
                          OrderType type = OrderType::kLimit) {
    return NewOrderRequest{
        .client_order_id = "CL1",
        .symbol = Symbol{"AAPL"},
        .side = side,
        .type = type,
        .price = price,
        .quantity = qty,
    };
  }
};

TEST_F(OrderManagerTest, SubmitLimitOrderAccepted) {
  auto result = om_.SubmitOrder(MakeReq());
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->status, OrderStatus::kAccepted);
  EXPECT_EQ(result->remaining_qty, 100);
  EXPECT_EQ(result->filled_qty, 0);
  EXPECT_EQ(om_.OrderCount(), 1U);
}

TEST_F(OrderManagerTest, MonotonicIds) {
  auto a = om_.SubmitOrder(MakeReq());
  auto b = om_.SubmitOrder(MakeReq());
  auto c = om_.SubmitOrder(MakeReq());
  ASSERT_TRUE(a && b && c);
  EXPECT_LT(a->internal_id, b->internal_id);
  EXPECT_LT(b->internal_id, c->internal_id);
}

TEST_F(OrderManagerTest, CrossingOrdersFillBothSides) {
  auto sell = om_.SubmitOrder(MakeReq(Side::kSell));
  ASSERT_TRUE(sell);
  auto buy = om_.SubmitOrder(MakeReq(Side::kBuy));
  ASSERT_TRUE(buy);
  EXPECT_EQ(buy->status, OrderStatus::kFilled);
  EXPECT_EQ(buy->filled_qty, 100);

  auto sell_after = om_.GetOrder(sell->internal_id);
  ASSERT_TRUE(sell_after);
  EXPECT_EQ(sell_after->status, OrderStatus::kFilled);
  EXPECT_EQ(sell_after->filled_qty, 100);
}

TEST_F(OrderManagerTest, PartialFillMarksPartiallyFilled) {
  ASSERT_TRUE(om_.SubmitOrder(MakeReq(Side::kSell, 30)));
  auto buy = om_.SubmitOrder(MakeReq(Side::kBuy, 100));
  ASSERT_TRUE(buy);
  EXPECT_EQ(buy->status, OrderStatus::kPartiallyFilled);
  EXPECT_EQ(buy->filled_qty, 30);
  EXPECT_EQ(buy->remaining_qty, 70);
}

TEST_F(OrderManagerTest, MarketOrderFills) {
  ASSERT_TRUE(om_.SubmitOrder(MakeReq(Side::kSell, 100, 10000)));
  auto mkt = om_.SubmitOrder(MakeReq(Side::kBuy, 100, 0, OrderType::kMarket));
  ASSERT_TRUE(mkt);
  EXPECT_EQ(mkt->status, OrderStatus::kFilled);
}

TEST_F(OrderManagerTest, MarketOrderNoLiquidityRejected) {
  auto mkt = om_.SubmitOrder(MakeReq(Side::kBuy, 100, 0, OrderType::kMarket));
  ASSERT_TRUE(mkt);
  EXPECT_EQ(mkt->status, OrderStatus::kRejected);
}

TEST_F(OrderManagerTest, CancelRestingOrder) {
  auto created = om_.SubmitOrder(MakeReq());
  ASSERT_TRUE(created);
  auto cancelled = om_.CancelOrder(CancelOrderRequest{.order_id = created->internal_id});
  ASSERT_TRUE(cancelled);
  EXPECT_EQ(cancelled->status, OrderStatus::kCancelled);
}

TEST_F(OrderManagerTest, CancelUnknownOrder) {
  auto result = om_.CancelOrder(CancelOrderRequest{.order_id = 9999});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

TEST_F(OrderManagerTest, CancelFilledOrderRejected) {
  auto sell = om_.SubmitOrder(MakeReq(Side::kSell));
  ASSERT_TRUE(om_.SubmitOrder(MakeReq(Side::kBuy)));  // fills sell
  auto result = om_.CancelOrder(CancelOrderRequest{.order_id = sell->internal_id});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidStateTransition);
}

TEST_F(OrderManagerTest, RiskRejectionStoresOrderWithRejectedStatus) {
  risk::RiskLimits tight;
  tight.max_order_qty = 10;
  risk_.SetLimits(tight);
  auto result = om_.SubmitOrder(MakeReq(Side::kBuy, 100));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachMaxQty);
  // Verify the rejected order is in the store.
  auto orders = om_.GetOrders(std::nullopt, OrderStatus::kRejected);
  EXPECT_EQ(orders.size(), 1U);
}

TEST_F(OrderManagerTest, InvalidRequestRejected) {
  auto result = om_.SubmitOrder(MakeReq(Side::kBuy, 0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidQuantity);
}

TEST_F(OrderManagerTest, FilterBySymbol) {
  NewOrderRequest aapl = MakeReq();
  NewOrderRequest goog = MakeReq();
  goog.symbol = Symbol{"GOOG"};
  ASSERT_TRUE(om_.SubmitOrder(aapl));
  ASSERT_TRUE(om_.SubmitOrder(goog));
  auto only_aapl = om_.GetOrders(Symbol{"AAPL"}, std::nullopt);
  EXPECT_EQ(only_aapl.size(), 1U);
  EXPECT_EQ(only_aapl[0].symbol.value, "AAPL");
}

TEST_F(OrderManagerTest, FilterByStatus) {
  ASSERT_TRUE(om_.SubmitOrder(MakeReq()));
  ASSERT_TRUE(om_.SubmitOrder(MakeReq()));
  auto accepted = om_.GetOrders(std::nullopt, OrderStatus::kAccepted);
  EXPECT_EQ(accepted.size(), 2U);
  auto filled = om_.GetOrders(std::nullopt, OrderStatus::kFilled);
  EXPECT_EQ(filled.size(), 0U);
}

TEST_F(OrderManagerTest, EventsRecordedForOrder) {
  auto order = om_.SubmitOrder(MakeReq());
  ASSERT_TRUE(order);
  auto events = om_.GetEvents(order->internal_id);
  ASSERT_GE(events.size(), 2U);
  EXPECT_EQ(events[0].type, EventType::kNewRequested);
  EXPECT_EQ(events[1].type, EventType::kAccepted);
}

TEST_F(OrderManagerTest, AllExecutionsTracked) {
  ASSERT_TRUE(om_.SubmitOrder(MakeReq(Side::kSell)));
  ASSERT_TRUE(om_.SubmitOrder(MakeReq(Side::kBuy)));
  auto fills = om_.GetAllExecutions();
  EXPECT_EQ(fills.size(), 1U);
  EXPECT_EQ(fills[0].quantity, 100);
}

TEST_F(OrderManagerTest, GetOrderNotFound) {
  auto result = om_.GetOrder(9999);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

}  // namespace
}  // namespace oems::order
