/**
 * @file matching_bench.cc
 * @brief Google Benchmark microbenchmarks for the matching engine.
 */

#include <benchmark/benchmark.h>

#include "core/matching/matching_engine.h"
#include "core/matching/order_book.h"

namespace {

using oems::OrderType;
using oems::Side;
using oems::Symbol;
using oems::matching::OrderBook;

void BM_AddLimitOrders(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    OrderBook book(Symbol{"AAPL"});
    state.ResumeTiming();
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      auto r = book.AddOrder(static_cast<oems::OrderId>(i + 1), Side::kBuy, OrderType::kLimit,
                             10000 + (i % 100), 100);
      benchmark::DoNotOptimize(r);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_AddLimitOrders)->Arg(1000)->Arg(10000);

void BM_CrossingOrders(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    OrderBook book(Symbol{"AAPL"});
    oems::OrderId id = 1;
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      book.AddOrder(id++, Side::kSell, OrderType::kLimit, 10000, 100).value();
    }
    state.ResumeTiming();
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      auto r = book.AddOrder(id++, Side::kBuy, OrderType::kLimit, 10000, 100);
      benchmark::DoNotOptimize(r);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_CrossingOrders)->Arg(1000)->Arg(10000);

void BM_Cancel(benchmark::State& state) {
  for (auto _ : state) {
    state.PauseTiming();
    OrderBook book(Symbol{"AAPL"});
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      book.AddOrder(static_cast<oems::OrderId>(i + 1), Side::kBuy, OrderType::kLimit,
                    10000 + (i % 100), 100)
          .value();
    }
    state.ResumeTiming();
    for (std::int32_t i = 0; i < state.range(0); ++i) {
      auto r = book.CancelOrder(static_cast<oems::OrderId>(i + 1));
      benchmark::DoNotOptimize(r);
    }
  }
  state.SetItemsProcessed(state.iterations() * state.range(0));
}
BENCHMARK(BM_Cancel)->Arg(1000)->Arg(10000);

}  // namespace
