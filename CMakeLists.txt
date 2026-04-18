cmake_minimum_required(VERSION 3.14)
project(whirlpool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# CI build option (default OFF)
option(CI_BUILD "Building in CI environment" OFF)

# Header-only library
add_library(whirlpool INTERFACE)
target_include_directories(whirlpool INTERFACE include)

# Single test executable
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests whirlpool)

# Pass CI_BUILD as a compiler definition
if(CI_BUILD)
    target_compile_definitions(tests PRIVATE CI_BUILD=1)
endif()

# Platform-specific compiler flags
if(MSVC)
    target_compile_options(tests PRIVATE /O2 /W3)
else()
    target_compile_options(tests PRIVATE -O2 -Wall -Wextra)
endif()

enable_testing()
add_test(NAME whirlpool_tests COMMAND tests)

# Benchmarks (optional, off by default for CI)
option(BUILD_BENCHMARKS "Build benchmarks" OFF)

if(BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)
    add_executable(bench_pool benchmarks/bench_pool.cpp)
    target_link_libraries(bench_pool whirlpool benchmark::benchmark)
    
    if(MSVC)
        target_compile_options(bench_pool PRIVATE /O2)
    else()
        target_compile_options(bench_pool PRIVATE -O2)
    endif()
endif()