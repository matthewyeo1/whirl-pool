file(GLOB _whirlpool_public_headers CONFIGURE_DEPENDS
    "${PROJECT_SOURCE_DIR}/include/lockfree/*.hpp")

foreach(_header IN LISTS _whirlpool_public_headers)
    get_filename_component(_header_name "${_header}" NAME_WE)
    file(RELATIVE_PATH _header_include
        "${PROJECT_SOURCE_DIR}/include" "${_header}")
    set(_source
        "${CMAKE_CURRENT_BINARY_DIR}/header_${_header_name}.cpp")
    file(WRITE "${_source}"
        "#include <${_header_include}>\nint main() { return 0; }\n")

    add_executable(contract_header_${_header_name} "${_source}")
    target_link_libraries(contract_header_${_header_name}
        PRIVATE whirlpool::whirlpool)
    whirlpool_apply_project_warnings(contract_header_${_header_name})
    whirlpool_enable_sanitizers(contract_header_${_header_name})
endforeach()

add_executable(contract_api contract_api.cpp)
target_link_libraries(contract_api PRIVATE whirlpool::whirlpool)
whirlpool_apply_project_warnings(contract_api)
whirlpool_enable_sanitizers(contract_api)
add_test(NAME contract_api COMMAND contract_api)

add_executable(contract_umbrella
    umbrella_main.cpp
    umbrella_other.cpp)
target_link_libraries(contract_umbrella PRIVATE whirlpool::whirlpool)
whirlpool_apply_project_warnings(contract_umbrella)
whirlpool_enable_sanitizers(contract_umbrella)
add_test(NAME contract_umbrella COMMAND contract_umbrella)

add_executable(contract_no_exceptions no_exceptions.cpp)
target_link_libraries(contract_no_exceptions PRIVATE whirlpool::whirlpool)
whirlpool_apply_project_warnings(contract_no_exceptions)
if(MSVC)
    target_compile_options(contract_no_exceptions PRIVATE /GR- /EHs-c-)
else()
    target_compile_options(contract_no_exceptions
        PRIVATE -fno-exceptions -fno-rtti)
endif()
add_test(NAME contract_no_exceptions COMMAND contract_no_exceptions)

add_executable(contract_cache_128 cache_128.cpp)
target_compile_features(contract_cache_128 PRIVATE cxx_std_17)
target_compile_definitions(contract_cache_128
    PRIVATE WHIRLPOOL_CACHE_LINE_SIZE=128)
target_include_directories(contract_cache_128
    PRIVATE "${PROJECT_SOURCE_DIR}/include")
whirlpool_apply_project_warnings(contract_cache_128)
whirlpool_enable_sanitizers(contract_cache_128)
add_test(NAME contract_cache_128 COMMAND contract_cache_128)

foreach(_invalid_cache_size IN ITEMS 0 63)
    try_compile(
        _invalid_cache_accepted
        "${CMAKE_CURRENT_BINARY_DIR}/invalid_cache_${_invalid_cache_size}"
        "${CMAKE_CURRENT_SOURCE_DIR}/invalid_cache_line.cpp"
        CMAKE_FLAGS
            "-DCMAKE_CXX_STANDARD=17"
            "-DCMAKE_CXX_STANDARD_REQUIRED=ON"
            "-DINCLUDE_DIRECTORIES=${PROJECT_SOURCE_DIR}/include"
        COMPILE_DEFINITIONS
            "-DWHIRLPOOL_CACHE_LINE_SIZE=${_invalid_cache_size}")
    if(_invalid_cache_accepted)
        message(FATAL_ERROR
            "Invalid cache-line size ${_invalid_cache_size} compiled")
    endif()
    unset(_invalid_cache_accepted CACHE)
endforeach()
