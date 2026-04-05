#ifndef OEMS_CORE_RISK_RISK_MANAGER_H_
#define OEMS_CORE_RISK_RISK_MANAGER_H_

/**
 * @file risk_manager.h
 * @brief Pre-trade risk controls (quantity, notional, price band, rate).
 */

#include <deque>
#include <optional>
#include <string>
#include <unordered_map>

#include "core/types/error.h"
#include "core/types/instrument.h"
#include "core/types/types.h"

namespace oems::risk {

/**
 * @brief Check request submitted to the risk manager.
 *
 * Kept minimal and independent of the Order module so risk can be
 * invoked before an order is assigned an internal id.
 */
struct RiskRequest {
  Symbol symbol;
  Side side{Side::kBuy};
  OrderType type{OrderType::kLimit};
  Price price{0};  ///< Ignored for market orders.
  Quantity quantity{0};
};

/**
 * @brief Runtime-tunable risk limits.
 */
struct RiskLimits {
  Quantity max_order_qty = 10'000;
  Price max_notional = 100'000'000;    ///< $1,000,000 in cents.
  std::int32_t price_band_bps = 1000;  ///< 10% = 1000 bps.
  std::int32_t max_orders_per_second = 100;
};

/**
 * @brief Pre-trade risk checks.
 *
 * Stateless with respect to orders; holds only reference prices and a
 * sliding-window rate counter.
 */
class RiskManager {
 public:
  explicit RiskManager(RiskLimits limits = {});

  /**
   * @brief Run all configured checks.
   * @return Success, or the first error code encountered.
   */
  auto Check(const RiskRequest& req) -> Result<void>;

  void SetLimits(RiskLimits limits) { limits_ = limits; }
  [[nodiscard]] auto GetLimits() const -> const RiskLimits& { return limits_; }

  /**
   * @brief Update the reference price for a symbol (used for price-band).
   */
  void SetReferencePrice(const Symbol& symbol, Price price);

  /**
   * @brief Look up the current reference price for a symbol.
   */
  [[nodiscard]] auto GetReferencePrice(const Symbol& symbol) const -> std::optional<Price>;

  /**
   * @brief Install / replace the per-symbol TSE configuration
   *        (lot size, tick bands, previous close, daily limit).
   *
   * Symbols without a config fall through every TSE-specific check.
   */
  void SetSymbolConfig(const Symbol& symbol, SymbolConfig config);

  /**
   * @brief Look up the current config for a symbol.
   * @return A pointer into the internal map, or nullptr if unconfigured.
   *         The pointer is invalidated by the next @c SetSymbolConfig call.
   */
  [[nodiscard]] auto GetSymbolConfig(const Symbol& symbol) const -> const SymbolConfig*;

 private:
  auto CheckPriceBand(const RiskRequest& req) const -> Result<void>;
  auto CheckRateLimit() -> Result<void>;
  // TSE-specific checks. Each returns success when no config is set for
  // the symbol or the relevant field is at its sentinel (disabled) value.
  // Full implementations land in follow-up PRs; W2 keeps them as no-ops
  // so the seam is in place for W3/W4/W5 without introducing behavioural
  // change.
  auto CheckLotSize(const RiskRequest& req) const -> Result<void>;
  auto CheckTickSize(const RiskRequest& req) const -> Result<void>;
  auto CheckDailyLimit(const RiskRequest& req) const -> Result<void>;

  RiskLimits limits_;
  std::unordered_map<std::string, Price> reference_prices_;
  std::unordered_map<std::string, SymbolConfig> configs_;
  std::deque<Timestamp> recent_orders_;
};

}  // namespace oems::risk

#endif  // OEMS_CORE_RISK_RISK_MANAGER_H_
