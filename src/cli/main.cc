/**
 * @file cli/main.cc
 * @brief oems-cli developer command-line client.
 *
 * Sends JSON requests to the local Mini OEMS HTTP server and prints
 * responses.  Uses POSIX sockets; no external HTTP client dependency.
 */

#include <arpa/inet.h>
#include <netinet/in.h>
#include <sys/socket.h>
#include <unistd.h>

#include <cstring>
#include <format>
#include <print>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr const char* kDefaultHost = "127.0.0.1";
constexpr std::int32_t kDefaultPort = 8080;

auto SendHttp(const std::string& method, const std::string& path, const std::string& body)
    -> std::string {
  std::int32_t fd = ::socket(AF_INET, SOCK_STREAM, 0);
  if (fd < 0) {
    return "";
  }
  sockaddr_in addr{};
  addr.sin_family = AF_INET;
  addr.sin_port = htons(static_cast<uint16_t>(kDefaultPort));
  ::inet_pton(AF_INET, kDefaultHost, &addr.sin_addr);
  if (::connect(fd, reinterpret_cast<sockaddr*>(&addr), sizeof(addr)) < 0) {
    ::close(fd);
    return "";
  }
  std::string req = std::format(
      "{} {} HTTP/1.1\r\nHost: localhost\r\nContent-Type: application/json\r\n"
      "Content-Length: {}\r\n\r\n{}",
      method, path, body.size(), body);
  ::send(fd, req.data(), req.size(), 0);
  std::string resp;
  std::vector<char> buf(4096);
  for (;;) {
    ssize_t n = ::recv(fd, buf.data(), buf.size(), 0);
    if (n <= 0) {
      break;
    }
    resp.append(buf.data(), static_cast<std::size_t>(n));
  }
  ::close(fd);
  if (resp.empty()) {
    return "";
  }
  // Extract body (after "\r\n\r\n").
  auto p = resp.find("\r\n\r\n");
  if (p == std::string::npos) {
    return resp;
  }
  return resp.substr(p + 4);
}

auto GetArg(const std::vector<std::string>& args, std::string_view flag) -> std::string {
  for (std::size_t i = 0; i + 1 < args.size(); ++i) {
    if (args[i] == flag) {
      return args[i + 1];
    }
  }
  return "";
}

auto Usage() -> std::int32_t {
  std::println("Usage: oems-cli <command> [options]");
  std::println("Commands:");
  std::println("  server-status");
  std::println(
      "  new-order  --symbol SYMBOL --side buy|sell --qty QTY "
      "--type limit|market [--price PRICE]");
  std::println("  cancel-order --order-id ID");
  std::println("  show-orders [--symbol S] [--status S]");
  std::println("  show-book --symbol S");
  std::println("  show-trades");
  return 1;
}

}  // namespace

int main(std::int32_t argc, char** argv) {
  if (argc < 2) {
    return Usage();
  }
  std::vector<std::string> args(argv + 1, argv + argc);
  const std::string& cmd = args[0];

  if (cmd == "server-status") {
    std::println("{}", SendHttp("GET", "/v1/health", ""));
    return 0;
  }
  if (cmd == "new-order") {
    auto sym = GetArg(args, "--symbol");
    auto side = GetArg(args, "--side");
    auto qty = GetArg(args, "--qty");
    auto type = GetArg(args, "--type");
    auto price = GetArg(args, "--price");
    auto cl = GetArg(args, "--cl-ord-id");
    if (cl.empty()) {
      cl = "CLI1";
    }
    if (price.empty()) {
      price = "0";
    }
    std::string body = std::format(
        R"({{"client_order_id":"{}","symbol":"{}","side":"{}","type":"{}","price":{},"quantity":{}}})",
        cl, sym, side, type, price, qty);
    std::println("{}", SendHttp("POST", "/v1/orders", body));
    return 0;
  }
  if (cmd == "cancel-order") {
    auto id = GetArg(args, "--order-id");
    std::println("{}", SendHttp("POST", "/v1/orders/" + id + "/cancel", ""));
    return 0;
  }
  if (cmd == "show-orders") {
    auto sym = GetArg(args, "--symbol");
    auto status = GetArg(args, "--status");
    std::string q;
    if (!sym.empty()) {
      q += "symbol=" + sym;
    }
    if (!status.empty()) {
      if (!q.empty()) {
        q += "&";
      }
      q += "status=" + status;
    }
    std::string path = "/v1/orders";
    if (!q.empty()) {
      path += "?" + q;
    }
    std::println("{}", SendHttp("GET", path, ""));
    return 0;
  }
  if (cmd == "show-book") {
    auto sym = GetArg(args, "--symbol");
    std::println("{}", SendHttp("GET", "/v1/books/" + sym, ""));
    return 0;
  }
  if (cmd == "show-trades") {
    std::println("{}", SendHttp("GET", "/v1/executions", ""));
    return 0;
  }
  return Usage();
}
