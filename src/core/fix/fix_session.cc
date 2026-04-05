#include "core/fix/fix_session.h"

#include <chrono>
#include <format>
#include <utility>

namespace oems::fix {

namespace {

auto SendingTimeStr() -> std::string {
  auto now = std::chrono::system_clock::now();
  auto t = std::chrono::system_clock::to_time_t(now);
  std::tm utc_tm{};
#ifdef _WIN32
  gmtime_s(&utc_tm, &t);
#else
  gmtime_r(&t, &utc_tm);
#endif
  auto ms = std::chrono::duration_cast<std::chrono::milliseconds>(now.time_since_epoch()) % 1000;
  return std::format("{:04d}{:02d}{:02d}-{:02d}:{:02d}:{:02d}.{:03d}", utc_tm.tm_year + 1900,
                     utc_tm.tm_mon + 1, utc_tm.tm_mday, utc_tm.tm_hour, utc_tm.tm_min,
                     utc_tm.tm_sec, static_cast<std::int32_t>(ms.count()));
}

}  // namespace

FixSession::FixSession(std::string sender_comp_id, std::string target_comp_id,
                       std::string begin_string)
    : begin_string_(std::move(begin_string)),
      sender_(std::move(sender_comp_id)),
      target_(std::move(target_comp_id)) {}

void FixSession::StampOutbound(FixMessage& msg) {
  ++outbound_seq_;
  msg.Set(tag::kBeginString, begin_string_);
  msg.Set(tag::kSenderCompId, sender_);
  msg.Set(tag::kTargetCompId, target_);
  msg.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(outbound_seq_));
  msg.Set(tag::kSendingTime, SendingTimeStr());
}

auto FixSession::OnMessage(const FixMessage& msg) -> Result<std::optional<FixMessage>> {
  auto mtype = msg.MsgType();
  auto incoming_seq = static_cast<std::uint64_t>(msg.GetInt(tag::kMsgSeqNum));

  // Initial state: only Logon is accepted.
  if (!logged_on_) {
    if (mtype != msg_type::kLogon) {
      return std::unexpected(OemsError::kFixSessionError);
    }
    if (incoming_seq > 0) {
      inbound_seq_ = incoming_seq;
    } else {
      inbound_seq_ = 1;
    }
    logged_on_ = true;
    auto resp = BuildLogon();
    return std::optional<FixMessage>{std::move(resp)};
  }

  // Sequence gap detection (v1: strict; no resend).
  if (incoming_seq != inbound_seq_ + 1) {
    return std::unexpected(OemsError::kFixSessionError);
  }
  inbound_seq_ = incoming_seq;

  if (mtype == msg_type::kHeartbeat) {
    return std::optional<FixMessage>{};  // no response
  }
  if (mtype == msg_type::kTestRequest) {
    auto req_id = msg.Get(tag::kTestReqID).value_or("");
    return std::optional<FixMessage>{BuildHeartbeat(req_id)};
  }
  if (mtype == msg_type::kLogout) {
    logged_on_ = false;
    return std::optional<FixMessage>{BuildLogout("ack")};
  }
  // App-layer messages are delivered to the caller via caller's own path.
  return std::optional<FixMessage>{};
}

auto FixSession::BuildLogon(std::int32_t heartbeat_interval_sec) -> FixMessage {
  FixMessage m;
  m.Set(tag::kMsgType, std::string(msg_type::kLogon));
  m.Set(tag::kEncryptMethod, static_cast<std::int64_t>(0));
  m.Set(tag::kHeartBtInt, static_cast<std::int64_t>(heartbeat_interval_sec));
  StampOutbound(m);
  return m;
}

auto FixSession::BuildHeartbeat(std::string_view test_req_id) -> FixMessage {
  FixMessage m;
  m.Set(tag::kMsgType, std::string(msg_type::kHeartbeat));
  if (!test_req_id.empty()) {
    m.Set(tag::kTestReqID, std::string(test_req_id));
  }
  StampOutbound(m);
  return m;
}

auto FixSession::BuildLogout(std::string_view text) -> FixMessage {
  FixMessage m;
  m.Set(tag::kMsgType, std::string(msg_type::kLogout));
  if (!text.empty()) {
    m.Set(tag::kText, std::string(text));
  }
  StampOutbound(m);
  return m;
}

}  // namespace oems::fix
