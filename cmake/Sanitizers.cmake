function(whirlpool_validate_sanitizer_mode)
    set(_mode "${WHIRLPOOL_ENABLE_SANITIZERS}")
    set(_valid_modes
        none address undefined address-undefined thread)
    if(NOT _mode IN_LIST _valid_modes)
        message(FATAL_ERROR
            "Invalid WHIRLPOOL_ENABLE_SANITIZERS='${_mode}'")
    endif()
endfunction()

function(whirlpool_enable_sanitizers target)
    whirlpool_validate_sanitizer_mode()
    set(_mode "${WHIRLPOOL_ENABLE_SANITIZERS}")
    if(_mode STREQUAL "none")
        return()
    endif()

    if(MSVC)
        if(NOT _mode STREQUAL "address")
            message(FATAL_ERROR
                "MSVC currently supports only the address sanitizer mode")
        endif()
        target_compile_options(${target} PRIVATE /fsanitize=address)
        return()
    endif()

    if(_mode STREQUAL "address")
        set(_flags -fsanitize=address)
    elseif(_mode STREQUAL "undefined")
        set(_flags -fsanitize=undefined)
    elseif(_mode STREQUAL "address-undefined")
        set(_flags -fsanitize=address,undefined)
    elseif(_mode STREQUAL "thread")
        set(_flags -fsanitize=thread)
    endif()

    target_compile_options(${target} PRIVATE
        ${_flags} -fno-omit-frame-pointer)
    target_link_options(${target} PRIVATE ${_flags})
endfunction()
