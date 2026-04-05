#include <arpa/inet.h>
#include <gtest/gtest.h>
#include <sys/socket.h>
#include <sys/types.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstring>
#include <string>
#include <vector>

namespace {

std::vector<std::string> g_recv_chunks;
std::size_t g_recv_index = 0;
std::string g_last_request;

auto test_socket(int domain, int type, int protocol) -> int {
  (void)domain;
  (void)type;
  (void)protocol;
  return 42;
}

auto test_connect(int sockfd, const sockaddr* addr, socklen_t addrlen) -> int {
  (void)sockfd;
  (void)addr;
  (void)addrlen;
  return 0;
}

auto test_inet_pton(int af, const char* src, void* dst) -> int {
  (void)af;
  (void)src;
  std::memset(dst, 0, sizeof(in_addr));
  return 1;
}

auto test_send(int sockfd, const void* buf, std::size_t len, int flags) -> ssize_t {
  (void)sockfd;
  (void)flags;
  g_last_request.assign(static_cast<const char*>(buf), len);
  return static_cast<ssize_t>(len);
}

auto test_recv(int sockfd, void* buf, std::size_t len, int flags) -> ssize_t {
  (void)sockfd;
  (void)flags;
  if (g_recv_index >= g_recv_chunks.size()) {
    return 0;
  }
  const auto& chunk = g_recv_chunks[g_recv_index++];
  const auto copy_size = std::min(len, chunk.size());
  std::memcpy(buf, chunk.data(), copy_size);
  return static_cast<ssize_t>(copy_size);
}

auto test_close(int fd) -> int {
  (void)fd;
  return 0;
}

}  // namespace

#define socket test_socket
#define connect test_connect
#define inet_pton test_inet_pton
#define send test_send
#define recv test_recv
#define close test_close
#define main oems_cli_entrypoint
#include "cli/main.cc"
#undef main
#undef close
#undef recv
#undef send
#undef inet_pton
#undef connect
#undef socket

namespace {

class SendHttpTest : public ::testing::Test {
 protected:
  void SetUp() override {
    g_recv_chunks.clear();
    g_recv_index = 0;
    g_last_request.clear();
  }
};

TEST_F(SendHttpTest, ReadsHttpResponseUntilPeerClosesConnection) {
  g_recv_chunks = {
      "HTTP/1.1 200 OK\r\nContent-Length: 15\r\n\r\n{\"st",
      "atus\":\"ok\"}\n",
  };

  const auto body = SendHttp("GET", "/v1/health", "");

  EXPECT_EQ(body, "{\"status\":\"ok\"}\n");
  EXPECT_NE(g_last_request.find("GET /v1/health HTTP/1.1"), std::string::npos);
}

}  // namespace
