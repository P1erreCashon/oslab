# Bootloader 开发指南

## 快速开始

### 环境准备
```bash
# 确保有RISC-V工具链
riscv64-unknown-elf-gcc --version
qemu-system-riscv64 --version

# 进入项目目录
cd /home/xv6/Desktop/code/oslab/bootloader
```

### 基本构建
```bash
# 构建Stage1 (512字节引导程序)
make stage1.bin

# 构建Stage2 (主引导程序)  
make stage2.bin

# 构建完整系统镜像
make bootdisk_stage3.img
```

### 运行测试
```bash
# 进入上级目录
cd ..

# 启动完整系统
qemu-system-riscv64 -machine virt -bios none \
  -kernel bootloader/stage1.bin \
  -device loader,addr=0x80030000,file=bootloader/stage2.bin \
  -global virtio-mmio.force-legacy=false \
  -drive file=bootloader/bootdisk_stage3.img,if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
  -m 128M -nographic
```

## 开发流程

### 1. 修改代码
- 编辑`common/`目录下的模块文件
- 修改`stage2/main.c`的主控制逻辑
- 更新`boot.S`或`stage2_start.S`的汇编代码

### 2. 增量构建
```bash
# 只重建Stage2
make stage2.bin

# 重建完整镜像
make bootdisk_stage3.img
```

### 3. 调试技巧
```bash
# 查看反汇编
make disasm-stage1
make disasm-stage2

# 查看十六进制内容
make hexdump-stage1
make hexdump-stage2

# 检查磁盘镜像内容
make inspect-disk
```

## 常见问题排查

### Stage1 问题
- **问题**: Stage1超过512字节
- **解决**: 简化`boot.S`代码，只保留必要功能

### Stage2 问题  
- **问题**: 无法跳转到Stage2
- **检查**: 确认Stage2地址正确 (0x80030000)

### VirtIO 问题
- **问题**: 磁盘读取失败
- **检查**: VirtIO设备初始化，队列状态

### 内存问题
- **问题**: 内存重叠或越界
- **检查**: `memory_layout.c`中的验证输出

### ELF 问题
- **问题**: 内核加载失败
- **检查**: ELF格式，文件偏移，读取完整性

## 代码风格指南

### 命名约定
- 文件名: `snake_case.c/.h`
- 函数名: `snake_case()`
- 变量名: `snake_case`
- 常量名: `UPPER_SNAKE_CASE`
- 结构体: `struct snake_case`

### 注释规范
```c
// 单行注释用于简单说明
/* 多行注释用于详细说明
 * 模块功能或复杂逻辑
 */

/**
 * 函数注释格式
 * @param: 参数说明
 * @return: 返回值说明  
 */
```

### 错误处理
```c
// 使用统一的错误报告宏
if (condition_failed) {
    ERROR_REPORT(ERROR_SPECIFIC_CODE);
    return -1;
}

// 带上下文的错误报告
if (device_init_failed) {
    ERROR_REPORT_CTX1(ERROR_DEVICE_NOT_READY, device_id);
    goto error;
}
```

## 扩展开发

### 添加新设备支持
1. **定义设备类型**
```c
// device_tree.h
typedef enum {
    DT_NODE_YOUR_DEVICE = 6,  // 添加新类型
} device_tree_node_type_t;
```

2. **实现设备节点添加**
```c
// device_tree.c
int device_tree_add_your_device(struct device_tree_builder *builder, 
                                uint64 base_addr, uint32 interrupt) {
    // 实现设备节点添加逻辑
}
```

3. **集成到主流程**
```c
// main.c
if (device_tree_add_your_device(&dt_builder, hw_desc.device_base, 
                                hw_desc.device_interrupt) != 0) {
    ERROR_REPORT(ERROR_DEVICE_TREE_FAILED);
    goto error;
}
```

### 添加新功能模块
1. **创建模块文件**
```bash
# 创建头文件
touch common/new_module.h

# 创建实现文件  
touch common/new_module.c
```

2. **更新构建系统**
```makefile
# Makefile中添加对象文件
COMMON_OBJS = ... common/new_module.o
```

3. **添加接口声明**
```c
// boot_types.h中添加函数声明
int new_module_init(void);
void new_module_cleanup(void);
```

## 调试技巧

### UART调试输出
```c
// 基础输出
uart_puts("Debug message\n");

// 数值输出
uart_put_hex(0x12345678);  // 十六进制
uart_put_dec(1234);        // 十进制

// 内存大小输出
uart_put_memsize(1024);    // "1 KB"
```

### 内存状态检查
```c
// 检查内存使用
uart_puts("Memory used: ");
uart_put_memsize(boot_memory_used());
uart_puts("\n");

// 验证内存布局
if (!memory_layout_validate()) {
    uart_puts("Memory layout validation failed!\n");
}
```

### 设备状态监控
```c
// 显示VirtIO状态
virtio_show_status();

// 打印设备树
device_tree_print(&dt_builder);
```

## 性能优化

### 编译优化
```bash
# 使用-Os优化大小和速度平衡
CFLAGS += -Os

# 添加调试信息但保持优化
CFLAGS += -g
```

### 内存访问优化
```c
// 使用汇编优化函数
fast_memcpy(dest, src, size);
fast_memset(ptr, size);

// 确保对齐分配
void *ptr = boot_alloc_page();  // 4KB对齐
```

### I/O优化
```c
// 批量磁盘操作
for (int i = 0; i < sectors; i++) {
    // 连续扇区读取减少开销
    virtio_disk_read_sync(start_sector + i, buffer + i * SECTOR_SIZE);
}
```

## 测试策略

### 单元测试
- 测试各个模块的独立功能
- 验证错误处理路径
- 检查边界条件

### 集成测试  
- 测试完整引导流程
- 验证内存布局正确性
- 检查设备初始化序列

### 回归测试
- 确保新更改不破坏现有功能
- 验证在不同QEMU版本上的兼容性
- 测试不同内核版本的加载

## 发布检查清单

### 代码质量
- [ ] 所有函数都有适当的错误处理
- [ ] 内存分配都有对应的边界检查
- [ ] 调试输出清晰且有意义
- [ ] 代码风格符合项目规范

### 功能验证
- [ ] Stage1能够成功跳转到Stage2
- [ ] VirtIO设备能够正确初始化
- [ ] ELF内核能够完整加载
- [ ] 内核能够成功启动

### 性能检查
- [ ] Stage1大小 ≤ 512字节
- [ ] Stage2大小 ≤ 32KB (建议)
- [ ] 内存使用在合理范围内
- [ ] 启动时间可接受

### 文档完整性
- [ ] README.md包含使用说明
- [ ] ARCHITECTURE.md包含设计说明
- [ ] 代码注释充分且准确
- [ ] 变更日志已更新

这个开发指南为bootloader项目的持续开发和维护提供了实用的参考。
