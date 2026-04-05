#include "core/matching/order_book.h"

#include <algorithm>
#include <utility>

namespace oems::matching {

OrderBook::OrderBook(Symbol symbol) : symbol_(std::move(symbol)) {}

auto OrderBook::AddOrder(OrderId id, Side side, OrderType type, Price price, Quantity qty)
    -> Result<AddOrderResult> {
  if (qty <= 0) {
    return std::unexpected(OemsError::kInvalidQuantity);
  }
  if (type == OrderType::kLimit && price <= 0) {
    return std::unexpected(OemsError::kInvalidPrice);
  }
  if (id_index_.contains(id)) {
    return std::unexpected(OemsError::kDuplicateOrder);
  }

  auto timestamp = Now();
  Quantity remaining = qty;
  auto fills = MatchAgainst(id, side, type, price, remaining, timestamp);

  AddOrderResult result;
  result.fills = std::move(fills);

  if (remaining > 0) {
    if (type == OrderType::kMarket) {
      // Market orders never rest.
      return result;
    }
    result.resting = Rest(id, side, price, remaining, timestamp);
  }
  return result;
}

auto OrderBook::CancelOrder(OrderId id) -> Result<BookEntry> {
  auto idx_it = id_index_.find(id);
  if (idx_it == id_index_.end()) {
    return std::unexpected(OemsError::kOrderNotFound);
  }
  auto [side, price] = idx_it->second;

  auto cancel_in = [&](auto& book_side) -> Result<BookEntry> {
    auto level_it = book_side.find(price);
    if (level_it == book_side.end()) {
      return std::unexpected(OemsError::kOrderNotFound);
    }
    auto& level = level_it->second;
    auto entry_it =
        std::ranges::find_if(level, [&](const BookEntry& e) { return e.order_id == id; });
    if (entry_it == level.end()) {
      return std::unexpected(OemsError::kOrderNotFound);
    }
    BookEntry removed = *entry_it;
    level.erase(entry_it);
    if (level.empty()) {
      book_side.erase(level_it);
    }
    id_index_.erase(id);
    return removed;
  };

  if (side == Side::kBuy) {
    return cancel_in(bids_);
  }
  return cancel_in(asks_);
}

auto OrderBook::Bids() const -> std::vector<PriceLevel> {
  std::vector<PriceLevel> levels;
  levels.reserve(bids_.size());
  for (const auto& [price, queue] : bids_) {
    Quantity total = 0;
    for (const auto& entry : queue) {
      total += entry.remaining_qty;
    }
    levels.push_back({.price = price, .total_qty = total, .order_count = queue.size()});
  }
  return levels;
}

auto OrderBook::Asks() const -> std::vector<PriceLevel> {
  std::vector<PriceLevel> levels;
  levels.reserve(asks_.size());
  for (const auto& [price, queue] : asks_) {
    Quantity total = 0;
    for (const auto& entry : queue) {
      total += entry.remaining_qty;
    }
    levels.push_back({.price = price, .total_qty = total, .order_count = queue.size()});
  }
  return levels;
}

auto OrderBook::MatchAgainst(OrderId aggressive_id, Side aggressive_side, OrderType type,
                             Price limit_price, Quantity& remaining_qty, Timestamp timestamp)
    -> std::vector<Fill> {
  std::vector<Fill> fills;

  auto match_side = [&](auto& opposite_side, auto price_acceptable) {
    while (remaining_qty > 0 && !opposite_side.empty()) {
      auto best_it = opposite_side.begin();
      Price best_price = best_it->first;
      if (type == OrderType::kLimit && !price_acceptable(best_price)) {
        break;
      }
      auto& queue = best_it->second;
      while (remaining_qty > 0 && !queue.empty()) {
        BookEntry& passive = queue.front();
        Quantity trade_qty = std::min(remaining_qty, passive.remaining_qty);

        fills.push_back(Fill{
            .execution_id = next_execution_id_++,
            .symbol = symbol_,
            .aggressive_order_id = aggressive_id,
            .passive_order_id = passive.order_id,
            .aggressive_side = aggressive_side,
            .price = passive.price,
            .quantity = trade_qty,
            .timestamp = timestamp,
        });

        remaining_qty -= trade_qty;
        passive.remaining_qty -= trade_qty;

        if (passive.remaining_qty == 0) {
          id_index_.erase(passive.order_id);
          queue.pop_front();
        }
      }
      if (queue.empty()) {
        opposite_side.erase(best_it);
      }
    }
  };

  if (aggressive_side == Side::kBuy) {
    // Buy crosses asks when ask_price <= limit_price.
    match_side(asks_, [limit_price](Price p) { return p <= limit_price; });
  } else {
    // Sell crosses bids when bid_price >= limit_price.
    match_side(bids_, [limit_price](Price p) { return p >= limit_price; });
  }
  return fills;
}

auto OrderBook::Rest(OrderId id, Side side, Price price, Quantity qty, Timestamp timestamp)
    -> BookEntry {
  BookEntry entry{
      .order_id = id,
      .side = side,
      .price = price,
      .remaining_qty = qty,
      .timestamp = timestamp,
  };
  if (side == Side::kBuy) {
    bids_[price].push_back(entry);
  } else {
    asks_[price].push_back(entry);
  }
  id_index_.emplace(id, IndexEntry{.side = side, .price = price});
  return entry;
}

}  // namespace oems::matching
