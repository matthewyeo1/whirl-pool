cmake_minimum_required(VERSION 3.14)
project(whirlpool_package_consumer LANGUAGES CXX)

find_package(whirlpool CONFIG REQUIRED)

add_executable(whirlpool_package_consumer main.cpp)
target_link_libraries(whirlpool_package_consumer
    PRIVATE whirlpool::whirlpool)
