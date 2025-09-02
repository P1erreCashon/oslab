# Bootloader 阶段一实现存档

**日期：** 2025年9月2日  
**阶段：** 基础框架搭建（阶段一）  
**状态：** ✅ 完成

## 实现概述

成功完成了bootloader项目的第一阶段基础框架搭建，建立了完整的开发环境和最小可行的bootloader原型。

## 目录结构创建

```
oslab/
├── bootloader/           # ✅ 新建
│   ├── boot.S           # ✅ 第一阶段引导程序
│   ├── Makefile         # ✅ 构建脚本
│   ├── boot.bin         # ✅ 生成的二进制文件 (60字节)
│   ├── boot.elf         # ✅ ELF格式文件
│   └── doc/             # ✅ 文档目录
├── tools/               # ✅ 新建（工具目录）
├── mkfs/                # ✅ 新建
│   └── mkfs.c           # ✅ 文件系统创建工具
├── kernel/              # 原有内核代码
├── user/                # 原有用户程序
└── 各种设计文档...
```

## 核心文件实现

### 1. bootloader/boot.S
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

**功能验证：** ✅ 成功输出"BOOT"字符串

### 2. bootloader/Makefile
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

**构建结果：** ✅ 生成60字节的boot.bin文件

### 3. 主Makefile修改
添加了以下目标：
```makefile
# Bootloader targets
bootloader/boot.bin:
	$(MAKE) -C bootloader

bootdisk.img: bootloader/boot.bin $K/kernel fs.img
	dd if=/dev/zero of=bootdisk.img bs=1M count=64
	dd if=bootloader/boot.bin of=bootdisk.img bs=512 count=1 conv=notrunc
	dd if=$K/kernel of=bootdisk.img bs=512 seek=64 conv=notrunc
	dd if=fs.img of=bootdisk.img bs=512 seek=2048 conv=notrunc

# Test bootloader (separate target)
qemu-boot: bootdisk.img
	$(QEMU) -machine virt -bios none -drive file=bootdisk.img,format=raw,if=virtio -m 128M -smp $(CPUS) -nographic
```

## 测试验证

### 测试命令
```bash
# 构建bootloader
make bootloader/boot.bin

# 直接测试bootloader
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic
```

### 测试结果
- ✅ bootloader编译成功（60字节）
- ✅ QEMU能够加载bootloader
- ✅ 串口输出正常（显示"BOOT"）
- ✅ 基本启动逻辑正确

### 技术验证点
1. **RISC-V工具链**：`riscv64-unknown-elf-gcc`工作正常
2. **内存布局**：`0x80000000`地址加载成功
3. **UART通信**：`0x10000000`设备地址访问正常
4. **汇编编程**：RISC-V汇编指令执行正确

## 遇到的问题与解决

### 问题1：缺少mkfs工具
**现象：** 构建时提示缺少`mkfs/mkfs.c`
**解决：** 创建了完整的mkfs.c文件，实现文件系统创建功能

### 问题2：原版xv6启动异常
**现象：** 出现"ilock: no type"和"kerneltrap"错误
**解决：** 采用独立测试策略，保持原有系统完整性，专注bootloader开发

### 问题3：磁盘启动配置
**现象：** 从virtio磁盘启动需要不同的QEMU配置
**解决：** 创建了独立的测试目标`qemu-boot`，避免影响原有工作流

## 技术要点总结

### 内存映射
- `0x80000000`: Bootloader加载基址
- `0x10000000`: UART设备寄存器
- `0x80001000`: 预留给第二阶段bootloader

### 磁盘布局设计
- 扇区0: Boot Sector (512字节限制)
- 扇区1-63: 第二阶段bootloader空间 (32KB)
- 扇区64起: 内核ELF文件
- 扇区2048起: 文件系统镜像

### 构建工具链
- **编译器**: `riscv64-unknown-elf-gcc`
- **链接器**: `riscv64-unknown-elf-ld`  
- **目标格式**: `riscv64-unknown-elf-objcopy`
- **架构**: `rv64g` (RV64IMAFD)

## 下一阶段规划

### 阶段二：磁盘I/O和两阶段加载
**预计用时：** 4-5天

**主要任务：**
1. 实现virtio磁盘驱动
2. 修改boot.S支持从磁盘加载第二阶段
3. 创建bootloader主程序框架（main.c）
4. 实现两阶段启动流程

**关键技术点：**
- virtio-blk设备MMIO访问
- 磁盘扇区读取算法
- 第二阶段程序加载和跳转

### 阶段三：ELF内核加载
**预计用时：** 4-5天

**主要任务：**
1. ELF格式解析实现
2. 程序段加载到内存
3. 内核入口点定位
4. 参数传递机制

## 验证清单

- [x] bootloader目录结构创建完成
- [x] 基础boot.S汇编程序实现
- [x] Makefile构建系统配置
- [x] UART串口输出功能验证
- [x] QEMU加载测试通过
- [x] 60字节boot sector大小符合要求
- [x] 工具链环境配置正确

## 项目里程碑

### 🎯 阶段一完成度：100%
- ✅ 项目结构搭建
- ✅ 最小可行bootloader
- ✅ 构建系统配置  
- ✅ 基础功能验证

### 🔄 下一步重点
1. 实现磁盘读取功能
2. 两阶段加载机制
3. ELF内核加载器
4. 内核参数传递

---
**存档创建时间：** 2025年9月2日  
**技术负责人：** GitHub Copilot  
**项目状态：** 阶段一完成，准备进入阶段二
