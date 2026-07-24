#include <lockfree/config.hpp>
#include <lockfree/cache/cache_aligned.hpp>
#include <lockfree/cache/padding.hpp>
#include <lockfree/utils.hpp>

static_assert(lockfree::cache_line_size == 128);
static_assert(lockfree::CACHE_LINE_SIZE == 128);
static_assert(alignof(lockfree::CacheAligned<int>) == 128);
static_assert(sizeof(lockfree::CacheAligned<int>) == 128);
static_assert(lockfree::pad_to_alignment<129> == 127);

int main() {
    return 0;
}
