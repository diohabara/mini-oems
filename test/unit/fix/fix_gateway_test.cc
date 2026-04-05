#include "core/fix/fix_gateway.h"

#include <gtest/gtest.h>

namespace oems::fix {
namespace {

class FixGatewayTest : public ::testing::Test {
 protected:
  risk::RiskManager risk_;
  matching::MatchingEngine engine_;
  order::OrderManager om_{risk_, engine_};
  FixGateway gateway_{om_};
  FixSession session_{"OEMS", "CPTY"};

  void LogOn() {
    FixMessage logon;
    logon.Set(tag::kMsgType, std::string(msg_type::kLogon));
    logon.Set(tag::kMsgSeqNum, static_cast<std::int64_t>(1));
    ASSERT_TRUE(session_.OnMessage(logon).has_value());
  }
};

TEST(FixConversionsTest, ParseFixSide) {
  EXPECT_EQ(*ParseFixSide("1"), Side::kBuy);
  EXPECT_EQ(*ParseFixSide("2"), Side::kSell);
  EXPECT_FALSE(ParseFixSide("9").has_value());
}

TEST(FixConversionsTest, ParseFixOrdType) {
  EXPECT_EQ(*ParseFixOrdType("1"), OrderType::kMarket);
  EXPECT_EQ(*ParseFixOrdType("2"), OrderType::kLimit);
  EXPECT_FALSE(ParseFixOrdType("9").has_value());
}

TEST(FixConversionsTest, FixStatusChars) {
  EXPECT_EQ(FixOrdStatusChar(OrderStatus::kAccepted), '0');
  EXPECT_EQ(FixOrdStatusChar(OrderStatus::kPartiallyFilled), '1');
  EXPECT_EQ(FixOrdStatusChar(OrderStatus::kFilled), '2');
  EXPECT_EQ(FixOrdStatusChar(OrderStatus::kCancelled), '4');
  EXPECT_EQ(FixOrdStatusChar(OrderStatus::kRejected), '8');
}

TEST_F(FixGatewayTest, NewOrderSinglePassesToOrderManager) {
  LogOn();
  FixMessage nos;
  nos.Set(tag::kMsgType, std::string(msg_type::kNewOrderSingle));
  nos.Set(tag::kClOrdID, "C1");
  nos.Set(tag::kSymbol, "AAPL");
  nos.Set(tag::kSide, "1");
  nos.Set(tag::kOrdType, "2");
  nos.Set(tag::kOrderQty, static_cast<std::int64_t>(100));
  nos.Set(tag::kPrice, static_cast<std::int64_t>(10000));
  auto er = gateway_.HandleNewOrderSingle(nos, session_);
  ASSERT_TRUE(er.has_value());
  EXPECT_EQ(er->MsgType(), "8");
  EXPECT_EQ(er->Get(tag::kSymbol).value_or(""), "AAPL");
  EXPECT_EQ(er->GetInt(tag::kOrderQty), 100);
  EXPECT_EQ(om_.OrderCount(), 1U);
}

TEST_F(FixGatewayTest, NewOrderSingleInvalidMsgType) {
  FixMessage m;
  m.Set(tag::kMsgType, "X");
  auto r = gateway_.HandleNewOrderSingle(m, session_);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error(), OemsError::kFixInvalidMsgType);
}

TEST_F(FixGatewayTest, NewOrderSingleBadSide) {
  LogOn();
  FixMessage nos;
  nos.Set(tag::kMsgType, std::string(msg_type::kNewOrderSingle));
  nos.Set(tag::kSide, "9");
  nos.Set(tag::kOrdType, "2");
  nos.Set(tag::kSymbol, "AAPL");
  nos.Set(tag::kOrderQty, static_cast<std::int64_t>(100));
  auto r = gateway_.HandleNewOrderSingle(nos, session_);
  ASSERT_FALSE(r.has_value());
  EXPECT_EQ(r.error(), OemsError::kInvalidSide);
}

TEST_F(FixGatewayTest, ExecutionReportFillProducesFTag) {
  LogOn();
  // Seed resting sell.
  om_.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                         .side = Side::kSell,
                                         .type = OrderType::kLimit,
                                         .price = 10000,
                                         .quantity = 100});
  FixMessage nos;
  nos.Set(tag::kMsgType, std::string(msg_type::kNewOrderSingle));
  nos.Set(tag::kClOrdID, "C1");
  nos.Set(tag::kSymbol, "AAPL");
  nos.Set(tag::kSide, "1");
  nos.Set(tag::kOrdType, "2");
  nos.Set(tag::kOrderQty, static_cast<std::int64_t>(100));
  nos.Set(tag::kPrice, static_cast<std::int64_t>(10000));
  auto er = gateway_.HandleNewOrderSingle(nos, session_);
  ASSERT_TRUE(er.has_value());
  EXPECT_EQ(er->Get(tag::kExecType).value_or(""), "F");
  EXPECT_EQ(er->Get(tag::kOrdStatus).value_or(""), "2");  // Filled
}

TEST_F(FixGatewayTest, OrderCancelRequest) {
  LogOn();
  auto submitted = om_.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                                          .side = Side::kBuy,
                                                          .type = OrderType::kLimit,
                                                          .price = 10000,
                                                          .quantity = 100});
  ASSERT_TRUE(submitted.has_value());

  FixMessage cancel;
  cancel.Set(tag::kMsgType, std::string(msg_type::kOrderCancelRequest));
  cancel.Set(tag::kOrderID, std::to_string(submitted->internal_id));
  auto er = gateway_.HandleOrderCancelRequest(cancel, session_);
  ASSERT_TRUE(er.has_value());
  EXPECT_EQ(er->Get(tag::kOrdStatus).value_or(""), "4");  // Cancelled
  EXPECT_EQ(er->Get(tag::kExecType).value_or(""), "4");
}

}  // namespace
}  // namespace oems::fix
