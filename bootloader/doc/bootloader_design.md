# xv6 Bootloader 实验设计方案

## 1. 项目背景与目标

### 1.1 当前状态分析
- 现有xv6使用QEMU的 `-kernel` 参数直接加载内核
- 内核直接从0x80000000开始执行（RISC-V架构）
- 缺少传统的bootloader启动流程
- 启动流程：QEMU → entry.S → start.c → main.c

### 1.2 实验目标
- 实现完整的两阶段bootloader
- 模拟真实计算机的启动过程
- 学习底层系统编程和启动机制
- 理解内存布局和地址映射

## 2. 技术方案设计

### 2.1 整体架构

```
启动流程：
硬件上电 → QEMU ROM → Boot Sector → Bootloader → 内核
```

#### 阶段划分：
1. **阶段0**: 硬件初始化（QEMU模拟）
2. **阶段1**: Boot Sector（512字节，存储在磁盘第一扇区）
3. **阶段2**: Bootloader主程序（从磁盘加载，功能完整）
4. **阶段3**: 内核加载与启动

### 2.2 内存布局规划

```
RISC-V 内存布局:
0x00000000 - 0x00000FFF : 设备树和启动参数
0x00001000 - 0x0000FFFF : Boot ROM (QEMU提供)
0x10000000 - 0x10000FFF : UART寄存器
0x80000000 - 0x80000FFF : Bootloader加载区域
0x80001000 - 0x801FFFFF : 内核镜像区域
0x80200000 - 0x87FFFFFF : 可用内存区域
```

## 3. 实现方案

### 3.1 目录结构

```
oslab/
├── bootloader/
│   ├── boot.S           # 第一阶段引导扇区
│   ├── main.c           # 第二阶段主程序
│   ├── boot.h           # 头文件定义
│   ├── boot.ld          # 链接脚本
│   ├── elf.h            # ELF格式定义
│   └── Makefile         # 构建脚本
├── kernel/              # 现有内核代码
├── user/                # 现有用户程序
├── mkfs/                # 文件系统工具
└── tools/               # 新增：磁盘镜像工具
    └── create_disk.c    # 创建可启动磁盘镜像
```

### 3.2 第一阶段：Boot Sector (boot.S)

**功能职责：**
- 设置基本运行环境
- 初始化串口输出
- 从磁盘加载第二阶段bootloader
- 跳转到第二阶段

**技术要点：**
```assembly
# boot.S 主要功能
.globl _start
_start:
    # 1. 设置栈指针
    la sp, stack_top
    
    # 2. 初始化串口
    call init_uart
    
    # 3. 打印启动信息
    la a0, boot_msg
    call print_string
    
    # 4. 从磁盘加载bootloader
    call load_bootloader
    
    # 5. 跳转到第二阶段
    la t0, 0x80000000
    jr t0

boot_msg: .string "xv6 bootloader starting...\n"
```

### 3.3 第二阶段：Bootloader主程序 (main.c)

**功能职责：**
- 内存检测与管理
- 文件系统初步支持
- ELF内核加载
- 启动参数准备
- 跳转到内核

**核心功能模块：**

#### 3.3.1 磁盘I/O模块
```c
// 磁盘操作接口
int disk_read(uint32_t sector, void* buffer, uint32_t count);
int disk_write(uint32_t sector, const void* buffer, uint32_t count);
```

#### 3.3.2 ELF加载模块
```c
// ELF内核加载
typedef struct {
    uint32_t magic;
    uint32_t entry;
    uint32_t phoff;
    uint16_t phnum;
} elf_header_t;

int load_kernel_elf(const char* kernel_path);
```

#### 3.3.3 内存管理模块
```c
// 简单内存管理
void* bootloader_alloc(size_t size);
void setup_memory_map(void);
uint64_t detect_memory_size(void);
```

### 3.4 内核适配修改

#### 3.4.1 修改入口点 (kernel/entry.S)
```assembly
# 原来直接从QEMU加载，现在需要处理bootloader传递的参数
_entry:
    # 获取bootloader传递的参数
    # a0: hart ID
    # a1: 设备树地址
    # a2: 内存大小
    
    # 保存参数到全局变量
    la t0, boot_hart_id
    sd a0, 0(t0)
    
    la t0, device_tree_addr
    sd a1, 0(t0)
    
    la t0, memory_size
    sd a2, 0(t0)
    
    # 设置栈并跳转到start
    la sp, stack0
    # ... 其余代码保持不变
```

#### 3.4.2 修改启动初始化 (kernel/main.c)
```c
// 添加全局变量
extern uint64_t boot_hart_id;
extern uint64_t device_tree_addr;
extern uint64_t memory_size;

void main() {
    if(cpuid() == 0) {
        consoleinit();
        printfinit();
        printf("\n");
        printf("xv6 kernel loaded by bootloader\n");
        printf("Memory size: %d MB\n", memory_size / (1024*1024));
        printf("\n");
        
        // 使用bootloader检测的内存大小初始化
        kinit_with_size(memory_size);
        // ... 其余初始化代码
    }
    // ...
}
```

### 3.5 构建系统修改

#### 3.5.1 修改主Makefile
```makefile
# 新增bootloader构建目标
BOOTLOADER_OBJS = bootloader/boot.o bootloader/main.o

# 修改QEMU选项，去掉-kernel参数
QEMUOPTS = -machine virt -bios none -drive file=disk.img,format=raw,if=virtio

# 构建可启动磁盘镜像
disk.img: bootloader/bootloader.bin kernel/kernel fs.img
	dd if=/dev/zero of=disk.img bs=1M count=64
	dd if=bootloader/bootloader.bin of=disk.img bs=512 count=1 conv=notrunc
	dd if=kernel/kernel of=disk.img bs=512 seek=1 conv=notrunc
	dd if=fs.img of=disk.img bs=512 seek=2048 conv=notrunc

bootloader/bootloader.bin: $(BOOTLOADER_OBJS) bootloader/boot.ld
	$(LD) $(LDFLAGS) -T bootloader/boot.ld -o bootloader/bootloader.elf $(BOOTLOADER_OBJS)
	$(OBJCOPY) -O binary bootloader/bootloader.elf bootloader/bootloader.bin
	@echo "Bootloader size: $$(stat -f%z bootloader/bootloader.bin 2>/dev/null || stat -c%s bootloader/bootloader.bin) bytes"
```

#### 3.5.2 Bootloader专用Makefile
```makefile
# bootloader/Makefile
CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -Wall -Werror -O -fno-omit-frame-pointer -ggdb -gdwarf-2
CFLAGS += -ffreestanding -fno-common -nostdlib -mno-relax
CFLAGS += -I../kernel

OBJS = boot.o main.o

bootloader.bin: $(OBJS) boot.ld
	$(LD) -T boot.ld -o bootloader.elf $(OBJS)
	$(OBJCOPY) -O binary bootloader.elf bootloader.bin

boot.o: boot.S
	$(CC) $(CFLAGS) -c boot.S

main.o: main.c
	$(CC) $(CFLAGS) -c main.c

clean:
	rm -f *.o *.elf *.bin
```

## 4. 实现步骤与里程碑

### 4.1 第一阶段实现 (Week 1)
- [ ] 创建bootloader目录结构
- [ ] 实现简单的boot.S（只打印消息）
- [ ] 修改Makefile支持bootloader构建
- [ ] 测试QEMU能否从磁盘启动

### 4.2 第二阶段实现 (Week 2)
- [ ] 实现磁盘读取功能
- [ ] 添加boot.S中的磁盘加载代码
- [ ] 实现bootloader主程序框架
- [ ] 测试两阶段加载流程

### 4.3 第三阶段实现 (Week 3)
- [ ] 实现ELF解析和内核加载
- [ ] 修改内核入口点适配bootloader
- [ ] 实现参数传递机制
- [ ] 完整启动流程测试

### 4.4 优化与扩展 (Week 4)
- [ ] 添加内存检测功能
- [ ] 实现启动选项菜单
- [ ] 添加错误处理和诊断信息
- [ ] 性能优化和代码清理

## 5. 关键技术难点

### 5.1 磁盘I/O实现
**挑战：** RISC-V架构下的磁盘访问机制
**解决方案：** 使用QEMU的virtio-blk设备，实现MMIO访问

### 5.2 ELF加载
**挑战：** 正确解析ELF头部，加载程序段到内存
**解决方案：** 参考现有kernel/exec.c的实现，简化为bootloader版本

### 5.3 内存布局管理
**挑战：** bootloader、内核、用户空间的内存不冲突
**解决方案：** 精心设计内存映射，使用链接脚本控制

### 5.4 参数传递
**挑战：** bootloader与内核之间的信息传递
**解决方案：** 使用寄存器传递关键参数，内存区域传递复杂数据

## 6. 测试与验证

### 6.1 单元测试
- 磁盘读写功能测试
- ELF解析功能测试
- 内存分配功能测试

### 6.2 集成测试
- 完整启动流程测试
- 内核功能正常性测试
- 用户程序运行测试

### 6.3 边界测试
- 大内核镜像加载测试
- 磁盘错误处理测试
- 内存不足处理测试

## 7. 扩展功能设想

### 7.1 多内核支持
- 支持加载不同版本的内核
- 启动菜单选择功能

### 7.2 网络启动
- PXE启动支持
- TFTP协议实现

### 7.3 加密启动
- 内核签名验证
- 安全启动链

## 8. 参考资料

### 8.1 技术文档
- RISC-V Instruction Set Manual
- QEMU RISC-V Virt Machine Documentation
- ELF Specification
- xv6 Book (MIT)

### 8.2 相关代码
- GRUB bootloader source
- U-Boot bootloader for RISC-V
- Linux kernel early boot code

## 9. 风险评估与应对

### 9.1 技术风险
**风险：** RISC-V架构理解不够深入
**应对：** 加强RISC-V手册学习，参考现有代码

**风险：** QEMU模拟器行为与真实硬件差异
**应对：** 聚焦QEMU环境，后期可扩展到真实硬件

### 9.2 时间风险
**风险：** 实现复杂度超出预期
**应对：** 分阶段实现，确保核心功能优先

**风险：** 调试困难，进度缓慢
**应对：** 完善日志输出，使用GDB调试

## 10. 总结

本bootloader实验将帮助深入理解操作系统启动过程，掌握底层系统编程技能。通过分阶段实现，可以循序渐进地完成一个功能完整的bootloader系统，为进一步的操作系统开发奠定基础。

---

*文档版本：v1.0*  
*创建日期：2025-08-31*  
*作者：OS实验项目组*
