# whirl-pool

Whirl-Pool is a C++17 header-only library for experimenting with and building
bounded, low-latency infrastructure. Project 3 extends the existing library
directly through small components in the `lockfree` namespace.

## Current components

- SPSC queue
- Bounded MPMC ring buffer
- Object pool
- MPMC queue and Treiber stack with experimental hazard-pointer reclamation
- Fixed-capacity hashmap, atomic counter, and experimental RCU variants

The MPMC queue, stack, hashmap, object pool, ring buffer, and RCU variants are
provisional until the correctness and lifecycle work recorded in
`PHASE_0_CONTRACTS_AND_AUDIT.md` is complete. Benchmark targets are hypotheses,
not universal performance guarantees.

## Build and test

Requirements:

- CMake 3.14+
- A C++17 compiler

```sh
cmake -S . -B build -DWHIRLPOOL_BUILD_TESTS=ON
cmake --build build --config Release --parallel
ctest --test-dir build -C Release --output-on-failure
```

`scripts/run_tests.sh` runs the same workflow. CMake presets are also available
for debug, release, ASan/UBSan, and TSan-capable builds when using CMake 3.21+.

## Consume from the build tree

```cmake
add_subdirectory(path/to/whirl-pool)
target_link_libraries(application PRIVATE whirlpool::whirlpool)
```

Tests default off when Whirl-Pool is included as a subdirectory.

## Install and consume

```sh
cmake -S . -B build \
    -DWHIRLPOOL_BUILD_TESTS=OFF \
    -DCMAKE_INSTALL_PREFIX=/path/to/whirlpool-install
cmake --build build --config Release
cmake --build build --target install --config Release
```

```cmake
find_package(whirlpool CONFIG REQUIRED)
target_link_libraries(application PRIVATE whirlpool::whirlpool)
```

Set `CMAKE_PREFIX_PATH` to the installation prefix when it is outside the
toolchain's normal package search paths.

## Configuration

| Option | Default |
|---|---|
| `WHIRLPOOL_BUILD_TESTS` | On for top-level builds |
| `WHIRLPOOL_BUILD_BENCHMARKS` | Off |
| `WHIRLPOOL_WARNINGS_AS_ERRORS` | Off |
| `WHIRLPOOL_ENABLE_SANITIZERS` | `none` |
| `WHIRLPOOL_CACHE_LINE_SIZE` | `64` |

`BUILD_TESTS` and `BUILD_BENCHMARKS` remain deprecated compatibility inputs for
one minor release.

## Benchmarks

Google Benchmark must be discoverable when benchmarks are enabled:

```sh
cmake -S . -B build -DWHIRLPOOL_BUILD_BENCHMARKS=ON
cmake --build build --config Release --parallel
```

The existing benchmark programs include setup overhead and are preliminary;
do not use them as published performance evidence.

## License

MIT
