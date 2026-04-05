#include "core/algo/vwap.h"

#include <gtest/gtest.h>

#include <chrono>
#include <numeric>

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

TEST(VwapTest, DefaultProfileSumsToOne) {
  auto profile = DefaultVolumeProfile(5);
  ASSERT_EQ(profile.size(), 5U);
  double sum = std::accumulate(profile.begin(), profile.end(), 0.0);
  EXPECT_NEAR(sum, 1.0, 1e-9);
}

TEST(VwapTest, DefaultProfileHeavierAtEdges) {
  auto profile = DefaultVolumeProfile(5);
  // U-shape: [0] and [4] should be > [2] (midpoint).
  EXPECT_GT(profile[0], profile[2]);
  EXPECT_GT(profile[4], profile[2]);
}

TEST(VwapTest, TotalEqualsParentQuantity) {
  auto slices = GenerateVwapSlices(MakeParams(1000, 5), Now(), {});
  ASSERT_EQ(slices.size(), 5U);
  Quantity total = 0;
  for (const auto& s : slices) {
    total += s.request.quantity;
  }
  EXPECT_EQ(total, 1000);
}

TEST(VwapTest, CustomProfile) {
  auto slices = GenerateVwapSlices(MakeParams(100, 4), Now(), {0.4, 0.3, 0.2, 0.1});
  ASSERT_EQ(slices.size(), 4U);
  EXPECT_EQ(slices[0].request.quantity, 40);
  EXPECT_EQ(slices[1].request.quantity, 30);
  EXPECT_EQ(slices[2].request.quantity, 20);
  EXPECT_EQ(slices[3].request.quantity, 10);
}

TEST(VwapTest, UnnormalizedProfileNormalised) {
  auto slices = GenerateVwapSlices(MakeParams(100, 4), Now(), {4.0, 3.0, 2.0, 1.0});
  Quantity total = 0;
  for (const auto& s : slices) {
    total += s.request.quantity;
  }
  EXPECT_EQ(total, 100);
}

TEST(VwapTest, ProfileLengthMismatchReturnsEmpty) {
  auto slices = GenerateVwapSlices(MakeParams(100, 4), Now(), {0.5, 0.5});
  EXPECT_TRUE(slices.empty());
}

TEST(VwapTest, ZeroQuantityEmpty) {
  auto slices = GenerateVwapSlices(MakeParams(0, 4), Now(), {});
  EXPECT_TRUE(slices.empty());
}

TEST(VwapTest, AlgoEngineIntegrationWouldRequireFull) {
  // Integration path is covered in order_flow_test.
  SUCCEED();
}

}  // namespace
}  // namespace oems::algo
