cmake_minimum_required(VERSION 3.14)
project(whirlpool_subdirectory_consumer LANGUAGES CXX)

if(NOT DEFINED WHIRLPOOL_SOURCE_DIR)
    message(FATAL_ERROR "WHIRLPOOL_SOURCE_DIR is required")
endif()

add_subdirectory("${WHIRLPOOL_SOURCE_DIR}" whirlpool EXCLUDE_FROM_ALL)

if(TARGET whirlpool_tests)
    message(FATAL_ERROR
        "Whirl-Pool tests were enabled in a subdirectory build")
endif()

add_executable(whirlpool_subdirectory_consumer main.cpp)
target_link_libraries(whirlpool_subdirectory_consumer
    PRIVATE whirlpool::whirlpool)
