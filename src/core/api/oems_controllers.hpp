#ifndef OEMS_CORE_API_OEMS_CONTROLLERS_HPP_
#define OEMS_CORE_API_OEMS_CONTROLLERS_HPP_

/**
 * @file oems_controllers.hpp
 * @brief oatpp controller implementations that bridge the auto-generated
 *        abstract API (from `docs/openapi.yaml`) to our domain services.
 *
 * All schemas, routes, and wire types are owned by `docs/openapi.yaml` and
 * regenerated via `just api-gen`.  This file contains ONLY the adapter logic.
 */

#include <memory>

#include "api/BooksApi.hpp"
#include "api/ExecutionsApi.hpp"
#include "api/HealthApi.hpp"
#include "api/OrdersApi.hpp"
#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "model/ApiError.hpp"
#include "model/Book.hpp"
#include "model/Fill.hpp"
#include "model/Health.hpp"
#include "model/Order.hpp"
#include "model/PriceLevel.hpp"

namespace oems::api {

/// Domain services shared across controllers (set by main before server starts).
struct Services {
  order::OrderManager* om{nullptr};
  matching::MatchingEngine* engine{nullptr};
};

/// Access the global services pointer (set via SetServices).
auto GetServices() -> Services*;
void SetServices(Services* svc);

// --- Conversion helpers (tested as plain functions) ---

auto DomainToDtoSide(Side s) -> oatpp::String;
auto DomainToDtoType(OrderType t) -> oatpp::String;
auto DomainToDtoStatus(OrderStatus s) -> oatpp::String;
auto ParseRequestSide(const oatpp::String& s) -> Result<Side>;
auto ParseRequestType(const oatpp::String& s) -> Result<OrderType>;
auto ErrorToStatus(OemsError err) -> std::int32_t;
auto ToDto(const order::Order& o) -> oatpp::Object<org::openapitools::server::model::Order>;
auto ToDto(const matching::Fill& f) -> oatpp::Object<org::openapitools::server::model::Fill>;

}  // namespace oems::api

#include OATPP_CODEGEN_BEGIN(ApiController)

namespace oems::api {

/** @brief GET /v1/health. */
class HealthController : public org::openapitools::server::api::HealthApi {
 public:
  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> get_health(
      const std::shared_ptr<IncomingRequest>& request) override;
};

/** @brief /v1/orders, /v1/orders/{id}, /v1/orders/{id}/cancel. */
class OrdersController : public org::openapitools::server::api::OrdersApi {
 public:
  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> submit_order(
      const std::shared_ptr<IncomingRequest>& request,
      const oatpp::Object<org::openapitools::server::model::NewOrderRequest>&
          newOrderRequest) override;

  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> cancel_order(
      const std::shared_ptr<IncomingRequest>& request, const oatpp::Int64& id) override;

  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> get_order(
      const std::shared_ptr<IncomingRequest>& request, const oatpp::Int64& id) override;

  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> list_orders(
      const std::shared_ptr<IncomingRequest>& request, const oatpp::String& symbol,
      const org::openapitools::server::model::OrderStatus& status) override;
};

/** @brief GET /v1/books/{symbol}. */
class BooksController : public org::openapitools::server::api::BooksApi {
 public:
  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> get_book(
      const std::shared_ptr<IncomingRequest>& request, const oatpp::String& symbol) override;
};

/** @brief GET /v1/executions. */
class ExecutionsController : public org::openapitools::server::api::ExecutionsApi {
 public:
  std::shared_ptr<oatpp::web::protocol::http::outgoing::Response> list_executions(
      const std::shared_ptr<IncomingRequest>& request) override;
};

}  // namespace oems::api

#include OATPP_CODEGEN_END(ApiController)

#endif  // OEMS_CORE_API_OEMS_CONTROLLERS_HPP_
