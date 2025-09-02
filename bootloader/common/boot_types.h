#ifndef BOOT_TYPES_H
#define BOOT_TYPES_H

// 基础类型定义 (复用kernel/types.h)
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long  uint64;

typedef uint64 pte_t;
typedef uint64 *pagetable_t;

// bootloader专用内存布局
#define BOOTLOADER_BASE     0x80000000  // 第一阶段加载地址
#define BOOTLOADER_STAGE2   0x80001000  // 第二阶段加载地址
#define BOOTLOADER_HEAP     0x80010000  // 堆内存起始
#define BOOTLOADER_STACK    0x80020000  // 栈内存起始
#define BOOTLOADER_BUFFER   0x80030000  // I/O缓冲区

// 磁盘布局定义
#define SECTOR_SIZE         512
#define STAGE2_START_SECTOR 1      // 第二阶段起始扇区
#define STAGE2_SECTORS      64     // 第二阶段占用扇区数 (32KB)
#define KERNEL_START_SECTOR 64     // 内核起始扇区
#define KERNEL_MAX_SECTORS  1024   // 内核最大扇区数 (512KB)

// 错误码定义
#define BOOT_SUCCESS         0
#define BOOT_ERROR_DISK     -1
#define BOOT_ERROR_MEMORY   -2
#define BOOT_ERROR_FORMAT   -3
#define BOOT_ERROR_TIMEOUT  -4

// 调试开关
#define BOOT_DEBUG          1

#if BOOT_DEBUG
#define debug_print(msg) uart_puts("[DEBUG] " msg "\n")
#else
#define debug_print(msg)
#endif

// 函数声明
void uart_putc(char c);
void uart_puts(const char *s);
void uart_put_hex(uint64 val);
void uart_put_dec(uint64 val);
void uart_put_memsize(uint64 bytes);
void *boot_alloc(int size);
void *boot_alloc_page(void);
uint64 boot_memory_used(void);
void *memset(void *dst, int c, uint64 n);
void *memmove(void *dst, const void *src, uint64 n);

#endif
