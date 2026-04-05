#include "core/algo/vwap.h"

#include <cmath>
#include <numeric>

namespace oems::algo {

auto DefaultVolumeProfile(std::int32_t buckets) -> std::vector<double> {
  // Simple U-shape: heavy at open and close, light midday.
  std::vector<double> weights;
  weights.reserve(buckets);
  if (buckets <= 0) {
    return weights;
  }
  for (std::int32_t i = 0; i < buckets; ++i) {
    // Parabola centered at midpoint; higher at ends.
    double mid = static_cast<double>(buckets - 1) / 2.0;
    double x = static_cast<double>(i) - mid;
    double w = 1.0 + ((x * x) / ((mid * mid) + 1.0));
    weights.push_back(w);
  }
  double sum = std::accumulate(weights.begin(), weights.end(), 0.0);
  for (auto& w : weights) {
    w /= sum;
  }
  return weights;
}

auto GenerateVwapSlices(const AlgoParams& params, Timestamp start, std::vector<double> profile)
    -> std::vector<ChildSlice> {
  std::vector<ChildSlice> slices;
  if (params.num_slices <= 0 || params.parent.quantity <= 0) {
    return slices;
  }
  if (profile.empty()) {
    profile = DefaultVolumeProfile(params.num_slices);
  }
  if (profile.size() != static_cast<std::size_t>(params.num_slices)) {
    return slices;  // mismatch
  }
  double sum = std::accumulate(profile.begin(), profile.end(), 0.0);
  if (sum <= 0) {
    return slices;
  }
  // Normalise.
  for (auto& w : profile) {
    w /= sum;
  }

  auto step = params.duration / params.num_slices;
  Quantity allocated = 0;
  for (std::int32_t i = 0; i < params.num_slices; ++i) {
    Quantity qty = 0;
    if (i < params.num_slices - 1) {
      qty = static_cast<Quantity>(
          std::llround(static_cast<double>(params.parent.quantity) * profile[i]));
      allocated += qty;
    } else {
      // Last slice absorbs rounding drift.
      qty = params.parent.quantity - allocated;
    }
    order::NewOrderRequest child = params.parent;
    child.quantity = qty;
    slices.push_back(ChildSlice{
        .request = child,
        .scheduled_at = start + step * i,
    });
  }
  return slices;
}

}  // namespace oems::algo
