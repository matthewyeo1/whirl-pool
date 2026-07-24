#include <lockfree.h>

int umbrella_other_translation_unit() {
    lockfree::compiler_barrier();
    lockfree::cpu_relax();
    return static_cast<int>(lockfree::cache_line_size);
}
