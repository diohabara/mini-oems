#include "core/risk/risk_manager.h"

#include <gtest/gtest.h>

#include <thread>

namespace oems::risk {
namespace {

RiskRequest MakeReq(Quantity qty = 100, Price price = 10000) {
  return RiskRequest{
      .symbol = Symbol{"AAPL"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = price,
      .quantity = qty,
  };
}

TEST(RiskManagerTest, ValidOrderPasses) {
  RiskManager risk;
  auto result = risk.Check(MakeReq());
  EXPECT_TRUE(result.has_value());
}

TEST(RiskManagerTest, ZeroQuantityRejected) {
  RiskManager risk;
  auto result = risk.Check(MakeReq(0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidQuantity);
}

TEST(RiskManagerTest, NegativeQuantityRejected) {
  RiskManager risk;
  auto result = risk.Check(MakeReq(-1));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidQuantity);
}

TEST(RiskManagerTest, EmptySymbolRejected) {
  RiskManager risk;
  RiskRequest req = MakeReq();
  req.symbol = Symbol{""};
  auto result = risk.Check(req);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidSymbol);
}

TEST(RiskManagerTest, ZeroPriceLimitRejected) {
  RiskManager risk;
  auto result = risk.Check(MakeReq(100, 0));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidPrice);
}

TEST(RiskManagerTest, MaxQuantityBreached) {
  RiskLimits limits;
  limits.max_order_qty = 100;
  RiskManager risk(limits);
  auto result = risk.Check(MakeReq(101));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachMaxQty);
}

TEST(RiskManagerTest, MaxQuantityAtBoundaryOk) {
  RiskLimits limits;
  limits.max_order_qty = 100;
  RiskManager risk(limits);
  auto result = risk.Check(MakeReq(100));
  EXPECT_TRUE(result.has_value());
}

TEST(RiskManagerTest, NotionalBreached) {
  RiskLimits limits;
  limits.max_notional = 1'000'000;  // $10k
  RiskManager risk(limits);
  // 100 shares * 20000c = 2_000_000 cents -> breach
  auto result = risk.Check(MakeReq(100, 20000));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachNotional);
}

TEST(RiskManagerTest, PriceBandBreachAbove) {
  RiskManager risk;
  risk.SetReferencePrice(Symbol{"AAPL"}, 10000);
  // Default band 10% => band [9000, 11000]
  auto result = risk.Check(MakeReq(100, 12000));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachPriceBand);
}

TEST(RiskManagerTest, PriceBandBreachBelow) {
  RiskManager risk;
  risk.SetReferencePrice(Symbol{"AAPL"}, 10000);
  auto result = risk.Check(MakeReq(100, 8000));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachPriceBand);
}

TEST(RiskManagerTest, PriceBandWithinOk) {
  RiskManager risk;
  risk.SetReferencePrice(Symbol{"AAPL"}, 10000);
  EXPECT_TRUE(risk.Check(MakeReq(100, 10500)).has_value());
  EXPECT_TRUE(risk.Check(MakeReq(100, 9500)).has_value());
}

TEST(RiskManagerTest, NoReferencePriceSkipsBand) {
  RiskManager risk;
  // No reference set -> band check skipped.
  auto result = risk.Check(MakeReq(100, 99999));
  EXPECT_TRUE(result.has_value());
}

TEST(RiskManagerTest, MarketOrderSkipsPriceBandAndNotional) {
  RiskLimits limits;
  limits.max_notional = 1000;
  RiskManager risk(limits);
  risk.SetReferencePrice(Symbol{"AAPL"}, 10000);
  RiskRequest req = MakeReq(100, 0);
  req.type = OrderType::kMarket;
  auto result = risk.Check(req);
  EXPECT_TRUE(result.has_value());
}

TEST(RiskManagerTest, RateLimitBurstRejected) {
  RiskLimits limits;
  limits.max_orders_per_second = 3;
  RiskManager risk(limits);
  EXPECT_TRUE(risk.Check(MakeReq()).has_value());
  EXPECT_TRUE(risk.Check(MakeReq()).has_value());
  EXPECT_TRUE(risk.Check(MakeReq()).has_value());
  auto result = risk.Check(MakeReq());
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachRateLimit);
}

TEST(RiskManagerTest, SetLimitsTakesEffect) {
  RiskManager risk;
  EXPECT_TRUE(risk.Check(MakeReq(5000)).has_value());
  RiskLimits tighter;
  tighter.max_order_qty = 10;
  risk.SetLimits(tighter);
  auto result = risk.Check(MakeReq(5000));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachMaxQty);
}

TEST(RiskManagerTest, GetReferencePrice) {
  RiskManager risk;
  EXPECT_FALSE(risk.GetReferencePrice(Symbol{"AAPL"}).has_value());
  risk.SetReferencePrice(Symbol{"AAPL"}, 10000);
  auto ref = risk.GetReferencePrice(Symbol{"AAPL"});
  ASSERT_TRUE(ref.has_value());
  EXPECT_EQ(*ref, 10000);
}

// --- TSE lot size (売買単位) ---

RiskRequest MakeTseReq(Quantity qty, Price price = 2500) {
  return RiskRequest{
      .symbol = Symbol{"7203"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = price,
      .quantity = qty,
  };
}

TEST(RiskManagerLotSizeTest, UnconfiguredSymbolAcceptsAnyMultiple) {
  RiskManager risk;
  // No SetSymbolConfig call: the rule is disabled by default.
  EXPECT_TRUE(risk.Check(MakeTseReq(150)).has_value());
  EXPECT_TRUE(risk.Check(MakeTseReq(73)).has_value());
}

TEST(RiskManagerLotSizeTest, ZeroLotSizeDisablesCheck) {
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 0;  // explicit disable
  risk.SetSymbolConfig(Symbol{"7203"}, cfg);
  EXPECT_TRUE(risk.Check(MakeTseReq(37)).has_value());
}

TEST(RiskManagerLotSizeTest, MultipleOfLotAccepted) {
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 100;
  risk.SetSymbolConfig(Symbol{"7203"}, cfg);
  EXPECT_TRUE(risk.Check(MakeTseReq(100)).has_value());
  EXPECT_TRUE(risk.Check(MakeTseReq(200)).has_value());
  EXPECT_TRUE(risk.Check(MakeTseReq(1000)).has_value());
}

TEST(RiskManagerLotSizeTest, NonMultipleOfLotRejected) {
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 100;
  risk.SetSymbolConfig(Symbol{"7203"}, cfg);

  auto result = risk.Check(MakeTseReq(150));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachLotSize);

  auto result2 = risk.Check(MakeTseReq(99));
  ASSERT_FALSE(result2.has_value());
  EXPECT_EQ(result2.error(), OemsError::kRiskBreachLotSize);
}

TEST(RiskManagerLotSizeTest, NonStandardLotSizeEnforced) {
  // Some TSE names still trade in 1,000-share lots — ensure the rule is
  // not hard-coded to 100.
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 1000;
  risk.SetSymbolConfig(Symbol{"1301"}, cfg);
  RiskRequest req = MakeTseReq(500);
  req.symbol = Symbol{"1301"};
  auto result = risk.Check(req);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachLotSize);
}

}  // namespace
}  // namespace oems::risk
