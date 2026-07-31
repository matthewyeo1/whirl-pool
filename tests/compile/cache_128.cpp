#include <lockfree/config.hpp>
#include <lockfree/utils.hpp>

static_assert(lockfree::cache_line_size == 128);
static_assert(lockfree::CACHE_LINE_SIZE == 128);

int main() {
    return 0;
}
