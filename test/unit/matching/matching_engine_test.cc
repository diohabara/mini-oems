#include "core/matching/matching_engine.h"

#include <gtest/gtest.h>

namespace oems::matching {
namespace {

TEST(MatchingEngineTest, BookCreatedLazily) {
  MatchingEngine eng;
  EXPECT_EQ(eng.BookCount(), 0U);
  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 1, Side::kBuy, OrderType::kLimit, 10000, 100));
  EXPECT_EQ(eng.BookCount(), 1U);
  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 2, Side::kBuy, OrderType::kLimit, 10001, 100));
  EXPECT_EQ(eng.BookCount(), 1U);
  ASSERT_TRUE(eng.AddOrder(Symbol{"GOOG"}, 3, Side::kBuy, OrderType::kLimit, 20000, 50));
  EXPECT_EQ(eng.BookCount(), 2U);
}

TEST(MatchingEngineTest, OrdersRoutedToCorrectBook) {
  MatchingEngine eng;
  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 1, Side::kSell, OrderType::kLimit, 10000, 100));
  ASSERT_TRUE(eng.AddOrder(Symbol{"GOOG"}, 2, Side::kSell, OrderType::kLimit, 20000, 50));
  // Buy in AAPL should match id 1 only, not touch GOOG.
  auto result = eng.AddOrder(Symbol{"AAPL"}, 3, Side::kBuy, OrderType::kLimit, 10000, 100);
  ASSERT_TRUE(result.has_value());
  ASSERT_EQ(result->fills.size(), 1U);
  EXPECT_EQ(result->fills[0].passive_order_id, 1U);
  EXPECT_EQ(result->fills[0].symbol.value, "AAPL");
}

TEST(MatchingEngineTest, GetBookUnknownSymbol) {
  MatchingEngine eng;
  auto result = eng.GetBook(Symbol{"NONE"});
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kBookNotFound);
}

TEST(MatchingEngineTest, CancelUnknownSymbol) {
  MatchingEngine eng;
  auto result = eng.CancelOrder(Symbol{"NONE"}, 1);
  ASSERT_FALSE(result.has_value());
  EXPECT_EQ(result.error(), OemsError::kBookNotFound);
}

TEST(MatchingEngineTest, CancelViaEngine) {
  MatchingEngine eng;
  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 1, Side::kBuy, OrderType::kLimit, 10000, 100));
  auto result = eng.CancelOrder(Symbol{"AAPL"}, 1);
  ASSERT_TRUE(result.has_value());
  EXPECT_EQ(result->order_id, 1U);
}

TEST(MatchingEngineTest, GetBookReturnsNonNull) {
  MatchingEngine eng;
  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 1, Side::kBuy, OrderType::kLimit, 10000, 100));
  auto book = eng.GetBook(Symbol{"AAPL"});
  ASSERT_TRUE(book.has_value());
  EXPECT_NE(*book, nullptr);
  EXPECT_EQ((*book)->OrderCount(), 1U);
}

TEST(MatchingEngineTest, ExecutionIdsAreUniqueAcrossSymbols) {
  MatchingEngine eng;

  ASSERT_TRUE(eng.AddOrder(Symbol{"AAPL"}, 1, Side::kSell, OrderType::kLimit, 10000, 10));
  auto aapl_fill = eng.AddOrder(Symbol{"AAPL"}, 2, Side::kBuy, OrderType::kLimit, 10000, 10);
  ASSERT_TRUE(aapl_fill.has_value());
  ASSERT_EQ(aapl_fill->fills.size(), 1U);

  ASSERT_TRUE(eng.AddOrder(Symbol{"GOOG"}, 3, Side::kSell, OrderType::kLimit, 20000, 10));
  auto goog_fill = eng.AddOrder(Symbol{"GOOG"}, 4, Side::kBuy, OrderType::kLimit, 20000, 10);
  ASSERT_TRUE(goog_fill.has_value());
  ASSERT_EQ(goog_fill->fills.size(), 1U);

  EXPECT_NE(aapl_fill->fills[0].execution_id, goog_fill->fills[0].execution_id);
}

}  // namespace
}  // namespace oems::matching
