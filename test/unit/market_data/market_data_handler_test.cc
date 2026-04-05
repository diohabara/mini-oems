#include "core/market_data/market_data_handler.h"

#include <gtest/gtest.h>

namespace oems::market_data {
namespace {

TEST(MarketDataHandlerTest, OnBboUpdatesSnapshot) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  mdh.OnBbo(Bbo{
      .symbol = Symbol{"AAPL"},
      .bid_price = 9900,
      .bid_qty = 100,
      .ask_price = 10100,
      .ask_qty = 100,
      .timestamp = Now(),
  });
  auto snap = mdh.GetSnapshot(Symbol{"AAPL"});
  ASSERT_TRUE(snap.has_value());
  EXPECT_EQ(snap->bbo.bid_price, 9900);
  EXPECT_EQ(snap->bbo.ask_price, 10100);
}

TEST(MarketDataHandlerTest, OnBboUpdatesReferencePrice) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  mdh.OnBbo(Bbo{
      .symbol = Symbol{"AAPL"},
      .bid_price = 9900,
      .bid_qty = 100,
      .ask_price = 10100,
      .ask_qty = 100,
      .timestamp = Now(),
  });
  auto ref = risk.GetReferencePrice(Symbol{"AAPL"});
  ASSERT_TRUE(ref.has_value());
  EXPECT_EQ(*ref, 10000);  // midpoint
}

TEST(MarketDataHandlerTest, OnTradeUpdatesLastTrade) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  mdh.OnTrade(Symbol{"AAPL"}, 10050, 100);
  auto snap = mdh.GetSnapshot(Symbol{"AAPL"});
  ASSERT_TRUE(snap.has_value());
  EXPECT_EQ(snap->last_trade_price, 10050);
  EXPECT_EQ(snap->last_trade_qty, 100);
}

TEST(MarketDataHandlerTest, MultipleSymbolsIndependent) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  mdh.OnTrade(Symbol{"AAPL"}, 10000, 100);
  mdh.OnTrade(Symbol{"GOOG"}, 20000, 50);
  EXPECT_EQ(mdh.SymbolCount(), 2U);
  EXPECT_EQ(mdh.GetSnapshot(Symbol{"AAPL"})->last_trade_price, 10000);
  EXPECT_EQ(mdh.GetSnapshot(Symbol{"GOOG"})->last_trade_price, 20000);
}

TEST(MarketDataHandlerTest, GetSnapshotUnknownSymbol) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  auto snap = mdh.GetSnapshot(Symbol{"NONE"});
  EXPECT_FALSE(snap.has_value());
}

TEST(MarketDataHandlerTest, BboWithOnlyBidUsesBidAsReference) {
  risk::RiskManager risk;
  MarketDataHandler mdh(risk);
  mdh.OnBbo(Bbo{.symbol = Symbol{"X"},
                .bid_price = 5000,
                .bid_qty = 1,
                .ask_price = 0,
                .ask_qty = 0,
                .timestamp = Now()});
  auto ref = risk.GetReferencePrice(Symbol{"X"});
  ASSERT_TRUE(ref.has_value());
  EXPECT_EQ(*ref, 5000);
}

}  // namespace
}  // namespace oems::market_data
