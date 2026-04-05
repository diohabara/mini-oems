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

 private:
  auto CheckPriceBand(const RiskRequest& req) const -> Result<void>;
  auto CheckRateLimit() -> Result<void>;

  RiskLimits limits_;
  std::unordered_map<std::string, Price> reference_prices_;
  std::deque<Timestamp> recent_orders_;
};

}  // namespace oems::risk

#endif  // OEMS_CORE_RISK_RISK_MANAGER_H_
