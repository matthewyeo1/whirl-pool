#include <lockfree/config.hpp>
#include <lockfree/counter.hpp>
#include <lockfree/error.hpp>
#include <lockfree/spsc_queue.hpp>

int main() {
    lockfree::SPSCQueue<int, 8> queue;
    lockfree::AtomicCounter<> counter;
    const lockfree::Result<int> result{7, lockfree::ErrorCode::ok};

    return queue.push(1) && queue.pop().value_or(0) == 1 &&
                   counter.next() == 0 && result
               ? 0
               : 1;
}
