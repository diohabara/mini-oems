/**
 * @file system_test.cc
 * @brief End-to-end system test exercising the full oatpp HTTP stack.
 *
 * Starts an oatpp server in a background thread, sends real HTTP requests
 * to a loopback socket, and verifies responses.  The server uses the
 * generated controller implementations backed by docs/openapi.yaml.
 */

#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <chrono>
#include <format>
#include <memory>
#include <string>
#include <thread>

#include "core/api/oems_controllers.hpp"
#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "core/risk/risk_manager.h"
#include "oatpp/network/Server.hpp"
#include "oatpp/network/tcp/server/ConnectionProvider.hpp"
#include "oatpp/parser/json/mapping/ObjectMapper.hpp"
#include "oatpp/web/server/HttpConnectionHandler.hpp"
#include "oatpp/web/server/HttpRouter.hpp"

namespace oems::system_test {
namespace {

constexpr std::int32_t kTestPort = 18180;

class HttpClient {
 public:
  static auto Send(const std::string& method, const std::string& path, const std::string& body)
      -> std::pair<std::int32_t, std::string> {
    std::int32_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
    if (fd < 0) return {-1, ""};
    sockaddr_in addr{};
    addr.sin_family = AF_INET;
    addr.sin_port = htons(kTestPort);
    ::inet_pton(AF_INET, "127.0.0.1", &addr.sin_addr);
    if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
      ::close(fd);
      return {-1, ""};
    }
    std::string req = std::format(
        "{} {} HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
        "Content-Length: {}\r\n\r\n{}",
        method, path, body.size(), body);
    ::send(fd, req.data(), req.size(), 0);
    std::string buf(65536, '\0');
    ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
    ::close(fd);
    if (n <= 0) return {-1, ""};
    buf.resize(static_cast<std::size_t>(n));
    auto sp1 = buf.find(' ');
    auto sp2 = buf.find(' ', sp1 + 1);
    std::int32_t status = std::atoi(buf.substr(sp1 + 1, sp2 - sp1 - 1).c_str());
    auto body_pos = buf.find("\r\n\r\n");
    std::string resp_body = body_pos == std::string::npos ? "" : buf.substr(body_pos + 4);
    return {status, resp_body};
  }
};

struct TestComponents {
  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::data::mapping::ObjectMapper>, object_mapper)
  ([] { return oatpp::parser::json::mapping::ObjectMapper::createShared(); }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>,
                         connection_provider)
  ([] {
    return oatpp::network::tcp::server::ConnectionProvider::createShared(
        {"0.0.0.0", static_cast<v_uint16>(kTestPort), oatpp::network::Address::IP_4});
  }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, http_router)
  ([] { return oatpp::web::server::HttpRouter::createShared(); }());

  OATPP_CREATE_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connection_handler)
  ([] {
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    return oatpp::web::server::HttpConnectionHandler::createShared(router);
  }());
};

class ServerFixture : public ::testing::Test {
 protected:
  risk::RiskManager risk_;
  matching::MatchingEngine engine_;
  std::unique_ptr<order::OrderManager> om_;
  api::Services services_{};
  std::unique_ptr<TestComponents> components_;
  std::shared_ptr<oatpp::network::Server> server_;
  std::thread server_thread_;

  static bool environment_initialised_;

  static void SetUpTestSuite() {
    if (!environment_initialised_) {
      oatpp::base::Environment::init();
      environment_initialised_ = true;
    }
  }

  void SetUp() override {
    om_ = std::make_unique<order::OrderManager>(risk_, engine_);
    services_.om = om_.get();
    services_.engine = &engine_;
    api::SetServices(&services_);

    components_ = std::make_unique<TestComponents>();
    OATPP_COMPONENT(std::shared_ptr<oatpp::web::server::HttpRouter>, router);
    router->addController(std::make_shared<api::HealthController>());
    router->addController(std::make_shared<api::OrdersController>());
    router->addController(std::make_shared<api::BooksController>());
    router->addController(std::make_shared<api::ExecutionsController>());

    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connection_handler);
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connection_provider);
    server_ = std::make_shared<oatpp::network::Server>(connection_provider, connection_handler);
    server_thread_ = std::thread([this] { server_->run(); });

    for (std::int32_t i = 0; i < 50; ++i) {
      std::this_thread::sleep_for(std::chrono::milliseconds(20));
      auto [status, _] = HttpClient::Send("GET", "/v1/health", "");
      if (status == 200) return;
    }
    FAIL() << "server did not start";
  }

  void TearDown() override {
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ServerConnectionProvider>, connection_provider);
    OATPP_COMPONENT(std::shared_ptr<oatpp::network::ConnectionHandler>, connection_handler);
    server_->stop();
    connection_provider->stop();
    connection_handler->stop();
    if (server_thread_.joinable()) server_thread_.join();
    components_.reset();
    api::SetServices(nullptr);
  }
};

bool ServerFixture::environment_initialised_ = false;

TEST_F(ServerFixture, HealthCheck) {
  auto [status, body] = HttpClient::Send("GET", "/v1/health", "");
  EXPECT_EQ(status, 200);
  EXPECT_NE(body.find("\"status\""), std::string::npos);
  EXPECT_NE(body.find("ok"), std::string::npos);
}

TEST_F(ServerFixture, FullOrderLifecycle) {
  auto [s1, b1] = HttpClient::Send(
      "POST", "/v1/orders",
      R"({"client_order_id":"S1","symbol":"AAPL","side":"sell","type":"limit","price":10000,"quantity":100})");
  EXPECT_EQ(s1, 201);

  auto [s2, b2] = HttpClient::Send(
      "POST", "/v1/orders",
      R"({"client_order_id":"B1","symbol":"AAPL","side":"buy","type":"limit","price":10000,"quantity":100})");
  EXPECT_EQ(s2, 201);
  EXPECT_NE(b2.find("Filled"), std::string::npos);

  auto [s3, b3] = HttpClient::Send("GET", "/v1/executions", "");
  EXPECT_EQ(s3, 200);
  EXPECT_NE(b3.find("\"quantity\""), std::string::npos);
}

TEST_F(ServerFixture, GetBookAfterOrders) {
  HttpClient::Send("POST", "/v1/orders",
                   R"({"symbol":"AAPL","side":"buy","type":"limit","price":10000,"quantity":100})");
  auto [status, body] = HttpClient::Send("GET", "/v1/books/AAPL", "");
  EXPECT_EQ(status, 200);
  EXPECT_NE(body.find("AAPL"), std::string::npos);
  EXPECT_NE(body.find("bids"), std::string::npos);
}

TEST_F(ServerFixture, UnknownRoute404) {
  auto [status, _] = HttpClient::Send("GET", "/v1/nope", "");
  EXPECT_EQ(status, 404);
}

TEST_F(ServerFixture, InvalidJsonRejected) {
  // oatpp surfaces malformed JSON as 500 via its default error handler;
  // the contract is "not 2xx" — a custom ErrorHandler could downgrade this
  // to 400 at the cost of framework coupling.
  auto [status, _] = HttpClient::Send("POST", "/v1/orders", "{bad");
  EXPECT_GE(status, 400);
  EXPECT_LT(status, 600);
}

TEST_F(ServerFixture, BookNotFound404) {
  auto [status, _] = HttpClient::Send("GET", "/v1/books/NOPE", "");
  EXPECT_EQ(status, 404);
}

TEST_F(ServerFixture, CancelOrder) {
  auto [s1, body] = HttpClient::Send(
      "POST", "/v1/orders",
      R"({"symbol":"AAPL","side":"buy","type":"limit","price":10000,"quantity":100})");
  ASSERT_EQ(s1, 201);
  auto p = body.find("\"internal_id\":");
  ASSERT_NE(p, std::string::npos);
  std::int32_t id = std::atoi(body.c_str() + p + 14);
  auto [s2, _] = HttpClient::Send("POST", "/v1/orders/" + std::to_string(id) + "/cancel", "");
  EXPECT_EQ(s2, 200);
}

TEST_F(ServerFixture, ListOrdersRejectsUnknownStatusFilter) {
  HttpClient::Send("POST", "/v1/orders",
                   R"({"symbol":"AAPL","side":"buy","type":"limit","price":10000,"quantity":100})");

  auto [status, body] =
      HttpClient::Send("GET", "/v1/orders?symbol=AAPL&status=DefinitelyNotReal", "");

  EXPECT_EQ(status, 400) << "body=" << body;
  EXPECT_NE(body.find("invalid"), std::string::npos) << "body=" << body;
}

}  // namespace
}  // namespace oems::system_test
