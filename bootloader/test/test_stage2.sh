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
echo "Expected output sequence:"
echo "  1. 'BOOT' from stage 1"
echo "  2. 'LDG2' (Stage 1 -> Stage 2)"
echo "  3. '=== Bootloader Stage 2 ===' (Stage 2 start)"
echo "  4. Memory layout validation"
echo "  5. VirtIO initialization messages"
echo "  6. Hardware detection and device tree"
echo "  7. Kernel loading progress (may timeout at sector 73)"
echo
echo "Starting QEMU test (10 second timeout)..."
echo "----------------------------------------"

# 运行测试 - 使用正确的Stage 2配置，输出到tty
{ timeout 10 qemu-system-riscv64 -machine virt -bios none \
    -kernel stage1.bin \
    -device loader,addr=0x80030000,file=stage2.bin \
    -global virtio-mmio.force-legacy=false \
    -drive file=bootdisk_stage2.img,if=none,format=raw,id=x0 \
    -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0 \
    -m 128M -nographic 2>&1; } | tee /dev/tty

echo "----------------------------------------"

echo
echo "=== Test completed ==="
