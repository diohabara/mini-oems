#include "core/market_data/market_data_handler.h"

namespace oems::market_data {

MarketDataHandler::MarketDataHandler(risk::RiskManager& risk) : risk_(risk) {}

void MarketDataHandler::OnBbo(const Bbo& bbo) {
  auto& snap = snapshots_[bbo.symbol.value];
  snap.bbo = bbo;
  snap.last_update = bbo.timestamp;

  Price ref = 0;
  if (bbo.bid_price > 0 && bbo.ask_price > 0) {
    ref = (bbo.bid_price + bbo.ask_price) / 2;
  } else if (bbo.bid_price > 0) {
    ref = bbo.bid_price;
  } else if (bbo.ask_price > 0) {
    ref = bbo.ask_price;
  }
  if (ref > 0) {
    risk_.SetReferencePrice(bbo.symbol, ref);
  }
}

void MarketDataHandler::OnTrade(const Symbol& symbol, Price price, Quantity qty) {
  auto& snap = snapshots_[symbol.value];
  snap.bbo.symbol = symbol;
  snap.last_trade_price = price;
  snap.last_trade_qty = qty;
  snap.last_update = Now();
  if (price > 0) {
    risk_.SetReferencePrice(symbol, price);
  }
}

auto MarketDataHandler::GetSnapshot(const Symbol& symbol) const -> std::optional<MarketSnapshot> {
  auto it = snapshots_.find(symbol.value);
  if (it == snapshots_.end()) {
    return std::nullopt;
  }
  return it->second;
}

}  // namespace oems::market_data
