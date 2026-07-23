#pragma once

#include <cstdint>

namespace lockfree {

enum class ErrorCode : std::uint8_t {
    ok,
    empty,
    full,
    out_of_memory,
    invalid_argument,
    unsupported,
    permission_denied,
    platform_error,
    stale_calibration
};

template<typename T>
struct Result {
    T value;
    ErrorCode error;

    [[nodiscard]] constexpr explicit operator bool() const noexcept {
        return error == ErrorCode::ok;
    }
};

} // namespace lockfree
