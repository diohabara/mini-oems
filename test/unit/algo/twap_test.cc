#include "core/algo/twap.h"

#include <gtest/gtest.h>

#include <chrono>

namespace oems::algo {
namespace {

AlgoParams MakeParams(Quantity qty, std::int32_t slices) {
  AlgoParams p;
  p.parent.symbol = Symbol{"AAPL"};
  p.parent.side = Side::kBuy;
  p.parent.type = OrderType::kLimit;
  p.parent.price = 10000;
  p.parent.quantity = qty;
  p.num_slices = slices;
  p.duration = std::chrono::seconds(60);
  return p;
}

TEST(TwapTest, EvenDivision) {
  auto slices = GenerateTwapSlices(MakeParams(1000, 5), Now());
  ASSERT_EQ(slices.size(), 5U);
  for (const auto& s : slices) {
    EXPECT_EQ(s.request.quantity, 200);
  }
}

TEST(TwapTest, RemainderInLastSlice) {
  auto slices = GenerateTwapSlices(MakeParams(1001, 5), Now());
  ASSERT_EQ(slices.size(), 5U);
  EXPECT_EQ(slices[0].request.quantity, 200);
  EXPECT_EQ(slices[3].request.quantity, 200);
  EXPECT_EQ(slices[4].request.quantity, 201);
}

TEST(TwapTest, TotalEqualsParentQuantity) {
  auto slices = GenerateTwapSlices(MakeParams(997, 7), Now());
  Quantity total = 0;
  for (const auto& s : slices) {
    total += s.request.quantity;
  }
  EXPECT_EQ(total, 997);
}

TEST(TwapTest, SingleSlice) {
  auto slices = GenerateTwapSlices(MakeParams(500, 1), Now());
  ASSERT_EQ(slices.size(), 1U);
  EXPECT_EQ(slices[0].request.quantity, 500);
}

TEST(TwapTest, ScheduledEvenlySpaced) {
  auto start = Now();
  auto slices = GenerateTwapSlices(MakeParams(100, 4), start);
  ASSERT_EQ(slices.size(), 4U);
  auto dt = std::chrono::seconds(60) / 4;
  EXPECT_EQ(slices[0].scheduled_at, start);
  EXPECT_EQ(slices[1].scheduled_at, start + dt);
  EXPECT_EQ(slices[2].scheduled_at, start + 2 * dt);
  EXPECT_EQ(slices[3].scheduled_at, start + 3 * dt);
}

TEST(TwapTest, ZeroSlicesEmpty) {
  auto slices = GenerateTwapSlices(MakeParams(100, 0), Now());
  EXPECT_TRUE(slices.empty());
}

TEST(TwapTest, ZeroQuantityEmpty) {
  auto slices = GenerateTwapSlices(MakeParams(0, 5), Now());
  EXPECT_TRUE(slices.empty());
}

TEST(TwapTest, ChildInheritsSymbolSidePrice) {
  auto slices = GenerateTwapSlices(MakeParams(100, 2), Now());
  ASSERT_EQ(slices.size(), 2U);
  EXPECT_EQ(slices[0].request.symbol.value, "AAPL");
  EXPECT_EQ(slices[0].request.side, Side::kBuy);
  EXPECT_EQ(slices[0].request.price, 10000);
}

}  // namespace
}  // namespace oems::algo
