#ifndef OEMS_CORE_ORDER_ORDER_MANAGER_H_
#define OEMS_CORE_ORDER_ORDER_MANAGER_H_

/**
 * @file order_manager.h
 * @brief Order lifecycle coordinator.
 *
 * OrderManager owns the in-memory view of all orders, routes requests
 * through risk, hands them to the matching engine, and emits lifecycle
 * events for auditability.
 */

#include <cstdint>
#include <optional>
#include <unordered_map>
#include <vector>

#include "core/matching/matching_engine.h"
#include "core/matching/order_book.h"
#include "core/order/order.h"
#include "core/order/order_event.h"
#include "core/risk/risk_manager.h"
#include "core/types/error.h"
#include "core/types/types.h"

namespace oems::order {

/**
 * @brief Coordinator for the end-to-end order lifecycle.
 *
 * @note This class is single-threaded.  Callers must serialize access.
 */
class OrderManager {
 public:
  /**
   * @brief Construct with references to collaborators.
   * @param risk   Pre-trade risk manager.
   * @param engine Matching engine.
   *
   * The collaborators must outlive the OrderManager.
   */
  OrderManager(risk::RiskManager& risk, matching::MatchingEngine& engine);

  /**
   * @brief Submit a new order.
   *
   * Steps: validate, risk check, assign internal id, push to matching,
   * process fills, emit events, return final order snapshot.
   */
  auto SubmitOrder(const NewOrderRequest& req) -> Result<Order>;

  /**
   * @brief Cancel a resting order by internal id.
   */
  auto CancelOrder(const CancelOrderRequest& req) -> Result<Order>;

  /**
   * @brief Look up a single order by internal id.
   */
  [[nodiscard]] auto GetOrder(OrderId id) const -> Result<Order>;

  /**
   * @brief Return all orders, optionally filtered by symbol and status.
   */
  [[nodiscard]] auto GetOrders(std::optional<Symbol> symbol,
                               std::optional<OrderStatus> status) const -> std::vector<Order>;

  /**
   * @brief Return the event history for an order in chronological order.
   */
  [[nodiscard]] auto GetEvents(OrderId id) const -> std::vector<OrderEvent>;

  /**
   * @brief Return every fill produced since startup.
   */
  [[nodiscard]] auto GetAllExecutions() const -> std::vector<matching::Fill>;

  /**
   * @brief Number of orders currently tracked.
   */
  [[nodiscard]] auto OrderCount() const -> std::size_t { return orders_.size(); }

 private:
  auto NextOrderId() -> OrderId { return next_order_id_++; }
  auto NextEventId() -> std::uint64_t { return next_event_id_++; }
  void AppendEvent(OrderId id, EventType type, std::string detail);
  void ApplyFills(Order& order, const std::vector<matching::Fill>& fills);

  risk::RiskManager& risk_;
  matching::MatchingEngine& engine_;

  OrderId next_order_id_{1};
  std::uint64_t next_event_id_{1};

  std::unordered_map<OrderId, Order> orders_;
  std::vector<OrderEvent> events_;
  std::vector<matching::Fill> executions_;
};

}  // namespace oems::order

#endif  // OEMS_CORE_ORDER_ORDER_MANAGER_H_
