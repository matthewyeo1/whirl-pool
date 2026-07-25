#include <lockfree.h>

int umbrella_other_translation_unit();

int main() {
    lockfree::AtomicCounter<> counter;
    lockfree::CacheAligned<int> aligned{1};
    lockfree::BusyPoll poll{
        {lockfree::SpinStrategy::noop, 1, 1}};
    return counter.next() == 0 &&
                   aligned.get() == 1 &&
                   poll.wait_for_attempts(
                       []() noexcept { return true; }, 1) ==
                       lockfree::WaitResult::ready &&
                   umbrella_other_translation_unit() ==
                       static_cast<int>(lockfree::cache_line_size)
               ? 0
               : 1;
}
