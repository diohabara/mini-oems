#include "core/risk/risk_manager.h"

#include <gtest/gtest.h>

#include <limits>
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

// --- TSE daily price limit (値幅制限) ---

RiskRequest MakeLimitReq(Price price, Quantity qty = 100) {
  return RiskRequest{
      .symbol = Symbol{"7203"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = price,
      .quantity = qty,
  };
}

SymbolConfig TseDailyConfig(Price prev_close, std::int32_t bps) {
  SymbolConfig cfg;
  cfg.previous_close = prev_close;
  cfg.daily_limit_bps = bps;
  return cfg;
}

TEST(RiskManagerDailyLimitTest, UnconfiguredSymbolSkipsCheck) {
  RiskManager risk;
  // 100x move is allowed in absence of config.
  EXPECT_TRUE(risk.Check(MakeLimitReq(300000)).has_value());
}

TEST(RiskManagerDailyLimitTest, ZeroPreviousCloseDisablesCheck) {
  RiskManager risk;
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(/*prev_close=*/0, /*bps=*/1000));
  EXPECT_TRUE(risk.Check(MakeLimitReq(999999)).has_value());
}

TEST(RiskManagerDailyLimitTest, ZeroBpsDisablesCheck) {
  RiskManager risk;
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(/*prev_close=*/3000, /*bps=*/0));
  EXPECT_TRUE(risk.Check(MakeLimitReq(999999)).has_value());
}

TEST(RiskManagerDailyLimitTest, MarketOrdersBypassCheck) {
  RiskManager risk;
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(3000, 100));
  RiskRequest req = MakeLimitReq(0);
  req.type = OrderType::kMarket;
  EXPECT_TRUE(risk.Check(req).has_value());
}

TEST(RiskManagerDailyLimitTest, PriceWithinLimitAccepted) {
  RiskManager risk;
  // 10% band around 3000 = ±300 → [2700, 3300] inclusive.
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(3000, 1000));
  EXPECT_TRUE(risk.Check(MakeLimitReq(3000)).has_value());
  EXPECT_TRUE(risk.Check(MakeLimitReq(3300)).has_value());  // exact upper bound
  EXPECT_TRUE(risk.Check(MakeLimitReq(2700)).has_value());  // exact lower bound
  EXPECT_TRUE(risk.Check(MakeLimitReq(3100)).has_value());
}

TEST(RiskManagerDailyLimitTest, PriceAboveStopHighRejected) {
  RiskManager risk;
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(3000, 1000));
  auto result = risk.Check(MakeLimitReq(3301));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachDailyLimit);
}

TEST(RiskManagerDailyLimitTest, PriceBelowStopLowRejected) {
  RiskManager risk;
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(3000, 1000));
  auto result = risk.Check(MakeLimitReq(2699));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachDailyLimit);
}

TEST(RiskManagerDailyLimitTest, LargePreviousCloseDoesNotOverflow) {
  // Sanity: large previous close + large price — the __int128 cross-multiply
  // must not overflow. Relax notional/max-qty limits so we hit the daily
  // check, not the other risk checks.
  RiskLimits limits;
  limits.max_order_qty = 1'000'000;
  limits.max_notional = std::numeric_limits<Price>::max();
  limits.price_band_bps = 10'000;  // effectively disable price band
  RiskManager risk(limits);
  risk.SetSymbolConfig(Symbol{"7203"}, TseDailyConfig(3'000'000, 1000));
  EXPECT_TRUE(risk.Check(MakeLimitReq(3'000'000, 1)).has_value());
  EXPECT_TRUE(risk.Check(MakeLimitReq(3'300'000, 1)).has_value());
  auto result = risk.Check(MakeLimitReq(3'300'001, 1));
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachDailyLimit);
}

}  // namespace
}  // namespace oems::risk
