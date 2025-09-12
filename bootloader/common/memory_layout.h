#ifndef MEMORY_LAYOUT_H
#define MEMORY_LAYOUT_H

// 注意：此文件被boot_types.h包含，不要直接包含boot_types.h以避免循环依赖

// Stage 3.3.1: 生产级内存布局定义
// 按计划重新设计内存映射，确保各区域不重叠且有明确边界

// === RISC-V 内存布局 (QEMU virt机器) ===
// 基于实际测试的工作配置，避免理论上的"完美"
// 
// 0x80000000-0x80022000  : 内核代码+数据+BSS (~138KB实际使用)
// 0x80022000-0x80030000  : 内核扩展空间 (56KB预留)
// 0x80030000-0x80040000  : Stage 2引导程序 (64KB) - 当前工作地址
// 0x80040000-0x80050000  : 引导信息和设备树 (64KB) - 当前工作
// 0x80050000-0x80060000  : virtio队列和缓冲区 (64KB) - 移动到这里
// 0x80060000-0x80070000  : 引导程序堆和临时数据 (64KB)
// 0x80070000-0x88000000  : 用户空间和内核堆 (~128MB)

// === 重新设计的内存区域 ===

// 内核区域 (基于实际测量的大小)
#define KERNEL_BASE_ADDR      0x80000000ULL
#define KERNEL_RESERVED_SIZE  0x30000ULL    // 192KB - 为内核预留足够空间  
#define KERNEL_MAX_ADDR       (KERNEL_BASE_ADDR + KERNEL_RESERVED_SIZE)

// Stage 2引导程序区域 (保持当前工作地址)
#define STAGE2_ADDR           0x80030000ULL  // 保持当前成功的地址
#define STAGE2_SIZE           0x10000ULL     // 64KB应该足够
#define STAGE2_MAX_ADDR       (STAGE2_ADDR + STAGE2_SIZE)

// 引导信息和设备树区域 (保持当前工作地址)
#define BOOT_INFO_REGION_ADDR 0x80040000ULL  // 保持当前成功的地址
#define BOOT_INFO_REGION_SIZE 0x10000ULL     // 64KB for boot_info + device tree
#define BOOT_INFO_MAX_ADDR    (BOOT_INFO_REGION_ADDR + BOOT_INFO_REGION_SIZE)

// virtio设备区域 (移动到更安全的位置)
#define VIRTIO_REGION_ADDR    0x80050000ULL  // 移动到boot_info之后
#define VIRTIO_REGION_SIZE    0x10000ULL     // 64KB for queues and buffers
#define VIRTIO_MAX_ADDR       (VIRTIO_REGION_ADDR + VIRTIO_REGION_SIZE)

// 引导程序堆和临时存储
#define BOOTLOADER_HEAP_ADDR  0x80060000ULL  // 在virtio区域之后
#define BOOTLOADER_HEAP_SIZE  0x10000ULL     // 64KB for bootloader heap
#define BOOTLOADER_MAX_ADDR   (BOOTLOADER_HEAP_ADDR + BOOTLOADER_HEAP_SIZE)

// 用户空间开始地址
#define USER_SPACE_START      0x80070000ULL  // 在所有引导区域之后
#define USER_SPACE_END        0x88000000ULL  // 128MB系统的结束

// === 向后兼容性别名 ===
#define BOOT_INFO_ADDR        BOOT_INFO_REGION_ADDR  // 保持兼容
#define BOOTLOADER_BUFFER     BOOTLOADER_HEAP_ADDR   // 保持兼容
#define BOOTLOADER_HEAP       BOOTLOADER_HEAP_ADDR   // 保持兼容

// === 内存区域验证宏 ===
#define IS_KERNEL_ADDR(addr)     ((addr) >= KERNEL_BASE_ADDR && (addr) < KERNEL_MAX_ADDR)
#define IS_STAGE2_ADDR(addr)     ((addr) >= STAGE2_ADDR && (addr) < STAGE2_MAX_ADDR)
#define IS_VIRTIO_ADDR(addr)     ((addr) >= VIRTIO_REGION_ADDR && (addr) < VIRTIO_MAX_ADDR)
#define IS_BOOT_INFO_ADDR(addr)  ((addr) >= BOOT_INFO_REGION_ADDR && (addr) < BOOT_INFO_MAX_ADDR)
#define IS_BOOTLOADER_ADDR(addr) ((addr) >= BOOTLOADER_HEAP_ADDR && (addr) < BOOTLOADER_MAX_ADDR)

// === 内存保护相关 ===
typedef enum {
    MEM_PROT_NONE  = 0,    // 无访问权限
    MEM_PROT_READ  = 1,    // 只读
    MEM_PROT_WRITE = 2,    // 可写 (implies readable)
    MEM_PROT_EXEC  = 4,    // 可执行
    MEM_PROT_RW    = 3,    // 读写
    MEM_PROT_RX    = 5,    // 读执行
    MEM_PROT_RWX   = 7     // 读写执行
} memory_protection_t;

// 内存区域描述符
struct memory_region {
    uint64 base_addr;
    uint64 size;
    memory_protection_t protection;
    const char *name;
};

// === 预定义内存区域 ===
extern const struct memory_region memory_regions[];
extern const int memory_region_count;

// === 内存布局验证函数 ===
bool memory_layout_validate(void);
void memory_layout_print(void);
bool memory_region_overlaps(uint64 addr1, uint64 size1, uint64 addr2, uint64 size2);

// === 内存保护功能 ===
int memory_protect_region(uint64 base_addr, uint64 size, memory_protection_t prot);
int memory_unprotect_region(uint64 base_addr, uint64 size);
bool memory_check_protection(uint64 addr, memory_protection_t required_prot);

#endif // MEMORY_LAYOUT_H
