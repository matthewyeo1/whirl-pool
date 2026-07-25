#!/usr/bin/env bash

set -euo pipefail

project_dir="$(cd "$(dirname "$0")/.." && pwd)"
build_dir="${project_dir}/build"

cmake -S "${project_dir}" -B "${build_dir}" \
    -DWHIRLPOOL_BUILD_TESTS=OFF \
    -DWHIRLPOOL_BUILD_BENCHMARKS=ON
cmake --build "${build_dir}" --config Release --parallel

benchmark_dir="${build_dir}"
if [[ -d "${build_dir}/Release" ]]; then
    benchmark_dir="${build_dir}/Release"
fi

for benchmark_name in pool queue compare hashmap; do
    "${benchmark_dir}/bench_${benchmark_name}"
done
