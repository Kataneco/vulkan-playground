#include "VulkanUtils.h"

size_t hash_combine(size_t lhs, size_t rhs) {
    lhs ^= rhs + 0x517cc1b727220a95 + (lhs << 6) + (lhs >> 2);
    return lhs;
}
