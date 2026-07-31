cmake_minimum_required(VERSION 3.14)
project(whirlpool VERSION 1.0.0 LANGUAGES CXX)

include(CMakePackageConfigHelpers)
include(GNUInstallDirs)
include(cmake/Sanitizers.cmake)

if(CMAKE_SOURCE_DIR STREQUAL PROJECT_SOURCE_DIR)
    set(WHIRLPOOL_TOP_LEVEL ON)
else()
    set(WHIRLPOOL_TOP_LEVEL OFF)
endif()

set(_whirlpool_tests_default ${WHIRLPOOL_TOP_LEVEL})
if(DEFINED BUILD_TESTS)
    message(DEPRECATION
        "BUILD_TESTS is deprecated; use WHIRLPOOL_BUILD_TESTS")
    set(_whirlpool_tests_default ${BUILD_TESTS})
endif()

set(_whirlpool_benchmarks_default OFF)
if(DEFINED BUILD_BENCHMARKS)
    message(DEPRECATION
        "BUILD_BENCHMARKS is deprecated; use WHIRLPOOL_BUILD_BENCHMARKS")
    set(_whirlpool_benchmarks_default ${BUILD_BENCHMARKS})
endif()

option(WHIRLPOOL_BUILD_TESTS
    "Build Whirl-Pool tests" ${_whirlpool_tests_default})
option(WHIRLPOOL_BUILD_BENCHMARKS
    "Build Whirl-Pool benchmarks" ${_whirlpool_benchmarks_default})
option(WHIRLPOOL_WARNINGS_AS_ERRORS
    "Treat warnings as errors for Whirl-Pool-owned targets" OFF)

set(WHIRLPOOL_ENABLE_SANITIZERS "none" CACHE STRING
    "Sanitizers: none, address, undefined, address-undefined, or thread")
set_property(CACHE WHIRLPOOL_ENABLE_SANITIZERS PROPERTY STRINGS
    none address undefined address-undefined thread)
whirlpool_validate_sanitizer_mode()

set(WHIRLPOOL_CACHE_LINE_SIZE "64" CACHE STRING
    "Cache-line size used by Whirl-Pool")
if(NOT WHIRLPOOL_CACHE_LINE_SIZE MATCHES "^[1-9][0-9]*$")
    message(FATAL_ERROR
        "WHIRLPOOL_CACHE_LINE_SIZE must be a positive integer")
endif()
math(EXPR _whirlpool_cache_line_mask
    "${WHIRLPOOL_CACHE_LINE_SIZE} - 1")
math(EXPR _whirlpool_cache_line_remainder
    "${WHIRLPOOL_CACHE_LINE_SIZE} & ${_whirlpool_cache_line_mask}")
if(NOT _whirlpool_cache_line_remainder EQUAL 0)
    message(FATAL_ERROR
        "WHIRLPOOL_CACHE_LINE_SIZE must be a power of two")
endif()

add_library(whirlpool INTERFACE)
add_library(whirlpool::whirlpool ALIAS whirlpool)
target_compile_features(whirlpool INTERFACE cxx_std_17)
target_compile_definitions(whirlpool INTERFACE
    WHIRLPOOL_CACHE_LINE_SIZE=${WHIRLPOOL_CACHE_LINE_SIZE})
target_include_directories(whirlpool INTERFACE
    $<BUILD_INTERFACE:${PROJECT_SOURCE_DIR}/include>
    $<INSTALL_INTERFACE:${CMAKE_INSTALL_INCLUDEDIR}>)

function(whirlpool_apply_project_warnings target)
    if(MSVC)
        # C4324 reports the intentional tail padding produced by cache-line
        # alignas declarations. The padding is part of the library's layout.
        target_compile_options(${target} PRIVATE /W4 /wd4324)
        if(WHIRLPOOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE /WX)
        endif()
    else()
        target_compile_options(${target} PRIVATE
            -Wall -Wextra -Wpedantic)
        if(WHIRLPOOL_WARNINGS_AS_ERRORS)
            target_compile_options(${target} PRIVATE -Werror)
        endif()
    endif()
endfunction()

if(WHIRLPOOL_BUILD_TESTS)
    enable_testing()

    add_executable(whirlpool_tests tests/test_main.cpp)
    target_link_libraries(whirlpool_tests PRIVATE whirlpool::whirlpool)
    whirlpool_apply_project_warnings(whirlpool_tests)
    whirlpool_enable_sanitizers(whirlpool_tests)
    add_test(NAME whirlpool_tests COMMAND whirlpool_tests)

    add_subdirectory(tests/compile)

    add_test(
        NAME whirlpool_package_contract
        COMMAND ${CMAKE_COMMAND}
            -DWHIRLPOOL_SOURCE_DIR=${PROJECT_SOURCE_DIR}
            -DWHIRLPOOL_BINARY_DIR=${PROJECT_BINARY_DIR}
            -DWHIRLPOOL_TEST_CONFIG=$<CONFIG>
            -P ${PROJECT_SOURCE_DIR}/tests/package/run_package_contract.cmake)
endif()

if(WHIRLPOOL_BUILD_BENCHMARKS)
    find_package(benchmark REQUIRED)

    foreach(_benchmark IN ITEMS pool queue compare hashmap)
        add_executable(bench_${_benchmark}
            benchmarks/bench_${_benchmark}.cpp)
        target_link_libraries(bench_${_benchmark}
            PRIVATE whirlpool::whirlpool benchmark::benchmark)
        whirlpool_apply_project_warnings(bench_${_benchmark})
        whirlpool_enable_sanitizers(bench_${_benchmark})
    endforeach()
endif()

set(WHIRLPOOL_INSTALL_CMAKEDIR
    "${CMAKE_INSTALL_LIBDIR}/cmake/whirlpool")

configure_package_config_file(
    cmake/whirlpoolConfig.cmake.in
    "${PROJECT_BINARY_DIR}/whirlpoolConfig.cmake"
    INSTALL_DESTINATION "${WHIRLPOOL_INSTALL_CMAKEDIR}")
write_basic_package_version_file(
    "${PROJECT_BINARY_DIR}/whirlpoolConfigVersion.cmake"
    VERSION ${PROJECT_VERSION}
    COMPATIBILITY SameMajorVersion)

install(TARGETS whirlpool EXPORT whirlpoolTargets)
install(DIRECTORY include/ DESTINATION ${CMAKE_INSTALL_INCLUDEDIR})
install(EXPORT whirlpoolTargets
    FILE whirlpoolTargets.cmake
    NAMESPACE whirlpool::
    DESTINATION "${WHIRLPOOL_INSTALL_CMAKEDIR}")
install(FILES
    "${PROJECT_BINARY_DIR}/whirlpoolConfig.cmake"
    "${PROJECT_BINARY_DIR}/whirlpoolConfigVersion.cmake"
    DESTINATION "${WHIRLPOOL_INSTALL_CMAKEDIR}")
install(FILES README.md LICENSE
    DESTINATION "${CMAKE_INSTALL_DOCDIR}")
