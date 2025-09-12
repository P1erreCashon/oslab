#include "boot_info.h"
#include "boot_types.h"
#include "device_tree.h"
#include "elf_loader.h"

// 全局引导信息结构
static struct boot_info *boot_info_ptr = (struct boot_info *)BOOT_INFO_ADDR;

// 初始化引导信息结构
void boot_info_init(struct boot_info *info) {
    // 清零整个结构 - 注意fast_memset参数是(ptr, size)
    fast_memset(info, sizeof(struct boot_info));
    
    // 设置标准字段
    info->magic = BOOT_INFO_MAGIC;
    info->version = BOOT_INFO_VERSION;
    info->hart_count = 1;  // QEMU默认单核
    
    // QEMU virt机器的内存布局
    info->memory_base = 0x80000000;
    info->memory_size = 128 * 1024 * 1024;  // 128MB
    
    // 设备地址（QEMU virt标准布局）
    info->virtio_mmio_base = 0x10001000;
    info->virtio_mmio_size = 0x1000;
    info->uart_base = 0x10000000;
    
    // 文件系统信息
    info->fs_base_sector = 2048;  // 从扇区2048开始
    info->fs_sector_count = 4000; // 4000扇区 = 2MB
}

// 根据内核加载信息设置引导参数
void boot_info_setup_kernel_params(const struct elf_load_info *load_info) {
    struct boot_info *info = get_boot_info();  // 使用同一个指针
    info->kernel_base = load_info->load_base;
    info->kernel_size = load_info->load_size;
    info->kernel_entry = load_info->entry_point;
}

void boot_info_setup_device_tree(const struct device_tree_builder *dt_builder, 
                                 const struct hardware_description *hw_desc) {
    struct boot_info *info = get_boot_info();
    
    // 设备树信息
    info->device_tree_addr = (uint64)device_tree_get_binary(dt_builder);
    info->device_tree_size = device_tree_get_binary_size(dt_builder);
    info->hardware_platform = (uint64)hw_desc->platform;
    
    uart_puts("[DEBUG] Device tree setup: addr=");
    uart_put_hex(info->device_tree_addr);
    uart_puts(" size=");
    uart_put_dec(info->device_tree_size);
    uart_puts("\n");
}

// 打印引导信息（调试用）
void boot_info_print(const struct boot_info *info) {
    uart_puts("=== Boot Information ===\n");
    uart_puts("[DEBUG] boot_info ptr: ");
    uart_put_hex((uint64)info);
    uart_puts("\n");
    uart_puts("[DEBUG] Expected magic: ");
    uart_put_hex(BOOT_INFO_MAGIC);
    uart_puts("\n");
    uart_puts("[DEBUG] Actual magic: ");
    uart_put_hex(info->magic);
    uart_puts("\n");
    
    uart_puts("Magic: ");
    uart_put_hex(info->magic);
    uart_puts("\nVersion: ");
    uart_put_dec(info->version);
    uart_puts("\nHart Count: ");
    uart_put_dec(info->hart_count);
    uart_puts("\nMemory: ");
    uart_put_hex(info->memory_base);
    uart_puts(" - ");
    uart_put_hex(info->memory_base + info->memory_size);
    uart_puts(" (");
    uart_put_memsize(info->memory_size);
    uart_puts(")\n");
    
    uart_puts("Kernel: ");
    uart_put_hex(info->kernel_base);
    uart_puts(" entry=");
    uart_put_hex(info->kernel_entry);
    uart_puts(" size=");
    uart_put_memsize(info->kernel_size);
    uart_puts("\n");
    
    uart_puts("UART: ");
    uart_put_hex(info->uart_base);
    uart_puts("\nVirtIO: ");
    uart_put_hex(info->virtio_mmio_base);
    uart_puts("\nFS: sector ");
    uart_put_dec(info->fs_base_sector);
    uart_puts(" count ");
    uart_put_dec(info->fs_sector_count);
    uart_puts("\nDevice Tree: ");
    uart_put_hex(info->device_tree_addr);
    uart_puts(" (");
    uart_put_memsize(info->device_tree_size);
    uart_puts(")\n=========================\n");
}

// 获取引导信息指针
struct boot_info *get_boot_info(void) {
    return boot_info_ptr;
}
