# Stage 3 Bootloader 使用说明

## 概述

这是xv6操作系统的增强型引导程序，实现了完整的Stage 3功能：
- **Stage 3.1**: 性能优化 - 高效的内核加载和内存管理
- **Stage 3.2**: 引导接口 - RISC-V标准的内核参数传递
- **Stage 3.3**: 高级特性 - 内存布局、设备树、错误处理

## 系统要求

- RISC-V 64位系统 (rv64g)
- QEMU virt机器
- 至少128MB内存
- VirtIO块设备支持

## 构建说明

### 1. 构建引导程序

```bash
cd bootloader
make clean
make stage2.bin
```

这将生成：
- `stage1.bin` - 第一阶段引导程序 (168字节)
- `stage2.bin` - 第二阶段引导程序 (~20KB)

### 2. 创建引导磁盘

```bash
make bootdisk_stage3.img
```

这将创建包含以下内容的64MB磁盘镜像：
- 扇区0: Stage 1引导程序
- 扇区1-39: Stage 2引导程序  
- 扇区64-571: xv6内核
- 扇区2048+: xv6文件系统

## 运行说明

### 基本启动

```bash
qemu-system-riscv64 \
    -machine virt -cpu rv64 -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80030000,file=stage2.bin \
    -global virtio-mmio.force-legacy=false \
    -drive file=bootdisk_stage3.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    -m 128M -smp 1 -nographic
```

### 测试套件

运行完整的Stage 3测试：

```bash
cd test
./test_stage3_complete.sh
```

## 功能特性

### Stage 3.1 - 性能优化

- **快速内存操作**: 64位对齐的`fast_memcpy`和`fast_memset`
- **详细进度报告**: 实时显示内核加载进度
- **完整内核支持**: 支持35KB代码段 + 103KB BSS段
- **内存使用优化**: 高效的堆分配器

### Stage 3.2 - 引导接口

- **RISC-V标准参数传递**: 
  - `a0` = Hart ID (0)
  - `a1` = Boot info指针 (0x80040000)
- **完整引导信息结构**: 包含内存、内核、设备、文件系统信息
- **内核兼容性**: 符合RISC-V引导约定

### Stage 3.3 - 高级特性

#### 3.3.1 内存布局和保护
- **生产级内存映射**:
  ```
  0x80000000-0x8002FFFF : 内核区域 (192KB)
  0x80030000-0x8003FFFF : Stage 2引导程序 (64KB)
  0x80040000-0x8004FFFF : 引导信息区域 (64KB)
  0x80050000-0x8005FFFF : VirtIO缓冲区 (64KB)
  0x80060000-0x8006FFFF : 引导程序堆 (64KB)
  ```
- **内存区域验证**: 自动检查重叠和边界
- **内存保护框架**: 为未来的页表保护做准备

#### 3.3.2 设备树和硬件抽象
- **硬件平台检测**: 自动识别QEMU virt机器
- **设备树生成**: 标准的设备节点描述
  - Memory节点 (128MB @ 0x80000000)
  - CPU节点 (RISC-V rv64)
  - UART节点 (0x10000000, IRQ 10)
  - VirtIO节点 (0x10001000, IRQ 1)
- **硬件信息验证**: 配置完整性检查

#### 3.3.3 错误处理和恢复
- **统一错误码**: `BOOT_SUCCESS`, `BOOT_ERROR_*`系列
- **详细诊断信息**: 每个操作的状态报告
- **故障回退机制**: 出错时安全停机
- **调试输出**: 广泛的调试信息支持故障排除

## 输出解读

### 成功启动的典型输出

```
=== Bootloader Stage 2 ===
Stage 2 started successfully!
=== Memory Layout Validation ===
Memory layout validation: PASSED
=== Memory Layout ===
Kernel: 0x80000000-0x8002FFFF (192 KB, RWX)
Stage2 Bootloader: 0x80030000-0x8003FFFF (64 KB, RX)
Boot Info: 0x80040000-0x8004FFFF (64 KB, RW)
VirtIO Buffers: 0x80050000-0x8005FFFF (64 KB, RW)
Bootloader Heap: 0x80060000-0x8006FFFF (64 KB, RW)

Hardware platform detected: QEMU virt
=== Hardware Information ===
Platform: QEMU virt
Memory: 0x80000000 (128 MB)
UART: 0x10000000 IRQ 10
VirtIO: 0x10001000 IRQ 1
PLIC: 0xC000000
CPUs: 1

Device tree finalized with 4 nodes
=== Boot Information ===
Magic: 0x52495343564B5256
Version: 1
Hart Count: 1
Memory: 0x80000000 - 0x88000000 (128 MB)
Kernel: 0x80000000 entry=0x80000000 size=135 KB
Device Tree: 0x80041000 (224 bytes)

=== JUMPING TO KERNEL ===
Hart ID: 0
Boot Info Address: 0x80040000
```

### 关键指标

- **内存布局验证**: 必须显示"PASSED"
- **设备树节点**: 应该有4个节点 (memory, cpu, uart, virtio)
- **引导信息魔数**: 0x52495343564B5256 ("RISCVKRN")
- **内核加载进度**: 应该达到100%
- **Hart ID**: 0 (主处理器)
- **Boot Info地址**: 0x80040000

## 故障排除

### 常见问题

1. **内存布局验证失败**
   - 检查内存区域是否重叠
   - 验证地址范围是否在有效内存内

2. **VirtIO初始化失败**
   - 确保QEMU命令正确
   - 检查virtio设备配置

3. **内核加载卡死**
   - 可能是磁盘读取超时
   - 检查virtio队列状态

4. **设备树生成失败**
   - 检查缓冲区是否足够
   - 验证硬件检测结果

### 调试选项

在`boot_types.h`中设置`BOOT_DEBUG 1`来启用详细调试输出。

## 性能指标

- **Stage 2大小**: ~20KB (在32KB限制内)
- **内核加载速度**: ~35KB代码段在5秒内完成
- **内存使用**: 总计约12KB引导程序内存
- **设备树大小**: 224字节 (4节点)

## 兼容性

- **RISC-V标准**: 完全符合RISC-V启动约定
- **xv6兼容**: 与标准xv6内核完全兼容
- **QEMU兼容**: 支持QEMU virt机器的所有标准配置

## 下一步

Stage 3完成后，可以进入Stage 3.4：
- 完整系统验证
- 性能基准测试
- 文档完善
- 代码清理

## 开发团队

基于xv6操作系统的增强引导程序
- 原始xv6: MIT PDOS
- Stage 3增强: 现代化引导流程实现
