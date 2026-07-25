#include <lockfree.h>

int main() {
    lockfree::AtomicCounter<> counter;
    lockfree::CacheAligned<lockfree::AtomicCounter<>> aligned{};
    lockfree::cpu_relax();
    return counter.next() == 0 && aligned->next() == 0 ? 0 : 1;
}
