#include "core/types/types.h"

#include <gtest/gtest.h>

#include "core/types/error.h"

namespace oems {
namespace {

// --- Symbol ---

TEST(SymbolTest, EqualSymbols) {
  Symbol a{"AAPL"};
  Symbol b{"AAPL"};
  EXPECT_EQ(a, b);
}

TEST(SymbolTest, DifferentSymbols) {
  Symbol a{"AAPL"};
  Symbol b{"GOOG"};
  EXPECT_NE(a, b);
}

TEST(SymbolTest, Ordering) {
  Symbol a{"AAPL"};
  Symbol b{"GOOG"};
  EXPECT_LT(a, b);
  EXPECT_GT(b, a);
}

TEST(SymbolTest, EmptySymbol) {
  Symbol empty{""};
  Symbol non_empty{"X"};
  EXPECT_LT(empty, non_empty);
}

// --- Side ---

TEST(SideTest, Values) { EXPECT_NE(Side::kBuy, Side::kSell); }

TEST(SideTest, Names) {
  EXPECT_STREQ(SideName(Side::kBuy), "Buy");
  EXPECT_STREQ(SideName(Side::kSell), "Sell");
}

// --- OrderType ---

TEST(OrderTypeTest, Values) { EXPECT_NE(OrderType::kLimit, OrderType::kMarket); }

TEST(OrderTypeTest, Names) {
  EXPECT_STREQ(OrderTypeName(OrderType::kLimit), "Limit");
  EXPECT_STREQ(OrderTypeName(OrderType::kMarket), "Market");
}

// --- OrderStatus ---

TEST(OrderStatusTest, AllStatusNames) {
  EXPECT_STREQ(OrderStatusName(OrderStatus::kPendingNew), "PendingNew");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kAccepted), "Accepted");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kPartiallyFilled), "PartiallyFilled");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kFilled), "Filled");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kPendingCancel), "PendingCancel");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kCancelled), "Cancelled");
  EXPECT_STREQ(OrderStatusName(OrderStatus::kRejected), "Rejected");
}

// --- OemsError ---

TEST(OemsErrorTest, AllErrorNames) {
  EXPECT_EQ(ErrorName(OemsError::kInvalidQuantity), "InvalidQuantity");
  EXPECT_EQ(ErrorName(OemsError::kInvalidPrice), "InvalidPrice");
  EXPECT_EQ(ErrorName(OemsError::kRiskBreachMaxQty), "RiskBreachMaxQty");
  EXPECT_EQ(ErrorName(OemsError::kBookNotFound), "BookNotFound");
  EXPECT_EQ(ErrorName(OemsError::kFixParseError), "FixParseError");
  EXPECT_EQ(ErrorName(OemsError::kDatabaseError), "DatabaseError");
  EXPECT_EQ(ErrorName(OemsError::kRouteNotFound), "RouteNotFound");
}

// --- Result ---

TEST(ResultTest, SuccessValue) {
  Result<std::int32_t> result{42};
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), 42);
}

TEST(ResultTest, ErrorValue) {
  Result<std::int32_t> result{std::unexpected(OemsError::kOrderNotFound)};
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

TEST(ResultTest, VoidSuccess) {
  Result<void> result{};
  EXPECT_TRUE(result.has_value());
}

TEST(ResultTest, VoidError) {
  Result<void> result{std::unexpected(OemsError::kRiskBreachMaxQty)};
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kRiskBreachMaxQty);
}

TEST(ResultTest, StringSuccess) {
  Result<std::string> result{"hello"};
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result.value(), "hello");
}

// --- Timestamp ---

TEST(TimestampTest, NowIsNotZero) {
  auto ts = Now();
  auto epoch = Timestamp{};
  EXPECT_GT(ts.time_since_epoch().count(), epoch.time_since_epoch().count());
}

}  // namespace
}  // namespace oems
