#pragma once

#include <cstdint>

namespace lockfree {

enum class OperatingSystem : std::uint8_t {
    Unknown,
    Linux,
    MacOS,
    Windows
};

enum class Architecture : std::uint8_t {
    Unknown,
    X86_64,
    AArch64
};

enum class Compiler : std::uint8_t {
    Unknown,
    GCC,
    Clang,
    MSVC
};

#if defined(_WIN32)
inline constexpr OperatingSystem current_operating_system =
    OperatingSystem::Windows;
#elif defined(__APPLE__) && defined(__MACH__)
inline constexpr OperatingSystem current_operating_system =
    OperatingSystem::MacOS;
#elif defined(__linux__)
inline constexpr OperatingSystem current_operating_system =
    OperatingSystem::Linux;
#else
inline constexpr OperatingSystem current_operating_system =
    OperatingSystem::Unknown;
#endif

#if defined(__x86_64__) || defined(_M_X64)
inline constexpr Architecture current_architecture =
    Architecture::X86_64;
#elif defined(__aarch64__) || defined(_M_ARM64)
inline constexpr Architecture current_architecture =
    Architecture::AArch64;
#else
inline constexpr Architecture current_architecture =
    Architecture::Unknown;
#endif

#if defined(__clang__)
inline constexpr Compiler current_compiler = Compiler::Clang;
#elif defined(_MSC_VER)
inline constexpr Compiler current_compiler = Compiler::MSVC;
#elif defined(__GNUC__)
inline constexpr Compiler current_compiler = Compiler::GCC;
#else
inline constexpr Compiler current_compiler = Compiler::Unknown;
#endif

inline constexpr bool has_native_cpu_relax =
    current_architecture == Architecture::X86_64 ||
    current_architecture == Architecture::AArch64;

} // namespace lockfree
