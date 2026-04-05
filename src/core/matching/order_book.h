#ifndef OEMS_CORE_MATCHING_ORDER_BOOK_H_
#define OEMS_CORE_MATCHING_ORDER_BOOK_H_

/**
 * @file order_book.h
 * @brief Price-time-priority order book for a single symbol.
 */

#include <deque>
#include <functional>
#include <map>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/types/error.h"
#include "core/types/types.h"

namespace oems::matching {

/**
 * @brief A single resting order at a price level.
 */
struct BookEntry {
  OrderId order_id;
  Side side;
  Price price;
  Quantity remaining_qty;
  Timestamp timestamp;
};

/**
 * @brief A fill generated when two orders match.
 *
 * @c aggressive_order_id is the incoming order that crossed the book;
 * @c passive_order_id is the resting order it matched against.
 * Execution price is always the passive order's price (price-time priority).
 */
struct Fill {
  ExecutionId execution_id{0};
  Symbol symbol;
  OrderId aggressive_order_id{0};
  OrderId passive_order_id{0};
  Side aggressive_side{Side::kBuy};
  Price price{0};
  Quantity quantity{0};
  Timestamp timestamp{};
};

/**
 * @brief Snapshot of one price level.
 */
struct PriceLevel {
  Price price;
  Quantity total_qty;
  std::size_t order_count;
};

/**
 * @brief The result of adding an order to the book.
 *
 * If the order fully matched against resting liquidity, @c resting is empty
 * and @c fills contains the produced fills.  If the order partially matched
 * (limit) or did not match at all, @c resting holds the remaining portion.
 */
struct AddOrderResult {
  std::vector<Fill> fills;
  std::optional<BookEntry> resting;
};

/**
 * @brief Price-time-priority order book for one symbol.
 *
 * Bids are kept in descending price order (best bid = highest price).
 * Asks are kept in ascending price order (best ask = lowest price).
 * Within each price level, orders are FIFO by insertion time.
 *
 * @note This class is single-writer.  Callers must serialize access
 *       externally (per architecture doc: single-writer matching per symbol).
 */
class OrderBook {
 public:
  /**
   * @brief Construct an empty book for a given symbol.
   * @param symbol The instrument symbol.
   */
  explicit OrderBook(Symbol symbol);

  /**
   * @brief Accept a new order and match it against resting liquidity.
   * @param id Unique order id.
   * @param side Buy or sell.
   * @param type Limit or market.
   * @param price Limit price in cents (ignored for market orders).
   * @param qty Order quantity; must be > 0.
   * @return AddOrderResult on success, or kInvalidQuantity /
   *         kDuplicateOrder / kNoLiquidity on failure.
   */
  auto AddOrder(OrderId id, Side side, OrderType type, Price price, Quantity qty)
      -> Result<AddOrderResult>;

  /**
   * @brief Cancel a resting order.
   * @param id The order to cancel.
   * @return The removed book entry, or kOrderNotFound.
   */
  auto CancelOrder(OrderId id) -> Result<BookEntry>;

  /**
   * @brief Restore a resting order directly onto the book without matching.
   */
  auto RestoreRestingOrder(OrderId id, Side side, Price price, Quantity qty, Timestamp timestamp)
      -> Result<void>;

  /**
   * @brief Return the bid side as sorted price levels (best first).
   */
  [[nodiscard]] auto Bids() const -> std::vector<PriceLevel>;

  /**
   * @brief Return the ask side as sorted price levels (best first).
   */
  [[nodiscard]] auto Asks() const -> std::vector<PriceLevel>;

  /**
   * @brief Return the symbol this book tracks.
   */
  [[nodiscard]] auto GetSymbol() const -> const Symbol& { return symbol_; }

  /**
   * @brief Return the total number of resting orders in the book.
   */
  [[nodiscard]] auto OrderCount() const -> std::size_t { return id_index_.size(); }

 private:
  /// @brief Match an incoming order against the opposite side.
  /// @return The fills produced; mutates @c remaining_qty.
  auto MatchAgainst(OrderId aggressive_id, Side aggressive_side, OrderType type, Price limit_price,
                    Quantity& remaining_qty, Timestamp timestamp) -> std::vector<Fill>;

  /// @brief Rest the remaining quantity on the book.
  auto Rest(OrderId id, Side side, Price price, Quantity qty, Timestamp timestamp) -> BookEntry;

  Symbol symbol_;

  /// Bids ordered by highest price first; FIFO within each level.
  std::map<Price, std::deque<BookEntry>, std::greater<>> bids_;

  /// Asks ordered by lowest price first; FIFO within each level.
  std::map<Price, std::deque<BookEntry>, std::less<>> asks_;

  /// Index from OrderId to the price level that holds it, for O(log n) cancel.
  struct IndexEntry {
    Side side;
    Price price;
  };
  std::unordered_map<OrderId, IndexEntry> id_index_;

  /// Monotonically increasing execution id generator.
  ExecutionId next_execution_id_{1};
};

}  // namespace oems::matching

#endif  // OEMS_CORE_MATCHING_ORDER_BOOK_H_
