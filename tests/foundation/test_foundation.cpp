#include <lockfree/cache/cache_aligned.hpp>
#include <lockfree/cache/padding.hpp>
#include <lockfree/platform/cpu_relax.hpp>
#include <lockfree/platform/features.hpp>
#include <lockfree/sync/atomic.hpp>
#include <lockfree/sync/compiler_barrier.hpp>
#include <lockfree/threading/busy_poll.hpp>

#include <atomic>
#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>
#include <type_traits>

namespace {

struct alignas(128) OverAligned {
    std::uint64_t value;
};

static_assert(alignof(lockfree::CacheAligned<int>) ==
              lockfree::cache_line_size);
static_assert(sizeof(lockfree::CacheAligned<int>) %
                  lockfree::cache_line_size ==
              0);
static_assert(alignof(lockfree::CacheAligned<int, 128>) == 128);
static_assert(sizeof(lockfree::CacheAligned<int, 128>) % 128 == 0);
static_assert(sizeof(std::array<lockfree::CacheAligned<int, 64>, 2>) ==
              128);
static_assert(alignof(lockfree::CacheAligned<OverAligned, 128>) == 128);
static_assert(lockfree::pad_to_alignment<0, 64> == 0);
static_assert(lockfree::pad_to_alignment<1, 64> == 63);
static_assert(lockfree::pad_to_alignment<64, 64> == 0);
static_assert(lockfree::pad_to_alignment<65, 128> == 63);
static_assert(std::is_empty_v<lockfree::Padding<0>>);
static_assert(sizeof(lockfree::Padding<17>) == 17);
static_assert(std::is_same_v<lockfree::Separated<int, 128>,
                             lockfree::CacheAligned<int, 128>>);

bool test_cache_layout() {
    lockfree::CacheAligned<int> aligned{42};
    return aligned.value == 42 && aligned.get() == 42 &&
           *aligned == 42 && *aligned.operator->() == 42;
}

bool test_atomic_helpers() {
    std::atomic<int> value{0};
    lockfree::store_relaxed(value, 1);
    if (lockfree::load_relaxed(value) != 1) {
        return false;
    }
    lockfree::store_release(value, 2);
    if (lockfree::load_acquire(value) != 2) {
        return false;
    }

    int expected = 2;
    if (!lockfree::compare_exchange_strong_relaxed(value, expected, 3)) {
        return false;
    }
    expected = 3;
    if (!lockfree::compare_exchange_strong_acquire(value, expected, 4)) {
        return false;
    }
    expected = 4;
    if (!lockfree::compare_exchange_strong_release(value, expected, 5)) {
        return false;
    }
    expected = 5;
    if (!lockfree::compare_exchange_strong_acq_rel(value, expected, 6)) {
        return false;
    }

    expected = 6;
    while (!lockfree::compare_exchange_weak_relaxed(value, expected, 7)) {
        expected = 6;
    }
    expected = 7;
    while (!lockfree::compare_exchange_weak_acquire(value, expected, 8)) {
        expected = 7;
    }
    expected = 8;
    while (!lockfree::compare_exchange_weak_release(value, expected, 9)) {
        expected = 8;
    }
    expected = 9;
    while (!lockfree::compare_exchange_weak_acq_rel(value, expected, 10)) {
        expected = 9;
    }

    volatile std::atomic<int> volatile_value{0};
    lockfree::store_relaxed(volatile_value, 1);
    lockfree::store_release(volatile_value, 2);
    expected = 2;
    if (!lockfree::compare_exchange_strong_relaxed(
            volatile_value, expected, 3)) {
        return false;
    }
    expected = 3;
    if (!lockfree::compare_exchange_strong_acquire(
            volatile_value, expected, 4)) {
        return false;
    }
    expected = 4;
    if (!lockfree::compare_exchange_strong_release(
            volatile_value, expected, 5)) {
        return false;
    }
    expected = 5;
    if (!lockfree::compare_exchange_strong_acq_rel(
            volatile_value, expected, 6)) {
        return false;
    }
    expected = 6;
    while (!lockfree::compare_exchange_weak_relaxed(
        volatile_value, expected, 7)) {
        expected = 6;
    }
    expected = 7;
    while (!lockfree::compare_exchange_weak_acquire(
        volatile_value, expected, 8)) {
        expected = 7;
    }
    expected = 8;
    while (!lockfree::compare_exchange_weak_release(
        volatile_value, expected, 9)) {
        expected = 8;
    }
    expected = 9;
    while (!lockfree::compare_exchange_weak_acq_rel(
        volatile_value, expected, 10)) {
        expected = 9;
    }

    lockfree::acquire_fence();
    lockfree::release_fence();
    lockfree::full_fence();
    lockfree::compiler_acquire_barrier();
    lockfree::compiler_release_barrier();
    lockfree::compiler_barrier();

    return lockfree::load_relaxed(volatile_value) == 10 &&
           lockfree::load_acquire(volatile_value) == 10;
}

bool test_release_acquire_publication() {
    constexpr int iterations = 10000;
    std::atomic<int> state{0};
    int payload = 0;
    std::atomic<bool> valid{true};

    std::thread producer([&] {
        for (int iteration = 1; iteration <= iterations; ++iteration) {
            while (lockfree::load_acquire(state) != 0) {
                lockfree::cpu_relax();
            }
            payload = iteration;
            lockfree::store_release(state, 1);
        }
    });

    std::thread consumer([&] {
        for (int iteration = 1; iteration <= iterations; ++iteration) {
            while (lockfree::load_acquire(state) != 1) {
                lockfree::cpu_relax();
            }
            if (payload != iteration) {
                valid.store(false, std::memory_order_relaxed);
            }
            lockfree::store_release(state, 0);
        }
    });

    producer.join();
    consumer.join();
    return valid.load(std::memory_order_relaxed);
}

bool test_fence_publication() {
    constexpr int iterations = 10000;
    std::atomic<int> state{0};
    std::atomic<int> payload{0};
    std::atomic<bool> valid{true};

    std::thread producer([&] {
        for (int iteration = 1; iteration <= iterations; ++iteration) {
            while (lockfree::load_acquire(state) != 0) {
                lockfree::cpu_relax();
            }
            lockfree::store_relaxed(payload, iteration);
            lockfree::release_fence();
            lockfree::store_relaxed(state, 1);
        }
    });

    std::thread consumer([&] {
        for (int iteration = 1; iteration <= iterations; ++iteration) {
            while (lockfree::load_relaxed(state) != 1) {
                lockfree::cpu_relax();
            }
            lockfree::acquire_fence();
            if (lockfree::load_relaxed(payload) != iteration) {
                valid.store(false, std::memory_order_relaxed);
            }
            lockfree::store_release(state, 0);
        }
    });

    producer.join();
    consumer.join();
    return valid.load(std::memory_order_relaxed);
}

bool test_busy_poll() {
    lockfree::BackoffState saturated{
        std::numeric_limits<std::uint32_t>::max()};
    saturated.advance();
    if (saturated.attempts() !=
        std::numeric_limits<std::uint32_t>::max()) {
        return false;
    }

    const lockfree::BusyPollConfig noop_config{
        lockfree::SpinStrategy::noop, 4, 2};
    lockfree::BusyPoll poll{noop_config};

    int probes = 0;
    if (poll.wait_forever([&]() noexcept { return probes++ == 2; }) !=
            lockfree::WaitResult::ready ||
        poll.state().attempts() != 2) {
        return false;
    }

    poll.reset();
    if (poll.wait([]() noexcept { return true; },
                  []() noexcept { return true; }) !=
            lockfree::WaitResult::ready ||
        poll.state().attempts() != 0) {
        return false;
    }

    if (poll.wait([]() noexcept { return false; },
                  []() noexcept { return true; }) !=
        lockfree::WaitResult::cancelled) {
        return false;
    }

    poll.reset();
    if (poll.wait_for_attempts(
            []() noexcept { return false; }, std::size_t{3}) !=
            lockfree::WaitResult::attempt_limit ||
        poll.state().attempts() != 3) {
        return false;
    }

    poll.reset();
    if (poll.wait_for_attempts(
            []() noexcept { return false; },
            []() noexcept { return true; },
            std::size_t{3}) != lockfree::WaitResult::cancelled ||
        poll.state().attempts() != 0) {
        return false;
    }

    if (poll.wait_until(
            []() noexcept { return true; },
            std::chrono::steady_clock::time_point::min()) !=
        lockfree::WaitResult::ready) {
        return false;
    }
    if (poll.wait_until(
            []() noexcept { return false; },
            std::chrono::steady_clock::time_point::min()) !=
        lockfree::WaitResult::timeout) {
        return false;
    }
    if (poll.wait_until(
            []() noexcept { return false; },
            []() noexcept { return true; },
            std::chrono::steady_clock::time_point::max()) !=
        lockfree::WaitResult::cancelled) {
        return false;
    }

    lockfree::BusyPoll pause_poll{
        {lockfree::SpinStrategy::pause, 2, 1}};
    lockfree::BusyPoll yield_poll{
        {lockfree::SpinStrategy::yield, 2, 1}};
    lockfree::BusyPoll adaptive_poll{
        {lockfree::SpinStrategy::adaptive, 2, 1}};
    pause_poll.step();
    yield_poll.step();
    adaptive_poll.step();
    adaptive_poll.step();

    return pause_poll.state().attempts() == 1 &&
           yield_poll.state().attempts() == 1 &&
           adaptive_poll.state().attempts() == 2;
}

bool test_platform() {
    static_assert(
        lockfree::has_native_cpu_relax ==
        (lockfree::current_architecture ==
             lockfree::Architecture::X86_64 ||
         lockfree::current_architecture ==
             lockfree::Architecture::AArch64));
#if defined(__linux__)
    if (lockfree::current_operating_system !=
        lockfree::OperatingSystem::Linux) {
        return false;
    }
#elif defined(__APPLE__)
    if (lockfree::current_operating_system !=
        lockfree::OperatingSystem::MacOS) {
        return false;
    }
#elif defined(_WIN32)
    if (lockfree::current_operating_system !=
        lockfree::OperatingSystem::Windows) {
        return false;
    }
#endif

#if defined(__x86_64__) || defined(_M_X64)
    if (lockfree::current_architecture !=
        lockfree::Architecture::X86_64) {
        return false;
    }
#elif defined(__aarch64__) || defined(_M_ARM64)
    if (lockfree::current_architecture !=
        lockfree::Architecture::AArch64) {
        return false;
    }
#endif

#if defined(__clang__)
    if (lockfree::current_compiler != lockfree::Compiler::Clang) {
        return false;
    }
#elif defined(_MSC_VER)
    if (lockfree::current_compiler != lockfree::Compiler::MSVC) {
        return false;
    }
#elif defined(__GNUC__)
    if (lockfree::current_compiler != lockfree::Compiler::GCC) {
        return false;
    }
#endif

    lockfree::cpu_relax();
    return true;
}

} // namespace

int main() {
    return test_cache_layout() && test_atomic_helpers() &&
                   test_release_acquire_publication() &&
                   test_fence_publication() && test_busy_poll() &&
                   test_platform()
               ? 0
               : 1;
}
