#include "core/fix/fix_session.h"

#include <gtest/gtest.h>

namespace oems::fix {
namespace {

TEST(FixSessionTest, InitiallyNotLoggedOn) {
  FixSession s("OEMS", "CPTY");
  EXPECT_FALSE(s.IsLoggedOn());
  EXPECT_EQ(s.OutboundSeq(), 0U);
}

TEST(FixSessionTest, LogonFlow) {
  FixSession s("OEMS", "CPTY");
  FixMessage in;
  in.Set(tag::kMsgType, std::string(msg_type::kLogon));
  in.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
  auto resp = s.OnMessage(in);
  ASSERT_TRUE(resp.has_value());
  ASSERT_TRUE(resp->has_value());
  EXPECT_EQ((*resp)->MsgType(), "A");
  EXPECT_TRUE(s.IsLoggedOn());
  EXPECT_EQ(s.OutboundSeq(), 1U);
  EXPECT_EQ(s.InboundSeq(), 1U);
}

TEST(FixSessionTest, MessageBeforeLogonRejected) {
  FixSession s("OEMS", "CPTY");
  FixMessage in;
  in.Set(tag::kMsgType, std::string(msg_type::kHeartbeat));
  auto resp = s.OnMessage(in);
  ASSERT_FALSE(resp.has_value());
  EXPECT_EQ(resp.error(), OemsError::kFixSessionError);
}

TEST(FixSessionTest, HeartbeatNoResponse) {
  FixSession s("OEMS", "CPTY");
  FixMessage logon;
  logon.Set(tag::kMsgType, std::string(msg_type::kLogon));
  logon.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
  ASSERT_TRUE(s.OnMessage(logon).has_value());

  FixMessage hb;
  hb.Set(tag::kMsgType, std::string(msg_type::kHeartbeat));
  hb.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(2));
  auto resp = s.OnMessage(hb);
  ASSERT_TRUE(resp.has_value());
  EXPECT_FALSE(resp->has_value());
}

TEST(FixSessionTest, TestRequestEchoes) {
  FixSession s("OEMS", "CPTY");
  FixMessage logon;
  logon.Set(tag::kMsgType, std::string(msg_type::kLogon));
  logon.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
  ASSERT_TRUE(s.OnMessage(logon).has_value());

  FixMessage tr;
  tr.Set(tag::kMsgType, std::string(msg_type::kTestRequest));
  tr.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(2));
  tr.Set(tag::kTestReqID, "PING42");
  auto resp = s.OnMessage(tr);
  ASSERT_TRUE(resp.has_value());
  ASSERT_TRUE(resp->has_value());
  EXPECT_EQ((*resp)->MsgType(), "0");
  EXPECT_EQ((*resp)->Get(tag::kTestReqID).value_or(""), "PING42");
}

TEST(FixSessionTest, SequenceGapRejected) {
  FixSession s("OEMS", "CPTY");
  FixMessage logon;
  logon.Set(tag::kMsgType, std::string(msg_type::kLogon));
  logon.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
  ASSERT_TRUE(s.OnMessage(logon).has_value());

  FixMessage gap;
  gap.Set(tag::kMsgType, std::string(msg_type::kHeartbeat));
  gap.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(5));  // expected 2
  auto resp = s.OnMessage(gap);
  ASSERT_FALSE(resp.has_value());
  EXPECT_EQ(resp.error(), OemsError::kFixSessionError);
}

TEST(FixSessionTest, LogoutClears) {
  FixSession s("OEMS", "CPTY");
  FixMessage logon;
  logon.Set(tag::kMsgType, std::string(msg_type::kLogon));
  logon.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
  ASSERT_TRUE(s.OnMessage(logon).has_value());
  FixMessage logout;
  logout.Set(tag::kMsgType, std::string(msg_type::kLogout));
  logout.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(2));
  auto resp = s.OnMessage(logout);
  ASSERT_TRUE(resp.has_value());
  ASSERT_TRUE(resp->has_value());
  EXPECT_EQ((*resp)->MsgType(), "5");
  EXPECT_FALSE(s.IsLoggedOn());
}

TEST(FixSessionTest, StampOutboundSetsHeader) {
  FixSession s("OEMS", "CPTY");
  FixMessage m;
  m.Set(tag::kMsgType, "D");
  s.StampOutbound(m);
  EXPECT_EQ(m.Get(tag::kSenderCompId).value_or(""), "OEMS");
  EXPECT_EQ(m.Get(tag::kTargetCompId).value_or(""), "CPTY");
  EXPECT_EQ(m.GetInt(tag::kMsgSeqNum), 1);
  EXPECT_FALSE(m.Get(tag::kSendingTime).value_or("").empty());
}

}  // namespace
}  // namespace oems::fix
