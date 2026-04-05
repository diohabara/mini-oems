#include <gtest/gtest.h>

#include "core/risk/risk_manager.h"
#include "core/types/instrument.h"

namespace oems::risk {
namespace {

TEST(SymbolConfigTest, GetReturnsNullptrForUnconfiguredSymbol) {
  RiskManager risk;
  EXPECT_EQ(risk.GetSymbolConfig(Symbol{"7203"}), nullptr);
}

TEST(SymbolConfigTest, SetInstallsConfigAndRoundTrips) {
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 100;
  cfg.previous_close = 3000;
  cfg.daily_limit_bps = 1000;
  cfg.tick_bands = {
      TickBand{.low = 0, .high = 2999, .tick = 1},
      TickBand{.low = 3000, .high = 4999, .tick = 5},
  };

  risk.SetSymbolConfig(Symbol{"7203"}, cfg);

  const SymbolConfig* got = risk.GetSymbolConfig(Symbol{"7203"});
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(got->symbol.value, "7203");
  EXPECT_EQ(got->lot_size, 100);
  EXPECT_EQ(got->previous_close, 3000);
  EXPECT_EQ(got->daily_limit_bps, 1000);
  ASSERT_EQ(got->tick_bands.size(), 2U);
  EXPECT_EQ(got->tick_bands[0].tick, 1);
  EXPECT_EQ(got->tick_bands[1].tick, 5);
}

TEST(SymbolConfigTest, SetOverwritesPreviousConfig) {
  RiskManager risk;

  SymbolConfig first;
  first.lot_size = 100;
  risk.SetSymbolConfig(Symbol{"7203"}, first);

  SymbolConfig second;
  second.lot_size = 1000;
  risk.SetSymbolConfig(Symbol{"7203"}, second);

  const SymbolConfig* got = risk.GetSymbolConfig(Symbol{"7203"});
  ASSERT_NE(got, nullptr);
  EXPECT_EQ(got->lot_size, 1000);
}

TEST(SymbolConfigTest, ConfigDoesNotChangeCheckBehaviourYet) {
  // W2 installs the plumbing only: the three new check hooks are stubs
  // that return success. A request that would violate lot/tick/daily
  // rules in later PRs must still be accepted here so W2 introduces
  // zero behavioural change.
  RiskManager risk;
  SymbolConfig cfg;
  cfg.lot_size = 100;  // 150 is not a multiple — W3 would reject.
  cfg.previous_close = 3000;
  cfg.daily_limit_bps = 100;                                       // ±1% — 5000 would blow through.
  cfg.tick_bands = {TickBand{.low = 0, .high = 9999, .tick = 5}};  // 5001 not on grid.
  risk.SetSymbolConfig(Symbol{"7203"}, cfg);

  RiskRequest req{
      .symbol = Symbol{"7203"},
      .side = Side::kBuy,
      .type = OrderType::kLimit,
      .price = 5001,
      .quantity = 150,
  };
  auto result = risk.Check(req);
  EXPECT_TRUE(result.has_value());
}

}  // namespace
}  // namespace oems::risk
