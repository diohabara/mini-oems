#include "core/matching/order_book.h"

#include <gtest/gtest.h>

namespace oems::matching {
namespace {

class OrderBookTest : public ::testing::Test {
 protected:
  OrderBook book_{Symbol{"AAPL"}};
};

// --- Basic insertion ---

TEST_F(OrderBookTest, LimitBuyRestsOnEmptyBook) {
  auto result = book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->fills.empty());
  ASSERT_TRUE(result->resting.has_value());
  EXPECT_EQ(result->resting->order_id, 1U);
  EXPECT_EQ(result->resting->price, 10000);
  EXPECT_EQ(result->resting->remaining_qty, 100);
  EXPECT_EQ(book_.OrderCount(), 1U);
}

TEST_F(OrderBookTest, LimitSellRestsOnEmptyBook) {
  auto result = book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10100, 50);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->fills.empty());
  ASSERT_TRUE(result->resting.has_value());
  EXPECT_EQ(book_.OrderCount(), 1U);
}

TEST_F(OrderBookTest, ZeroQuantityRejected) {
  auto result = book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 0);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidQuantity);
}

TEST_F(OrderBookTest, NegativeQuantityRejected) {
  auto result = book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, -10);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidQuantity);
}

TEST_F(OrderBookTest, ZeroPriceLimitRejected) {
  auto result = book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 0, 100);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kInvalidPrice);
}

TEST_F(OrderBookTest, DuplicateOrderIdRejected) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 100));
  auto dup = book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10001, 50);
  ASSERT_FALSE(dup.has_value());
  EXPECT_EQ(dup.error(), OemsError::kDuplicateOrder);
}

// --- Matching ---

TEST_F(OrderBookTest, CrossingLimitProducesFillAtPassivePrice) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 100));
  auto result = book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10005, 100);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->fills.size(), 1U);
  EXPECT_EQ(result->fills[0].price, 10000);  // passive price
  EXPECT_EQ(result->fills[0].quantity, 100);
  EXPECT_EQ(result->fills[0].aggressive_order_id, 2U);
  EXPECT_EQ(result->fills[0].passive_order_id, 1U);
  EXPECT_EQ(result->fills[0].aggressive_side, Side::kBuy);
  EXPECT_FALSE(result->resting.has_value());
  EXPECT_EQ(book_.OrderCount(), 0U);
}

TEST_F(OrderBookTest, MarketBuySweepsAsks) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 50));
  ASSERT_TRUE(book_.AddOrder(2, Side::kSell, OrderType::kLimit, 10001, 50));
  auto result = book_.AddOrder(3, Side::kBuy, OrderType::kMarket, 0, 75);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->fills.size(), 2U);
  EXPECT_EQ(result->fills[0].price, 10000);
  EXPECT_EQ(result->fills[0].quantity, 50);
  EXPECT_EQ(result->fills[1].price, 10001);
  EXPECT_EQ(result->fills[1].quantity, 25);
  EXPECT_FALSE(result->resting.has_value());
}

TEST_F(OrderBookTest, MarketSellSweepsBids) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10001, 50));
  ASSERT_TRUE(book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10000, 50));
  auto result = book_.AddOrder(3, Side::kSell, OrderType::kMarket, 0, 75);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->fills.size(), 2U);
  EXPECT_EQ(result->fills[0].price, 10001);  // best bid first
  EXPECT_EQ(result->fills[0].quantity, 50);
  EXPECT_EQ(result->fills[1].price, 10000);
  EXPECT_EQ(result->fills[1].quantity, 25);
}

TEST_F(OrderBookTest, MarketOrderInsufficientLiquidityNoRest) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 30));
  auto result = book_.AddOrder(2, Side::kBuy, OrderType::kMarket, 0, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->fills.size(), 1U);
  EXPECT_EQ(result->fills[0].quantity, 30);
  EXPECT_FALSE(result->resting.has_value());  // market never rests
  EXPECT_EQ(book_.OrderCount(), 0U);
}

TEST_F(OrderBookTest, LimitPartialFillRemainderRests) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 30));
  auto result = book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10000, 100);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->fills.size(), 1U);
  EXPECT_EQ(result->fills[0].quantity, 30);
  ASSERT_TRUE(result->resting.has_value());
  EXPECT_EQ(result->resting->remaining_qty, 70);
  EXPECT_EQ(book_.OrderCount(), 1U);
}

TEST_F(OrderBookTest, PriceTimePriorityFifo) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 50));
  ASSERT_TRUE(book_.AddOrder(2, Side::kSell, OrderType::kLimit, 10000, 50));
  auto result = book_.AddOrder(3, Side::kBuy, OrderType::kLimit, 10000, 50);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->fills.size(), 1U);
  EXPECT_EQ(result->fills[0].passive_order_id, 1U);  // first in
}

TEST_F(OrderBookTest, LimitOrderDoesNotCrossAboveLimit) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10010, 50));
  auto result = book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10000, 50);
  ASSERT_TRUE(result.has_value());
  EXPECT_TRUE(result->fills.empty());
  ASSERT_TRUE(result->resting.has_value());
  EXPECT_EQ(book_.OrderCount(), 2U);
}

TEST_F(OrderBookTest, MultiLevelSweep) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 10));
  ASSERT_TRUE(book_.AddOrder(2, Side::kSell, OrderType::kLimit, 10001, 10));
  ASSERT_TRUE(book_.AddOrder(3, Side::kSell, OrderType::kLimit, 10002, 10));
  auto result = book_.AddOrder(4, Side::kBuy, OrderType::kLimit, 10002, 30);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->fills.size(), 3U);
  EXPECT_EQ(result->fills[0].price, 10000);
  EXPECT_EQ(result->fills[1].price, 10001);
  EXPECT_EQ(result->fills[2].price, 10002);
  EXPECT_EQ(book_.OrderCount(), 0U);
}

// --- Cancel ---

TEST_F(OrderBookTest, CancelRestingOrder) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 100));
  auto result = book_.CancelOrder(1);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->order_id, 1U);
  EXPECT_EQ(book_.OrderCount(), 0U);
}

TEST_F(OrderBookTest, CancelUnknownOrder) {
  auto result = book_.CancelOrder(42);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

TEST_F(OrderBookTest, CancelFilledOrderReturnsNotFound) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10000, 100));
  ASSERT_TRUE(book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10000, 100));
  auto result = book_.CancelOrder(1);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kOrderNotFound);
}

// --- Snapshots ---

TEST_F(OrderBookTest, BidsSortedBestFirst) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 10));
  ASSERT_TRUE(book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10002, 20));
  ASSERT_TRUE(book_.AddOrder(3, Side::kBuy, OrderType::kLimit, 10001, 30));
  auto bids = book_.Bids();
  ASSERT_EQ(bids.size(), 3U);
  EXPECT_EQ(bids[0].price, 10002);
  EXPECT_EQ(bids[1].price, 10001);
  EXPECT_EQ(bids[2].price, 10000);
}

TEST_F(OrderBookTest, AsksSortedBestFirst) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kSell, OrderType::kLimit, 10002, 10));
  ASSERT_TRUE(book_.AddOrder(2, Side::kSell, OrderType::kLimit, 10000, 20));
  ASSERT_TRUE(book_.AddOrder(3, Side::kSell, OrderType::kLimit, 10001, 30));
  auto asks = book_.Asks();
  ASSERT_EQ(asks.size(), 3U);
  EXPECT_EQ(asks[0].price, 10000);
  EXPECT_EQ(asks[1].price, 10001);
  EXPECT_EQ(asks[2].price, 10002);
}

TEST_F(OrderBookTest, PriceLevelAggregatesQty) {
  ASSERT_TRUE(book_.AddOrder(1, Side::kBuy, OrderType::kLimit, 10000, 10));
  ASSERT_TRUE(book_.AddOrder(2, Side::kBuy, OrderType::kLimit, 10000, 20));
  auto bids = book_.Bids();
  ASSERT_EQ(bids.size(), 1U);
  EXPECT_EQ(bids[0].total_qty, 30);
  EXPECT_EQ(bids[0].order_count, 2U);
}

TEST_F(OrderBookTest, GetSymbolReturnsConstructedSymbol) {
  EXPECT_EQ(book_.GetSymbol().value, "AAPL");
}

}  // namespace
}  // namespace oems::matching
