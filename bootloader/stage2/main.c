#include "../common/boot_types.h"
#include "../common/virtio_boot.h"
#include "../common/elf_loader.h"

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
    int init_result = virtio_disk_boot_init();
    uart_puts("Init result: ");
    uart_put_dec(init_result);
    uart_puts("\n");
    
    if (init_result != BOOT_SUCCESS) {
        uart_puts("Failed to initialize virtio disk: ");
        uart_put_dec(init_result);
        uart_puts("\n");
        goto error;
    }
    
    uart_puts("Virtio disk initialized successfully!\n");
    
    uart_puts("Testing disk read with boot sector first...\n");
    
    // 先测试读取引导扇区(扇区0)来验证磁盘驱动
    char *test_buf = (char *)BOOTLOADER_BUFFER;
    uart_puts("Attempting to read boot sector (sector 0)...\n");
    
    if (virtio_disk_read_sync(0, test_buf) != BOOT_SUCCESS) {
        uart_puts("Failed to read boot sector\n");
        goto error;
    }
    
    uart_puts("Boot sector read successful! First 16 bytes:\n");
    for (int i = 0; i < 16; i++) {
        uart_put_hex((uint8)test_buf[i]);
        uart_putc(' ');
        if ((i & 7) == 7) uart_putc('\n');
    }
    uart_puts("\n");
    
    uart_puts("Loading kernel from disk...\n");
    
    // 分配内核缓冲区 - 需要足够大来加载整个内核
    char *kernel_buf = (char *)BOOTLOADER_BUFFER;
    const int max_kernel_sectors = 16; // 先读较少扇区进行测试
    
    uart_puts("Reading kernel ELF (");
    uart_put_dec(max_kernel_sectors);
    uart_puts(" sectors) from sector ");
    uart_put_dec(KERNEL_START_SECTOR);
    uart_puts("...\n");
    
    // 读取内核ELF文件 - 读取更多扇区以确保完整
    for (int i = 0; i < max_kernel_sectors; i++) {
        if (virtio_disk_read_sync(KERNEL_START_SECTOR + i, kernel_buf + i * SECTOR_SIZE) != BOOT_SUCCESS) {
            uart_puts("Failed to read kernel sector ");
            uart_put_dec(KERNEL_START_SECTOR + i);
            uart_puts("\n");
            goto error;
        }
        
        // 每读64个扇区显示进度
        if ((i & 63) == 0) {
            uart_putc('.');
        }
        
        // 早期检查ELF魔数，如果已经读到完整头部就可以提前判断
        if (i >= 2) { // 至少读了1KB，足够ELF头
            struct elf_header *elf = (struct elf_header *)kernel_buf;
            if (elf->e_magic == ELF_MAGIC && i >= (elf->e_phoff + elf->e_phnum * elf->e_phentsize) / SECTOR_SIZE) {
                uart_puts("\nEarly termination: read enough for ELF (");
                uart_put_dec(i + 1);
                uart_puts(" sectors)\n");
                break;
            }
        }
    }
    
    uart_puts("\n");
    
    // 使用完整的ELF加载器
    struct elf_load_info load_info;
    elf_error_t elf_result = elf_load_kernel(kernel_buf, &load_info);
    
    if (elf_result != ELF_SUCCESS) {
        uart_puts("ELF kernel loading failed: ");
        uart_put_dec(elf_result);
        uart_puts("\n");
        goto error;
    }
    
    // 显示最终内存使用
    uart_puts("Total memory used: ");
    uart_put_memsize(boot_memory_used());
    uart_puts("\n");
    
    uart_puts("Preparing to jump to kernel...\n");
    uart_puts("Entry point: ");
    uart_put_hex(load_info.entry_point);
    uart_puts("\n");
    uart_puts("Kernel base: ");
    uart_put_hex(load_info.load_base);
    uart_puts(", size: ");
    uart_put_hex(load_info.load_size);
    uart_puts("\n");
    
    // 内存屏障和缓存刷新
    asm volatile("fence" ::: "memory");
    asm volatile("fence.i" ::: "memory");
    
    uart_puts("=== JUMPING TO KERNEL ===\n");
    uart_puts("Setting up kernel boot environment...\n");
    
    // Linus式解决方案：为内核提供临时栈
    // 内核栈在0x80008a10，但我们只复制了4KB到0x80001000
    // 解决方案：给内核一个临时栈，让它自己设置正确的栈
    
    uint64 temp_stack = 0x80030000; // 使用我们的缓冲区作为临时栈
    uart_puts("Setting temporary stack at: ");
    uart_put_hex(temp_stack);
    uart_puts("\n");
    
    uart_puts("Final jump to: ");
    uart_put_hex(load_info.entry_point);
    uart_puts("\n");
    
    // 设置临时栈并跳转到内核
    asm volatile(
        "li sp, %1\n"                   // 设置临时栈指针
        "li a0, 0\n"                    // hart ID = 0
        "li a1, 0\n"                    // device tree ptr = 0
        "jr %0\n"                       // 跳转到内核入口点
        :
        : "r"(load_info.entry_point), "i"(0x80030000)
        : "a0", "a1"
    );
    
    // 永远不应该到达这里
    uart_puts("ERROR: Returned from kernel!\n");
    
error:
    uart_puts("Boot failed, halting...\n");
    while (1) {
        asm volatile("wfi");
    }
}
