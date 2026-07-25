#pragma once

#include "lockfree/config.hpp"

#include <array>
#include <cstddef>

namespace lockfree {

template <std::size_t CurrentSize, std::size_t Alignment>
struct PadToAlignment {
    static_assert(Alignment != 0,
                  "padding alignment must be nonzero");
    static_assert((Alignment & (Alignment - 1)) == 0,
                  "padding alignment must be a power of two");

    static constexpr std::size_t value =
        (Alignment - (CurrentSize % Alignment)) % Alignment;
};

template <std::size_t CurrentSize,
          std::size_t Alignment = cache_line_size>
inline constexpr std::size_t pad_to_alignment =
    PadToAlignment<CurrentSize, Alignment>::value;

template <std::size_t Bytes>
struct Padding {
    std::array<std::byte, Bytes> bytes{};
};

template <>
struct Padding<0> {};

} // namespace lockfree
