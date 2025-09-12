#ifndef BOOT_TYPES_H
#define BOOT_TYPES_H

// 基础类型定义 (复用kernel/types.h)
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long  uint64;

typedef int bool;
#define true 1
#define false 0

typedef uint64 pte_t;
typedef uint64 *pagetable_t;

// 包含新的内存布局定义
#include "memory_layout.h"

// === Stage 3.3: 更新内存布局 ===
// 使用memory_layout.h中定义的生产级布局
// 保持向后兼容性的别名

// 向后兼容性定义
#define STAGE1_ADDR     KERNEL_BASE_ADDR      // Stage1加载到内核位置
#define KERNEL_ADDR     KERNEL_BASE_ADDR      // 内核基地址
// STAGE2_ADDR, BOOT_INFO_ADDR, BOOTLOADER_BUFFER 已在memory_layout.h中定义

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

// 汇编优化函数声明
void fast_memcpy(void *dest, const void *src, uint64 size);
void fast_memset(void *ptr, uint64 size);
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

// 引导信息函数
struct boot_info;
struct elf_load_info;
void boot_info_init(struct boot_info *info);
void boot_info_setup_kernel_params(const struct elf_load_info *load_info);
void boot_info_print(const struct boot_info *info);
struct boot_info *get_boot_info(void);

#endif
