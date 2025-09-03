# Bootloader Stage 3 实现计划

**目标**: 完整的ELF内核加载和启动机制  
**日期**: 2025年9月2日  
**状态**: 计划制定 📋

## 阶段目标

Stage 3的核心目标是实现**完整的xv6内核ELF文件加载和启动**，使bootloader能够从磁盘加载真实的xv6内核并成功启动。

### 主要任务

1. **修复Stage 2运行问题** ⚠️
2. **完善ELF解析器** 🔧
3. **实现完整内核加载** 🚀
4. **内核启动参数传递** 📡
5. **整合测试和验证** ✅

## 详细任务分解

### 任务1: 修复Stage 2运行问题 (高优先级)

**问题**: 当前Stage 2测试超时，需要诊断和修复

**子任务**:
- [ ] 1.1 调试Stage 1到Stage 2的跳转
- [ ] 1.2 验证virtio设备初始化
- [ ] 1.3 修复内存布局冲突
- [ ] 1.4 确保UART调试输出正常

**实现细节**:
```c
// stage2/main.c - 添加早期调试输出
void bootloader_main(void) {
    // 第一行就输出，确认Stage 2启动
    uart_puts("=== STAGE 2 START ===\n");
    
    // 显示内存地址确认加载位置
    uart_puts("Stage 2 addr: ");
    uart_put_hex((uint64)bootloader_main);
    uart_puts("\n");
    
    // 逐步验证每个初始化步骤
    uart_puts("Initializing virtio...\n");
    // ...
}
```

### 任务2: 完善ELF解析器 (核心功能)

**目标**: 实现完整的ELF文件解析和程序段加载

**子任务**:
- [ ] 2.1 扩展ELF头结构定义
- [ ] 2.2 实现程序头表解析
- [ ] 2.3 实现程序段加载逻辑
- [ ] 2.4 处理BSS段清零
- [ ] 2.5 验证内存权限设置

**新增文件**: `bootloader/common/elf_loader.c`
```c
#include "boot_types.h"

// 完整的ELF头定义
struct elf_header {
    uint8 e_ident[16];
    uint16 e_type;
    uint16 e_machine;
    uint32 e_version;
    uint64 e_entry;     // 入口点地址
    uint64 e_phoff;     // 程序头表偏移
    uint64 e_shoff;     // 节头表偏移
    uint32 e_flags;
    uint16 e_ehsize;
    uint16 e_phentsize; // 程序头大小
    uint16 e_phnum;     // 程序头数量
    uint16 e_shentsize;
    uint16 e_shnum;
    uint16 e_shstrndx;
};

// 程序头结构
struct program_header {
    uint32 p_type;
    uint32 p_flags;
    uint64 p_offset;    // 文件偏移
    uint64 p_vaddr;     // 虚拟地址
    uint64 p_paddr;     // 物理地址
    uint64 p_filesz;    // 文件中大小
    uint64 p_memsz;     // 内存中大小
    uint64 p_align;
};

#define PT_LOAD 1       // 可加载段

// ELF加载函数
int elf_load_kernel(char *elf_data, uint64 *entry_point);
```

### 任务3: 实现完整内核加载 (整合功能)

**目标**: 从磁盘加载真实的xv6内核ELF文件

**子任务**:
- [ ] 3.1 修改磁盘布局以包含xv6内核
- [ ] 3.2 实现分段加载算法
- [ ] 3.3 处理内核重定位
- [ ] 3.4 验证内核完整性

**磁盘布局调整**:
```
磁盘镜像结构 (Stage 3):
├── 扇区0: Boot Sector (512字节)
├── 扇区1-63: Stage 2 (32KB)  
├── 扇区64-2047: xv6内核ELF (1MB)
└── 扇区2048+: 文件系统 (fs.img)
```

**实现**: 修改主项目Makefile
```makefile
# 创建包含真实内核的引导磁盘
bootdisk_stage3.img: bootloader/stage1.bin bootloader/stage2.bin $K/kernel fs.img
	dd if=/dev/zero of=bootdisk_stage3.img bs=1M count=64
	dd if=bootloader/stage1.bin of=bootdisk_stage3.img bs=512 count=1 conv=notrunc
	dd if=bootloader/stage2.bin of=bootdisk_stage3.img bs=512 seek=1 conv=notrunc
	dd if=$K/kernel of=bootdisk_stage3.img bs=512 seek=64 conv=notrunc
	dd if=fs.img of=bootdisk_stage3.img bs=512 seek=2048 conv=notrunc
```

### 任务4: 内核启动参数传递 (兼容性)

**目标**: 确保bootloader能正确传递启动信息给内核

**子任务**:
- [ ] 4.1 研究xv6内核期望的启动环境
- [ ] 4.2 设置正确的寄存器状态
- [ ] 4.3 传递设备树信息(如需要)
- [ ] 4.4 确保内存映射兼容

**关键点**:
```c
// 跳转前的寄存器设置
void jump_to_kernel(uint64 entry_point) {
    uart_puts("Jumping to kernel...\n");
    
    // 清理缓存和内存屏障
    asm volatile("fence");
    
    // 设置内核启动参数 (如果需要)
    // a0 = hart_id, a1 = device_tree_addr等
    
    // 跳转到内核入口点
    asm volatile(
        "fence.i\n"
        "jr %0"
        :
        : "r" (entry_point)
        : "memory"
    );
}
```

### 任务5: 整合测试和验证 (质量保证)

**目标**: 确保完整启动流程稳定可靠

**子任务**:
- [ ] 5.1 创建完整的端到端测试
- [ ] 5.2 验证与原版xv6的兼容性
- [ ] 5.3 性能测试和优化
- [ ] 5.4 错误处理完善

**测试脚本**: `bootloader/test/test_stage3.sh`
```bash
#!/bin/bash
echo "=== Stage 3 Full Test ==="

# 构建完整系统
make -C .. bootdisk_stage3.img

# 运行完整启动测试
echo "Testing complete bootloader -> kernel flow..."
timeout 30 qemu-system-riscv64 -machine virt -bios none \
    -drive file=../bootdisk_stage3.img,format=raw,if=virtio \
    -m 128M -nographic
```

## 技术挑战和解决方案

### 挑战1: ELF加载复杂性
**问题**: xv6内核ELF文件较大，需要分段加载
**解决**: 实现流式加载，逐段读取和放置

### 挑战2: 内存布局冲突  
**问题**: bootloader和内核的内存使用可能冲突
**解决**: 精确规划内存布局，确保不重叠

### 挑战3: 启动环境差异
**问题**: 直接从bootloader启动与QEMU默认启动环境不同
**解决**: 研究xv6内核的启动需求，模拟相同环境

## 验收标准

### Stage 3完成标准:
- [ ] Stage 1 -> Stage 2 -> xv6内核完整启动流程
- [ ] xv6内核能正常运行，显示shell提示符
- [ ] 基本用户程序(如ls, cat)能正常执行
- [ ] 文件系统读写功能正常
- [ ] 启动时间不超过5秒

### 质量标准:
- [ ] 错误处理覆盖率95%以上
- [ ] 启动成功率99%以上
- [ ] 完整的调试输出和日志
- [ ] 代码注释率80%以上

## 时间计划

### 第一周 (9月2日-9月9日)
- [ ] 任务1: 修复Stage 2问题
- [ ] 任务2: 基础ELF解析器

### 第二周 (9月9日-9月16日)  
- [ ] 任务3: 内核加载实现
- [ ] 任务4: 启动参数传递

### 第三周 (9月16日-9月23日)
- [ ] 任务5: 整合测试
- [ ] 文档完善和代码优化

## 风险评估

### 高风险 ❌
- ELF格式兼容性问题
- 内存布局冲突难以解决

### 中风险 ⚠️  
- virtio驱动在复杂场景下的稳定性
- xv6内核启动环境要求

### 低风险 ✅
- 基础功能已验证稳定
- 开发环境和工具链成熟

---

**Stage 3成功标志**: 
```
$ make qemu-boot
BOOT
LDG2
=== Bootloader Stage 2 ===
Loading kernel ELF...
Kernel loaded: entry=0x80000000
Jumping to kernel...

xv6 kernel is booting

hart 1 starting
hart 2 starting  
init: starting sh
$ ls
.              1 1 1024
..             1 1 1024
README         2 2 2059
cat            2 3 32864
echo           2 4 31720
...
```
