#ifndef OEMS_CORE_MARKET_DATA_MARKET_DATA_HANDLER_H_
#define OEMS_CORE_MARKET_DATA_MARKET_DATA_HANDLER_H_

/**
 * @file market_data_handler.h
 * @brief Ingest BBO / tick updates, maintain snapshots, drive reference prices.
 */

#include <optional>
#include <string>
#include <unordered_map>

#include "core/risk/risk_manager.h"
#include "core/types/types.h"

namespace oems::market_data {

/**
 * @brief Best bid / offer for one symbol.
 */
struct Bbo {
  Symbol symbol;
  Price bid_price{0};
  Quantity bid_qty{0};
  Price ask_price{0};
  Quantity ask_qty{0};
  Timestamp timestamp{};
};

/**
 * @brief Aggregated snapshot per symbol.
 */
struct MarketSnapshot {
  Bbo bbo;
  Price last_trade_price{0};
  Quantity last_trade_qty{0};
  Timestamp last_update{};
};

/**
 * @brief Maintains per-symbol market snapshots and updates risk
 *        reference prices so price-band checks use recent data.
 *
 * The handler is deliberately isolated from the order path: it never
 * calls into OrderManager.  Order flow may read snapshots but is not
 * blocked by market-data bursts.
 */
class MarketDataHandler {
 public:
  explicit MarketDataHandler(risk::RiskManager& risk);

  /**
   * @brief Ingest a BBO update.
   *
   * Updates the snapshot and pushes the mid-price (or bid fallback) to
   * the risk manager as the reference price.
   */
  void OnBbo(const Bbo& bbo);

  /**
   * @brief Inject a synthetic trade tick for demos/tests.
   */
  void OnTrade(const Symbol& symbol, Price price, Quantity qty);

  /**
   * @brief Return the current snapshot for a symbol if it exists.
   */
  [[nodiscard]] auto GetSnapshot(const Symbol& symbol) const -> std::optional<MarketSnapshot>;

  /**
   * @brief Number of symbols currently tracked.
   */
  [[nodiscard]] auto SymbolCount() const -> std::size_t { return snapshots_.size(); }

 private:
  risk::RiskManager& risk_;
  std::unordered_map<std::string, MarketSnapshot> snapshots_;
};

}  // namespace oems::market_data

#endif  // OEMS_CORE_MARKET_DATA_MARKET_DATA_HANDLER_H_
