#include "core/risk/risk_manager.h"

#include <chrono>
#include <cstdlib>
#include <utility>

namespace oems::risk {

RiskManager::RiskManager(RiskLimits limits) : limits_(limits) {}

auto RiskManager::Check(const RiskRequest& req) -> Result<void> {
  if (req.quantity <= 0) {
    return std::unexpected(OemsError::kInvalidQuantity);
  }
  if (req.symbol.value.empty()) {
    return std::unexpected(OemsError::kInvalidSymbol);
  }
  if (req.type == OrderType::kLimit && req.price <= 0) {
    return std::unexpected(OemsError::kInvalidPrice);
  }

  if (req.quantity > limits_.max_order_qty) {
    return std::unexpected(OemsError::kRiskBreachMaxQty);
  }

  if (req.type == OrderType::kLimit) {
    // notional = price * quantity, computed in __int128 to avoid overflow
    // on attacker-supplied prices.
    __int128 notional = static_cast<__int128>(req.price) * req.quantity;
    if (notional > static_cast<__int128>(limits_.max_notional)) {
      return std::unexpected(OemsError::kRiskBreachNotional);
    }
    if (auto band = CheckPriceBand(req); !band.has_value()) {
      return band;
    }
  }

  if (auto lot = CheckLotSize(req); !lot.has_value()) {
    return lot;
  }
  if (auto tick = CheckTickSize(req); !tick.has_value()) {
    return tick;
  }
  if (auto daily = CheckDailyLimit(req); !daily.has_value()) {
    return daily;
  }

  return CheckRateLimit();
}

void RiskManager::SetReferencePrice(const Symbol& symbol, Price price) {
  reference_prices_[symbol.value] = price;
}

auto RiskManager::GetReferencePrice(const Symbol& symbol) const -> std::optional<Price> {
  auto it = reference_prices_.find(symbol.value);
  if (it == reference_prices_.end()) {
    return std::nullopt;
  }
  return it->second;
}

auto RiskManager::CheckPriceBand(const RiskRequest& req) const -> Result<void> {
  auto it = reference_prices_.find(req.symbol.value);
  if (it == reference_prices_.end()) {
    // No reference => skip.
    return {};
  }
  Price ref = it->second;
  if (ref <= 0) {
    return {};
  }
  // |price - ref| / ref > band_bps / 10000
  Price diff = std::abs(req.price - ref);
  // Cross-multiply in __int128 to avoid overflow: diff * 10000 > ref * band_bps
  __int128 lhs = static_cast<__int128>(diff) * 10000;
  __int128 rhs = static_cast<__int128>(ref) * limits_.price_band_bps;
  if (lhs > rhs) {
    return std::unexpected(OemsError::kRiskBreachPriceBand);
  }
  return {};
}

void RiskManager::SetSymbolConfig(const Symbol& symbol, SymbolConfig config) {
  config.symbol = symbol;
  configs_[symbol.value] = std::move(config);
}

auto RiskManager::GetSymbolConfig(const Symbol& symbol) const -> const SymbolConfig* {
  auto it = configs_.find(symbol.value);
  if (it == configs_.end()) {
    return nullptr;
  }
  return &it->second;
}

auto RiskManager::CheckLotSize(const RiskRequest& req) const -> Result<void> {
  auto it = configs_.find(req.symbol.value);
  if (it == configs_.end()) {
    // Unconfigured symbol: fall through (matches existing behaviour).
    return {};
  }
  const Quantity lot = it->second.lot_size;
  if (lot <= 0) {
    // Sentinel 0 means the rule is disabled for this symbol.
    return {};
  }
  if (req.quantity % lot != 0) {
    return std::unexpected(OemsError::kRiskBreachLotSize);
  }
  return {};
}

auto RiskManager::CheckTickSize(const RiskRequest& req) const -> Result<void> {
  if (req.type != OrderType::kLimit) {
    // Market orders carry no price — nothing to validate.
    return {};
  }
  auto it = configs_.find(req.symbol.value);
  if (it == configs_.end()) {
    return {};
  }
  const auto& bands = it->second.tick_bands;
  if (bands.empty()) {
    // Sentinel empty => disabled.
    return {};
  }
  for (const auto& band : bands) {
    if (req.price >= band.low && req.price <= band.high) {
      if (band.tick <= 0 || req.price % band.tick != 0) {
        return std::unexpected(OemsError::kRiskBreachTickSize);
      }
      return {};
    }
  }
  // Price did not fall into any configured band.
  return std::unexpected(OemsError::kRiskBreachTickSize);
}

auto RiskManager::CheckDailyLimit(const RiskRequest& req) const -> Result<void> {
  if (req.type != OrderType::kLimit) {
    // Market orders carry no price — nothing to validate.
    return {};
  }
  auto it = configs_.find(req.symbol.value);
  if (it == configs_.end()) {
    return {};
  }
  const auto& cfg = it->second;
  if (cfg.previous_close <= 0 || cfg.daily_limit_bps <= 0) {
    // Sentinel: unset → disabled.
    return {};
  }
  // |price - previous_close| > previous_close * daily_limit_bps / 10000
  // Cross-multiply in __int128 to avoid overflow (pattern matches
  // CheckPriceBand / the notional check).
  const Price diff = req.price >= cfg.previous_close ? req.price - cfg.previous_close
                                                     : cfg.previous_close - req.price;
  __int128 lhs = static_cast<__int128>(diff) * 10000;
  __int128 rhs = static_cast<__int128>(cfg.previous_close) * cfg.daily_limit_bps;
  if (lhs > rhs) {
    return std::unexpected(OemsError::kRiskBreachDailyLimit);
  }
  return {};
}

auto RiskManager::CheckRateLimit() -> Result<void> {
  auto now = Now();
  auto window_start = now - std::chrono::seconds(1);
  while (!recent_orders_.empty() && recent_orders_.front() < window_start) {
    recent_orders_.pop_front();
  }
  if (std::cmp_greater_equal(recent_orders_.size(), limits_.max_orders_per_second)) {
    return std::unexpected(OemsError::kRiskBreachRateLimit);
  }
  recent_orders_.push_back(now);
  return {};
}

}  // namespace oems::risk
