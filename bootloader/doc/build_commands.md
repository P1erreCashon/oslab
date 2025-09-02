# 构建命令记录

## 本次构建过程中使用的所有命令

### 项目结构创建
```bash
mkdir bootloader
mkdir tools  
mkdir mkfs
```

### Bootloader构建
```bash
# 进入bootloader目录构建
make -C bootloader

# 或直接构建
make bootloader/boot.bin
```

### 测试命令
```bash
# 直接测试bootloader（推荐）
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic

# 构建磁盘镜像
make bootdisk.img

# 从磁盘启动测试（待实现磁盘读取功能）
make qemu-boot
```

### 清理命令
```bash
# 清理所有构建文件
make clean

# 只清理bootloader
make -C bootloader clean
```

### 调试命令
```bash
# 使用GDB调试bootloader
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic -s -S &
riscv64-unknown-elf-gdb bootloader/boot.elf
(gdb) target remote localhost:1234
(gdb) b _start
(gdb) c
```

## 构建输出示例

### 成功的bootloader构建
```
make[1]: 进入目录"/home/xv6/Desktop/code/oslab/bootloader"
riscv64-unknown-elf-gcc -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax -c boot.S -o boot.o
riscv64-unknown-elf-ld -Ttext 0x80000000 -o boot.elf boot.o
riscv64-unknown-elf-objcopy -O binary boot.elf boot.bin
Boot sector size: 60 bytes
make[1]: 离开目录"/home/xv6/Desktop/code/oslab/bootloader"
```

### 成功的QEMU测试
```
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic
BOOT
```

## 工具链信息

### 必需工具
- `riscv64-unknown-elf-gcc` - RISC-V交叉编译器
- `riscv64-unknown-elf-ld` - RISC-V链接器
- `riscv64-unknown-elf-objcopy` - 二进制转换工具
- `qemu-system-riscv64` - RISC-V模拟器

### 编译标志
```makefile
CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax
```

### 链接地址
```makefile
-Ttext 0x80000000  # bootloader加载地址
```

## 下一阶段准备

### 需要实现的功能
1. **virtio磁盘驱动** - 从磁盘读取数据
2. **两阶段加载** - boot.S加载main.c
3. **ELF解析器** - 解析内核格式
4. **内存管理** - 简单的内存分配

### 预期文件结构
```
bootloader/
├── boot.S           # ✅ 已实现
├── main.c           # 🔄 下一阶段
├── boot.h           # 🔄 下一阶段  
├── boot.ld          # 🔄 下一阶段
├── elf.h            # 🔄 下一阶段
└── virtio.c         # 🔄 下一阶段
```
