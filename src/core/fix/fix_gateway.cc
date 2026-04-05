#include "core/fix/fix_gateway.h"

#include <atomic>
#include <charconv>
#include <limits>
#include <string>

namespace oems::fix {

namespace {

auto NextSyntheticOrderId() -> OrderId {
  static std::atomic<OrderId> next_order_id{std::numeric_limits<OrderId>::max()};
  return next_order_id.fetch_sub(1, std::memory_order_relaxed);
}

auto NextExecId() -> std::uint64_t {
  static std::atomic<std::uint64_t> next_exec_id{1};
  return next_exec_id.fetch_add(1, std::memory_order_relaxed);
}

}  // namespace

FixGateway::FixGateway(order::OrderManager& om) : om_(om) {}

auto ParseFixSide(std::string_view s) -> Result<Side> {
  if (s == "1") {
    return Side::kBuy;
  }
  if (s == "2") {
    return Side::kSell;
  }
  return std::unexpected(OemsError::kInvalidSide);
}

auto ParseFixOrdType(std::string_view s) -> Result<OrderType> {
  if (s == "1") {
    return OrderType::kMarket;
  }
  if (s == "2") {
    return OrderType::kLimit;
  }
  return std::unexpected(OemsError::kInvalidOrderType);
}

auto FixSideChar(Side s) -> char { return s == Side::kBuy ? '1' : '2'; }

auto FixOrdTypeChar(OrderType t) -> char { return t == OrderType::kMarket ? '1' : '2'; }

auto FixOrdStatusChar(OrderStatus s) -> char {
  switch (s) {
    case OrderStatus::kPendingNew:
      return 'A';
    case OrderStatus::kAccepted:
      return '0';
    case OrderStatus::kPartiallyFilled:
      return '1';
    case OrderStatus::kFilled:
      return '2';
    case OrderStatus::kPendingCancel:
      return '6';
    case OrderStatus::kCancelled:
      return '4';
    case OrderStatus::kRejected:
      return '8';
  }
  return '0';
}

auto FixGateway::BuildExecutionReport(const order::Order& order, FixSession& session,
                                      char exec_type) -> FixMessage {
  FixMessage m;
  m.Set(tag::kMsgType, std::string(msg_type::kExecutionReport));
  m.Set(tag::kOrderID, std::to_string(order.internal_id));
  m.Set(tag::kClOrdID, order.client_order_id);
  m.Set(tag::kExecID, std::to_string(NextExecId()));
  m.Set(tag::kSymbol, order.symbol.value);
  m.Set(tag::kSide, std::string(1, FixSideChar(order.side)));
  m.Set(tag::kOrderQty, std::to_string(order.order_qty));
  m.Set(tag::kLeavesQty, std::to_string(order.remaining_qty));
  m.Set(tag::kCumQty, std::to_string(order.filled_qty));
  m.Set(tag::kOrdStatus, std::string(1, FixOrdStatusChar(order.status)));
  m.Set(tag::kExecType, std::string(1, exec_type));
  if (order.type == OrderType::kLimit) {
    m.Set(tag::kPrice, std::to_string(order.price));
  }
  session.StampOutbound(m);
  return m;
}

auto FixGateway::HandleNewOrderSingle(const FixMessage& msg, FixSession& session)
    -> Result<FixMessage> {
  if (msg.MsgType() != msg_type::kNewOrderSingle) {
    return std::unexpected(OemsError::kFixInvalidMsgType);
  }
  auto cl_ord_id = msg.Get(tag::kClOrdID).value_or("");
  auto symbol = msg.Get(tag::kSymbol).value_or("");
  auto side_s = msg.Get(tag::kSide).value_or("");
  auto ord_type_s = msg.Get(tag::kOrdType).value_or("");
  auto qty = msg.GetInt(tag::kOrderQty);
  auto price = msg.GetInt(tag::kPrice);

  auto side = ParseFixSide(side_s);
  if (!side.has_value()) {
    return std::unexpected(side.error());
  }
  auto ord_type = ParseFixOrdType(ord_type_s);
  if (!ord_type.has_value()) {
    return std::unexpected(ord_type.error());
  }

  order::NewOrderRequest req{
      .client_order_id = std::string(cl_ord_id),
      .symbol = Symbol{std::string(symbol)},
      .side = *side,
      .type = *ord_type,
      .price = price,
      .quantity = qty,
  };
  auto submitted = om_.SubmitOrder(req);
  if (!submitted.has_value()) {
    // Return a rejection ExecutionReport.
    order::Order rejected;
    rejected.internal_id = NextSyntheticOrderId();
    rejected.client_order_id = req.client_order_id;
    rejected.symbol = req.symbol;
    rejected.side = req.side;
    rejected.type = req.type;
    rejected.price = req.price;
    rejected.order_qty = req.quantity;
    rejected.remaining_qty = req.quantity;
    rejected.status = OrderStatus::kRejected;
    return BuildExecutionReport(rejected, session, '8');
  }
  char exec_type = '0';  // New
  if (submitted->status == OrderStatus::kFilled ||
      submitted->status == OrderStatus::kPartiallyFilled) {
    exec_type = 'F';
  } else if (submitted->status == OrderStatus::kRejected) {
    exec_type = '8';
  }
  return BuildExecutionReport(*submitted, session, exec_type);
}

auto FixGateway::HandleOrderCancelRequest(const FixMessage& msg, FixSession& session)
    -> Result<FixMessage> {
  if (msg.MsgType() != msg_type::kOrderCancelRequest) {
    return std::unexpected(OemsError::kFixInvalidMsgType);
  }
  auto order_id_s = msg.Get(tag::kOrderID).value_or("");
  OrderId id = 0;
  if (!order_id_s.empty()) {
    std::from_chars(order_id_s.data(), order_id_s.data() + order_id_s.size(), id);
  }
  auto cancelled = om_.CancelOrder(order::CancelOrderRequest{.order_id = id});
  if (!cancelled.has_value()) {
    return std::unexpected(cancelled.error());
  }
  return BuildExecutionReport(*cancelled, session, '4');
}

}  // namespace oems::fix
