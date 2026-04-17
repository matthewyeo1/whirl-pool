cmake_minimum_required(VERSION 3.14)
project(whirlpool VERSION 0.1.0 LANGUAGES CXX)

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)

# Header-only library
add_library(whirlpool INTERFACE)
target_include_directories(whirlpool INTERFACE include)

# Single test executable
add_executable(tests tests/test_main.cpp)
target_link_libraries(tests whirlpool)
target_compile_options(tests PRIVATE /O2 /W3)

enable_testing()
add_test(NAME whirlpool_tests COMMAND tests)