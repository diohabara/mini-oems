#ifndef OEMS_CORE_FIX_FIX_GATEWAY_H_
#define OEMS_CORE_FIX_FIX_GATEWAY_H_

/**
 * @file fix_gateway.h
 * @brief Translates FIX app-layer messages to/from internal order domain.
 */

#include "core/fix/fix_message.h"
#include "core/fix/fix_session.h"
#include "core/matching/order_book.h"
#include "core/order/order.h"
#include "core/order/order_manager.h"
#include "core/types/error.h"

namespace oems::fix {

/**
 * @brief Stateless translator between FIX and internal order commands.
 *
 * The gateway owns no sockets in v1; the caller drives it by handing in
 * parsed FixMessages.  This keeps it trivially testable.
 */
class FixGateway {
 public:
  explicit FixGateway(order::OrderManager& om);

  /**
   * @brief Handle an inbound application-layer message.
   *
   * Returns the outbound ExecutionReport (accepted or filled) for
   * NewOrderSingle, or a session-level Reject/Logout marker on failure.
   */
  auto HandleNewOrderSingle(const FixMessage& msg, FixSession& session) -> Result<FixMessage>;

  /**
   * @brief Handle an OrderCancelRequest.
   */
  auto HandleOrderCancelRequest(const FixMessage& msg, FixSession& session) -> Result<FixMessage>;

  /**
   * @brief Build an ExecutionReport from an internal order snapshot.
   */
  auto BuildExecutionReport(const order::Order& order, FixSession& session, char exec_type)
      -> FixMessage;

 private:
  order::OrderManager& om_;
};

/// @brief Parse FIX Side tag (1=Buy, 2=Sell).
auto ParseFixSide(std::string_view s) -> Result<Side>;

/// @brief Parse FIX OrdType tag (1=Market, 2=Limit).
auto ParseFixOrdType(std::string_view s) -> Result<OrderType>;

/// @brief Convert internal Side to FIX value.
auto FixSideChar(Side s) -> char;

/// @brief Convert internal OrderType to FIX value.
auto FixOrdTypeChar(OrderType t) -> char;

/// @brief Convert internal OrderStatus to FIX OrdStatus value.
auto FixOrdStatusChar(OrderStatus s) -> char;

}  // namespace oems::fix

#endif  // OEMS_CORE_FIX_FIX_GATEWAY_H_
