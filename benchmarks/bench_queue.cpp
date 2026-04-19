#include <benchmark/benchmark.h>
#include "lockfree/mpmc_queue.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace lockfree;

static void BM_MPMC_MultiThread(benchmark::State& state) {
    const int NUM_PRODUCERS = 2;
    const int NUM_CONSUMERS = 2;
    const int ITEMS = 10000;

    for (auto _ : state) {
        MPMCQueue<int> q;
        std::atomic<int> consumed{0};
        std::atomic<int> producers_finished{0};
        const int TOTAL_ITEMS = NUM_PRODUCERS * ITEMS;

        std::vector<std::thread> producers;
        std::vector<std::thread> consumers;

        // Producers
        for (int p = 0; p < NUM_PRODUCERS; ++p) {
            producers.emplace_back([&]() {
                for (int i = 0; i < ITEMS; ++i) {
                    q.push(i);
                }
                producers_finished.fetch_add(1, std::memory_order_release);
            });
        }

        // Consumers - FIXED: exit on count, not empty()
        for (int c = 0; c < NUM_CONSUMERS; ++c) {
            consumers.emplace_back([&]() {
                while (true) {
                    auto v = q.pop();
                    if (v.has_value()) {
                        int current = consumed.fetch_add(1, std::memory_order_relaxed);
                        if (current + 1 >= TOTAL_ITEMS) {
                            break;  // All items consumed
                        }
                    } else {
                        // Check if producers are done and all items consumed
                        if (producers_finished.load(std::memory_order_acquire) == NUM_PRODUCERS &&
                            consumed.load(std::memory_order_acquire) >= TOTAL_ITEMS) {
                            break;
                        }
                        std::this_thread::yield();
                    }
                }
            });
        }

        for (auto& t : producers) t.join();
        for (auto& t : consumers) t.join();
        
        // Verify all items consumed
        if (consumed.load() != TOTAL_ITEMS) {
            state.SkipWithError("Not all items consumed!");
        }
    }
}
BENCHMARK(BM_MPMC_MultiThread);

BENCHMARK_MAIN();