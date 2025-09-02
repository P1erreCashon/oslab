#!/bin/bash

echo "=== Bootloader Stage 3 Complete Test ==="
echo "Date: $(date)"
echo

cd /home/xv6/Desktop/code/oslab/bootloader

# 检查依赖
echo "Checking dependencies..."

if [ ! -f ../kernel/kernel ]; then
    echo "Building xv6 kernel first..."
    cd ..
    make $K/kernel
    cd bootloader
fi

if [ ! -f ../fs.img ]; then
    echo "Building file system..."
    cd ..
    make fs.img
    cd bootloader
fi

echo "✓ Dependencies ready"
echo

# 构建Stage 3组件
echo "=== Building Stage 3 Components ==="
make clean

echo "Building stage 1..."
make stage1.bin
if [ $? -ne 0 ]; then
    echo "ERROR: Stage 1 build failed!"
    exit 1
fi

echo "Building stage 2 with ELF loader..."
make stage2.bin
if [ $? -ne 0 ]; then
    echo "ERROR: Stage 2 build failed!"
    exit 1
fi

echo "Creating Stage 3 bootdisk with real kernel..."
make bootdisk_stage3.img
if [ $? -ne 0 ]; then
    echo "ERROR: Stage 3 bootdisk creation failed!"
    exit 1
fi

# 显示构建结果
echo
echo "=== Stage 3 Build Results ==="
echo "Stage 1 size: $(wc -c < stage1.bin) bytes (limit: 512)"
echo "Stage 2 size: $(wc -c < stage2.bin) bytes (limit: 32768)"
echo "Kernel size: $(wc -c < ../kernel/kernel) bytes"
echo "Bootdisk size: $(wc -c < bootdisk_stage3.img) bytes"
echo

# 验证大小限制
stage1_size=$(wc -c < stage1.bin)
if [ $stage1_size -gt 512 ]; then
    echo "ERROR: Stage 1 exceeds 512 bytes!"
    exit 1
fi

stage2_size=$(wc -c < stage2.bin)
if [ $stage2_size -gt 32768 ]; then
    echo "WARNING: Stage 2 exceeds 32KB"
fi

echo "=== Testing Stage 3 Bootloader ==="
echo "Expected sequence:"
echo "  1. BOOT (Stage 1)"
echo "  2. LDG2 (Stage 1 -> Stage 2)"
echo "  3. === Bootloader Stage 2 === (Stage 2 start)"
echo "  4. ELF Kernel Loader messages"
echo "  5. xv6 kernel is booting"
echo "  6. init: starting sh"
echo "  7. $ (shell prompt)"
echo

echo "Press Ctrl+A X to exit QEMU when test is complete"
echo "Starting QEMU..."

# 运行完整的Stage 3测试
qemu-system-riscv64 -machine virt -bios none \
    -drive file=bootdisk_stage3.img,format=raw,if=virtio \
    -m 128M -nographic

echo
echo "=== Stage 3 Test Completed ==="

# 检查是否成功
if [ $? -eq 0 ]; then
    echo "✅ Test completed successfully"
else
    echo "❌ Test exited with error"
fi
