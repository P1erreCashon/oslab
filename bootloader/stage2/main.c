#include "../common/boot_types.h"
#include "../common/virtio_boot.h"
#include "../common/elf_loader.h"
#include "../common/boot_info.h"
#include "../common/memory_layout.h"
#include "../common/device_tree.h"
#include "../common/error_handling.h"

// 第二阶段主函数
void bootloader_main(void) {
    uart_puts("\n=== Bootloader Stage 2 ===\n");
    uart_puts("Stage 2 started successfully!\n");
    
    // Stage 3.3.3: 初始化错误处理系统
    error_system_init();
    
    // Stage 3.3: 验证内存布局
    if (!memory_layout_validate()) {
        ERROR_REPORT(ERROR_MEMORY_OVERLAP);
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
        error_action_t action = ERROR_REPORT_CTX1(ERROR_VIRTIO_INIT_FAILED, init_result);
        uart_puts("Failed to initialize virtio disk: ");
        uart_put_dec(init_result);
        uart_puts("\n");
        
        if (action == ERROR_ACTION_ABORT) {
            goto error;
        }
        // 可以在这里添加fallback逻辑
        goto error;
    }
    
    uart_puts("Virtio disk initialized successfully!\n");
    
    uart_puts("Testing disk read with boot sector first...\n");
    
    // 先测试读取引导扇区(扇区0)来验证磁盘驱动
    char *test_buf = (char *)BOOTLOADER_BUFFER;
    uart_puts("Attempting to read boot sector (sector 0)...\n");
    
    if (virtio_disk_read_sync(0, test_buf) != BOOT_SUCCESS) {
        error_action_t action = ERROR_REPORT_CTX1(ERROR_DISK_READ_FAILED, 0);
        uart_puts("Failed to read boot sector\n");
        
        if (action == ERROR_ACTION_RETRY) {
            uart_puts("Retrying boot sector read...\n");
            if (virtio_disk_read_sync(0, test_buf) != BOOT_SUCCESS) {
                ERROR_REPORT_CTX1(ERROR_DISK_READ_FAILED, 0);
                uart_puts("Boot sector read retry failed\n");
                goto error;
            }
            uart_puts("Boot sector read retry succeeded\n");
        } else {
            goto error;
        }
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
        ERROR_REPORT(ERROR_HARDWARE_FAULT);
        uart_puts("Failed to detect hardware platform\n");
        goto error;
    }
    hardware_print_info(&hw_desc);
    
    if (hardware_validate_config(&hw_desc) != 0) {
        ERROR_REPORT(ERROR_INVALID_DEVICE);
        uart_puts("Hardware configuration validation failed\n");
        goto error;
    }
    
    uart_puts("Generating device tree...\n");
    struct device_tree_builder dt_builder;
    char *dt_buffer = (char *)BOOT_INFO_REGION_ADDR + 4096; // 4KB offset from boot_info
    device_tree_init(&dt_builder, dt_buffer, 60 * 1024); // 60KB for device tree
    
    // 构建设备树
    if (device_tree_add_memory(&dt_builder, hw_desc.memory_base, hw_desc.memory_size) != 0) {
        ERROR_REPORT(ERROR_BOOT_DEVICE_TREE_FAILED);
        uart_puts("Failed to add memory to device tree\n");
        goto error;
    }
    
    if (device_tree_add_cpu(&dt_builder, 0) != 0) {
        ERROR_REPORT(ERROR_BOOT_DEVICE_TREE_FAILED);
        uart_puts("Failed to add CPU to device tree\n");  
        goto error;
    }
    
    if (device_tree_add_uart(&dt_builder, hw_desc.uart_base, hw_desc.uart_interrupt) != 0) {
        ERROR_REPORT(ERROR_BOOT_DEVICE_TREE_FAILED);
        uart_puts("Failed to add UART to device tree\n");
        goto error;
    }
    
    if (device_tree_add_virtio(&dt_builder, hw_desc.virtio_base, hw_desc.virtio_interrupt) != 0) {
        ERROR_REPORT(ERROR_BOOT_DEVICE_TREE_FAILED);
        uart_puts("Failed to add VirtIO to device tree\n");
        goto error;
    }
    
    if (device_tree_finalize(&dt_builder) != 0) {
        ERROR_REPORT(ERROR_BOOT_DEVICE_TREE_FAILED);
        uart_puts("Failed to finalize device tree\n");
        goto error;
    }
    
    device_tree_print(&dt_builder);
    
    uart_puts("Loading kernel from disk...\n");
    
    // 分配内核缓冲区 - 使用安全的地址，远离内核加载区域
    char *kernel_buf = (char *)0x80068000;  // 在BOOTLOADER_BUFFER + 32KB，安全区域
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
        
        // 注释掉早期终止逻辑 - 它导致代码段没有被读取！
        // 早期检查ELF魔数，如果已经读到完整头部就可以提前判断
        // if (i >= 2) { // 至少读了1KB，足够ELF头
        //     struct elf_header *elf = (struct elf_header *)kernel_buf;
        //     if (elf->e_magic == ELF_MAGIC && i >= (elf->e_phoff + elf->e_phnum * elf->e_phentsize) / SECTOR_SIZE) {
        //         uart_puts("\nEarly termination: read enough for ELF (");
        //         uart_put_dec(i + 1);
        //         uart_puts(" sectors)\n");
        //         break;
        //     }
        // }
    }
    
    uart_puts("\n");
    
    // 使用完整的ELF加载器
    struct elf_load_info load_info;
    elf_error_t elf_result = elf_load_kernel(kernel_buf, &load_info);
    
    if (elf_result != ELF_SUCCESS) {
        error_code_t error_code = ERROR_ELF_LOAD_FAILED;
        
        // 根据ELF错误类型选择具体的错误代码
        switch (elf_result) {
            case ELF_ERROR_INVALID_MAGIC:
                error_code = ERROR_ELF_INVALID_MAGIC;
                break;
            case ELF_ERROR_INVALID_ARCH:
                error_code = ERROR_ELF_INVALID_CLASS;  // 架构错误映射到类错误
                break;
            case ELF_ERROR_INVALID_PHNUM:
                error_code = ERROR_ELF_NO_SEGMENTS;    // 程序头错误映射到段错误
                break;
            default:
                error_code = ERROR_ELF_LOAD_FAILED;
                break;
        }
        
        ERROR_REPORT_CTX1(error_code, elf_result);
        uart_puts("ELF kernel loading failed: ");
        uart_put_dec(elf_result);
        uart_puts("\n");
        goto error;
    }
    
    // === CRITICAL DEBUGGING: 验证ELF加载是否真的成功 ===
    uart_puts("=== IMMEDIATE KERNEL VERIFICATION ===\n");
    uint32 *kernel_start_immediate = (uint32 *)0x80000000;
    uart_puts("RIGHT after elf_load_kernel() - First instruction: ");
    uart_put_hex(*kernel_start_immediate);
    uart_puts("\n");
    uart_puts("Entry point from ELF: ");
    uart_put_hex(load_info.entry_point);
    uart_puts("\n");
    uart_puts("Load base: ");
    uart_put_hex(load_info.load_base); 
    uart_puts("\n");
    
    // 验证内核代码是否正确加载
    uint32 *kernel_start_check = (uint32 *)load_info.entry_point;
    uart_puts("After ELF loading - Kernel first instruction: ");
    uart_put_hex(*kernel_start_check);
    uart_puts("\n");
    
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
    
    // 检查当前特权级
    uint64 current_mode;
    asm volatile("csrr %0, mstatus" : "=r"(current_mode));
    uart_puts("Current privilege level (mstatus): ");
    uart_put_hex(current_mode);
    uart_puts("\n");
    
    // 检查内核第一条指令
    uint32 *kernel_start = (uint32 *)load_info.entry_point;
    uart_puts("Kernel first instruction: ");
    uart_put_hex(*kernel_start);
    uart_puts("\n");
    
    // 内存屏障和缓存刷新
    asm volatile("fence" ::: "memory");
    asm volatile("fence.i" ::: "memory");
    
    uart_puts("\n");
    uart_puts(">>>>>>> BOOTLOADER HANDOFF TO KERNEL <<<<<<<\n");
    uart_puts("Entry point: ");
    uart_put_hex(load_info.entry_point);
    uart_puts("\n");
    uart_puts("Goodbye from bootloader!\n");
    uart_puts("==========================================\n\n");
    
    // 跳转到内核 - 让内核自己设置栈，我们只传递参数
    asm volatile(
        "mv a0, %0\n"                   // hart ID
        "mv a1, %1\n"                   // boot info pointer  
        "jr %2\n"                       // 跳转到内核入口点
        :
        : "r"(hart_id), "r"(dtb_ptr), "r"(load_info.entry_point)
        : "a0", "a1"
    );
    
    // 永远不应该到达这里
    uart_puts("ERROR: Returned from kernel!\n");
    
error:
    ERROR_REPORT(ERROR_SYSTEM_HALT);
    uart_puts("Boot failed, displaying error statistics...\n");
    error_print_statistics();
    uart_puts("Halting...\n");
    while (1) {
        asm volatile("wfi");
    }
}
