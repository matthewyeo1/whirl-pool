#include <benchmark/benchmark.h>

#include "lockfree/mpmc_queue.hpp"

#include <atomic>
#include <memory_resource>
#include <thread>
#include <vector>

using namespace lockfree;

static void BM_MPMC_MultiThread(benchmark::State& state) {
    constexpr int NUM_PRODUCERS = 2;
    constexpr int NUM_CONSUMERS = 2;
    constexpr int ITEMS_PER_PRODUCER = 10000;
    constexpr int ITEMS_PER_CONSUMER =
        (NUM_PRODUCERS * ITEMS_PER_PRODUCER) / NUM_CONSUMERS;
    constexpr int NUM_WORKERS = NUM_PRODUCERS + NUM_CONSUMERS;

    // The pool is thread-safe and remains alive until after q is destroyed.
    // A warm-up round fills it, so timed rounds do not include cold heap growth.
    std::pmr::synchronized_pool_resource node_pool;
    MPMCQueue<int> q(&node_pool);
    std::atomic<int> generation{0};
    std::atomic<int> workers_ready{0};
    std::atomic<int> workers_finished{0};
    std::atomic<int> producers_finished{0};
    std::atomic<bool> lost_item{false};
    std::atomic<bool> stop{false};
    std::vector<std::thread> workers;
    workers.reserve(NUM_WORKERS);

    auto wait_for_next_generation = [&](int& observed_generation) {
        int generation_value;
        while ((generation_value = generation.load(std::memory_order_acquire)) ==
               observed_generation) {
            std::this_thread::yield();
        }
        observed_generation = generation_value;
    };

    for (int producer = 0; producer < NUM_PRODUCERS; ++producer) {
        workers.emplace_back([&, producer]() {
            int observed_generation = 0;
            workers_ready.fetch_add(1, std::memory_order_release);
            while (true) {
                wait_for_next_generation(observed_generation);
                if (stop.load(std::memory_order_acquire)) break;

                for (int item = 0; item < ITEMS_PER_PRODUCER; ++item) {
                    q.push(producer * ITEMS_PER_PRODUCER + item);
                }
                producers_finished.fetch_add(1, std::memory_order_release);
                workers_finished.fetch_add(1, std::memory_order_release);
            }
        });
    }

    for (int consumer = 0; consumer < NUM_CONSUMERS; ++consumer) {
        workers.emplace_back([&]() {
            int observed_generation = 0;
            workers_ready.fetch_add(1, std::memory_order_release);
            while (true) {
                wait_for_next_generation(observed_generation);
                if (stop.load(std::memory_order_acquire)) break;

                int received = 0;
                while (received < ITEMS_PER_CONSUMER) {
                    if (q.pop().has_value()) {
                        ++received;
                    } else if (producers_finished.load(std::memory_order_acquire) ==
                               NUM_PRODUCERS) {
                        lost_item.store(true, std::memory_order_relaxed);
                        break;
                    } else {
                        std::this_thread::yield();
                    }
                }
                workers_finished.fetch_add(1, std::memory_order_release);
            }
        });
    }

    while (workers_ready.load(std::memory_order_acquire) != NUM_WORKERS) {
        std::this_thread::yield();
    }

    const auto run_round = [&]() {
        workers_finished.store(0, std::memory_order_relaxed);
        producers_finished.store(0, std::memory_order_relaxed);
        lost_item.store(false, std::memory_order_relaxed);
        generation.fetch_add(1, std::memory_order_release);
        while (workers_finished.load(std::memory_order_acquire) != NUM_WORKERS) {
            std::this_thread::yield();
        }
        return !lost_item.load(std::memory_order_relaxed);
    };

    if (!run_round()) {
        state.SkipWithError("MPMC queue lost an item during warm-up");
    } else {
        for (auto _ : state) {
            if (!run_round()) {
                state.SkipWithError("MPMC queue lost an item");
                break;
            }
        }
    }

    stop.store(true, std::memory_order_release);
    generation.fetch_add(1, std::memory_order_release);
    for (auto& worker : workers) worker.join();

    state.SetItemsProcessed(state.iterations() *
        NUM_PRODUCERS * ITEMS_PER_PRODUCER * 2);
}

BENCHMARK(BM_MPMC_MultiThread)->UseRealTime();

BENCHMARK_MAIN();
