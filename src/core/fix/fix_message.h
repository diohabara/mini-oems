#ifndef OEMS_CORE_FIX_FIX_MESSAGE_H_
#define OEMS_CORE_FIX_FIX_MESSAGE_H_

/**
 * @file fix_message.h
 * @brief FIX tag-value message parser and builder (SOH-delimited).
 */

#include <algorithm>
#include <charconv>
#include <cstdint>
#include <optional>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "core/types/error.h"

namespace oems::fix {

constexpr char kSoh = '\x01';

/// Commonly used FIX tag numbers.
namespace tag {
constexpr std::int32_t kBeginString = 8;
constexpr std::int32_t kBodyLength = 9;
constexpr std::int32_t kMsgType = 35;
constexpr std::int32_t kSenderCompId = 49;
constexpr std::int32_t kTargetCompId = 56;
constexpr std::int32_t kMsgSeqNum = 34;
constexpr std::int32_t kSendingTime = 52;
constexpr std::int32_t kCheckSum = 10;

constexpr std::int32_t kClOrdID = 11;
constexpr std::int32_t kOrderID = 37;
constexpr std::int32_t kExecID = 17;
constexpr std::int32_t kSymbol = 55;
constexpr std::int32_t kSide = 54;
constexpr std::int32_t kOrderQty = 38;
constexpr std::int32_t kOrdType = 40;
constexpr std::int32_t kPrice = 44;
constexpr std::int32_t kOrdStatus = 39;
constexpr std::int32_t kExecType = 150;
constexpr std::int32_t kLastQty = 32;
constexpr std::int32_t kLastPx = 31;
constexpr std::int32_t kLeavesQty = 151;
constexpr std::int32_t kCumQty = 14;
constexpr std::int32_t kAvgPx = 6;
constexpr std::int32_t kTransactTime = 60;
constexpr std::int32_t kTestReqID = 112;
constexpr std::int32_t kHeartBtInt = 108;
constexpr std::int32_t kEncryptMethod = 98;
constexpr std::int32_t kText = 58;
}  // namespace tag

/// Commonly used MsgType codes.
namespace msg_type {
constexpr const char* kHeartbeat = "0";
constexpr const char* kTestRequest = "1";
constexpr const char* kLogout = "5";
constexpr const char* kExecutionReport = "8";
constexpr const char* kLogon = "A";
constexpr const char* kReject = "3";
constexpr const char* kNewOrderSingle = "D";
constexpr const char* kOrderCancelRequest = "F";
}  // namespace msg_type

/**
 * @brief A tag=value FIX message.
 */
class FixMessage {
 public:
  /// Parse a raw buffer into a FixMessage.  Does not verify CheckSum.
  static auto Parse(std::string_view raw) -> Result<FixMessage>;

  /// Parse, then also verify BodyLength and CheckSum.
  static auto ParseStrict(std::string_view raw) -> Result<FixMessage>;

  /// Serialise to wire format (injects BodyLength and CheckSum).
  [[nodiscard]] auto Serialize() const -> std::string;

  /// Get the value of a tag.  Returns nullopt if the tag is not present.
  [[nodiscard]] auto Get(std::int32_t tag) const -> std::optional<std::string_view>;

  /// Convenience accessor for MsgType (tag 35).
  [[nodiscard]] auto MsgType() const -> std::string_view { return Get(tag::kMsgType).value_or(""); }

  /// Parse a tag as an int64; returns 0 if absent or malformed.
  [[nodiscard]] auto GetInt(std::int32_t tag) const -> std::int64_t;

  /// Number of fields in the message (including header/trailer).
  [[nodiscard]] auto FieldCount() const -> std::size_t { return fields_.size(); }

  /// Builder-style mutators.
  void Set(std::int32_t tag, std::string value);
  void Set(std::int32_t tag, std::int64_t value);

  /**
   * @brief Compute FIX CheckSum for a buffer (sum of bytes mod 256).
   */
  static auto ComputeCheckSum(std::string_view buf) -> unsigned;

 private:
  std::vector<std::pair<std::int32_t, std::string>> fields_;
};

}  // namespace oems::fix

#endif  // OEMS_CORE_FIX_FIX_MESSAGE_H_
