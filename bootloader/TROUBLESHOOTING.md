# Bootloader 问题排查指南

## 常见问题及解决方案

### 启动失败问题

#### 问题1: QEMU启动后无任何输出

**现象**:

- QEMU启动但终端无输出
- 系统似乎挂起

**可能原因**:

- Stage1代码有问题
- UART初始化失败
- 栈指针设置错误

**排查步骤**:

```bash
# 1. 检查Stage1大小
ls -la stage1.bin
# 应该 ≤ 512字节

# 2. 查看Stage1反汇编
make disasm-stage1

# 3. 检查十六进制内容
make hexdump-stage1
```

**解决方案**:

- 确认 `boot.S`中栈指针设置正确: `li sp, 0x80040000`
- 验证UART地址正确: `li t0, 0x10000000`

#### 问题2: 显示"BOOT"后停止

**现象**:

- 能看到"BOOT"输出
- 但没有"LDG2"或后续输出

**可能原因**:

- Stage2地址错误
- 跳转指令问题

**排查步骤**:

```bash
# 检查跳转地址
grep "0x80030000" boot.S

# 验证Stage2是否在正确位置
qemu-system-riscv64 ... -device loader,addr=0x80030000,file=bootloader/stage2.bin
```

#### 问题3: 显示"LDG2"后停止

**现象**:

- Stage1执行完成
- Stage2未能正常启动

**可能原因**:

- Stage2汇编入口有问题
- C函数调用失败

**排查步骤**:

```bash
# 检查Stage2入口
make disasm-stage2 | head -20

# 验证栈设置
grep "sp" stage2/stage2_start.S
```

### VirtIO驱动问题

#### 问题4: VirtIO初始化失败

**现象**:

```
ERROR: Disk init failed
VirtIO initialization failed
```

**可能原因**:

- VirtIO设备未找到
- 设备特性协商失败
- 队列设置错误

**排查步骤**:

```bash
# 1. 确认QEMU参数正确
qemu-system-riscv64 ... \
  -global virtio-mmio.force-legacy=false \
  -drive file=bootloader/bootdisk_stage3.img,if=none,format=raw,id=x0 \
  -device virtio-blk-device,drive=x0,bus=virtio-mmio-bus.0

# 2. 检查设备扫描输出
# 应该看到: "Found virtio block device at 0x10001000"
```

**解决方案**:

- 确认磁盘镜像存在且可读
- 检查QEMU版本兼容性
- 验证VirtIO参数设置

#### 问题5: 磁盘读取失败

**现象**:

```
ERROR: Stage2 load failed at sector X
Disk read failed
```

**可能原因**:

- 队列描述符耗尽
- 设备未就绪
- 扇区地址错误

**调试代码**:

```c
// 在virtio_boot.c中添加调试
uart_puts("Queue status: ");
uart_put_hex(*R(virtio_base, VIRTIO_MMIO_QUEUE_READY));
uart_puts("\n");

uart_puts("Device status: ");  
uart_put_hex(*R(virtio_base, VIRTIO_MMIO_STATUS));
uart_puts("\n");
```

### 内存管理问题

#### 问题6: 内存布局验证失败

**现象**:

```
Memory layout validation: FAILED
Memory region overlap detected
```

**可能原因**:

- 内存区域定义重叠
- 内存大小计算错误

**排查步骤**:

```bash
# 检查内存布局定义
grep -E "0x8[0-9A-F]{7}" common/memory_layout.h

# 验证区域大小
grep -E "SIZE.*0x" common/memory_layout.h
```

**解决方案**:

- 确保所有内存区域不重叠
- 验证区域大小合理
- 检查对齐要求

#### 问题7: 内存分配失败

**现象**:

```
boot_alloc() returned NULL
Out of memory
```

**可能原因**:

- 堆空间不足 (64KB限制)
- 内存碎片化

**解决方案**:

```c
// 检查内存使用
uart_puts("Memory used: ");
uart_put_memsize(boot_memory_used());
uart_puts(" / 64KB\n");

// 考虑增加堆大小或优化内存使用
```

### ELF加载问题

#### 问题8: ELF魔数验证失败

**现象**:

```
Invalid ELF magic number
ELF validation failed: -1
```

**可能原因**:

- 内核文件损坏
- 文件读取不完整
- 磁盘镜像构建错误

**排查步骤**:

```bash
# 检查内核ELF文件
file kernel/kernel
readelf -h kernel/kernel

# 验证磁盘镜像中的内核
dd if=bootloader/bootdisk_stage3.img bs=512 skip=64 count=1 | hexdump -C | head
# 应该看到ELF魔数: 7f 45 4c 46
```

#### 问题9: 内核代码为0

**现象**:

```
After ELF loading - Kernel first instruction: 0x0
Source data check - first 16 bytes: 0x0 0x0 0x0 0x0
```

**可能原因**:

- 早期终止优化导致代码段未读取
- ELF偏移计算错误

**已知解决方案**:
这是一个已知问题，通过禁用早期终止优化解决:

```c
// 在main.c中注释掉早期终止逻辑
// if (elf->e_magic == ELF_MAGIC && ...) {
//     uart_puts("\nEarly termination...");
//     break;
// }
```

#### 问题10: 内核跳转后挂起

**现象**:

- ELF加载成功
- 内核指令正确
- 跳转后无输出

**可能原因**:

- 权限级别设置错误
- 栈指针冲突
- 引导信息传递错误

**排查步骤**:

```c
// 添加跳转前调试
uart_puts("Before kernel jump:\n");
uart_puts("Entry point: ");
uart_put_hex(load_info.entry_point);
uart_puts("\nKernel instruction: ");
uint32 *kernel_check = (uint32 *)load_info.entry_point;
uart_put_hex(*kernel_check);
uart_puts("\n");
```

### 构建系统问题

#### 问题11: Stage1大小超限

**现象**:

```
Stage 1 size: 600 bytes
make: *** [stage1.bin] Error 1
```

**解决方案**:

- 简化 `boot.S`代码
- 减少输出字符串
- 使用更高效的汇编指令

#### 问题12: 链接错误

**现象**:

```
undefined reference to 'function_name'
```

**解决方案**:

- 检查函数声明在头文件中
- 确认对象文件在Makefile中
- 验证函数名拼写正确

### 调试技巧

#### 通用调试方法

```c
// 1. 添加检查点
uart_puts("Checkpoint 1: Function entry\n");

// 2. 显示变量值
uart_puts("Variable X: ");
uart_put_hex(variable_x);
uart_puts("\n");

// 3. 内存转储
uint32 *ptr = (uint32 *)address;
for (int i = 0; i < 4; i++) {
    uart_put_hex(ptr[i]);
    uart_puts(" ");
}
uart_puts("\n");
```

#### GDB调试 (高级)

```bash
# 启动QEMU with GDB
qemu-system-riscv64 ... -s -S

# 另一终端连接GDB
riscv64-unknown-elf-gdb
(gdb) target remote localhost:1234
(gdb) symbol-file bootloader/stage2.elf
(gdb) break bootloader_main
(gdb) continue
```

### 性能问题

#### 启动时间过长

**可能原因**:

- 磁盘I/O效率低
- 不必要的延迟
- BSS清零耗时

**优化方案**:

- 使用批量磁盘读取
- 采用汇编优化的内存操作
- 减少调试输出

#### 内存使用过多

**监控方法**:

```c
uart_puts("Memory usage: ");
uart_put_memsize(boot_memory_used());
uart_puts(" / 64KB (");
uart_put_dec((boot_memory_used() * 100) / (64 * 1024));
uart_puts("%)\n");
```

### 健康检查清单

定期运行以下检查确保系统正常:

```bash
# 1. 构建检查
make clean && make bootdisk_stage3.img

# 2. 大小检查  
ls -la stage1.bin stage2.bin
# Stage1 ≤ 512字节, Stage2 建议 ≤ 32KB

# 3. 功能测试
timeout 30 qemu-system-riscv64 [参数...] | grep "xv6 kernel is booting"

# 4. 内存检查
grep "Memory layout validation: PASSED" [输出]

# 5. 设备检查
grep "Virtio disk initialized successfully" [输出]
```

遇到问题时，按照这个指南逐步排查，大部分问题都能快速定位和解决。
