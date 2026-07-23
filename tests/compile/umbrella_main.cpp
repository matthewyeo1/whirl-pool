#include <lockfree.h>

int umbrella_other_translation_unit();

int main() {
    lockfree::AtomicCounter<> counter;
    return counter.next() == 0 &&
                   umbrella_other_translation_unit() ==
                       static_cast<int>(lockfree::cache_line_size)
               ? 0
               : 1;
}
