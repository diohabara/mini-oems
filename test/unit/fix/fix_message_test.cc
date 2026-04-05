#include "core/fix/fix_message.h"

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
