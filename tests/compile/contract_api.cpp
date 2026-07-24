#include <lockfree/config.hpp>
#include <lockfree/cache/cache_aligned.hpp>
#include <lockfree/cache/padding.hpp>
#include <lockfree/counter.hpp>
#include <lockfree/error.hpp>
#include <lockfree/platform/features.hpp>
#include <lockfree/spsc_queue.hpp>
#include <lockfree/sync/atomic.hpp>
#include <lockfree/threading/busy_poll.hpp>
#include <lockfree/version.hpp>

#include <atomic>
#include <cstdint>
#include <type_traits>

static_assert(lockfree::cache_line_size ==
              static_cast<std::size_t>(WHIRLPOOL_CACHE_LINE_SIZE));
static_assert(std::is_same_v<
              std::underlying_type_t<lockfree::ErrorCode>,
              std::uint8_t>);
static_assert(LOCKFREE_VERSION_MAJOR == 0);
static_assert(LOCKFREE_VERSION_MINOR == 2);
static_assert(LOCKFREE_VERSION_PATCH == 0);
static_assert(std::is_trivially_copyable_v<lockfree::ErrorCode>);
static_assert(std::is_trivially_copyable_v<
              lockfree::Result<std::uint64_t>>);
static_assert(alignof(lockfree::CacheAligned<int>) ==
              lockfree::cache_line_size);
static_assert(lockfree::pad_to_alignment<65, 64> == 63);

int main() {
    const lockfree::Result<int> success{42, lockfree::ErrorCode::ok};
    const lockfree::Result<int> failure{
        0, lockfree::ErrorCode::invalid_argument};
    if (!success || failure) {
        return 1;
    }

    lockfree::AtomicCounter<std::uint64_t> counter;
    if (counter.next() != 0 || counter.current() != 1) {
        return 2;
    }

    lockfree::SPSCQueue<int, 8> queue;
    for (int value = 0; value < 7; ++value) {
        if (!queue.push(value)) {
            return 3;
        }
    }
    if (queue.push(7)) {
        return 4;
    }

    std::atomic<int> atomic_value{0};
    lockfree::store_release(atomic_value, 1);
    if (lockfree::load_acquire(atomic_value) != 1) {
        return 5;
    }

    lockfree::BusyPoll poll{
        {lockfree::SpinStrategy::noop, 1, 1}};
    if (poll.wait_for_attempts(
            []() noexcept { return true; }, 1) !=
        lockfree::WaitResult::ready) {
        return 6;
    }

    return 0;
}
