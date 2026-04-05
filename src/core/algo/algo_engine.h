#ifndef OEMS_CORE_ALGO_ALGO_ENGINE_H_
#define OEMS_CORE_ALGO_ALGO_ENGINE_H_

/**
 * @file algo_engine.h
 * @brief Parent/child execution algorithm coordinator.
 *
 * For v1 the engine generates and immediately submits all child slices
 * (time-scheduled execution is left as a future enhancement).  Child
 * orders are submitted via OrderManager so they pass through risk and
 * audit just like externally submitted orders.
 */

#include <unordered_map>
#include <vector>

#include "core/algo/twap.h"
#include "core/order/order_manager.h"
#include "core/types/error.h"
#include "core/types/types.h"

namespace oems::algo {

/**
 * @brief Algo strategy selector.
 */
enum class AlgoType : std::uint8_t { kTwap, kVwap };

/**
 * @brief Invocation parameters for an algo run.
 */
struct AlgoRequest {
  AlgoType type{AlgoType::kTwap};
  AlgoParams params;
  std::vector<double> vwap_profile;  ///< Only used for VWAP.
};

/**
 * @brief Snapshot of an algo run's progress.
 */
struct AlgoRun {
  AlgoType type{AlgoType::kTwap};
  AlgoParams params;
  std::vector<OrderId> child_order_ids;
  std::size_t submitted{0};
  std::size_t rejected{0};
};

/**
 * @brief Coordinator that launches algo runs and submits child orders.
 */
class AlgoEngine {
 public:
  explicit AlgoEngine(order::OrderManager& om);

  /**
   * @brief Launch an algo run.
   * @return Synthetic run id (assigned sequentially).
   */
  auto StartAlgo(const AlgoRequest& req) -> Result<std::uint64_t>;

  /**
   * @brief Return a snapshot of a run by id.
   */
  [[nodiscard]] auto GetRun(std::uint64_t run_id) const -> Result<AlgoRun>;

  /**
   * @brief Number of active/historical runs.
   */
  [[nodiscard]] auto RunCount() const -> std::size_t { return runs_.size(); }

 private:
  order::OrderManager& om_;
  std::uint64_t next_run_id_{1};
  std::unordered_map<std::uint64_t, AlgoRun> runs_;
};

}  // namespace oems::algo

#endif  // OEMS_CORE_ALGO_ALGO_ENGINE_H_
