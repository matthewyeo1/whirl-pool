#!/bin/bash
# Run all tests (excluding known flaky ones)

set -e

cd "$(dirname "$0")/.."  

if [ ! -f "build/Release/tests.exe" ] && [ ! -f "build/tests" ]; then
    echo "Building tests..."
    mkdir -p build
    cd build
    cmake .. -DBUILD_TESTS=ON
    cmake --build . --config Release
    cd ..
fi

echo "Running tests..."
cd build

if [[ "$OSTYPE" == "msys" ]] || [[ "$OSTYPE" == "win32" ]]; then
    ./Release/tests.exe
else
    ./tests
fi