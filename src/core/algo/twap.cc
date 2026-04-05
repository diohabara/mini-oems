#include "core/algo/twap.h"

namespace oems::algo {

auto GenerateTwapSlices(const AlgoParams& params, Timestamp start) -> std::vector<ChildSlice> {
  std::vector<ChildSlice> slices;
  if (params.num_slices <= 0 || params.parent.quantity <= 0) {
    return slices;
  }
  Quantity per_slice = params.parent.quantity / params.num_slices;
  Quantity remainder = params.parent.quantity % params.num_slices;

  auto step = params.duration / params.num_slices;
  for (std::int32_t i = 0; i < params.num_slices; ++i) {
    Quantity qty = per_slice;
    if (i == params.num_slices - 1) {
      qty += remainder;
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
