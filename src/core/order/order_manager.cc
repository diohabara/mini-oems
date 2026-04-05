#include "core/order/order_manager.h"

#include <algorithm>
#include <format>
#include <utility>

#include "core/persistence/database.h"

namespace oems::order {

OrderManager::OrderManager(risk::RiskManager& risk, matching::MatchingEngine& engine,
                           persistence::Database* db)
    : risk_(risk), engine_(engine), db_(db) {}

auto OrderManager::SubmitOrder(const NewOrderRequest& req) -> Result<Order> {
  // Basic validation (risk will also validate).
  if (req.quantity <= 0) {
    return std::unexpected(OemsError::kInvalidQuantity);
  }
  if (req.type == OrderType::kLimit && req.price <= 0) {
    return std::unexpected(OemsError::kInvalidPrice);
  }
  if (req.symbol.value.empty()) {
    return std::unexpected(OemsError::kInvalidSymbol);
  }

  // Risk check.
  risk::RiskRequest risk_req{
      .symbol = req.symbol,
      .side = req.side,
      .type = req.type,
      .price = req.price,
      .quantity = req.quantity,
  };
  if (auto ok = risk_.Check(risk_req); !ok.has_value()) {
    // Create a rejected order for audit.
    Order rejected{
        .internal_id = NextOrderId(),
        .client_order_id = req.client_order_id,
        .symbol = req.symbol,
        .side = req.side,
        .type = req.type,
        .price = req.price,
        .order_qty = req.quantity,
        .filled_qty = 0,
        .remaining_qty = req.quantity,
        .status = OrderStatus::kRejected,
        .created_at = Now(),
        .updated_at = Now(),
    };
    orders_[rejected.internal_id] = rejected;
    AppendEvent(rejected.internal_id, EventType::kNewRequested, req.client_order_id);
    AppendEvent(rejected.internal_id, EventType::kRejected, std::string(ErrorName(ok.error())));
    if (db_ != nullptr) {
      (void)db_->SaveOrder(rejected);
    }
    return std::unexpected(ok.error());
  }

  // Build pending order.
  OrderId id = NextOrderId();
  auto now = Now();
  Order order{
      .internal_id = id,
      .client_order_id = req.client_order_id,
      .symbol = req.symbol,
      .side = req.side,
      .type = req.type,
      .price = req.price,
      .order_qty = req.quantity,
      .filled_qty = 0,
      .remaining_qty = req.quantity,
      .status = OrderStatus::kPendingNew,
      .created_at = now,
      .updated_at = now,
  };
  AppendEvent(id, EventType::kNewRequested, req.client_order_id);

  // Submit to matching.
  auto match_result = engine_.AddOrder(req.symbol, id, req.side, req.type, req.price, req.quantity);
  if (!match_result.has_value()) {
    order.status = OrderStatus::kRejected;
    order.updated_at = Now();
    orders_[id] = order;
    AppendEvent(id, EventType::kRejected, std::string(ErrorName(match_result.error())));
    if (db_ != nullptr) {
      (void)db_->SaveOrder(order);
    }
    return std::unexpected(match_result.error());
  }

  // Accept and apply fills.
  order.status = OrderStatus::kAccepted;
  order.updated_at = Now();
  AppendEvent(id, EventType::kAccepted, "");

  ApplyFills(order, match_result->fills);

  // Determine final status based on remaining / market order outcome.
  if (order.remaining_qty == 0) {
    order.status = OrderStatus::kFilled;
  } else if (order.type == OrderType::kMarket) {
    // Market order unable to fully fill remains as whatever we have.
    order.status = order.filled_qty > 0 ? OrderStatus::kPartiallyFilled : OrderStatus::kRejected;
    if (order.status == OrderStatus::kRejected) {
      AppendEvent(id, EventType::kRejected, "no liquidity for market order");
    }
  } else if (order.filled_qty > 0) {
    order.status = OrderStatus::kPartiallyFilled;
  }
  order.updated_at = Now();
  orders_[id] = order;
  if (db_ != nullptr) {
    (void)db_->SaveOrder(order);
  }
  return order;
}

auto OrderManager::CancelOrder(const CancelOrderRequest& req) -> Result<Order> {
  auto it = orders_.find(req.order_id);
  if (it == orders_.end()) {
    return std::unexpected(OemsError::kOrderNotFound);
  }
  Order& order = it->second;
  if (order.status == OrderStatus::kFilled || order.status == OrderStatus::kCancelled ||
      order.status == OrderStatus::kRejected) {
    return std::unexpected(OemsError::kInvalidStateTransition);
  }

  AppendEvent(order.internal_id, EventType::kCancelRequested, "");
  auto cancelled = engine_.CancelOrder(order.symbol, order.internal_id);
  if (!cancelled.has_value()) {
    return std::unexpected(cancelled.error());
  }

  order.status = OrderStatus::kCancelled;
  order.updated_at = Now();
  AppendEvent(order.internal_id, EventType::kCancelled, "");
  if (db_ != nullptr) {
    (void)db_->SaveOrder(order);
  }
  return order;
}

auto OrderManager::RestoreFromDatabase() -> Result<void> {
  if (db_ == nullptr) {
    return {};
  }

  auto orders = db_->LoadOrders();
  if (!orders.has_value()) {
    return std::unexpected(orders.error());
  }
  auto events = db_->LoadAllEvents();
  if (!events.has_value()) {
    return std::unexpected(events.error());
  }
  auto executions = db_->LoadExecutions();
  if (!executions.has_value()) {
    return std::unexpected(executions.error());
  }

  orders_.clear();
  events_ = std::move(*events);
  executions_ = std::move(*executions);

  OrderId max_order_id = 0;
  std::uint64_t max_event_id = 0;
  ExecutionId max_execution_id = 0;

  for (const auto& event : events_) {
    max_event_id = std::max(max_event_id, event.event_id);
  }
  for (const auto& fill : executions_) {
    max_execution_id = std::max(max_execution_id, fill.execution_id);
  }
  engine_.SeedNextExecutionId(max_execution_id + 1);

  for (const auto& order : *orders) {
    orders_[order.internal_id] = order;
    max_order_id = std::max(max_order_id, order.internal_id);
    if ((order.status == OrderStatus::kAccepted || order.status == OrderStatus::kPartiallyFilled) &&
        order.type == OrderType::kLimit && order.remaining_qty > 0) {
      if (auto restored = engine_.RestoreRestingOrder(order); !restored.has_value()) {
        return std::unexpected(restored.error());
      }
    }
  }

  next_order_id_ = max_order_id + 1;
  next_event_id_ = max_event_id + 1;
  return {};
}

auto OrderManager::GetOrder(OrderId id) const -> Result<Order> {
  auto it = orders_.find(id);
  if (it == orders_.end()) {
    return std::unexpected(OemsError::kOrderNotFound);
  }
  return it->second;
}

auto OrderManager::GetOrders(std::optional<Symbol> symbol, std::optional<OrderStatus> status) const
    -> std::vector<Order> {
  std::vector<Order> out;
  out.reserve(orders_.size());
  for (const auto& [_, order] : orders_) {
    if (symbol.has_value() && order.symbol != *symbol) {
      continue;
    }
    if (status.has_value() && order.status != *status) {
      continue;
    }
    out.push_back(order);
  }
  return out;
}

auto OrderManager::GetEvents(OrderId id) const -> std::vector<OrderEvent> {
  std::vector<OrderEvent> out;
  for (const auto& ev : events_) {
    if (ev.order_id == id) {
      out.push_back(ev);
    }
  }
  return out;
}

auto OrderManager::GetAllExecutions() const -> std::vector<matching::Fill> { return executions_; }

void OrderManager::AppendEvent(OrderId id, EventType type, std::string detail) {
  OrderEvent event{
      .event_id = NextEventId(),
      .order_id = id,
      .type = type,
      .timestamp = Now(),
      .detail = std::move(detail),
  };
  events_.push_back(event);
  if (db_ != nullptr) {
    (void)db_->AppendEvent(event);
  }
}

void OrderManager::ApplyFills(Order& order, const std::vector<matching::Fill>& fills) {
  // Precondition: `order` must refer to a local variable owned by the caller,
  // NOT to an element inside `orders_`.  Mutating `orders_` below would
  // otherwise invalidate the reference.
  for (const auto& fill : fills) {
    order.filled_qty += fill.quantity;
    order.remaining_qty -= fill.quantity;
    executions_.push_back(fill);
    if (db_ != nullptr) {
      (void)db_->SaveExecution(fill);
    }

    // Also apply to the passive order (counterparty side).
    auto passive_it = orders_.find(fill.passive_order_id);
    if (passive_it != orders_.end()) {
      Order& passive = passive_it->second;
      passive.filled_qty += fill.quantity;
      passive.remaining_qty -= fill.quantity;
      passive.updated_at = Now();
      if (passive.remaining_qty == 0) {
        passive.status = OrderStatus::kFilled;
        AppendEvent(passive.internal_id, EventType::kFill,
                    std::format("exec={} qty={}", fill.execution_id, fill.quantity));
      } else {
        passive.status = OrderStatus::kPartiallyFilled;
        AppendEvent(passive.internal_id, EventType::kPartialFill,
                    std::format("exec={} qty={}", fill.execution_id, fill.quantity));
      }
      if (db_ != nullptr) {
        (void)db_->SaveOrder(passive);
      }
    }

    AppendEvent(order.internal_id,
                (order.remaining_qty == 0) ? EventType::kFill : EventType::kPartialFill,
                std::format("exec={} qty={}", fill.execution_id, fill.quantity));
  }
}

}  // namespace oems::order
