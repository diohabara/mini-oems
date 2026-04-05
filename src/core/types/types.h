#ifndef OEMS_CORE_TYPES_TYPES_H_
#define OEMS_CORE_TYPES_TYPES_H_

/**
 * @file types.h
 * @brief Core value types shared across all Mini OEMS modules.
 *
 * All domain primitives (OrderId, Price, Quantity, Symbol, enumerations)
 * are defined here so every module shares a single vocabulary.
 */

#include <chrono>
#include <compare>
#include <cstdint>
#include <string>

namespace oems {

/// @brief Unique internal order identifier (monotonically increasing).
using OrderId = std::uint64_t;

/// @brief Execution identifier.
using ExecutionId = std::uint64_t;

/// @brief Price in integer minor currency units.  Avoids floating-point.
///
/// For JPY (the TSE target market) the minor unit is 1 yen — there are no
/// sub-yen units on TSE.  For currencies with a fractional minor unit
/// (e.g. USD cents) the caller is responsible for matching the chosen
/// scale end-to-end; v1 targets TSE exclusively so the unit is yen.
using Price = std::int64_t;

/// @brief Order or fill quantity (shares).
using Quantity = std::int64_t;

/// @brief Wall-clock timestamp used for events and audit.
using Timestamp = std::chrono::time_point<std::chrono::system_clock>;

/**
 * @brief Instrument symbol (e.g. "AAPL", "7203.T").
 *
 * Wraps a std::string with defaulted comparison operators so it can be
 * used as a map key or in sorted containers.
 */
struct Symbol {
  std::string value;

  auto operator<=>(const Symbol&) const = default;
  bool operator==(const Symbol&) const = default;
};

/**
 * @brief Order side: buy or sell.
 */
enum class Side : std::uint8_t {
  kBuy,
  kSell,
};

/**
 * @brief Supported order types for v1.
 */
enum class OrderType : std::uint8_t {
  kLimit,
  kMarket,
};

/**
 * @brief Order lifecycle status.
 *
 * State machine:
 * @code
 *   kPendingNew -> kAccepted -> kPartiallyFilled -> kFilled
 *                             -> kPendingCancel   -> kCancelled
 *   kPendingNew -> kRejected
 * @endcode
 */
enum class OrderStatus : std::uint8_t {
  kPendingNew,
  kAccepted,
  kPartiallyFilled,
  kFilled,
  kPendingCancel,
  kCancelled,
  kRejected,
};

/**
 * @brief Time-in-force qualifier.
 */
enum class TimeInForce : std::uint8_t {
  kDay,
  kGtc,
  kIoc,
};

/**
 * @brief Return the human-readable name for a Side value.
 * @param side The side to convert.
 * @return "Buy" or "Sell".
 */
constexpr auto SideName(Side side) -> const char* {
  switch (side) {
    case Side::kBuy:
      return "Buy";
    case Side::kSell:
      return "Sell";
  }
  return "Unknown";
}

/**
 * @brief Return the human-readable name for an OrderType value.
 * @param type The order type to convert.
 * @return "Limit" or "Market".
 */
constexpr auto OrderTypeName(OrderType type) -> const char* {
  switch (type) {
    case OrderType::kLimit:
      return "Limit";
    case OrderType::kMarket:
      return "Market";
  }
  return "Unknown";
}

/**
 * @brief Return the human-readable name for an OrderStatus value.
 * @param status The status to convert.
 * @return Status string.
 */
constexpr auto OrderStatusName(OrderStatus status) -> const char* {
  switch (status) {
    case OrderStatus::kPendingNew:
      return "PendingNew";
    case OrderStatus::kAccepted:
      return "Accepted";
    case OrderStatus::kPartiallyFilled:
      return "PartiallyFilled";
    case OrderStatus::kFilled:
      return "Filled";
    case OrderStatus::kPendingCancel:
      return "PendingCancel";
    case OrderStatus::kCancelled:
      return "Cancelled";
    case OrderStatus::kRejected:
      return "Rejected";
  }
  return "Unknown";
}

/**
 * @brief Return the current wall-clock timestamp.
 * @return system_clock::now().
 */
inline auto Now() -> Timestamp { return std::chrono::system_clock::now(); }

}  // namespace oems

#endif  // OEMS_CORE_TYPES_TYPES_H_
