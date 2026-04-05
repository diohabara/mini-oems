#include <gtest/gtest.h>

#include "core/algo/algo_engine.h"
#include "core/market_data/market_data_handler.h"
#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "core/risk/risk_manager.h"

namespace oems::integration {
namespace {

TEST(OrderFlowIntegration, BuyThenSellCrossesAndFills) {
  risk::RiskManager risk;
  matching::MatchingEngine engine;
  order::OrderManager om(risk, engine);

  auto buy = om.SubmitOrder(order::NewOrderRequest{.client_order_id = "B1",
                                                   .symbol = Symbol{"AAPL"},
                                                   .side = Side::kBuy,
                                                   .type = OrderType::kLimit,
                                                   .price = 10000,
                                                   .quantity = 100});
  ASSERT_TRUE(buy.has_value());
  EXPECT_EQ(buy->status, OrderStatus::kAccepted);

  auto sell = om.SubmitOrder(order::NewOrderRequest{.client_order_id = "S1",
                                                    .symbol = Symbol{"AAPL"},
                                                    .side = Side::kSell,
                                                    .type = OrderType::kLimit,
                                                    .price = 10000,
                                                    .quantity = 100});
  ASSERT_TRUE(sell.has_value());
  EXPECT_EQ(sell->status, OrderStatus::kFilled);

  auto refreshed_buy = om.GetOrder(buy->internal_id);
  ASSERT_TRUE(refreshed_buy.has_value());
  EXPECT_EQ(refreshed_buy->status, OrderStatus::kFilled);

  auto fills = om.GetAllExecutions();
  ASSERT_EQ(fills.size(), 1U);
  EXPECT_EQ(fills[0].quantity, 100);
}

TEST(OrderFlowIntegration, RiskRejectionBeforeMatching) {
  risk::RiskLimits tight;
  tight.max_order_qty = 50;
  risk::RiskManager risk(tight);
  matching::MatchingEngine engine;
  order::OrderManager om(risk, engine);

  auto result = om.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                                      .side = Side::kBuy,
                                                      .type = OrderType::kLimit,
                                                      .price = 10000,
                                                      .quantity = 100});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachMaxQty);
  // Book should be empty.
  EXPECT_EQ(engine.BookCount(), 0U);
}

TEST(OrderFlowIntegration, CancelBeforeFill) {
  risk::RiskManager risk;
  matching::MatchingEngine engine;
  order::OrderManager om(risk, engine);

  auto submitted = om.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                                         .side = Side::kBuy,
                                                         .type = OrderType::kLimit,
                                                         .price = 10000,
                                                         .quantity = 100});
  ASSERT_TRUE(submitted.has_value());
  auto cancelled = om.CancelOrder(order::CancelOrderRequest{.order_id = submitted->internal_id});
  ASSERT_TRUE(cancelled.has_value());
  EXPECT_EQ(cancelled->status, OrderStatus::kCancelled);
  EXPECT_EQ(om.GetAllExecutions().size(), 0U);
}

TEST(OrderFlowIntegration, MarketDataFeedsRisk) {
  risk::RiskManager risk;
  matching::MatchingEngine engine;
  order::OrderManager om(risk, engine);
  market_data::MarketDataHandler mdh(risk);
  mdh.OnBbo(market_data::Bbo{.symbol = Symbol{"AAPL"},
                             .bid_price = 9900,
                             .bid_qty = 100,
                             .ask_price = 10100,
                             .ask_qty = 100,
                             .timestamp = Now()});
  // Price-band breach (default 10% band around 10000 reference).
  auto result = om.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                                      .side = Side::kBuy,
                                                      .type = OrderType::kLimit,
                                                      .price = 12000,
                                                      .quantity = 10});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachPriceBand);
}

TEST(OrderFlowIntegration, TwapSlicingThroughOrderManager) {
  risk::RiskManager risk;
  matching::MatchingEngine engine;
  order::OrderManager om(risk, engine);
  algo::AlgoEngine algos(om);
  algo::AlgoRequest req;
  req.type = algo::AlgoType::kTwap;
  req.params.parent.symbol = Symbol{"AAPL"};
  req.params.parent.side = Side::kBuy;
  req.params.parent.type = OrderType::kLimit;
  req.params.parent.price = 10000;
  req.params.parent.quantity = 1000;
  req.params.num_slices = 4;
  req.params.duration = std::chrono::seconds(60);
  auto run_id = algos.StartAlgo(req);
  ASSERT_TRUE(run_id.has_value());
  auto run = algos.GetRun(*run_id);
  ASSERT_TRUE(run.has_value());
  EXPECT_EQ(run->submitted, 4U);
  EXPECT_EQ(om.OrderCount(), 4U);
}

}  // namespace
}  // namespace oems::integration
