#include <lockfree.h>

int umbrella_other_translation_unit() {
    return static_cast<int>(lockfree::cache_line_size);
}
