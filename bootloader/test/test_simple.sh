#!/bin/bash

echo "=== Simple Stage 2 Test ==="
cd /home/xv6/Desktop/code/oslab/bootloader

# 构建所有组件
make clean
make stage1.bin stage2.bin

# 创建简单的引导镜像，将stage2直接放在正确位置
echo "Creating bootable image with Stage 2 at correct address..."

# 创建64MB磁盘镜像
dd if=/dev/zero of=test_boot.img bs=1M count=64

# 将Stage 1放在MBR位置
dd if=stage1.bin of=test_boot.img bs=512 count=1 conv=notrunc

# 将Stage 2放在扇区1开始的位置
dd if=stage2.bin of=test_boot.img bs=512 seek=1 conv=notrunc

# 检查大小
echo "Stage 1 size: $(wc -c < stage1.bin) bytes"
echo "Stage 2 size: $(wc -c < stage2.bin) bytes"

echo "Starting QEMU with debug output..."
echo "Expected: BOOT -> LDG2 -> Stage 2 virtio messages"
echo

# 使用更明确的virtio-blk设备配置
timeout 15 qemu-system-riscv64 -machine virt -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80001000,file=stage2.bin \
    -drive file=test_boot.img,format=raw,if=none,id=hd0 \
    -device virtio-blk-device,drive=hd0 \
    -m 128M -nographic

echo "Test completed (with timeout)"
