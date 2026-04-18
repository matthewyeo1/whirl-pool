#include <benchmark/benchmark.h>    // Installed via vcpkg
#include "lockfree/pool.hpp"
#include <string>
#include <vector>
#include <iostream>

// ============ SMALL OBJECT ============
struct Particle { 
    float x, y, z; 
    Particle() : x(0), y(0), z(0) {}
};

// ============= LARGE OBJECT =============
struct Chunk {
    double matrix[16];
    static constexpr int FIXED_SIZE = 100;
    int data[FIXED_SIZE]; 
    char name[32];         
    
    Chunk() : matrix{}, data{}, name("test") {}
};

static void BM_NewDelete_Small(benchmark::State& state) {
    for (auto _ : state) {
        auto* p = new Particle();
        benchmark::DoNotOptimize(p);
        delete p;
    }
}
BENCHMARK(BM_NewDelete_Small);

// Initialize pool
static void BM_ObjectPool_Small(benchmark::State& state) {
    lockfree::ObjectPool<Particle, 10000> pool;

    for (auto _ : state) {
        auto p = pool.acquire();
        benchmark::DoNotOptimize(p);
        // Auto-releases when p goes out of scope
    }
}
BENCHMARK(BM_ObjectPool_Small);

static void BM_NewDelete_Large(benchmark::State& state) {
    for (auto _ : state) {
        auto* p = new Chunk();
        benchmark::DoNotOptimize(p);
        delete p;
    }
}
BENCHMARK(BM_NewDelete_Large);

static void BM_ObjectPool_Large(benchmark::State& state) {
    lockfree::ObjectPool<Chunk, 10000> pool;
    for (auto _ : state) {
        auto p = pool.acquire();
        benchmark::DoNotOptimize(p);
    }
}
BENCHMARK(BM_ObjectPool_Large);

BENCHMARK_MAIN();