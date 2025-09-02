# 第二阶段文件结构规划

## 📁 目录结构设计

```
bootloader/
├── stage1/                  # 第一阶段代码 (512字节限制)
│   ├── boot.S              # ✅ 已实现 - 第一阶段汇编入口
│   └── boot_stage1.c       # 🆕 第一阶段C代码支持
├── stage2/                  # 第二阶段代码 (32KB限制)
│   ├── stage2_start.S      # 🆕 第二阶段汇编入口
│   ├── main.c              # 🆕 第二阶段主程序
│   └── stage2.ld           # 🆕 第二阶段链接脚本
├── common/                  # 共享代码模块
│   ├── boot_types.h        # 🆕 基础类型定义
│   ├── uart.c              # 🆕 串口输出功能
│   ├── memory.c            # 🆕 内存管理
│   ├── virtio_boot.h       # 🆕 virtio接口定义
│   └── virtio_boot.c       # 🆕 virtio磁盘驱动
├── test/                    # 测试代码
│   ├── test_kernel.c       # 🆕 简单测试内核
│   ├── test_kernel.ld      # 🆕 测试内核链接脚本
│   └── test_stage2.sh      # 🆕 测试脚本
├── doc/                     # 文档目录
│   ├── stage1_archive.md   # ✅ 第一阶段存档
│   ├── stage2_plan.md      # 🆕 第二阶段计划
│   ├── stage2_progress.md  # 🆕 第二阶段进度记录
│   └── api_reference.md    # 🆕 API参考文档
├── Makefile                # ✅ 已存在 - 需要扩展
└── README.md               # 🆕 bootloader模块说明
```

## 📋 文件创建计划

### 第1天需要创建的文件
```
✅ 已规划：
- bootloader/common/boot_types.h
- bootloader/common/memory.c  
- bootloader/common/uart.c
- bootloader/common/virtio_boot.h
- bootloader/common/virtio_boot.c

⏳ 计划创建：
- bootloader/stage1/boot_stage1.c
- bootloader/doc/stage2_progress.md
```

### 第2天需要创建的文件
```
⏳ 计划创建：
- bootloader/stage2/stage2_start.S
- bootloader/stage2/main.c
- bootloader/stage2/stage2.ld
- bootloader/test/test_kernel.c
```

### 第3天需要创建的文件
```
⏳ 计划创建：
- bootloader/test/test_kernel.ld
- bootloader/test/test_stage2.sh
- bootloader/README.md
- bootloader/doc/api_reference.md
```

## 🔧 构建系统规划

### 扩展后的bootloader/Makefile

```makefile
CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld  
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax
CFLAGS += -Icommon -O2 -g

# 通用模块对象文件
COMMON_OBJS = common/uart.o common/memory.o common/virtio_boot.o

# 第一阶段构建 (512字节限制)
stage1.bin: stage1/boot.S stage1/boot_stage1.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -c stage1/boot.S -o stage1/boot.o
	$(CC) $(CFLAGS) -c stage1/boot_stage1.c -o stage1/boot_stage1.o
	$(LD) -Ttext 0x80000000 -o stage1.elf \
		stage1/boot.o stage1/boot_stage1.o $(COMMON_OBJS)
	$(OBJCOPY) -O binary stage1.elf stage1.bin
	@size=$$(wc -c < stage1.bin); \
	echo "Stage 1 size: $$size bytes"; \
	if [ $$size -gt 512 ]; then \
		echo "ERROR: Stage 1 exceeds 512 bytes!"; \
		exit 1; \
	fi

# 第二阶段构建 (32KB限制)
stage2.bin: stage2/stage2_start.S stage2/main.c $(COMMON_OBJS)
	$(CC) $(CFLAGS) -c stage2/stage2_start.S -o stage2/stage2_start.o
	$(CC) $(CFLAGS) -c stage2/main.c -o stage2/main.o
	$(LD) -T stage2/stage2.ld -o stage2.elf \
		stage2/stage2_start.o stage2/main.o $(COMMON_OBJS)
	$(OBJCOPY) -O binary stage2.elf stage2.bin
	@size=$$(wc -c < stage2.bin); \
	echo "Stage 2 size: $$size bytes"; \
	if [ $$size -gt 32768 ]; then \
		echo "WARNING: Stage 2 exceeds 32KB"; \
	fi

# 测试内核构建
test/test_kernel.bin: test/test_kernel.c
	$(CC) $(CFLAGS) -c test/test_kernel.c -o test/test_kernel.o
	$(LD) -T test/test_kernel.ld -o test/test_kernel.elf test/test_kernel.o
	$(OBJCOPY) -O binary test/test_kernel.elf test/test_kernel.bin

# 完整磁盘镜像
bootdisk_stage2.img: stage1.bin stage2.bin test/test_kernel.bin
	dd if=/dev/zero of=bootdisk_stage2.img bs=1M count=64
	dd if=stage1.bin of=bootdisk_stage2.img bs=512 count=1 conv=notrunc
	dd if=stage2.bin of=bootdisk_stage2.img bs=512 seek=1 conv=notrunc
	dd if=test/test_kernel.bin of=bootdisk_stage2.img bs=512 seek=64 conv=notrunc
	@echo "Bootdisk image created: bootdisk_stage2.img"

# 测试目标
test: bootdisk_stage2.img
	@echo "=== Testing bootloader stage 2 ==="
	@echo "Use Ctrl+A X to exit QEMU"
	qemu-system-riscv64 -machine virt -bios none \
		-drive file=bootdisk_stage2.img,format=raw,if=virtio \
		-m 128M -nographic

# 调试目标
debug: bootdisk_stage2.img
	@echo "=== Debug mode - GDB server on port 1234 ==="
	qemu-system-riscv64 -machine virt -bios none \
		-drive file=bootdisk_stage2.img,format=raw,if=virtio \
		-m 128M -nographic -s -S

# 反汇编查看
disasm-stage1: stage1.bin
	$(OBJDUMP) -d stage1.elf

disasm-stage2: stage2.bin
	$(OBJDUMP) -d stage2.elf

# 十六进制查看
hexdump-stage1: stage1.bin
	hexdump -C stage1.bin | head -20

hexdump-stage2: stage2.bin  
	hexdump -C stage2.bin | head -20

# 磁盘内容查看
inspect-disk: bootdisk_stage2.img
	@echo "=== Boot Sector (first 512 bytes) ==="
	hexdump -C bootdisk_stage2.img | head -32
	@echo "=== Stage 2 Sector (sector 1) ==="
	dd if=bootdisk_stage2.img bs=512 skip=1 count=1 2>/dev/null | hexdump -C | head -32

# 清理
clean:
	rm -f stage1/*.o stage1/*.elf stage1/*.bin
	rm -f stage2/*.o stage2/*.elf stage2/*.bin
	rm -f common/*.o
	rm -f test/*.o test/*.elf test/*.bin
	rm -f *.img *.d

.PHONY: test debug clean disasm-stage1 disasm-stage2 hexdump-stage1 hexdump-stage2 inspect-disk
```

## 📊 内存布局详细规划

### 第二阶段内存映射
```
地址范围              大小      用途说明
0x80000000-0x800001FF   512B    第一阶段代码 (boot.S + C函数)
0x80000200-0x80000FFF   3.5KB   第一阶段栈空间
0x80001000-0x80008FFF   32KB    第二阶段代码空间
0x80010000-0x8001FFFF   64KB    bootloader堆内存
0x80020000-0x8002FFFF   64KB    第二阶段栈空间
0x80030000-0x8003FFFF   64KB    I/O缓冲区
0x80040000-0x80199FFF   1.4MB   预留空间
0x80200000-0x803FFFFF   2MB     内核加载区域
```

### virtio队列内存分配
```
队列类型              地址              大小      对齐
描述符表             0x80010000        4KB      页对齐
可用环               0x80011000        4KB      页对齐  
已用环               0x80012000        4KB      页对齐
请求缓冲区           0x80013000        4KB      页对齐
```

## 🧪 测试策略

### 单元测试计划
1. **内存分配器测试**: 验证boot_alloc功能
2. **UART输出测试**: 验证所有输出函数
3. **virtio初始化测试**: 验证设备识别和配置
4. **磁盘读取测试**: 验证单扇区和多扇区读取
5. **两阶段加载测试**: 验证完整启动流程

### 集成测试计划
1. **从磁盘启动测试**: 验证QEMU从virtio磁盘启动
2. **第二阶段加载测试**: 验证stage1成功加载stage2
3. **内核加载测试**: 验证stage2成功加载测试内核
4. **错误处理测试**: 验证各种错误场景的处理

### 性能测试计划
1. **启动时间测试**: 记录各阶段耗时
2. **内存使用测试**: 检查内存分配和使用情况
3. **磁盘I/O性能**: 测试读取速度和稳定性

## 🎯 成功标准

### 必须实现的功能
- [x] 第一阶段能够启动并输出调试信息
- [ ] virtio磁盘驱动能够正常初始化
- [ ] 能够从磁盘读取扇区数据
- [ ] 第一阶段能够加载并跳转到第二阶段
- [ ] 第二阶段能够显示启动信息
- [ ] 第二阶段能够读取并验证内核ELF头
- [ ] 完整的两阶段启动流程正常工作

### 期望达到的质量
- [ ] 代码结构清晰，注释详细
- [ ] 错误处理覆盖主要失败场景
- [ ] 构建系统简单易用
- [ ] 调试信息丰富有用
- [ ] 内存使用高效合理

### 扩展功能 (时间允许的话)
- [ ] 启动菜单选择功能
- [ ] 磁盘分区表解析
- [ ] 更详细的ELF加载器
- [ ] 配置文件支持

---
**规划完成时间**: 2025年9月2日  
**预期开始实施**: 2025年9月3日  
**目标完成时间**: 2025年9月7日
