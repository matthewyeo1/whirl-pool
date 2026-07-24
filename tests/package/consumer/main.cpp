#include <lockfree.h>

int main() {
    lockfree::SPSCQueue<int, 8> queue;
    const lockfree::Result<int> result{42, lockfree::ErrorCode::ok};
    lockfree::CacheAligned<int> aligned{result.value};
    lockfree::BusyPoll poll{
        {lockfree::SpinStrategy::noop, 1, 1}};
    return queue.push(aligned.get()) && result &&
                   poll.wait_for_attempts(
                       []() noexcept { return true; }, 1) ==
                       lockfree::WaitResult::ready
               ? 0
               : 1;
}
