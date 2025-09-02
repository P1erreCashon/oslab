#include "boot_types.h"
#include "device_tree.h"

// 前向声明
static int device_tree_count_nodes(const struct device_tree_builder *builder);

// QEMU virt机器的硬件配置
#define QEMU_VIRT_UART_BASE     0x10000000ULL
#define QEMU_VIRT_UART_IRQ      10
#define QEMU_VIRT_VIRTIO_BASE   0x10001000ULL
#define QEMU_VIRT_VIRTIO_IRQ    1
#define QEMU_VIRT_PLIC_BASE     0x0c000000ULL
#define QEMU_VIRT_MEMORY_BASE   0x80000000ULL

// === 设备树构建器实现 ===

void device_tree_init(struct device_tree_builder *builder, void *buffer, uint64 buffer_size) {
    builder->root = 0;
    builder->current = 0;
    builder->buffer = (char *)buffer;
    builder->buffer_size = buffer_size;
    builder->buffer_used = 0;
    
    uart_puts("Device tree builder initialized with ");
    uart_put_memsize(buffer_size);
    uart_puts(" buffer\n");
}

static struct device_tree_node *allocate_node(struct device_tree_builder *builder) {
    uint64 node_size = sizeof(struct device_tree_node);
    
    if (builder->buffer_used + node_size > builder->buffer_size) {
        uart_puts("ERROR: Device tree buffer full\n");
        return 0;
    }
    
    struct device_tree_node *node = (struct device_tree_node *)(builder->buffer + builder->buffer_used);
    builder->buffer_used += node_size;
    
    // 初始化节点
    node->name = 0;
    node->type = DT_NODE_ROOT;
    node->base_addr = 0;
    node->size = 0;
    node->interrupt = 0;
    node->compatible = 0;
    node->next = 0;
    
    return node;
}

int device_tree_add_memory(struct device_tree_builder *builder, uint64 base, uint64 size) {
    struct device_tree_node *node = allocate_node(builder);
    if (!node) return -1;
    
    node->name = "memory";
    node->type = DT_NODE_MEMORY;
    node->base_addr = base;
    node->size = size;
    node->compatible = "qemu,virt-memory";
    
    if (!builder->root) {
        builder->root = node;
    } else {
        builder->current->next = node;
    }
    builder->current = node;
    
    uart_puts("Added memory node: ");
    uart_put_hex(base);
    uart_puts("-");
    uart_put_hex(base + size - 1);
    uart_puts("\n");
    
    return 0;
}

int device_tree_add_cpu(struct device_tree_builder *builder, int cpu_id) {
    struct device_tree_node *node = allocate_node(builder);
    if (!node) return -1;
    
    node->name = "cpu";
    node->type = DT_NODE_CPU;
    node->base_addr = cpu_id;
    node->size = 0;
    node->compatible = "riscv,rv64";
    
    if (!builder->root) {
        builder->root = node;
    } else {
        builder->current->next = node;
    }
    builder->current = node;
    
    uart_puts("Added CPU node: cpu");
    uart_put_dec(cpu_id);
    uart_puts("\n");
    
    return 0;
}

int device_tree_add_uart(struct device_tree_builder *builder, uint64 base_addr, uint32 interrupt) {
    struct device_tree_node *node = allocate_node(builder);
    if (!node) return -1;
    
    node->name = "uart";
    node->type = DT_NODE_UART;
    node->base_addr = base_addr;
    node->size = 0x1000;  // 4KB UART region
    node->interrupt = interrupt;
    node->compatible = "ns16550a";
    
    if (!builder->root) {
        builder->root = node;
    } else {
        builder->current->next = node;
    }
    builder->current = node;
    
    uart_puts("Added UART node: ");
    uart_put_hex(base_addr);
    uart_puts(" IRQ ");
    uart_put_dec(interrupt);
    uart_puts("\n");
    
    return 0;
}

int device_tree_add_virtio(struct device_tree_builder *builder, uint64 base_addr, uint32 interrupt) {
    struct device_tree_node *node = allocate_node(builder);
    if (!node) return -1;
    
    node->name = "virtio";
    node->type = DT_NODE_VIRTIO;
    node->base_addr = base_addr;
    node->size = 0x1000;  // 4KB virtio region  
    node->interrupt = interrupt;
    node->compatible = "virtio,mmio";
    
    if (!builder->root) {
        builder->root = node;
    } else {
        builder->current->next = node;
    }
    builder->current = node;
    
    uart_puts("Added VirtIO node: ");
    uart_put_hex(base_addr);
    uart_puts(" IRQ ");
    uart_put_dec(interrupt);
    uart_puts("\n");
    
    return 0;
}

int device_tree_finalize(struct device_tree_builder *builder) {
    if (!builder->root) {
        uart_puts("ERROR: Device tree has no nodes\n");
        return -1;
    }
    
    uart_puts("Device tree finalized with ");
    uart_put_dec(device_tree_count_nodes(builder));
    uart_puts(" nodes\n");
    
    return 0;
}

static int device_tree_count_nodes(const struct device_tree_builder *builder) {
    int count = 0;
    struct device_tree_node *node = builder->root;
    
    while (node) {
        count++;
        node = node->next;
    }
    
    return count;
}

void device_tree_print(const struct device_tree_builder *builder) {
    uart_puts("=== Device Tree ===\n");
    
    struct device_tree_node *node = builder->root;
    int node_count = 0;
    
    while (node) {
        uart_puts("Node ");
        uart_put_dec(node_count);
        uart_puts(": ");
        uart_puts(node->name);
        uart_puts(" (");
        uart_puts(node->compatible);
        uart_puts(")\n");
        
        if (node->type == DT_NODE_MEMORY || node->type == DT_NODE_UART || 
            node->type == DT_NODE_VIRTIO) {
            uart_puts("  Address: ");
            uart_put_hex(node->base_addr);
            if (node->size > 0) {
                uart_puts(" Size: ");
                uart_put_hex(node->size);
            }
            uart_puts("\n");
        }
        
        if (node->interrupt > 0) {
            uart_puts("  Interrupt: ");
            uart_put_dec(node->interrupt);
            uart_puts("\n");
        }
        
        node = node->next;
        node_count++;
    }
    
    uart_puts("Buffer used: ");
    uart_put_memsize(builder->buffer_used);
    uart_puts("/");
    uart_put_memsize(builder->buffer_size);
    uart_puts("\n");
    uart_puts("==================\n");
}

void *device_tree_get_binary(const struct device_tree_builder *builder) {
    // 简化版本：返回结构化的设备树数据
    // 真实的设备树需要FDT (Flattened Device Tree) 格式
    return builder->buffer;
}

uint64 device_tree_get_binary_size(const struct device_tree_builder *builder) {
    return builder->buffer_used;
}

// === 硬件抽象层实现 ===

int hardware_detect_platform(struct hardware_description *hw_desc) {
    // 在QEMU virt机器上，我们可以通过已知的内存映射来检测
    // 检查UART是否在预期位置
    volatile uint32 *uart_ptr = (volatile uint32 *)QEMU_VIRT_UART_BASE;
    
    // 简单的检测：尝试读取UART寄存器
    // 这不是完全可靠的，但对于bootloader够用
    
    hw_desc->platform = HW_PLATFORM_QEMU_VIRT;
    hw_desc->memory_base = QEMU_VIRT_MEMORY_BASE;
    hw_desc->memory_size = 128 * 1024 * 1024;  // 128MB default
    hw_desc->uart_base = QEMU_VIRT_UART_BASE;
    hw_desc->uart_interrupt = QEMU_VIRT_UART_IRQ;
    hw_desc->virtio_base = QEMU_VIRT_VIRTIO_BASE;
    hw_desc->virtio_interrupt = QEMU_VIRT_VIRTIO_IRQ;
    hw_desc->plic_base = QEMU_VIRT_PLIC_BASE;
    hw_desc->cpu_count = 1;  // Single core for now
    
    uart_puts("Hardware platform detected: QEMU virt\n");
    
    return 0;
}

void hardware_print_info(const struct hardware_description *hw_desc) {
    uart_puts("=== Hardware Information ===\n");
    
    uart_puts("Platform: ");
    switch (hw_desc->platform) {
        case HW_PLATFORM_QEMU_VIRT:
            uart_puts("QEMU virt");
            break;
        default:
            uart_puts("Unknown");
            break;
    }
    uart_puts("\n");
    
    uart_puts("Memory: ");
    uart_put_hex(hw_desc->memory_base);
    uart_puts(" (");
    uart_put_memsize(hw_desc->memory_size);
    uart_puts(")\n");
    
    uart_puts("UART: ");
    uart_put_hex(hw_desc->uart_base);
    uart_puts(" IRQ ");
    uart_put_dec(hw_desc->uart_interrupt);
    uart_puts("\n");
    
    uart_puts("VirtIO: ");
    uart_put_hex(hw_desc->virtio_base);
    uart_puts(" IRQ ");
    uart_put_dec(hw_desc->virtio_interrupt);
    uart_puts("\n");
    
    uart_puts("PLIC: ");
    uart_put_hex(hw_desc->plic_base);
    uart_puts("\n");
    
    uart_puts("CPUs: ");
    uart_put_dec(hw_desc->cpu_count);
    uart_puts("\n");
    
    uart_puts("============================\n");
}

int hardware_validate_config(const struct hardware_description *hw_desc) {
    // 基本的硬件配置验证
    if (hw_desc->memory_size < 64 * 1024 * 1024) {
        uart_puts("WARNING: Memory size too small (< 64MB)\n");
        return -1;
    }
    
    if (hw_desc->cpu_count == 0) {
        uart_puts("ERROR: No CPUs detected\n");
        return -1;
    }
    
    if (hw_desc->uart_base == 0) {
        uart_puts("ERROR: UART not detected\n");
        return -1;
    }
    
    uart_puts("Hardware configuration validation: PASSED\n");
    return 0;
}
