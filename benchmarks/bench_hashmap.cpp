#include <benchmark/benchmark.h>
#include "lockfree/hashmap.hpp"
#include <unordered_map>
#include <mutex>

static constexpr size_t CAPACITY = 65536;
static constexpr int NUM_KEYS = 10000;

// ============ STD::MAP BENCHMARKS ============

static void BM_StdMap_Insert(benchmark::State& state) {
    std::unordered_map<int, double> map;
    std::mutex mtx;
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        std::lock_guard<std::mutex> lock(mtx);
        map[key] = static_cast<double>(key);
    }
}
BENCHMARK(BM_StdMap_Insert);

static void BM_StdMap_Find(benchmark::State& state) {
    std::unordered_map<int, double> map;
    std::mutex mtx;
    
    // Pre-populate
    for (int i = 0; i < NUM_KEYS; i++) {
        map[i] = static_cast<double>(i);
    }
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        std::lock_guard<std::mutex> lock(mtx);
        auto it = map.find(key);
        benchmark::DoNotOptimize(it);
    }
}
BENCHMARK(BM_StdMap_Find);

static void BM_StdMap_Erase(benchmark::State& state) {
    std::unordered_map<int, double> map;
    std::mutex mtx;
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        std::lock_guard<std::mutex> lock(mtx);
        map.erase(key);
        map[key] = static_cast<double>(key);  // Re-insert to keep size constant
    }
}
BENCHMARK(BM_StdMap_Erase);

// ============ WHIRL-POOL HASHMAP BENCHMARKS ============

static void BM_WhirlPool_Insert(benchmark::State& state) {
    lockfree::HashMap<int, double, CAPACITY> map;
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        map.insert(key, static_cast<double>(key));
    }
}
BENCHMARK(BM_WhirlPool_Insert);

static void BM_WhirlPool_Find(benchmark::State& state) {
    lockfree::HashMap<int, double, CAPACITY> map;
    
    // Pre-populate
    for (int i = 0; i < NUM_KEYS; i++) {
        map.insert(i, static_cast<double>(i));
    }
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        auto val = map.find(key);
        benchmark::DoNotOptimize(val);
    }
}
BENCHMARK(BM_WhirlPool_Find);

static void BM_WhirlPool_Erase(benchmark::State& state) {
    lockfree::HashMap<int, double, CAPACITY> map;
    
    // Pre-populate
    for (int i = 0; i < NUM_KEYS; i++) {
        map.insert(i, static_cast<double>(i));
    }
    
    for (auto _ : state) {
        int key = state.iterations() % NUM_KEYS;
        map.erase(key);
        map.insert(key, static_cast<double>(key));  // Re-insert to keep size constant
    }
}
BENCHMARK(BM_WhirlPool_Erase);

BENCHMARK_MAIN();