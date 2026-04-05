#ifndef OEMS_CORE_FIX_FIX_SESSION_H_
#define OEMS_CORE_FIX_FIX_SESSION_H_

/**
 * @file fix_session.h
 * @brief FIX4/FIXT session state machine (minimal subset for v1).
 */

#include <cstdint>
#include <string>
#include <string_view>

#include "core/fix/fix_message.h"
#include "core/types/error.h"

namespace oems::fix {

/**
 * @brief Session identity and sequence counters for one FIX session.
 *
 * v1 handles: Logon, Logout, Heartbeat, TestRequest.  No resend support.
 */
class FixSession {
 public:
  FixSession(std::string sender_comp_id, std::string target_comp_id,
             std::string begin_string = "FIX.4.4");

  /**
   * @brief Dispatch an inbound message through the state machine.
   * @return Optional outbound response (or session error).
   */
  auto OnMessage(const FixMessage& msg) -> Result<std::optional<FixMessage>>;

  /// Build an outbound Logon message (seq += 1).
  auto BuildLogon(std::int32_t heartbeat_interval_sec = 30) -> FixMessage;

  /// Build an outbound Heartbeat (optionally echoing a TestReqID).
  auto BuildHeartbeat(std::string_view test_req_id = "") -> FixMessage;

  /// Build an outbound Logout.
  auto BuildLogout(std::string_view text = "") -> FixMessage;

  /// Wrap a partial message with header fields (BeginString, SenderCompID,
  /// TargetCompID, MsgSeqNum, SendingTime).  Called by Build* helpers; also
  /// usable for app-layer messages like ExecutionReport.
  void StampOutbound(FixMessage& msg);

  [[nodiscard]] auto IsLoggedOn() const -> bool { return logged_on_; }
  [[nodiscard]] auto InboundSeq() const -> std::uint64_t { return inbound_seq_; }
  [[nodiscard]] auto OutboundSeq() const -> std::uint64_t { return outbound_seq_; }
  [[nodiscard]] auto SenderCompId() const -> const std::string& { return sender_; }
  [[nodiscard]] auto TargetCompId() const -> const std::string& { return target_; }

 private:
  std::string begin_string_;
  std::string sender_;
  std::string target_;
  std::uint64_t inbound_seq_{0};   // last received
  std::uint64_t outbound_seq_{0};  // last sent
  bool logged_on_{false};
};

}  // namespace oems::fix

#endif  // OEMS_CORE_FIX_FIX_SESSION_H_
