#include <benchmark/benchmark.h>
#include "lockfree/spsc_queue.hpp"
#include "lockfree/ringbuffer.hpp"
#include <thread>
#include <vector>
#include <atomic>

using namespace lockfree;

static void BM_SPSC_vs_RingBuffer_SPSC(benchmark::State& state) {
    const int ITEMS = 100000;

    for (auto _ : state) {
        // Test SPSCQueue
        {
            SPSCQueue<int, 1024> spsc;
            std::atomic<int> consumed{0};
            std::atomic<bool> producer_done{false};
            const int TOTAL_ITEMS = ITEMS;

            std::thread producer([&]() {
                for (int i = 0; i < TOTAL_ITEMS; ++i) {
                    while (!spsc.push(i)) {
                        std::this_thread::yield();
                    }
                }
                producer_done.store(true, std::memory_order_release);
            });

            std::thread consumer([&]() {
                while (true) {
                    auto v = spsc.pop();
                    if (v.has_value()) {
                        int current = consumed.fetch_add(1, std::memory_order_relaxed);
                        if (current + 1 >= TOTAL_ITEMS) {
                            break;
                        }
                    } else {
                        if (producer_done.load(std::memory_order_acquire) &&
                            consumed.load(std::memory_order_acquire) >= TOTAL_ITEMS) {
                            break;
                        }
                        std::this_thread::yield();
                    }
                }
            });

            producer.join();
            consumer.join();
            
            if (consumed.load() != TOTAL_ITEMS) {
                state.SkipWithError("SPSC: Not all items consumed!");
            }
        }

        // Test RingBuffer
        {
            lockfree::RingBuffer<int, 1024> ring;
            std::atomic<int> consumed{0};
            std::atomic<bool> producer_done{false};
            const int TOTAL_ITEMS = ITEMS;

            std::thread producer([&]() {
                for (int i = 0; i < TOTAL_ITEMS; ++i) {
                    while (!ring.push(i)) {
                        std::this_thread::yield();
                    }
                }
                producer_done.store(true, std::memory_order_release);
            });

            std::thread consumer([&]() {
                while (true) {
                    auto v = ring.pop();
                    if (v.has_value()) {
                        int current = consumed.fetch_add(1, std::memory_order_relaxed);
                        if (current + 1 >= TOTAL_ITEMS) {
                            break;
                        }
                    } else {
                        if (producer_done.load(std::memory_order_acquire) &&
                            consumed.load(std::memory_order_acquire) >= TOTAL_ITEMS) {
                            break;
                        }
                        std::this_thread::yield();
                    }
                }
            });

            producer.join();
            consumer.join();
            
            if (consumed.load() != TOTAL_ITEMS) {
                state.SkipWithError("RingBuffer: Not all items consumed!");
            }
        }
    }
}
BENCHMARK(BM_SPSC_vs_RingBuffer_SPSC);

BENCHMARK_MAIN();