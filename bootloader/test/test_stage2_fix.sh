#!/bin/bash

echo "=== Stage 2 Disk Fix Test ==="
echo "Focus: Fix virtio disk read issues"
echo

cd /home/xv6/Desktop/code/oslab/bootloader

# 构建Stage 2
echo "Building Stage 2..."
make clean
make stage1.bin stage2.bin

if [ $? -ne 0 ]; then
    echo "❌ Build failed!"
    exit 1
fi

echo "✅ Build successful"
echo "Stage 1 size: $(wc -c < stage1.bin) bytes"
echo "Stage 2 size: $(wc -c < stage2.bin) bytes"
echo

# 创建测试磁盘 - 使用相同的配置
echo "Creating test disk..."
make bootdisk_stage3.img

echo "Testing with correct virtio configuration..."
echo "Expected: boot sector read success + kernel sector read attempts"
echo

# 使用与xv6相同的virtio配置
timeout 30 qemu-system-riscv64 -machine virt -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80001000,file=stage2.bin \
    -global virtio-mmio.force-legacy=false \
    -drive file=bootdisk_stage3.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    -m 128M -nographic

echo
echo "=== Fix Test Completed ==="

if [ $? -eq 0 ]; then
    echo "✅ Test completed successfully"
else
    echo "❌ Test exited with error"
fi
