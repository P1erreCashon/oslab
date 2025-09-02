#!/bin/bash

echo "=== Bootloader Stage 2 Test Script ==="
echo "Date: $(date)"
echo

# 检查工具链
echo "Checking toolchain..."
if ! command -v riscv64-unknown-elf-gcc &> /dev/null; then
    echo "ERROR: riscv64-unknown-elf-gcc not found"
    exit 1
fi

if ! command -v qemu-system-riscv64 &> /dev/null; then
    echo "ERROR: qemu-system-riscv64 not found"
    exit 1
fi

echo "✓ Toolchain OK"
echo

# 清理并构建
echo "=== Building bootloader stage 2 ==="
make clean
echo

echo "Building stage 1..."
make stage1.bin

if [ $? -ne 0 ]; then
    echo "ERROR: Stage 1 build failed!"
    exit 1
fi
echo "✓ Stage 1 built successfully"

echo "Building stage 2..."
make stage2.bin

if [ $? -ne 0 ]; then
    echo "ERROR: Stage 2 build failed!"
    exit 1
fi
echo "✓ Stage 2 built successfully"

echo "Building test kernel..."
make test/test_kernel.bin

if [ $? -ne 0 ]; then
    echo "ERROR: Test kernel build failed!"
    exit 1
fi
echo "✓ Test kernel built successfully"

echo "Creating bootdisk image..."
make bootdisk_stage2.img

if [ $? -ne 0 ]; then
    echo "ERROR: Bootdisk creation failed!"
    exit 1
fi
echo "✓ Bootdisk image created successfully"

# 显示构建结果
echo
echo "=== Build Results ==="
echo "Stage 1 size: $(wc -c < stage1.bin) bytes (limit: 512)"
echo "Stage 2 size: $(wc -c < stage2.bin) bytes (limit: 32768)"
echo "Test kernel size: $(wc -c < test/test_kernel.bin) bytes"
echo "Bootdisk size: $(wc -c < bootdisk_stage2.img) bytes"
echo

# 检查大小限制
stage1_size=$(wc -c < stage1.bin)
if [ $stage1_size -gt 512 ]; then
    echo "ERROR: Stage 1 size exceeds 512 bytes!"
    exit 1
fi

stage2_size=$(wc -c < stage2.bin)
if [ $stage2_size -gt 32768 ]; then
    echo "WARNING: Stage 2 size exceeds 32KB"
fi

echo "=== Testing bootloader ==="
echo "Starting QEMU (Press Ctrl+A X to exit)..."
echo "Expected output:"
echo "  1. 'BOOT' from stage 1"
echo "  2. Virtio initialization messages"
echo "  3. Stage 2 loading progress"
echo "  4. Test kernel success message"
echo

# 运行测试 - 直接从磁盘引导第一扇区
qemu-system-riscv64 -machine virt -bios none \
    -kernel bootdisk_stage2.img \
    -drive file=bootdisk_stage2.img,format=raw,if=virtio \
    -m 128M -nographic

echo
echo "=== Test completed ==="
