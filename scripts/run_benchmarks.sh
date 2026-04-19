#!/bin/bash

set -e

cd "$(dirname "$0")/.."

if [ ! -f "build/Release/bench_pool.exe" ] && [ ! -f "build/bench_pool" ]; then
    echo "Building benchmarks..."
    mkdir -p build
    cd build
    cmake .. -DBUILD_BENCHMARKS=ON -DCMAKE_TOOLCHAIN_FILE=C:/Users/user/vcpkg/scripts/buildsystems/vcpkg.cmake
    cmake --build . --config Release
    cd ..
fi

echo "Running benchmarks..."
cd build

if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    ./Release/bench_pool.exe
    echo ""
    ./Release/bench_hashmap.exe
else
    ./bench_pool
    echo ""
    ./bench_hashmap
fi