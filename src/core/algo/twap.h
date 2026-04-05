#ifndef OEMS_CORE_ALGO_TWAP_H_
#define OEMS_CORE_ALGO_TWAP_H_

/**
 * @file twap.h
 * @brief Time-Weighted Average Price (TWAP) slice generator.
 */

#include <chrono>
#include <vector>

#include "core/order/order.h"
#include "core/types/types.h"

namespace oems::algo {

/**
 * @brief One child-order slice scheduled for a TWAP/VWAP parent.
 */
struct ChildSlice {
  order::NewOrderRequest request;
  Timestamp scheduled_at;
};

/**
 * @brief Parameters driving slice generation.
 */
struct AlgoParams {
  order::NewOrderRequest parent;
  std::chrono::seconds duration{60};
  std::int32_t num_slices{10};
};

/**
 * @brief Generate evenly spaced, equal-quantity child slices.
 *
 * Quantity is distributed evenly across slices.  Any remainder from
 * integer division is added to the last slice so the total equals the
 * parent quantity exactly.  Slices are scheduled at @c start + k * dt
 * where @c dt = duration / num_slices.
 *
 * @param params Parent order and slicing parameters.
 * @param start Scheduling origin (usually @c Now()).
 * @return Ordered vector of slices (may be empty if params invalid).
 */
auto GenerateTwapSlices(const AlgoParams& params, Timestamp start) -> std::vector<ChildSlice>;

}  // namespace oems::algo

#endif  // OEMS_CORE_ALGO_TWAP_H_
