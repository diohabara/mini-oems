#ifndef OEMS_CORE_ORDER_ORDER_EVENT_H_
#define OEMS_CORE_ORDER_ORDER_EVENT_H_

/**
 * @file order_event.h
 * @brief Append-only order lifecycle event type.
 */

#include <cstdint>
#include <string>

#include "core/types/types.h"

namespace oems::order {

/**
 * @brief Type of lifecycle event emitted by OrderManager.
 */
enum class EventType : std::uint8_t {
  kNewRequested,
  kAccepted,
  kRejected,
  kPartialFill,
  kFill,
  kCancelRequested,
  kCancelled,
};

/**
 * @brief Human-readable name of an EventType.
 */
constexpr auto EventTypeName(EventType t) -> const char* {
  switch (t) {
    case EventType::kNewRequested:
      return "NewRequested";
    case EventType::kAccepted:
      return "Accepted";
    case EventType::kRejected:
      return "Rejected";
    case EventType::kPartialFill:
      return "PartialFill";
    case EventType::kFill:
      return "Fill";
    case EventType::kCancelRequested:
      return "CancelRequested";
    case EventType::kCancelled:
      return "Cancelled";
  }
  return "Unknown";
}

/**
 * @brief Append-only record of an order lifecycle change.
 */
struct OrderEvent {
  std::uint64_t event_id{0};
  OrderId order_id{0};
  EventType type{EventType::kNewRequested};
  Timestamp timestamp{};
  std::string detail;  ///< Free-form description for audit.
};

}  // namespace oems::order

#endif  // OEMS_CORE_ORDER_ORDER_EVENT_H_
