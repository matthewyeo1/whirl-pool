#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${project_dir}/build"

cmake -S "${project_dir}" -B "${build_dir}" \
    -DWHIRLPOOL_BUILD_TESTS=ON \
    -DWHIRLPOOL_BUILD_BENCHMARKS=OFF
cmake --build "${build_dir}" --config Release --parallel
ctest --test-dir "${build_dir}" -C Release --output-on-failure
