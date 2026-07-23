if(NOT DEFINED WHIRLPOOL_SOURCE_DIR OR
   NOT DEFINED WHIRLPOOL_BINARY_DIR)
    message(FATAL_ERROR
        "WHIRLPOOL_SOURCE_DIR and WHIRLPOOL_BINARY_DIR are required")
endif()

set(_root "${WHIRLPOOL_BINARY_DIR}/package-contract")
set(_prefix "${_root}/install")
set(_install_build "${_root}/install-build")
set(_consumer_build "${_root}/consumer-build")
set(_subdirectory_build "${_root}/subdirectory-build")
file(REMOVE_RECURSE "${_root}")
file(MAKE_DIRECTORY "${_root}")

set(_config_args)
if(DEFINED WHIRLPOOL_TEST_CONFIG AND
   NOT WHIRLPOOL_TEST_CONFIG STREQUAL "")
    list(APPEND _config_args --config "${WHIRLPOOL_TEST_CONFIG}")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${WHIRLPOOL_SOURCE_DIR}"
        -B "${_install_build}"
        "-DCMAKE_INSTALL_PREFIX=${_prefix}"
        -DWHIRLPOOL_BUILD_TESTS=OFF
        -DWHIRLPOOL_BUILD_BENCHMARKS=OFF
    RESULT_VARIABLE _install_configure_result)
if(NOT _install_configure_result EQUAL 0)
    message(FATAL_ERROR "Whirl-Pool install configure failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_install_build}"
        --target install ${_config_args}
    RESULT_VARIABLE _install_build_result)
if(NOT _install_build_result EQUAL 0)
    message(FATAL_ERROR "Whirl-Pool install build failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${WHIRLPOOL_SOURCE_DIR}/tests/package/consumer"
        -B "${_consumer_build}"
        "-DCMAKE_PREFIX_PATH=${_prefix}"
    RESULT_VARIABLE _consumer_configure_result)
if(NOT _consumer_configure_result EQUAL 0)
    message(FATAL_ERROR "Installed consumer configure failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_consumer_build}"
        ${_config_args}
    RESULT_VARIABLE _consumer_build_result)
if(NOT _consumer_build_result EQUAL 0)
    message(FATAL_ERROR "Installed consumer build failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}"
        -S "${WHIRLPOOL_SOURCE_DIR}/tests/package/subdirectory"
        -B "${_subdirectory_build}"
        "-DWHIRLPOOL_SOURCE_DIR=${WHIRLPOOL_SOURCE_DIR}"
    RESULT_VARIABLE _subdirectory_configure_result)
if(NOT _subdirectory_configure_result EQUAL 0)
    message(FATAL_ERROR "Subdirectory consumer configure failed")
endif()

execute_process(
    COMMAND "${CMAKE_COMMAND}" --build "${_subdirectory_build}"
        ${_config_args}
    RESULT_VARIABLE _subdirectory_build_result)
if(NOT _subdirectory_build_result EQUAL 0)
    message(FATAL_ERROR "Subdirectory consumer build failed")
endif()
