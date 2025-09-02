#include "boot_types.h"

// 简单的线性分配器状态
static char *heap_current = (char *)BOOTLOADER_HEAP;
static char *heap_end = (char *)BOOTLOADER_BUFFER;
static uint64 allocated_bytes = 0;

// bootloader专用的内存分配
void *boot_alloc(int size) {
    // 8字节对齐
    size = (size + 7) & ~7;
    
    if (heap_current + size > heap_end) {
        return 0;  // 内存不足
    }
    
    void *ptr = heap_current;
    heap_current += size;
    allocated_bytes += size;
    
    return ptr;
}

// 页对齐的内存分配 (4KB)
void *boot_alloc_page(void) {
    // 确保页对齐
    uint64 addr = ((uint64)heap_current + 4095) & ~4095;
    heap_current = (char *)(addr + 4096);
    
    if (heap_current > heap_end) {
        return 0;
    }
    
    allocated_bytes += 4096;
    return (void *)addr;
}

// 获取分配统计
uint64 boot_memory_used(void) {
    return allocated_bytes;
}

// 简化的内存清零函数
void *memset(void *dst, int c, uint64 n) {
    char *cdst = (char *)dst;
    for (uint64 i = 0; i < n; i++) {
        cdst[i] = c;
    }
    return dst;
}

// 内存复制函数
void *memmove(void *dst, const void *src, uint64 n) {
    const char *csrc = src;
    char *cdst = dst;
    
    if (src < dst && csrc + n > cdst) {
        // 反向复制避免重叠
        csrc += n;
        cdst += n;
        while (n-- > 0) {
            *--cdst = *--csrc;
        }
    } else {
        // 正向复制
        for (uint64 i = 0; i < n; i++) {
            cdst[i] = csrc[i];
        }
    }
    return dst;
}
