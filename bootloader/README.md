# RISC-V Bootloader 从零构建指南

## 项目概述

这是一个完整的两阶段RISC-V bootloader实现，能够在QEMU virt机器上启动xv6操作系统。本文档将带你从零开始，一步步构建和运行这个现代化的bootloader系统。

## 快速开始

如果你只想立即运行系统，执行以下命令：

```bash
# 进入项目目录
cd /home/xv6/Desktop/code/oslab/bootloader

# 一键构建所有组件
make clean && make bootdisk_stage3.img

# 启动系统
qemu-system-riscv64 -machine virt -cpu rv64 -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80030000,file=stage2.bin \
    -global virtio-mmio.force-legacy=false \
    -drive file=bootdisk_stage3.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    -m 128M -smp 1 -nographic
```

## 项目架构

```
bootloader/
├── common/          # 公共库和驱动
├── stage1/          # 第一阶段引导程序 (≤512字节)
├── stage2/          # 第二阶段引导程序 (VirtIO + ELF加载)
├── test/            # 测试脚本和内核
├── doc/             # 详细文档
├── README.md        # 从零构建指南 (本文档)
├── ARCHITECTURE.md  # 技术架构文档
├── DEVELOPMENT.md   # 开发指南
└── TROUBLESHOOTING.md # 问题排查手册
```

## 从零开始构建指南

### 前置条件

确保你的系统已安装：
- RISC-V交叉编译工具链 (`riscv64-unknown-elf-gcc`)
- QEMU RISC-V模拟器 (`qemu-system-riscv64`)
- 标准的构建工具 (`make`, `dd`)

### 第一步：清理环境

从一个干净的环境开始：

```bash
# 进入bootloader目录
cd /home/xv6/Desktop/code/oslab/bootloader

# 清理所有编译产物
rm -f *.o *.elf *.bin *.img
rm -f stage1/*.o stage2/*.o common/*.o

# 清理xv6内核编译产物
cd /home/xv6/Desktop/code/oslab
make clean
```

### 第二步：构建Stage1引导扇区

```bash
cd /home/xv6/Desktop/code/oslab/bootloader
make stage1.bin
```

成功标识：`Stage 1 size: 172 bytes`

### 第三步：构建Stage2引导程序

```bash
make stage2.bin
```

成功标识：`Stage 2 size: 27528 bytes`

### 第四步：构建xv6内核

```bash
cd /home/xv6/Desktop/code/oslab
make kernel/kernel
```

### 第五步：构建文件系统

```bash
make fs.img
```

成功标识：`balloc: first 746 blocks have been allocated`

### 第六步：创建引导磁盘镜像

```bash
cd bootloader
make bootdisk_stage3.img
```

成功标识：`Stage 3 bootdisk image created: bootdisk_stage3.img`

### 第七步：启动虚拟机

```bash
# 启动完整的操作系统
qemu-system-riscv64 -machine virt -cpu rv64 -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80030000,file=stage2.bin \
    -global virtio-mmio.force-legacy=false \
    -drive file=bootdisk_stage3.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    -m 128M -smp 1 -nographic
```

**启动成功输出**：
```
BOOT
Stage2: Starting VirtIO initialization
Stage2: Attempting to load kernel from disk...
Stage2: Successfully loaded and started xv6 kernel
xv6 kernel is booting

hart 0 starting
hart 1 starting  
hart 2 starting
init: starting sh
$ 
```

看到 `$ ` 提示符后，你可以输入xv6命令：
- `ls` - 列出文件
- `cat README` - 查看文件内容  
- `echo hello` - 输出文本
- `usertests` - 运行测试

**退出QEMU**：按 `Ctrl+A` 然后按 `x`

## 构建命令总结

完整的构建序列：

```bash
# 1. 清理环境
cd /home/xv6/Desktop/code/oslab/bootloader
make clean

# 2. 构建bootloader组件
make stage1.bin      # 第一阶段 (≤512字节)
make stage2.bin      # 第二阶段 (VirtIO + ELF加载)

# 3. 构建xv6系统
cd /home/xv6/Desktop/code/oslab
make kernel/kernel   # 内核
make fs.img          # 文件系统

# 4. 打包引导镜像
cd bootloader
make bootdisk_stage3.img

# 5. 运行系统 - 使用第七步中的完整命令
```

## 架构说明

### 文件组织结构

**核心启动文件**：
- `boot.S` - 第一阶段汇编引导程序 (≤512字节)
- `stage2/stage2_start.S` - 第二阶段汇编入口
- `stage2/main.c` - 第二阶段主控制器

**公共库模块**：
- `uart.c` - UART串口驱动
- `virtio_boot.c` - VirtIO块设备驱动
- `memory.c` - 动态内存分配器
- `elf_loader.c` - ELF内核加载器
- `device_tree.c` - 设备树生成器
- `error_handling.c` - 错误处理框架

### 引导流程

1. **Stage1**: 设置栈指针、UART，跳转到Stage2
2. **Stage2初始化**: 错误处理、内存布局验证
3. **硬件检测**: VirtIO设备扫描和驱动初始化
4. **系统准备**: 设备树生成、内存管理设置
5. **内核加载**: 从磁盘读取并解析ELF内核文件
6. **内核跳转**: 设置参数并移交控制权给内核

### 技术特色

- **分层架构**: 硬件抽象层、系统服务层、应用层
- **错误处理**: 统一错误码和分级处理
- **内存安全**: 严格布局和边界保护
- **标准兼容**: VirtIO、ELF64、RISC-V ABI
- **调试友好**: 详细输出和状态监控

## 常见问题

**Q: 构建失败怎么办？**
A: 检查RISC-V工具链安装，查看错误输出信息

**Q: QEMU启动后无输出？**
A: 检查QEMU参数和机器类型配置

**Q: 如何修改功能？**
A: 参考代码注释，了解各模块功能后谨慎修改

成功运行后，可以尝试运行xv6命令如 `ls`, `cat`, `echo` 等。
