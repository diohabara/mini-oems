#ifndef OEMS_CORE_ALGO_VWAP_H_
#define OEMS_CORE_ALGO_VWAP_H_

/**
 * @file vwap.h
 * @brief Volume-Weighted Average Price (VWAP) slice generator.
 */

#include <vector>

#include "core/algo/twap.h"  // reuses ChildSlice, AlgoParams
#include "core/types/types.h"

namespace oems::algo {

/**
 * @brief Default front-loaded volume profile (matches typical U-shape).
 * Weights must sum to 1.0.
 */
auto DefaultVolumeProfile(std::int32_t buckets) -> std::vector<double>;

/**
 * @brief Generate slices weighted by a volume profile.
 *
 * The profile supplies one weight per slice (length must equal
 * @c params.num_slices).  Weights are normalised internally if they do
 * not sum to 1.0.  Any rounding remainder is added to the last slice.
 *
 * @param params Parent order and slicing parameters.
 * @param start Scheduling origin.
 * @param profile Per-slice weights; empty => use @c DefaultVolumeProfile.
 * @return Ordered vector of slices.
 */
auto GenerateVwapSlices(const AlgoParams& params, Timestamp start, std::vector<double> profile)
    -> std::vector<ChildSlice>;

}  // namespace oems::algo

#endif  // OEMS_CORE_ALGO_VWAP_H_
