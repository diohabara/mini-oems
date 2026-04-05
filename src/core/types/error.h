#ifndef OEMS_CORE_TYPES_ERROR_H_
#define OEMS_CORE_TYPES_ERROR_H_

/**
 * @file error.h
 * @brief Unified error codes and Result alias for Mini OEMS.
 *
 * Uses C++23 std::expected to provide a monadic error-handling style
 * throughout the codebase.  Every fallible function returns
 * @c Result<T> instead of throwing exceptions.
 */

#include <cstdint>
#include <expected>
#include <string_view>

namespace oems {

/**
 * @brief Enumeration of all domain error codes.
 *
 * Grouped by subsystem for readability.
 */
enum class OemsError : std::uint8_t {
  // --- Validation ---
  kInvalidQuantity,
  kInvalidPrice,
  kInvalidSymbol,
  kInvalidSide,
  kInvalidOrderType,

  // --- Order lifecycle ---
  kOrderNotFound,
  kDuplicateOrder,
  kInvalidStateTransition,

  // --- Risk ---
  kRiskBreachMaxQty,
  kRiskBreachNotional,
  kRiskBreachPriceBand,
  kRiskBreachRateLimit,

  // --- Matching ---
  kBookNotFound,
  kNoLiquidity,

  // --- FIX ---
  kFixParseError,
  kFixSessionError,
  kFixInvalidMsgType,

  // --- Persistence ---
  kDatabaseError,

  // --- API ---
  kHttpParseError,
  kRouteNotFound,
};

/**
 * @brief Return the human-readable name of an OemsError value.
 * @param err The error code.
 * @return Short descriptive string.
 */
constexpr auto ErrorName(OemsError err) -> std::string_view {
  switch (err) {
    case OemsError::kInvalidQuantity:
      return "InvalidQuantity";
    case OemsError::kInvalidPrice:
      return "InvalidPrice";
    case OemsError::kInvalidSymbol:
      return "InvalidSymbol";
    case OemsError::kInvalidSide:
      return "InvalidSide";
    case OemsError::kInvalidOrderType:
      return "InvalidOrderType";
    case OemsError::kOrderNotFound:
      return "OrderNotFound";
    case OemsError::kDuplicateOrder:
      return "DuplicateOrder";
    case OemsError::kInvalidStateTransition:
      return "InvalidStateTransition";
    case OemsError::kRiskBreachMaxQty:
      return "RiskBreachMaxQty";
    case OemsError::kRiskBreachNotional:
      return "RiskBreachNotional";
    case OemsError::kRiskBreachPriceBand:
      return "RiskBreachPriceBand";
    case OemsError::kRiskBreachRateLimit:
      return "RiskBreachRateLimit";
    case OemsError::kBookNotFound:
      return "BookNotFound";
    case OemsError::kNoLiquidity:
      return "NoLiquidity";
    case OemsError::kFixParseError:
      return "FixParseError";
    case OemsError::kFixSessionError:
      return "FixSessionError";
    case OemsError::kFixInvalidMsgType:
      return "FixInvalidMsgType";
    case OemsError::kDatabaseError:
      return "DatabaseError";
    case OemsError::kHttpParseError:
      return "HttpParseError";
    case OemsError::kRouteNotFound:
      return "RouteNotFound";
  }
  return "Unknown";
}

/**
 * @brief Convenience alias: every fallible function returns
 *        @c Result<T> = @c std::expected<T, OemsError>.
 * @tparam T The success type.
 */
template <typename T>
using Result = std::expected<T, OemsError>;

}  // namespace oems

#endif  // OEMS_CORE_TYPES_ERROR_H_
