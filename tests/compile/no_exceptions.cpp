#include <lockfree/config.hpp>
#include <lockfree/cache/cache_aligned.hpp>
#include <lockfree/counter.hpp>
#include <lockfree/error.hpp>
#include <lockfree/platform/cpu_relax.hpp>
#include <lockfree/spsc_queue.hpp>
#include <lockfree/sync/atomic.hpp>
#include <lockfree/threading/busy_poll.hpp>

#include <atomic>

int main() {
    lockfree::SPSCQueue<int, 8> queue;
    lockfree::AtomicCounter<> counter;
    const lockfree::Result<int> result{7, lockfree::ErrorCode::ok};
    lockfree::CacheAligned<int> aligned{7};
    std::atomic<int> atomic_value{0};
    lockfree::store_release(atomic_value, aligned.value);
    lockfree::BusyPoll poll{
        {lockfree::SpinStrategy::noop, 1, 1}};
    const auto wait_result = poll.wait_for_attempts(
        [&]() noexcept {
            return lockfree::load_acquire(atomic_value) == 7;
        },
        1);
    lockfree::cpu_relax();

    return queue.push(1) && queue.pop().value_or(0) == 1 &&
                   counter.next() == 0 && result &&
                   wait_result == lockfree::WaitResult::ready
               ? 0
               : 1;
}
