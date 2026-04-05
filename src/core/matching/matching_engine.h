#ifndef OEMS_CORE_MATCHING_MATCHING_ENGINE_H_
#define OEMS_CORE_MATCHING_MATCHING_ENGINE_H_

/**
 * @file matching_engine.h
 * @brief Multi-symbol matching engine routing to per-symbol order books.
 */

#include <string>
#include <unordered_map>
#include <algorithm>

#include "core/order/order.h"
#include "core/matching/order_book.h"
#include "core/types/error.h"
#include "core/types/types.h"

namespace oems::matching {

/**
 * @brief Routes orders to the correct per-symbol @c OrderBook.
 *
 * Books are created lazily on first order for a symbol.
 */
class MatchingEngine {
 public:
  MatchingEngine() = default;

  /**
   * @brief Submit an order for matching.
   * @param symbol Instrument symbol.
   * @param id Unique order id.
   * @param side Buy or sell.
   * @param type Limit or market.
   * @param price Limit price in cents (ignored for market orders).
   * @param qty Order quantity; must be > 0.
   * @return AddOrderResult on success, or domain error.
   */
  auto AddOrder(const Symbol& symbol, OrderId id, Side side, OrderType type, Price price,
                Quantity qty) -> Result<AddOrderResult>;

  /**
   * @brief Cancel a resting order in the specified book.
   */
  auto CancelOrder(const Symbol& symbol, OrderId id) -> Result<BookEntry>;

  /**
   * @brief Look up the book for a symbol (read-only).
   * @return Pointer to book, or kBookNotFound.
   */
  [[nodiscard]] auto GetBook(const Symbol& symbol) const -> Result<const OrderBook*>;

  /**
   * @brief Restore a persisted resting limit order directly into its book.
   */
  auto RestoreRestingOrder(const order::Order& order) -> Result<void>;

  /**
   * @brief Seed the next globally unique execution id after recovery.
   */
  void SeedNextExecutionId(ExecutionId next_execution_id) {
    next_execution_id_ = std::max<ExecutionId>(1, next_execution_id);
  }

  /**
   * @brief Number of distinct symbols currently tracked.
   */
  [[nodiscard]] auto BookCount() const -> std::size_t { return books_.size(); }

 private:
  auto GetOrCreateBook(const Symbol& symbol) -> OrderBook&;

  std::unordered_map<std::string, OrderBook> books_;
  ExecutionId next_execution_id_{1};
};

}  // namespace oems::matching

#endif  // OEMS_CORE_MATCHING_MATCHING_ENGINE_H_
