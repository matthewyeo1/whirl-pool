#include <lockfree/config.hpp>
#include <lockfree/counter.hpp>
#include <lockfree/error.hpp>
#include <lockfree/spsc_queue.hpp>

#include <cstdint>
#include <type_traits>

static_assert(lockfree::cache_line_size ==
              static_cast<std::size_t>(WHIRLPOOL_CACHE_LINE_SIZE));
static_assert(std::is_same_v<
              std::underlying_type_t<lockfree::ErrorCode>,
              std::uint8_t>);
static_assert(std::is_trivially_copyable_v<lockfree::ErrorCode>);
static_assert(std::is_trivially_copyable_v<
              lockfree::Result<std::uint64_t>>);

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

    return 0;
}
