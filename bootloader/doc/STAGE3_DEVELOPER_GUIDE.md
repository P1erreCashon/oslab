# Stage 3 开发者指南

## 项目结构

```
bootloader/
├── stage1/             # 第一阶段 (512字节限制)
├── stage2/             # 第二阶段主程序
├── common/             # 共享模块
│   ├── boot_types.h        # 基础类型和兼容性定义
│   ├── memory_layout.h/.c  # Stage 3.3.1: 内存布局管理
│   ├── device_tree.h/.c    # Stage 3.3.2: 设备树和硬件抽象
│   ├── boot_info.h/.c      # Stage 3.2: 引导信息接口
│   ├── elf_loader.h/.c     # ELF内核加载器
│   ├── virtio_boot.h/.c    # VirtIO块设备驱动
│   ├── fast_mem.S          # Stage 3.1: 优化的内存操作
│   ├── uart.c              # UART串口驱动
│   ├── memory.c            # 内存分配器
│   └── string.c            # 字符串操作
├── test/               # 测试套件
└── doc/               # 文档
```

## 核心架构

### Stage 3.1: 性能优化架构

**快速内存操作** (`fast_mem.S`)
```assembly
fast_memcpy:
    # 64位对齐的高速内存复制
    # 支持大块数据传输
    
fast_memset:  
    # 64位对齐的高速内存清零
    # 用于BSS段清除
```

**进度报告系统**
- 实时显示内核加载进度 (0-100%)
- 详细的内存操作状态
- 每1KB显示进度更新

### Stage 3.2: 引导接口架构

**Boot Info结构** (`boot_info.h`)
```c
struct boot_info {
    uint64 magic;           // 0x52495343564B5256 ("RISCVKRN")
    uint64 version;         // 版本号
    uint64 hart_count;      // 处理器数量
    uint64 memory_base;     // 内存基地址
    uint64 memory_size;     // 内存大小
    uint64 kernel_base;     // 内核加载地址
    uint64 kernel_size;     // 内核大小
    uint64 kernel_entry;    // 内核入口点
    uint64 device_tree_addr; // 设备树地址
    uint64 device_tree_size; // 设备树大小
    // ... 其他字段
};
```

**RISC-V参数传递**
```c
// 符合RISC-V引导约定
asm volatile(
    "mv a0, %0\n"    // Hart ID
    "mv a1, %1\n"    // Boot info pointer  
    "jr %2\n"        // 跳转到内核
    : : "r"(hart_id), "r"(boot_info_ptr), "r"(kernel_entry)
);
```

### Stage 3.3: 高级特性架构

#### 3.3.1 内存布局管理

**生产级内存映射**
```
0x80000000-0x8002FFFF : 内核区域 (192KB, RWX)
0x80030000-0x8003FFFF : Stage2 (64KB, RX)  
0x80040000-0x8004FFFF : Boot Info (64KB, RW)
0x80050000-0x8005FFFF : VirtIO (64KB, RW)
0x80060000-0x8006FFFF : Bootloader Heap (64KB, RW)
```

**内存保护框架**
```c
typedef enum {
    MEM_PROT_NONE = 0,
    MEM_PROT_READ = 1,
    MEM_PROT_WRITE = 2, 
    MEM_PROT_EXEC = 4
} memory_protection_t;
```

#### 3.3.2 设备树和硬件抽象

**硬件平台检测**
```c
struct hardware_description {
    hardware_platform_t platform;  // QEMU_VIRT
    uint64 memory_base;            // 0x80000000
    uint64 memory_size;            // 128MB
    uint64 uart_base;              // 0x10000000
    uint64 virtio_base;            // 0x10001000
    // ...
};
```

**设备树节点**
- Memory节点: 描述系统内存布局
- CPU节点: RISC-V处理器信息
- UART节点: 串口设备配置
- VirtIO节点: 块设备信息

## API参考

### 内存管理API

```c
// 内存布局验证
bool memory_layout_validate(void);
void memory_layout_print(void);

// 内存保护 (框架，引导阶段简化实现)
int memory_protect_region(uint64 base, uint64 size, memory_protection_t prot);
bool memory_check_protection(uint64 addr, memory_protection_t required_prot);
```

### 设备树API

```c
// 设备树构建
void device_tree_init(struct device_tree_builder *builder, void *buffer, uint64 size);
int device_tree_add_memory(struct device_tree_builder *builder, uint64 base, uint64 size);
int device_tree_add_cpu(struct device_tree_builder *builder, int cpu_id);
int device_tree_finalize(struct device_tree_builder *builder);

// 硬件抽象
int hardware_detect_platform(struct hardware_description *hw_desc);
int hardware_validate_config(const struct hardware_description *hw_desc);
```

### 引导信息API

```c
// 引导信息管理
void boot_info_init(struct boot_info *info);
void boot_info_setup_kernel_params(const struct elf_load_info *load_info);
void boot_info_setup_device_tree(const struct device_tree_builder *dt_builder,
                                 const struct hardware_description *hw_desc);
struct boot_info *get_boot_info(void);
```

## 调试指南

### 启用调试输出

在`boot_types.h`中设置：
```c
#define BOOT_DEBUG 1
```

### 常见调试场景

1. **内存布局问题**
   - 检查"Memory Layout Validation"输出
   - 查看内存区域重叠警告

2. **VirtIO驱动问题**  
   - 观察"Virtio Status"部分
   - 检查队列初始化状态

3. **ELF加载问题**
   - 查看"ELF Kernel Loader"部分
   - 检查程序头和段信息

4. **设备树问题**
   - 观察"Device Tree"节点列表
   - 检查缓冲区使用情况

### 性能分析

- **编译大小**: Stage 2应在32KB内
- **加载时间**: 内核加载应在10秒内完成
- **内存使用**: 引导程序总内存使用<16KB

## 扩展开发

### 添加新设备支持

1. 在`hardware_description`中添加设备字段
2. 在`hardware_detect_platform()`中添加检测逻辑
3. 在设备树构建中添加相应节点
4. 更新引导信息结构

### 支持新平台

1. 创建新的`hardware_platform_t`枚举值
2. 在`hardware_detect_platform()`中添加检测代码
3. 定义平台特定的内存映射
4. 更新设备树生成逻辑

## 测试

### 快速测试
```bash
./test/test_stage3_simple.sh
```

### 完整测试 (需要更多时间)
```bash
./test/test_stage3_complete.sh
```

### 手动验证

查看关键输出：
- "Memory layout validation: PASSED"
- "Hardware platform detected: QEMU virt" 
- "Device tree finalized with 4 nodes"
- "Magic: 0x52495343564B5256"
- 内核加载进度达到100%

## 已知限制

1. **磁盘超时**: 在某些配置下可能出现virtio磁盘读取超时
2. **单核支持**: 当前只支持单处理器配置
3. **简化设备树**: 不是完整的FDT格式，但包含必要信息
4. **引导阶段保护**: 内存保护在引导阶段是框架性的

## 下一步开发

Stage 3完成后，建议的后续开发：

1. **Stage 3.4**: 完整系统验证
   - 端到端xv6启动测试
   - 性能基准测试
   - 与原始xv6的兼容性验证

2. **多平台支持**: 扩展到其他RISC-V平台

3. **高级引导特性**: 引导菜单、多内核选择

4. **安全功能**: 引导验证、签名检查

## 贡献指南

1. 保持现有的错误处理风格
2. 添加足够的调试输出
3. 遵循现有的命名约定
4. 更新相关测试和文档
