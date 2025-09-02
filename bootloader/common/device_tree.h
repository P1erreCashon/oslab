#ifndef DEVICE_TREE_H
#define DEVICE_TREE_H

// Stage 3.3.2: 设备树生成和硬件抽象层

// 设备树节点类型
typedef enum {
    DT_NODE_ROOT,
    DT_NODE_MEMORY,
    DT_NODE_CPU,
    DT_NODE_UART,
    DT_NODE_VIRTIO,
    DT_NODE_INTERRUPT_CONTROLLER
} device_tree_node_type_t;

// 简化的设备树节点结构
struct device_tree_node {
    const char *name;
    device_tree_node_type_t type;
    uint64 base_addr;
    uint64 size;
    uint32 interrupt;
    const char *compatible;
    struct device_tree_node *next;
};

// 设备树构建器
struct device_tree_builder {
    struct device_tree_node *root;
    struct device_tree_node *current;
    char *buffer;
    uint64 buffer_size;
    uint64 buffer_used;
};

// === 设备树API ===

// 初始化设备树构建器
void device_tree_init(struct device_tree_builder *builder, void *buffer, uint64 buffer_size);

// 添加设备节点
int device_tree_add_memory(struct device_tree_builder *builder, uint64 base, uint64 size);
int device_tree_add_cpu(struct device_tree_builder *builder, int cpu_id);
int device_tree_add_uart(struct device_tree_builder *builder, uint64 base_addr, uint32 interrupt);
int device_tree_add_virtio(struct device_tree_builder *builder, uint64 base_addr, uint32 interrupt);

// 生成设备树二进制文件 (简化版)
int device_tree_finalize(struct device_tree_builder *builder);

// 设备树调试显示
void device_tree_print(const struct device_tree_builder *builder);

// 获取设备树二进制数据 (for kernel)
void *device_tree_get_binary(const struct device_tree_builder *builder);
uint64 device_tree_get_binary_size(const struct device_tree_builder *builder);

// === 硬件抽象层 ===

// 硬件平台类型
typedef enum {
    HW_PLATFORM_QEMU_VIRT,
    HW_PLATFORM_UNKNOWN
} hardware_platform_t;

// 硬件描述结构
struct hardware_description {
    hardware_platform_t platform;
    uint64 memory_base;
    uint64 memory_size;
    uint64 uart_base;
    uint32 uart_interrupt;
    uint64 virtio_base;
    uint32 virtio_interrupt;
    uint64 plic_base;
    int cpu_count;
};

// 硬件抽象API
int hardware_detect_platform(struct hardware_description *hw_desc);
void hardware_print_info(const struct hardware_description *hw_desc);
int hardware_validate_config(const struct hardware_description *hw_desc);

#endif // DEVICE_TREE_H
