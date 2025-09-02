#ifndef BOOT_INFO_H
#define BOOT_INFO_H

#include "boot_types.h"

// RISC-V Boot Information Structure
// 遵循RISC-V引导约定，但为xv6扩展
struct boot_info {
    // 标准RISC-V引导信息
    uint64 magic;            // 魔数 0x52495343564B5256 ("RISCVKRN")
    uint64 version;          // 版本号 (1)
    uint64 hart_count;       // CPU核心数量
    uint64 memory_base;      // 内存起始地址
    uint64 memory_size;      // 系统内存大小
    
    // 内核加载信息
    uint64 kernel_base;      // 内核加载地址
    uint64 kernel_size;      // 内核大小
    uint64 kernel_entry;     // 内核入口点
    
    // 设备信息
    uint64 virtio_mmio_base; // virtio-mmio设备基地址
    uint64 virtio_mmio_size; // virtio-mmio区域大小
    uint64 uart_base;        // UART设备基地址
    
    // 文件系统信息
    uint64 fs_base_sector;   // 文件系统起始扇区
    uint64 fs_sector_count;  // 文件系统扇区数
    
    // 预留空间用于未来扩展
    uint64 reserved[8];
};

// 引导信息魔数
#define BOOT_INFO_MAGIC 0x52495343564B5256UL  // "RISCVKRN"
#define BOOT_INFO_VERSION 1

// 函数声明
void boot_info_init(struct boot_info *info);
void boot_info_setup_kernel_params(const struct elf_load_info *load_info);
void boot_info_print(const struct boot_info *info);

#endif
