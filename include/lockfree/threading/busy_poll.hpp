#pragma once

#include "lockfree/platform/cpu_relax.hpp"
#include "lockfree/sync/compiler_barrier.hpp"

#include <chrono>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <thread>

namespace lockfree {

enum class SpinStrategy : std::uint8_t {
    pause,
    yield,
    noop,
    adaptive
};

enum class WaitResult : std::uint8_t {
    ready,
    cancelled,
    timeout,
    attempt_limit
};

struct BusyPollConfig {
    SpinStrategy strategy{SpinStrategy::adaptive};
    std::uint32_t maximum_pause_count{64};
    std::uint32_t adaptive_yield_threshold{16};
};

class BackoffState {
public:
    constexpr explicit BackoffState(std::uint32_t attempts = 0) noexcept
        : attempts_(attempts) {}

    constexpr void reset() noexcept { attempts_ = 0; }

    constexpr std::uint32_t attempts() const noexcept {
        return attempts_;
    }

    constexpr void advance() noexcept {
        if (attempts_ != std::numeric_limits<std::uint32_t>::max()) {
            ++attempts_;
        }
    }

private:
    std::uint32_t attempts_;
};

class BusyPoll {
public:
    constexpr explicit BusyPoll(BusyPollConfig config = {}) noexcept
        : config_(config) {}

    void step() noexcept {
        switch (config_.strategy) {
        case SpinStrategy::pause:
            pause();
            break;
        case SpinStrategy::yield:
            std::this_thread::yield();
            break;
        case SpinStrategy::noop:
            compiler_barrier();
            break;
        case SpinStrategy::adaptive:
            if (state_.attempts() < config_.adaptive_yield_threshold) {
                pause();
            } else {
                std::this_thread::yield();
            }
            break;
        }
        state_.advance();
    }

    constexpr void reset() noexcept { state_.reset(); }

    constexpr const BackoffState& state() const noexcept { return state_; }
    constexpr const BusyPollConfig& config() const noexcept {
        return config_;
    }

    template <typename Ready>
    WaitResult wait_forever(Ready&& ready) noexcept(noexcept(ready())) {
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            step();
        }
    }

    template <typename Ready, typename Cancelled>
    WaitResult wait(Ready&& ready, Cancelled&& cancelled)
        noexcept(noexcept(ready()) && noexcept(cancelled())) {
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            if (cancelled()) {
                return WaitResult::cancelled;
            }
            step();
        }
    }

    template <typename Ready>
    WaitResult wait_for_attempts(Ready&& ready,
                                 std::size_t maximum_attempts)
        noexcept(noexcept(ready())) {
        std::size_t failed_attempts = 0;
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            if (failed_attempts == maximum_attempts) {
                return WaitResult::attempt_limit;
            }
            step();
            ++failed_attempts;
        }
    }

    template <typename Ready, typename Cancelled>
    WaitResult wait_for_attempts(Ready&& ready,
                                 Cancelled&& cancelled,
                                 std::size_t maximum_attempts)
        noexcept(noexcept(ready()) && noexcept(cancelled())) {
        std::size_t failed_attempts = 0;
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            if (cancelled()) {
                return WaitResult::cancelled;
            }
            if (failed_attempts == maximum_attempts) {
                return WaitResult::attempt_limit;
            }
            step();
            ++failed_attempts;
        }
    }

    template <typename Ready>
    WaitResult wait_until(
        Ready&& ready,
        std::chrono::steady_clock::time_point deadline)
        noexcept(noexcept(ready())) {
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return WaitResult::timeout;
            }
            step();
        }
    }

    template <typename Ready, typename Cancelled>
    WaitResult wait_until(
        Ready&& ready,
        Cancelled&& cancelled,
        std::chrono::steady_clock::time_point deadline)
        noexcept(noexcept(ready()) && noexcept(cancelled())) {
        for (;;) {
            if (ready()) {
                return WaitResult::ready;
            }
            if (cancelled()) {
                return WaitResult::cancelled;
            }
            if (std::chrono::steady_clock::now() >= deadline) {
                return WaitResult::timeout;
            }
            step();
        }
    }

private:
    void pause() noexcept {
        std::uint32_t pause_count = 1;
        const auto attempts = state_.attempts();
        if (attempts < 31) {
            pause_count <<= attempts;
        } else {
            pause_count = std::numeric_limits<std::uint32_t>::max();
        }
        if (pause_count > config_.maximum_pause_count) {
            pause_count = config_.maximum_pause_count;
        }
        for (std::uint32_t index = 0; index < pause_count; ++index) {
            cpu_relax();
        }
    }

    BusyPollConfig config_;
    BackoffState state_;
};

} // namespace lockfree
