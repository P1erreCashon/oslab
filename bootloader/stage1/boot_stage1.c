#include "../common/boot_types.h"
#include "../common/virtio_boot.h"

// 第二阶段加载函数
int load_stage2(void) {
    uart_puts("Loading stage2...\n");
    
    // 初始化virtio磁盘
    debug_print("Initializing virtio disk");
    if (virtio_disk_boot_init() != BOOT_SUCCESS) {
        uart_puts("ERROR: Disk init failed\n");
        return -1;
    }
    
    // 显示内存使用情况
    uart_puts("Memory used: ");
    uart_put_memsize(boot_memory_used());
    uart_puts("\n");
    
    // 从扇区1开始读取32KB的第二阶段程序
    char *stage2_addr = (char *)BOOTLOADER_STAGE2;
    
    uart_puts("Loading ");
    uart_put_dec(STAGE2_SECTORS);
    uart_puts(" sectors to 0x");
    uart_put_hex(BOOTLOADER_STAGE2);
    uart_puts("\n");
    
    for (int i = 0; i < STAGE2_SECTORS; i++) {
        if (virtio_disk_read_sync(STAGE2_START_SECTOR + i, stage2_addr + i * SECTOR_SIZE) != BOOT_SUCCESS) {
            uart_puts("ERROR: Stage2 load failed at sector ");
            uart_put_dec(STAGE2_START_SECTOR + i);
            uart_puts("\n");
            return -1;
        }
        
        // 每加载16个扇区显示进度
        if ((i & 15) == 0) {
            uart_putc('.');
        }
    }
    
    uart_puts("\nStage2 loaded successfully\n");
    debug_print("Stage2 loading completed");
    return 0;
}
