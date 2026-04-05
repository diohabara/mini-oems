#ifndef OEMS_CORE_ORDER_ORDER_H_
#define OEMS_CORE_ORDER_ORDER_H_

/**
 * @file order.h
 * @brief Immutable Order value type and request types.
 */

#include <string>

#include "core/types/types.h"

namespace oems::order {

/**
 * @brief Immutable snapshot of an order.
 *
 * State transitions are modelled by replacing one @c Order with a new one.
 * The old value is preserved by the append-only event log in OrderManager.
 */
struct Order {
  OrderId internal_id{0};
  std::string client_order_id;
  Symbol symbol;
  Side side{Side::kBuy};
  OrderType type{OrderType::kLimit};
  Price price{0};
  Quantity order_qty{0};
  Quantity filled_qty{0};
  Quantity remaining_qty{0};
  OrderStatus status{OrderStatus::kPendingNew};
  Timestamp created_at{};
  Timestamp updated_at{};
};

/**
 * @brief External request to submit a new order.
 */
struct NewOrderRequest {
  std::string client_order_id;
  Symbol symbol;
  Side side{Side::kBuy};
  OrderType type{OrderType::kLimit};
  Price price{0};
  Quantity quantity{0};
};

/**
 * @brief External request to cancel a resting order.
 */
struct CancelOrderRequest {
  OrderId order_id{0};
};

}  // namespace oems::order

#endif  // OEMS_CORE_ORDER_ORDER_H_
