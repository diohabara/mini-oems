/**
 * @file main.cc
 * @brief Mini OEMS server entry point.
 *
 * Opens SQLite for durable state, wires up the core domain modules, then
 * starts the oatpp HTTP server whose routes are defined by the generated
 * abstract API (derived from `docs/openapi.yaml`).
 */

#include <cstdlib>
#include <memory>
#include <print>
#include <string>

#include "core/api/oems_controllers.hpp"
#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "core/persistence/database.h"
#include "core/risk/risk_manager.h"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

namespace {

constexpr std::int32_t kDefaultPort = 8080;
constexpr const char* kDefaultDbPath = "oems.db";

/// Register oatpp components (connection provider, router, object mapper).
class AppComponent {
 public:
  explicit AppComponent(std::int32_t port) : port_(port) {}

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, objectMapper)
  ([] { return oatpp::parser::json::mapping::ObjectMapper::createShared(); }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>,
                         connectionProvider)
  ([this] {
    return oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"0.0.0.0", static_cast<v_uint16>(port_), oatpp::network::Address::IP_4});
  }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, httpRouter)
  ([] { return oatpp::web::server::HttpRouter::createShared(); }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connectionHandler)
  ([] {
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    return oatpp::web::server::HttpConnectionHandler::createShared(router);
  }());

 private:
  std::int32_t port_;
};

}  // namespace

auto main(std::int32_t argc, char** argv) -> std::int32_t {
  std::int32_t port = kDefaultPort;
  std::string db_path = kDefaultDbPath;
  if (argc >= 2) {
    char* end = nullptr;
    auto parsed = std::strtol(argv[1], &end, 10);
    if (end != argv[1] && *end == '\0') {
      port = static_cast<std::int32_t>(parsed);
    }
  }
  if (argc >= 3) {
    db_path = argv[2];
  }

  oatpp::base::Environment::init();
  {
    AppComponent components(port);

    auto db = oems::persistence::Database::Open(db_path);
    if (!db.has_value()) {
      std::println("failed to open database {}", db_path);
      oatpp::base::Environment::destroy();
      return 1;
    }
    if (auto r = db->Migrate(); !r.has_value()) {
      std::println("migrate failed");
      oatpp::base::Environment::destroy();
      return 1;
    }
    if (auto r = db->AppendAudit("startup", std::string{"opened "} + db_path); !r) {
      std::println("audit log write failed");
    }

    oems::risk::RiskManager risk;
    oems::matching::MatchingEngine engine;
    oems::order::OrderManager om(risk, engine);

    oems::api::Services services{.om = &om, .engine = &engine};
    oems::api::SetServices(&services);

    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    router->addController(std::make_shared<oems::api::HealthController>());
    router->addController(std::make_shared<oems::api::OrdersController>());
    router->addController(std::make_shared<oems::api::BooksController>());
    router->addController(std::make_shared<oems::api::ExecutionsController>());

    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connection_handler);
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connection_provider);

    std::println("mini-oems listening on 0.0.0.0:{} (db={})", port, db_path);
    oatpp::network::Server server(connection_provider, connection_handler);
    server.run();
  }
  oatpp::base::Environment::destroy();
  return 0;
}
