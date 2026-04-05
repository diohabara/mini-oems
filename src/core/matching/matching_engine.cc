#include "core/matching/matching_engine.h"

#include <cassert>

namespace oems::matching {

auto MatchingEngine::AddOrder(const Symbol& symbol, OrderId id, Side side, OrderType type,
                              Price price, Quantity qty) -> Result<AddOrderResult> {
  return GetOrCreateBook(symbol).AddOrder(id, side, type, price, qty);
}

auto MatchingEngine::CancelOrder(const Symbol& symbol, OrderId id) -> Result<BookEntry> {
  auto it = books_.find(symbol.value);
  if (it == books_.end()) {
    return std::unexpected(OemsError::kBookNotFound);
  }
  return it->second.CancelOrder(id);
}

auto MatchingEngine::GetBook(const Symbol& symbol) const -> Result<const OrderBook*> {
  auto it = books_.find(symbol.value);
  if (it == books_.end()) {
    return std::unexpected(OemsError::kBookNotFound);
  }
  return &it->second;
}

auto MatchingEngine::GetOrCreateBook(const Symbol& symbol) -> OrderBook& {
  auto it = books_.find(symbol.value);
  if (it != books_.end()) {
    return it->second;
  }
  auto [new_it, inserted] = books_.emplace(symbol.value, OrderBook{symbol});
  assert(inserted && "earlier find() returned end(); emplace must insert");
  (void)inserted;
  return new_it->second;
}

}  // namespace oems::matching
