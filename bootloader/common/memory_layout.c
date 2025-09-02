#include "boot_types.h"

// === 内存区域定义 ===
// 基于实际测试的工作配置
const struct memory_region memory_regions[] = {
    {
        .base_addr = KERNEL_BASE_ADDR,
        .size = KERNEL_RESERVED_SIZE,
        .protection = MEM_PROT_RWX,  // 内核需要执行权限
        .name = "Kernel"
    },
    {
        .base_addr = STAGE2_ADDR,        // 保持0x80030000
        .size = STAGE2_SIZE,
        .protection = MEM_PROT_RX,       // 引导程序只读执行
        .name = "Stage2 Bootloader"
    },
    {
        .base_addr = BOOT_INFO_REGION_ADDR,  // 保持0x80040000
        .size = BOOT_INFO_REGION_SIZE,
        .protection = MEM_PROT_RW,       // 引导信息可读写
        .name = "Boot Info"
    },
    {
        .base_addr = VIRTIO_REGION_ADDR,     // 移动到0x80050000
        .size = VIRTIO_REGION_SIZE,
        .protection = MEM_PROT_RW,       // virtio缓冲区可读写
        .name = "VirtIO Buffers"
    },
    {
        .base_addr = BOOTLOADER_HEAP_ADDR,   // 0x80060000
        .size = BOOTLOADER_HEAP_SIZE,
        .protection = MEM_PROT_RW,       // 引导程序堆可读写
        .name = "Bootloader Heap"
    }
};

const int memory_region_count = sizeof(memory_regions) / sizeof(memory_regions[0]);

// === 内存区域重叠检查 ===
bool memory_region_overlaps(uint64 addr1, uint64 size1, uint64 addr2, uint64 size2) {
    uint64 end1 = addr1 + size1;
    uint64 end2 = addr2 + size2;
    
    // 如果两个区域不重叠，那么一个完全在另一个之前或之后
    return !(end1 <= addr2 || end2 <= addr1);
}

// === 内存布局验证 ===
bool memory_layout_validate(void) {
    uart_puts("=== Memory Layout Validation ===\n");
    
    bool valid = true;
    
    // 检查所有区域是否重叠
    for (int i = 0; i < memory_region_count; i++) {
        for (int j = i + 1; j < memory_region_count; j++) {
            if (memory_region_overlaps(
                memory_regions[i].base_addr, memory_regions[i].size,
                memory_regions[j].base_addr, memory_regions[j].size)) {
                
                uart_puts("ERROR: Memory regions overlap!\n");
                uart_puts("  Region ");
                uart_put_dec(i);
                uart_puts(" (");
                uart_puts(memory_regions[i].name);
                uart_puts("): ");
                uart_put_hex(memory_regions[i].base_addr);
                uart_puts("-");
                uart_put_hex(memory_regions[i].base_addr + memory_regions[i].size - 1);
                uart_puts("\n");
                uart_puts("  Region ");
                uart_put_dec(j);
                uart_puts(" (");
                uart_puts(memory_regions[j].name);
                uart_puts("): ");
                uart_put_hex(memory_regions[j].base_addr);
                uart_puts("-");
                uart_put_hex(memory_regions[j].base_addr + memory_regions[j].size - 1);
                uart_puts("\n");
                
                valid = false;
            }
        }
    }
    
    // 检查区域是否在有效的内存范围内
    for (int i = 0; i < memory_region_count; i++) {
        uint64 region_end = memory_regions[i].base_addr + memory_regions[i].size;
        if (memory_regions[i].base_addr < KERNEL_BASE_ADDR || region_end > USER_SPACE_END) {
            uart_puts("ERROR: Memory region out of bounds!\n");
            uart_puts("  Region ");
            uart_put_dec(i);
            uart_puts(" (");
            uart_puts(memory_regions[i].name);
            uart_puts("): ");
            uart_put_hex(memory_regions[i].base_addr);
            uart_puts("-");
            uart_put_hex(region_end - 1);
            uart_puts("\n");
            uart_puts("  Valid range: ");
            uart_put_hex(KERNEL_BASE_ADDR);
            uart_puts("-");
            uart_put_hex(USER_SPACE_END - 1);
            uart_puts("\n");
            
            valid = false;
        }
    }
    
    if (valid) {
        uart_puts("Memory layout validation: PASSED\n");
    } else {
        uart_puts("Memory layout validation: FAILED\n");
    }
    
    return valid;
}

// === 内存布局显示 ===
void memory_layout_print(void) {
    uart_puts("=== Memory Layout ===\n");
    
    for (int i = 0; i < memory_region_count; i++) {
        const struct memory_region *region = &memory_regions[i];
        
        uart_puts(region->name);
        uart_puts(": ");
        uart_put_hex(region->base_addr);
        uart_puts("-");
        uart_put_hex(region->base_addr + region->size - 1);
        uart_puts(" (");
        uart_put_memsize(region->size);
        uart_puts(", ");
        
        // 显示保护属性
        if (region->protection & MEM_PROT_READ)  uart_putc('R');
        if (region->protection & MEM_PROT_WRITE) uart_putc('W');
        if (region->protection & MEM_PROT_EXEC)  uart_putc('X');
        if (region->protection == MEM_PROT_NONE) uart_puts("---");
        
        uart_puts(")\n");
    }
    
    // 显示内存间隙
    uart_puts("\nMemory gaps:\n");
    for (int i = 0; i < memory_region_count - 1; i++) {
        uint64 gap_start = memory_regions[i].base_addr + memory_regions[i].size;
        uint64 gap_end = memory_regions[i + 1].base_addr;
        
        if (gap_end > gap_start) {
            uart_puts("  Gap: ");
            uart_put_hex(gap_start);
            uart_puts("-");
            uart_put_hex(gap_end - 1);
            uart_puts(" (");
            uart_put_memsize(gap_end - gap_start);
            uart_puts(")\n");
        }
    }
    
    uart_puts("====================\n");
}

// === 简单的内存保护实现 ===
// 注意：这是引导阶段的保护，不是完整的页表保护

static struct {
    bool protection_enabled;
    uint64 protected_regions[8][2];  // [base_addr, size] pairs
    int protected_count;
} memory_protection_state = {false, {{0, 0}}, 0};

int memory_protect_region(uint64 base_addr, uint64 size, memory_protection_t prot) {
    // 简单实现：记录保护区域，但在引导阶段不实际设置页表
    // 这个功能在真实的操作系统中由内核的MMU处理
    
    if (memory_protection_state.protected_count >= 8) {
        return -1; // 达到最大保护区域数量
    }
    
    memory_protection_state.protected_regions[memory_protection_state.protected_count][0] = base_addr;
    memory_protection_state.protected_regions[memory_protection_state.protected_count][1] = size;
    memory_protection_state.protected_count++;
    memory_protection_state.protection_enabled = true;
    
    uart_puts("Memory protection set for ");
    uart_put_hex(base_addr);
    uart_puts("-");
    uart_put_hex(base_addr + size - 1);
    uart_puts("\n");
    
    return 0;
}

int memory_unprotect_region(uint64 base_addr, uint64 size) {
    // 简单实现：从保护列表中移除
    for (int i = 0; i < memory_protection_state.protected_count; i++) {
        if (memory_protection_state.protected_regions[i][0] == base_addr &&
            memory_protection_state.protected_regions[i][1] == size) {
            
            // 移动后面的元素向前
            for (int j = i; j < memory_protection_state.protected_count - 1; j++) {
                memory_protection_state.protected_regions[j][0] = 
                    memory_protection_state.protected_regions[j + 1][0];
                memory_protection_state.protected_regions[j][1] = 
                    memory_protection_state.protected_regions[j + 1][1];
            }
            
            memory_protection_state.protected_count--;
            
            uart_puts("Memory protection removed for ");
            uart_put_hex(base_addr);
            uart_puts("-");
            uart_put_hex(base_addr + size - 1);
            uart_puts("\n");
            
            return 0;
        }
    }
    
    return -1; // 未找到匹配的保护区域
}

bool memory_check_protection(uint64 addr, memory_protection_t required_prot) {
    if (!memory_protection_state.protection_enabled) {
        return true; // 保护未启用，允许所有访问
    }
    
    // 检查地址是否在保护区域内
    for (int i = 0; i < memory_protection_state.protected_count; i++) {
        uint64 base = memory_protection_state.protected_regions[i][0];
        uint64 size = memory_protection_state.protected_regions[i][1];
        
        if (addr >= base && addr < base + size) {
            // 在保护区域内，检查权限（简化版本）
            // 在真实系统中，这里会查询页表
            return false; // 简单起见，保护区域拒绝访问
        }
    }
    
    return true; // 不在保护区域内，允许访问
}
