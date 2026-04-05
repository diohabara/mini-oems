#include "core/fix/fix_message.h"

#include <cstdio>

#include <gtest/gtest.h>

namespace oems::fix {
namespace {

std::string ToWire(std::initializer_list<std::pair<std::int32_t, std::string>> fields) {
  std::string s;
  for (const auto& [t, v] : fields) {
    s += std::to_string(t) + "=" + v + kSoh;
  }
  return s;
}

void RewriteChecksum(std::string& wire) {
  auto checksum_tag = wire.rfind(
      "\x01"
      "10=");
  ASSERT_NE(checksum_tag, std::string::npos);

  auto checksum_value_start = checksum_tag + 4;
  auto checksum_value_end = wire.find(kSoh, checksum_value_start);
  ASSERT_NE(checksum_value_end, std::string::npos);

  auto checksum = FixMessage::ComputeCheckSum(wire.substr(0, checksum_tag + 1));
  char buf[4];
  std::snprintf(buf, sizeof(buf), "%03u", checksum);
  wire.replace(checksum_value_start, checksum_value_end - checksum_value_start, buf);
}

TEST(FixMessageTest, ParseSimple) {
  auto raw = ToWire({{8, "FIX.4.4"}, {35, "D"}, {49, "CPTY"}, {56, "OEMS"}});
  auto msg = FixMessage::Parse(raw);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->Get(tag::kBeginString).value_or(""), "FIX.4.4");
  EXPECT_EQ(msg->MsgType(), "D");
  EXPECT_EQ(msg->Get(tag::kSenderCompId).value_or(""), "CPTY");
  EXPECT_EQ(msg->Get(tag::kTargetCompId).value_or(""), "OEMS");
}

TEST(FixMessageTest, GetInt) {
  auto raw = ToWire({{34, "42"}, {38, "100"}});
  auto msg = FixMessage::Parse(raw);
  ASSERT_TRUE(msg.has_value());
  EXPECT_EQ(msg->GetInt(tag::kMsgSeqNum), 42);
  EXPECT_EQ(msg->GetInt(tag::kOrderQty), 100);
  EXPECT_EQ(msg->GetInt(999), 0);  // missing
}

TEST(FixMessageTest, ParseMissingEquals) {
  std::string raw = "8FIX.4.4";
  raw += kSoh;
  auto msg = FixMessage::Parse(raw);
  ASSERT_FALSE(msg.has_value());
  EXPECT_EQ(msg.error(), OemsError::kFixParseError);
}

TEST(FixMessageTest, ParseMissingSoh) {
  std::string raw = "8=FIX.4.4";
  auto msg = FixMessage::Parse(raw);
  ASSERT_FALSE(msg.has_value());
}

TEST(FixMessageTest, ParseNonNumericTag) {
  std::string raw = "abc=xyz";
  raw += kSoh;
  auto msg = FixMessage::Parse(raw);
  ASSERT_FALSE(msg.has_value());
}

TEST(FixMessageTest, SerializeInjectsBodyLengthAndChecksum) {
  FixMessage m;
  m.Set(tag::kBeginString, "FIX.4.4");
  m.Set(tag::kMsgType, "D");
  m.Set(tag::kSenderCompId, "CPTY");
  m.Set(tag::kTargetCompId, "OEMS");
  auto wire = m.Serialize();
  // Expect it starts with "8=FIX.4.4" + SOH + "9="
  EXPECT_EQ(wire.substr(0, 10), std::string("8=FIX.4.4") + kSoh);
  EXPECT_NE(wire.find("10="), std::string::npos);
}

TEST(FixMessageTest, RoundTripPreservesFields) {
  FixMessage m;
  m.Set(tag::kBeginString, "FIX.4.4");
  m.Set(tag::kMsgType, "D");
  m.Set(tag::kSenderCompId, "A");
  m.Set(tag::kTargetCompId, "B");
  m.Set(tag::kClOrdID, "CL1");
  m.Set(tag::kSymbol, "AAPL");
  m.Set(tag::kOrderQty, static_cast<std::int64_t>(100));
  m.Set(tag::kPrice, static_cast<std::int64_t>(10000));
  auto wire = m.Serialize();
  auto parsed = FixMessage::Parse(wire);
  ASSERT_TRUE(parsed.has_value());
  EXPECT_EQ(parsed->MsgType(), "D");
  EXPECT_EQ(parsed->Get(tag::kClOrdID).value_or(""), "CL1");
  EXPECT_EQ(parsed->Get(tag::kSymbol).value_or(""), "AAPL");
  EXPECT_EQ(parsed->GetInt(tag::kOrderQty), 100);
  EXPECT_EQ(parsed->GetInt(tag::kPrice), 10000);
}

TEST(FixMessageTest, ChecksumValid) {
  FixMessage m;
  m.Set(tag::kBeginString, "FIX.4.4");
  m.Set(tag::kMsgType, "A");
  m.Set(tag::kSenderCompId, "X");
  m.Set(tag::kTargetCompId, "Y");
  auto wire = m.Serialize();
  auto strict = FixMessage::ParseStrict(wire);
  EXPECT_TRUE(strict.has_value());
}

TEST(FixMessageTest, StrictParseRejectsMismatchedBodyLength) {
  FixMessage m;
  m.Set(tag::kBeginString, "FIX.4.4");
  m.Set(tag::kMsgType, "A");
  m.Set(tag::kSenderCompId, "X");
  m.Set(tag::kTargetCompId, "Y");

  auto wire = m.Serialize();
  auto body_length_pos = wire.find("9=");
  ASSERT_NE(body_length_pos, std::string::npos);
  auto body_length_end = wire.find(kSoh, body_length_pos);
  ASSERT_NE(body_length_end, std::string::npos);

  wire.replace(body_length_pos + 2, body_length_end - (body_length_pos + 2), "999");
  RewriteChecksum(wire);

  auto strict = FixMessage::ParseStrict(wire);
  ASSERT_FALSE(strict.has_value());
  EXPECT_EQ(strict.error(), OemsError::kFixParseError);
}

TEST(FixMessageTest, ComputeCheckSum) {
  // sum of ASCII bytes of "AB" is 65+66 = 131
  EXPECT_EQ(FixMessage::ComputeCheckSum("AB"), 131U);
  EXPECT_EQ(FixMessage::ComputeCheckSum(""), 0U);
}

TEST(FixMessageTest, SetOverwritesExisting) {
  FixMessage m;
  m.Set(tag::kMsgType, "D");
  m.Set(tag::kMsgType, "F");
  EXPECT_EQ(m.MsgType(), "F");
}

}  // namespace
}  // namespace oems::fix
