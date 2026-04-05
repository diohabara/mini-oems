/**
 * @file order_manager_bench.cc
 * @brief Microbenchmarks for end-to-end order submission latency.
 */

#include <benchmark/benchmark.h>

#include "core/matching/matching_engine.h"
#include "core/order/order_manager.h"
#include "core/risk/risk_manager.h"

namespace {

using namespace oems;

void BM_SubmitLimitThroughStack(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    risk::RiskLimits limits;
    limits.max_orders_per_second = 1'000'000;
    risk::RiskManager risk(limits);
    matching::MatchingEngine engine;
    order::OrderManager om(risk, engine);
    state.ResumeTiming();
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      auto r = om.SubmitOrder(order::NewOrderRequest{.symbol = Symbol{"AAPL"},
                                                     .side = Side::kBuy,
                                                     .type = OrderType::kLimit,
                                                     .price = 10000 + (i % 100),
                                                     .quantity = 100});
      benchmark::DoNotOptimize(r);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_SubmitLimitThroughStack)->Arg(1000)->Arg(10000);

}  // namespace
