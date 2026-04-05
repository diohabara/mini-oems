#include "core/algo/algo_engine.h"

#include "core/algo/vwap.h"

namespace oems::algo {

AlgoEngine::AlgoEngine(order::OrderManager& om) : om_(om) {}

auto AlgoEngine::StartAlgo(const AlgoRequest& req) -> Result<std::uint64_t> {
  if (req.params.num_slices <= 0 || req.params.parent.quantity <= 0) {
    return std::unexpected(OemsError::kInvalidQuantity);
  }
  auto start = Now();
  std::vector<ChildSlice> slices;
  switch (req.type) {
    case AlgoType::kTwap:
      slices = GenerateTwapSlices(req.params, start);
      break;
    case AlgoType::kVwap:
      slices = GenerateVwapSlices(req.params, start, req.vwap_profile);
      break;
  }
  if (slices.empty()) {
    return std::unexpected(OemsError::kInvalidQuantity);
  }

  AlgoRun run;
  run.type = req.type;
  run.params = req.params;
  for (const auto& slice : slices) {
    auto result = om_.SubmitOrder(slice.request);
    if (result.has_value()) {
      run.child_order_ids.push_back(result->internal_id);
      ++run.submitted;
    } else {
      ++run.rejected;
    }
  }
  std::uint64_t id = next_run_id_++;
  runs_.emplace(id, std::move(run));
  return id;
}

auto AlgoEngine::GetRun(std::uint64_t run_id) const -> Result<AlgoRun> {
  auto it = runs_.find(run_id);
  if (it == runs_.end()) {
    return std::unexpected(OemsError::kOrderNotFound);
  }
  return it->second;
}

}  // namespace oems::algo
