#include "../common/boot_types.h"
#include "../common/virtio_boot.h"
#include "../common/elf_loader.h"
#include "../common/boot_info.h"
#include "../common/memory_layout.h"
#include "../common/device_tree.h"

// 第二阶段主函数
void bootloader_main(void) {
    uart_puts("\n=== Bootloader Stage 2 ===\n");
    uart_puts("Stage 2 started successfully!\n");
    
    // Stage 3.3: 验证内存布局
    if (!memory_layout_validate()) {
        uart_puts("CRITICAL: Memory layout validation failed!\n");
        goto error;
    }
    memory_layout_print();
    
    // 初始化引导信息结构
    struct boot_info *boot_info = get_boot_info();
    boot_info_init(boot_info);
    
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
    
    // Stage 3.3.2: 硬件检测和设备树生成
    uart_puts("Detecting hardware platform...\n");
    struct hardware_description hw_desc;
    if (hardware_detect_platform(&hw_desc) != 0) {
        uart_puts("Failed to detect hardware platform\n");
        goto error;
    }
    hardware_print_info(&hw_desc);
    
    if (hardware_validate_config(&hw_desc) != 0) {
        uart_puts("Hardware configuration validation failed\n");
        goto error;
    }
    
    uart_puts("Generating device tree...\n");
    struct device_tree_builder dt_builder;
    char *dt_buffer = (char *)BOOT_INFO_REGION_ADDR + 4096; // 4KB offset from boot_info
    device_tree_init(&dt_builder, dt_buffer, 60 * 1024); // 60KB for device tree
    
    // 构建设备树
    if (device_tree_add_memory(&dt_builder, hw_desc.memory_base, hw_desc.memory_size) != 0) {
        uart_puts("Failed to add memory to device tree\n");
        goto error;
    }
    
    if (device_tree_add_cpu(&dt_builder, 0) != 0) {
        uart_puts("Failed to add CPU to device tree\n");  
        goto error;
    }
    
    if (device_tree_add_uart(&dt_builder, hw_desc.uart_base, hw_desc.uart_interrupt) != 0) {
        uart_puts("Failed to add UART to device tree\n");
        goto error;
    }
    
    if (device_tree_add_virtio(&dt_builder, hw_desc.virtio_base, hw_desc.virtio_interrupt) != 0) {
        uart_puts("Failed to add VirtIO to device tree\n");
        goto error;
    }
    
    if (device_tree_finalize(&dt_builder) != 0) {
        uart_puts("Failed to finalize device tree\n");
        goto error;
    }
    
    device_tree_print(&dt_builder);
    
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
    
    // 设置引导信息中的内核参数
    uart_puts("[BEFORE setup] Magic: ");
    uart_put_hex(boot_info->magic);
    uart_puts("\n");
    
    boot_info_setup_kernel_params(&load_info);
    boot_info_setup_device_tree(&dt_builder, &hw_desc);
    
    uart_puts("[AFTER setup] Magic: ");
    uart_put_hex(boot_info->magic);
    uart_puts("\n");
    
    // 显示完整的引导信息
    boot_info_print(boot_info);
    
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
    
    // 按RISC-V引导约定设置寄存器
    // a0 = hart ID (0 for boot hart)
    // a1 = device tree pointer (0 for now, 将来可指向boot_info)
    uint64 hart_id = 0;
    uint64 dtb_ptr = (uint64)boot_info;  // 传递引导信息地址
    
    uart_puts("Hart ID: ");
    uart_put_dec(hart_id);
    uart_puts("\nBoot Info Address: ");
    uart_put_hex(dtb_ptr);
    uart_puts("\n");
    
    // 设置临时栈并跳转到内核
    asm volatile(
        "li sp, %2\n"                   // 设置临时栈指针
        "mv a0, %0\n"                   // hart ID
        "mv a1, %1\n"                   // boot info pointer
        "jr %3\n"                       // 跳转到内核入口点
        :
        : "r"(hart_id), "r"(dtb_ptr), "i"(0x80030000), "r"(load_info.entry_point)
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
