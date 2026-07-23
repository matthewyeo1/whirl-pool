#include <lockfree.h>

int main() {
    lockfree::SPSCQueue<int, 8> queue;
    const lockfree::Result<int> result{42, lockfree::ErrorCode::ok};
    return queue.push(result.value) && result ? 0 : 1;
}
