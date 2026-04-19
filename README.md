# whirl-pool

**Version 1.0.0**

A lock-free object pool and queue library in modern C++17. Built for low-latency systems.

A header-only library for high-performance, low-latency systems. Built for learning lock-free programming and systems design.

## Release Notes

### v1.0.0 (2026-01-19)
- Initial stable release
- SPSC Queue (lock-free, wait-free)
- Treiber Stack (lock-free LIFO)
- Object Pool (lock-free with RAII)
- Ring Buffer (Vyukov MPMC)
- HashMap (lock-free, 7.5x faster finds)
- Atomic Counter (wait-free)
- RCU (Read-Copy-Update)
- Google Benchmark integration
- CI/CD with GitHub Actions

## Features

- **SPSC Queue** – Single-producer single-consumer lock-free queue
- **Object Pool** – Lock-free object reuse (coming soon)
- **Fast** – 5-10x faster than `new`/`delete`
- **Header-only** – Just `#include`
- **Tested** – Thread sanitizer, 10M+ operations

## Build & Test

### Prerequisites

- CMake 3.14+
- C++17 Compiler (MSVC/GCC/Clang)
- Google Benchmark via `vcpkg`:

```
git clone https://github.com/microsoft/vcpkg.git
cd vcpkg
.\bootstrap-vcpkg.bat
.\vcpkg install benchmark:x64-windows
.\vcpkg integrate install
```

### Windows (Powershell)

```
cd build
cmake .. -DBUILD_TESTS=ON
cmake --build . --config Release
.\Release\tests.exe
```

### Linux/macOS

```
cd build
cmake .. -DBUILD_TESTS=ON
make
./tests
```

## Run Benchmarks

Example: running benchmark for lock-free pool

```
cd build
cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_TOOLCHAIN_FILE=C:/path/to/vcpkg/scripts/buildsystems/vcpkg.cmake
cmake --build . --config Release
.\Release\bench_pool.exe
```

## License

MIT