#include "core/api/oems_controllers.hpp"

#include <string>

namespace oems::api {

// ---------------------------------------------------------------------------
// Services singleton (set by main).
// ---------------------------------------------------------------------------

namespace {
Services* g_services = nullptr;
}

auto GetServices() -> Services* { return g_services; }
void SetServices(Services* svc) { g_services = svc; }

// ---------------------------------------------------------------------------
// Conversions
// ---------------------------------------------------------------------------

auto DomainToDtoSide(Side s) -> oatpp::String {
  return s == Side::kBuy ? oatpp::String("Buy") : oatpp::String("Sell");
}

auto DomainToDtoType(OrderType t) -> oatpp::String {
  return t == OrderType::kLimit ? oatpp::String("Limit") : oatpp::String("Market");
}

auto DomainToDtoStatus(OrderStatus s) -> oatpp::String {
  return oatpp::String(OrderStatusName(s));
}

auto ParseRequestSide(const oatpp::String& s) -> Result<Side> {
  if (!s) {
    return std::unexpected(OemsError::kInvalidSide);
  }
  std::string v = *s;
  for (auto& c : v) c = static_cast<char>(std::tolower(c));
  if (v == "buy") return Side::kBuy;
  if (v == "sell") return Side::kSell;
  return std::unexpected(OemsError::kInvalidSide);
}

auto ParseRequestType(const oatpp::String& s) -> Result<OrderType> {
  if (!s) {
    return std::unexpected(OemsError::kInvalidOrderType);
  }
  std::string v = *s;
  for (auto& c : v) c = static_cast<char>(std::tolower(c));
  if (v == "limit") return OrderType::kLimit;
  if (v == "market") return OrderType::kMarket;
  return std::unexpected(OemsError::kInvalidOrderType);
}

auto ErrorToStatus(OemsError err) -> std::int32_t {
  switch (err) {
    case OemsError::kOrderNotFound:
    case OemsError::kBookNotFound:
    case OemsError::kRouteNotFound:
      return 404;
    case OemsError::kInvalidQuantity:
    case OemsError::kInvalidPrice:
    case OemsError::kInvalidSymbol:
    case OemsError::kInvalidSide:
    case OemsError::kInvalidOrderType:
    case OemsError::kHttpParseError:
      return 400;
    default:
      return 422;
  }
}

namespace model_ns = org::openapitools::server::model;

auto ToDto(const order::Order& o) -> oatpp::Object<model_ns::Order> {
  auto dto = model_ns::Order::createShared();
  dto->internal_id = static_cast<v_int64>(o.internal_id);
  dto->client_order_id = oatpp::String(o.client_order_id);
  dto->symbol = oatpp::String(o.symbol.value);
  dto->side = DomainToDtoSide(o.side);
  dto->type = DomainToDtoType(o.type);
  dto->price = static_cast<v_int64>(o.price);
  dto->order_qty = static_cast<v_int64>(o.order_qty);
  dto->filled_qty = static_cast<v_int64>(o.filled_qty);
  dto->remaining_qty = static_cast<v_int64>(o.remaining_qty);
  dto->status = DomainToDtoStatus(o.status);
  return dto;
}

auto ToDto(const matching::Fill& f) -> oatpp::Object<model_ns::Fill> {
  auto dto = model_ns::Fill::createShared();
  dto->execution_id = static_cast<v_int64>(f.execution_id);
  dto->symbol = oatpp::String(f.symbol.value);
  dto->aggressive_order_id = static_cast<v_int64>(f.aggressive_order_id);
  dto->passive_order_id = static_cast<v_int64>(f.passive_order_id);
  dto->side = DomainToDtoSide(f.aggressive_side);
  dto->price = static_cast<v_int64>(f.price);
  dto->quantity = static_cast<v_int64>(f.quantity);
  return dto;
}

namespace {

auto MakeError(std::string_view message) -> oatpp::Object<model_ns::ApiError> {
  auto err = model_ns::ApiError::createShared();
  err->error = oatpp::String(std::string(message));
  return err;
}

auto DtoPriceLevel(const matching::PriceLevel& lvl) -> oatpp::Object<model_ns::PriceLevel> {
  auto dto = model_ns::PriceLevel::createShared();
  dto->price = static_cast<v_int64>(lvl.price);
  dto->total_qty = static_cast<v_int64>(lvl.total_qty);
  dto->order_count = static_cast<v_int64>(lvl.order_count);
  return dto;
}

}  // namespace

}  // namespace oems::api

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace oems::api {

namespace model_ns = org::openapitools::server::model;

// ---------------------------------------------------------------------------
// HealthController
// ---------------------------------------------------------------------------

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> HealthController::get_health(
    const std::shared_ptr<IncomingRequest>& request) {
  (void)request;
  auto health = model_ns::Health::createShared();
  health->status = "ok";
  return createDtoResponse(Status::CODE_200, health);
}

// ---------------------------------------------------------------------------
// OrdersController
// ---------------------------------------------------------------------------

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> OrdersController::submit_order(
    const std::shared_ptr<IncomingRequest>& request,
    const oatpp::Object<model_ns::NewOrderRequest>& newOrderRequest) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");

  if (!newOrderRequest || !newOrderRequest->symbol || !newOrderRequest->side ||
      !newOrderRequest->type || !newOrderRequest->quantity) {
    return createDtoResponse(Status::CODE_400, MakeError("missing required fields"));
  }

  auto side = ParseRequestSide(newOrderRequest->side);
  if (!side.has_value()) {
    return createDtoResponse(Status::CODE_400, MakeError("invalid side"));
  }
  auto type = ParseRequestType(newOrderRequest->type);
  if (!type.has_value()) {
    return createDtoResponse(Status::CODE_400, MakeError("invalid type"));
  }

  order::NewOrderRequest req{
      .client_order_id = newOrderRequest->client_order_id
                             ? std::string(*newOrderRequest->client_order_id)
                             : std::string(),
      .symbol = Symbol{std::string(*newOrderRequest->symbol)},
      .side = *side,
      .type = *type,
      .price = newOrderRequest->price ? static_cast<Price>(*newOrderRequest->price) : 0,
      .quantity = static_cast<Quantity>(*newOrderRequest->quantity),
  };
  auto result = services->om->SubmitOrder(req);
  if (!result.has_value()) {
    Status status_code{ErrorToStatus(result.error()), "Error"};
    return createDtoResponse(status_code, MakeError(ErrorName(result.error())));
  }
  return createDtoResponse(Status::CODE_201, ToDto(*result));
}

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> OrdersController::cancel_order(
    const std::shared_ptr<IncomingRequest>& request, const oatpp::Int64& id) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");
  if (!id) {
    return createDtoResponse(Status::CODE_400, MakeError("missing id"));
  }
  auto result = services->om->CancelOrder(
      order::CancelOrderRequest{.order_id = static_cast<OrderId>(*id)});
  if (!result.has_value()) {
    Status status_code{ErrorToStatus(result.error()), "Error"};
    return createDtoResponse(status_code, MakeError(ErrorName(result.error())));
  }
  return createDtoResponse(Status::CODE_200, ToDto(*result));
}

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> OrdersController::get_order(
    const std::shared_ptr<IncomingRequest>& request, const oatpp::Int64& id) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");
  if (!id) {
    return createDtoResponse(Status::CODE_400, MakeError("missing id"));
  }
  auto result = services->om->GetOrder(static_cast<OrderId>(*id));
  if (!result.has_value()) {
    Status status_code{ErrorToStatus(result.error()), "Error"};
    return createDtoResponse(status_code, MakeError(ErrorName(result.error())));
  }
  return createDtoResponse(Status::CODE_200, ToDto(*result));
}

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> OrdersController::list_orders(
    const std::shared_ptr<IncomingRequest>& request, const oatpp::String& symbol,
    const model_ns::OrderStatus& status) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");

  std::optional<Symbol> sym_filter;
  if (symbol && !symbol->empty()) {
    sym_filter = Symbol{std::string(*symbol)};
  }
  std::optional<OrderStatus> status_filter;
  if (status && !status->empty()) {
    std::string s = *status;
    if (s == "PendingNew") status_filter = OrderStatus::kPendingNew;
    else if (s == "Accepted") status_filter = OrderStatus::kAccepted;
    else if (s == "PartiallyFilled") status_filter = OrderStatus::kPartiallyFilled;
    else if (s == "Filled") status_filter = OrderStatus::kFilled;
    else if (s == "PendingCancel") status_filter = OrderStatus::kPendingCancel;
    else if (s == "Cancelled") status_filter = OrderStatus::kCancelled;
    else if (s == "Rejected") status_filter = OrderStatus::kRejected;
  }

  auto orders = services->om->GetOrders(sym_filter, status_filter);
  auto list = oatpp::List<oatpp::Object<model_ns::Order>>::createShared();
  for (const auto& o : orders) {
    list->push_back(ToDto(o));
  }
  return createDtoResponse(Status::CODE_200, list);
}

// ---------------------------------------------------------------------------
// BooksController
// ---------------------------------------------------------------------------

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> BooksController::get_book(
    const std::shared_ptr<IncomingRequest>& request, const oatpp::String& symbol) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");
  if (!symbol || symbol->empty()) {
    return createDtoResponse(Status::CODE_400, MakeError("missing symbol"));
  }
  auto book = services->engine->GetBook(Symbol{std::string(*symbol)});
  if (!book.has_value()) {
    return createDtoResponse(Status::CODE_404, MakeError(ErrorName(book.error())));
  }
  auto dto = model_ns::Book::createShared();
  dto->symbol = *symbol;
  dto->bids = oatpp::Vector<oatpp::Object<model_ns::PriceLevel>>::createShared();
  dto->asks = oatpp::Vector<oatpp::Object<model_ns::PriceLevel>>::createShared();
  for (const auto& lvl : (*book)->Bids()) {
    dto->bids->push_back(DtoPriceLevel(lvl));
  }
  for (const auto& lvl : (*book)->Asks()) {
    dto->asks->push_back(DtoPriceLevel(lvl));
  }
  return createDtoResponse(Status::CODE_200, dto);
}

// ---------------------------------------------------------------------------
// ExecutionsController
// ---------------------------------------------------------------------------

std::shared_ptr<oatpp::web::protocol::http::outgoing::Response>
ExecutionsController::list_executions(const std::shared_ptr<IncomingRequest>& request) {
  (void)request;
  auto* services = GetServices();
  OATPP_ASSERT_HTTP(services != nullptr, Status::CODE_500, "services not initialised");
  auto fills = services->om->GetAllExecutions();
  auto list = oatpp::List<oatpp::Object<model_ns::Fill>>::createShared();
  for (const auto& f : fills) {
    list->push_back(ToDto(f));
  }
  return createDtoResponse(Status::CODE_200, list);
}

}  // namespace oems::api

#include OATPP_CODEGEN_END(ApiController)
