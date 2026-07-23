#include <lockfree.h>

int main() {
    lockfree::AtomicCounter<> counter;
    return counter.next() == 0 ? 0 : 1;
}
