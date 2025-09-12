#include "boot_types.h"

// 简单的内存复制实现
void *memcpy(void *dest, const void *src, uint64 size) {
    char *d = (char*)dest;
    const char *s = (const char*)src;
    for (uint64 i = 0; i < size; i++) {
        d[i] = s[i];
    }
    return dest;
}
