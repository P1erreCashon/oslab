#include "../common/boot_types.h"
#include "../common/virtio_boot.h"

// 简单的ELF头检查
struct elf_header {
    uint32 magic;
    uint8 class;
    uint8 data;
    uint8 version;
    uint8 pad[9];
    uint16 type;
    uint16 machine;
    uint32 version2;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
};

#define ELF_MAGIC 0x464C457F

// 第二阶段主函数
void bootloader_main(void) {
    uart_puts("\n=== Bootloader Stage 2 ===\n");
    uart_puts("Stage 2 started successfully!\n");
    
    // 显示内存使用情况
    uart_puts("Memory usage: ");
    uart_put_memsize(boot_memory_used());
    uart_puts("\n");
    
    // 显示virtio设备状态
    virtio_show_status();
    
    uart_puts("Initializing virtio disk...\n");
    if (virtio_disk_boot_init() != BOOT_SUCCESS) {
        uart_puts("Failed to initialize virtio disk\n");
        goto error;
    }
    
    uart_puts("Loading kernel...\n");
    
    // 分配缓冲区读取内核ELF头
    char *kernel_buf = (char *)BOOTLOADER_BUFFER;
    
    // 读取内核ELF头 (从扇区64开始)
    if (virtio_disk_read_sync(KERNEL_START_SECTOR, kernel_buf) != BOOT_SUCCESS) {
        uart_puts("Failed to read kernel ELF header\n");
        goto error;
    }
    
    // 验证ELF魔数
    struct elf_header *elf = (struct elf_header *)kernel_buf;
    if (elf->magic != ELF_MAGIC) {
        uart_puts("Invalid kernel format - not ELF\n");
        uart_puts("Found magic: ");
        uart_put_hex(elf->magic);
        uart_puts(", expected: ");
        uart_put_hex(ELF_MAGIC);
        uart_puts("\n");
        goto error;
    }
    
    uart_puts("Valid ELF kernel found\n");
    uart_puts("Kernel entry point: ");
    uart_put_hex(elf->entry);
    uart_puts("\n");
    uart_puts("Program headers: ");
    uart_put_dec(elf->phnum);
    uart_puts(" at offset ");
    uart_put_hex(elf->phoff);
    uart_puts("\n");
    
    // 简单加载：直接将内核数据复制到0x80200000
    // 这是第二阶段的简化实现，第三阶段会实现完整的ELF加载
    char *kernel_addr = (char *)0x80200000;
    int kernel_sectors = KERNEL_MAX_SECTORS;
    
    uart_puts("Loading kernel to ");
    uart_put_hex(0x80200000);
    uart_puts(", ");
    uart_put_dec(kernel_sectors);
    uart_puts(" sectors\n");
    
    for (int i = 0; i < kernel_sectors; i++) {
        if (virtio_disk_read_sync(KERNEL_START_SECTOR + i, kernel_addr + i * SECTOR_SIZE) != BOOT_SUCCESS) {
            uart_puts("Kernel load failed at sector ");
            uart_put_dec(KERNEL_START_SECTOR + i);
            uart_puts("\n");
            goto error;
        }
        
        // 每加载64个扇区显示进度
        if ((i & 63) == 0) {
            uart_putc('.');
        }
    }
    
    uart_puts("\nKernel loaded successfully\n");
    
    // 显示最终内存使用
    uart_puts("Total memory used: ");
    uart_put_memsize(boot_memory_used());
    uart_puts("\n");
    
    uart_puts("Jumping to kernel at ");
    uart_put_hex(elf->entry);
    uart_puts("...\n");
    
    // 跳转到内核入口点
    void (*kernel_entry)(void) = (void (*)(void))elf->entry;
    kernel_entry();
    
error:
    uart_puts("Boot failed, halting...\n");
    while (1) {
        asm volatile("wfi");
    }
}
