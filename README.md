# whirl-pool
A lock-free object pool and queue library in modern C++17. Built for low-latency systems.

A header-only library for high-performance, low-latency systems. Built for learning lock-free programming and systems design.

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

## License

MIT