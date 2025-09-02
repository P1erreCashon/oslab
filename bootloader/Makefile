CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld  
OBJCOPY = riscv64-unknown-elf-objcopy
OBJDUMP = riscv64-unknown-elf-objdump

CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax
CFLAGS += -Icommon -Os -g  # 使用-Os优化大小和速度

# 通用模块对象文件
COMMON_OBJS = common/uart.o common/memory.o common/virtio_boot.o common/elf_loader.o common/fast_mem.o common/string.o common/boot_info.o common/memory_layout.o

# 第一阶段构建 (512字节限制) - 只使用汇编
stage1.bin: boot.S
	$(CC) $(CFLAGS) -c boot.S -o stage1/boot.o
	$(LD) -Ttext 0x80000000 -o stage1.elf stage1/boot.o
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

# Stage 3 磁盘镜像 - 包含真实xv6内核
bootdisk_stage3.img: stage1.bin stage2.bin ../kernel/kernel ../fs.img
	dd if=/dev/zero of=bootdisk_stage3.img bs=1M count=64
	dd if=stage1.bin of=bootdisk_stage3.img bs=512 count=1 conv=notrunc
	dd if=stage2.bin of=bootdisk_stage3.img bs=512 seek=1 conv=notrunc
	dd if=../kernel/kernel of=bootdisk_stage3.img bs=512 seek=64 conv=notrunc
	dd if=../fs.img of=bootdisk_stage3.img bs=512 seek=2048 conv=notrunc
	@echo "Stage 3 bootdisk image created: bootdisk_stage3.img"

# C文件编译规则
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 汇编文件编译规则
%.o: %.S
	$(CC) $(CFLAGS) -c $< -o $@

# 兼容性目标 (保持与第一阶段的兼容)
boot.bin: stage1.bin
	cp stage1.bin boot.bin

# 测试目标
test: bootdisk_stage2.img
	@echo "=== Testing bootloader stage 2 ==="
	@echo "Use Ctrl+A X to exit QEMU"
	qemu-system-riscv64 -machine virt -bios none \
		-drive file=bootdisk_stage2.img,format=raw,if=virtio \
		-m 128M -nographic

# Stage 3 测试目标
test-stage3: bootdisk_stage3.img
	@echo "=== Testing bootloader stage 3 with real xv6 kernel ==="
	@echo "Expected: bootloader -> xv6 kernel -> shell prompt"
	@echo "Use Ctrl+A X to exit QEMU"
	qemu-system-riscv64 -machine virt -bios none \
		-drive file=bootdisk_stage3.img,format=raw,if=virtio \
		-m 128M -nographic

# 调试目标
debug: bootdisk_stage2.img
	@echo "=== Debug mode - GDB server on port 1234 ==="
	@echo "In another terminal run:"
	@echo "  riscv64-unknown-elf-gdb stage1.elf"
	@echo "  (gdb) target remote localhost:1234"
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
	rm -f *.img *.d boot.bin

.PHONY: test debug clean disasm-stage1 disasm-stage2 hexdump-stage1 hexdump-stage2 inspect-disk
