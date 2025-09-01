# Bootloader 快速开始指南

## 立即开始的最小步骤

### 第一步：创建基础目录结构

```bash
mkdir bootloader
mkdir tools
cd bootloader
```

### 第二步：创建最简单的测试版本

创建 `bootloader/boot.S`:

```assembly
.section .text
.global _start

_start:
    # 设置栈指针
    li sp, 0x80000000
    
    # UART基地址 (QEMU virt machine)
    li t0, 0x10000000
    
    # 输出 "BOOT" 字符串
    li t1, 'B'
    sb t1, 0(t0)
    li t1, 'O' 
    sb t1, 0(t0)
    li t1, 'O'
    sb t1, 0(t0) 
    li t1, 'T'
    sb t1, 0(t0)
    li t1, '\n'
    sb t1, 0(t0)
    
    # 无限循环
halt:
    wfi
    j halt
```

### 第三步：创建简单的Makefile

创建 `bootloader/Makefile`:

```makefile
CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld  
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax

boot.bin: boot.S
	$(CC) $(CFLAGS) -c boot.S -o boot.o
	$(LD) -Ttext 0x80000000 -o boot.elf boot.o
	$(OBJCOPY) -O binary boot.elf boot.bin
	@echo "Boot sector size: $$(wc -c < boot.bin) bytes"

clean:
	rm -f *.o *.elf *.bin

.PHONY: clean
```

### 第四步：修改根目录Makefile

在主项目的 `Makefile` 中添加：

```makefile
# 在现有QEMUOPTS之前添加
bootdisk.img: bootloader/boot.bin kernel/kernel fs.img
	dd if=/dev/zero of=bootdisk.img bs=1M count=64
	dd if=bootloader/boot.bin of=bootdisk.img bs=512 count=1 conv=notrunc
	dd if=kernel/kernel of=bootdisk.img bs=512 seek=64 conv=notrunc
	dd if=fs.img of=bootdisk.img bs=512 seek=2048 conv=notrunc

# 修改QEMUOPTS (注释掉原来的，添加新的)
# QEMUOPTS = -machine virt -bios none -kernel $K/kernel -m 128M -smp $(CPUS) -nographic
QEMUOPTS = -machine virt -bios none -drive file=bootdisk.img,format=raw,if=virtio -m 128M -smp $(CPUS) -nographic

# 修改qemu目标依赖
qemu: bootdisk.img
	$(QEMU) $(QEMUOPTS)

# 添加bootloader构建目标
bootloader/boot.bin:
	$(MAKE) -C bootloader
```

### 第五步：测试运行

```bash
# 构建bootloader
make bootloader/boot.bin

# 创建磁盘镜像并启动
make qemu
```

如果成功，你应该能在QEMU控制台看到 "BOOT" 输出。

## 调试技巧

### 如果没有输出

1. 检查UART地址是否正确 (QEMU virt machine 使用 0x10000000)
2. 确认RISC-V工具链已正确安装
3. 检查内存地址是否冲突

### 如果QEMU无法启动

1. 检查磁盘镜像是否正确创建
2. 确认QEMU版本支持RISC-V
3. 检查链接地址是否正确

### 使用GDB调试

```bash
# 终端1: 启动QEMU等待调试器
make qemu-gdb

# 终端2: 启动GDB
riscv64-unknown-elf-gdb bootloader/boot.elf
(gdb) target remote localhost:1234
(gdb) b _start
(gdb) c
```

## 下一步计划

一旦基础版本能够工作：

1. 添加更多串口输出功能
2. 实现简单的磁盘读取
3. 加载更大的第二阶段程序
4. 最终加载内核ELF文件

## 常见问题

**Q: 工具链在哪里下载？**
A: 可以从RISC-V官网下载预编译版本，或使用包管理器安装

**Q: 为什么选择0x80000000作为加载地址？**
A: 这是QEMU RISC-V virt machine的标准内存布局起始地址

**Q: 如何知道bootloader是否真的在运行？**
A: 通过串口输出是最直接的方式，也可以用GDB单步调试

**Q: 磁盘镜像格式有什么要求？**
A: QEMU可以识别raw格式，bootloader需要放在第一个扇区(512字节)

开始动手实现吧！有任何问题可以随时询问。
