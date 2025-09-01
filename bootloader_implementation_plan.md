# Bootloader 实现详细计划

## 实现优先级与时间安排

### 阶段一：基础框架搭建 (3-4天)

#### 任务1：创建项目结构
- 创建 `bootloader/` 目录
- 设置基本的Makefile
- 创建必要的头文件

#### 任务2：实现最简单的Boot Sector
```assembly
# bootloader/boot.S - 最小可行版本
.section .text
.global _start

_start:
    # 设置栈指针
    li sp, 0x80000000
    
    # 打印启动消息
    li a0, 0x10000000  # UART base address
    li a1, 'B'
    sb a1, 0(a0)       # 写入UART
    
    # 无限循环（暂时）
halt:
    j halt
```

#### 任务3：修改Makefile支持bootloader构建
```makefile
# 在主Makefile中添加
bootloader/boot.bin: bootloader/boot.S
	$(CC) $(CFLAGS) -c bootloader/boot.S -o bootloader/boot.o
	$(LD) -Ttext 0x80000000 -o bootloader/boot.elf bootloader/boot.o
	$(OBJCOPY) -O binary bootloader/boot.elf bootloader/boot.bin
```

### 阶段二：磁盘启动支持 (4-5天)

#### 任务4：实现磁盘镜像创建
```bash
# 创建启动磁盘脚本 tools/create_bootdisk.sh
#!/bin/bash
dd if=/dev/zero of=bootdisk.img bs=512 count=2048
dd if=bootloader/boot.bin of=bootdisk.img bs=512 count=1 conv=notrunc
```

#### 任务5：修改QEMU启动参数
```makefile
# 修改QEMUOPTS
QEMUOPTS = -machine virt -bios none -drive file=bootdisk.img,format=raw,if=virtio -nographic
```

#### 任务6：实现UART串口输出
```c
// bootloader/uart.c - 串口驱动
#define UART_BASE 0x10000000

void uart_putc(char c) {
    volatile char *uart = (char *)UART_BASE;
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}
```

### 阶段三：两阶段加载实现 (5-6天)

#### 任务7：实现virtio磁盘驱动
```c
// bootloader/virtio.c - 简化的virtio驱动
#define VIRTIO_MMIO_BASE 0x10001000

struct virtio_blk_req {
    uint32_t type;
    uint32_t reserved;
    uint64_t sector;
    char data[512];
    uint8_t status;
};

int virtio_disk_read(uint64_t sector, void *buf) {
    // 实现virtio磁盘读取
    // 这里需要按照virtio规范实现
}
```

#### 任务8：Boot Sector加载第二阶段
```assembly
# bootloader/boot.S 完整版
_start:
    # 初始化环境
    li sp, 0x80000000
    
    # 初始化串口
    call uart_init
    
    # 打印启动消息
    la a0, msg_boot
    call uart_puts
    
    # 从磁盘扇区1读取bootloader主程序
    li a0, 1                    # 扇区号
    li a1, 0x80001000          # 加载地址
    call disk_read
    
    # 跳转到主程序
    li t0, 0x80001000
    jr t0

msg_boot: .string "xv6 boot sector loading...\n"
```

#### 任务9：Bootloader主程序框架
```c
// bootloader/main.c
#include "boot.h"

void bootloader_main(void) {
    uart_puts("xv6 bootloader stage 2 starting...\n");
    
    // 内存检测
    uint64_t memory_size = detect_memory();
    uart_printf("Memory detected: %d MB\n", memory_size / (1024*1024));
    
    // 加载内核
    uart_puts("Loading kernel...\n");
    if (load_kernel() < 0) {
        uart_puts("Kernel load failed!\n");
        halt();
    }
    
    // 跳转到内核
    uart_puts("Starting kernel...\n");
    jump_to_kernel();
}
```

### 阶段四：ELF内核加载 (4-5天)

#### 任务10：ELF解析实现
```c
// bootloader/elf.c
typedef struct {
    uint32_t magic;
    uint8_t  ident[12];
    uint16_t type;
    uint16_t machine;
    uint32_t version;
    uint64_t entry;
    uint64_t phoff;
    uint64_t shoff;
    uint32_t flags;
    uint16_t ehsize;
    uint16_t phentsize;
    uint16_t phnum;
} elf_header_t;

int load_elf_kernel(void *elf_data) {
    elf_header_t *elf = (elf_header_t *)elf_data;
    
    // 检查ELF魔数
    if (elf->magic != 0x464c457f) {
        return -1;
    }
    
    // 加载程序段
    program_header_t *ph = (program_header_t *)((char *)elf + elf->phoff);
    for (int i = 0; i < elf->phnum; i++) {
        if (ph[i].type == PT_LOAD) {
            memcpy((void *)ph[i].paddr, 
                   (char *)elf + ph[i].offset, 
                   ph[i].filesz);
        }
    }
    
    return elf->entry;
}
```

#### 任务11：从文件系统加载内核
```c
// bootloader/fs.c - 简化的文件系统支持
struct superblock {
    uint32_t magic;
    uint32_t size;
    uint32_t nblocks;
    uint32_t ninodes;
};

int load_kernel_from_fs(void) {
    // 读取超级块
    struct superblock sb;
    disk_read(1, &sb, 1);
    
    if (sb.magic != FSMAGIC) {
        return -1;
    }
    
    // 查找kernel文件的inode
    struct inode *kernel_inode = find_file("kernel");
    if (!kernel_inode) {
        return -1;
    }
    
    // 读取kernel文件内容
    char *kernel_data = malloc(kernel_inode->size);
    read_inode_data(kernel_inode, kernel_data);
    
    // 解析并加载ELF
    return load_elf_kernel(kernel_data);
}
```

### 阶段五：内核适配与参数传递 (3-4天)

#### 任务12：修改内核入口点
```assembly
# kernel/entry.S 修改
_entry:
    # 检查是否由bootloader启动
    # bootloader会在a3寄存器放置特殊标识
    li t0, 0xDEADBEEF
    bne a3, t0, qemu_direct_boot
    
bootloader_boot:
    # 保存bootloader传递的参数
    # a0: hart id
    # a1: memory size  
    # a2: device tree address
    la t1, boot_info
    sd a0, 0(t1)   # hart_id
    sd a1, 8(t1)   # memory_size
    sd a2, 16(t1)  # device_tree_addr
    j setup_stack

qemu_direct_boot:
    # 原有的直接启动逻辑
    csrr a0, mhartid
    
setup_stack:
    # 设置栈并继续
    la sp, stack0
    li a0, 1024*4
    csrr a1, mhartid
    addi a1, a1, 1
    mul a0, a0, a1
    add sp, sp, a0
    call start
```

#### 任务13：内核启动信息处理
```c
// kernel/main.c 修改
struct boot_info {
    uint64_t hart_id;
    uint64_t memory_size;
    uint64_t device_tree_addr;
};

extern struct boot_info boot_info;

void main() {
    if(cpuid() == 0) {
        consoleinit();
        printfinit();
        printf("\n");
        
        if (boot_info.memory_size > 0) {
            printf("xv6 kernel loaded by bootloader\n");
            printf("Memory size: %d MB\n", boot_info.memory_size / (1024*1024));
        } else {
            printf("xv6 kernel loaded by QEMU\n");
        }
        
        printf("\n");
        
        // 使用检测到的内存大小初始化
        kinit();
        // ... 其余初始化
    }
    
    scheduler();
}
```

### 阶段六：测试与优化 (2-3天)

#### 任务14：添加错误处理和调试信息
```c
// bootloader/debug.c
void panic(const char *msg) {
    uart_puts("BOOTLOADER PANIC: ");
    uart_puts(msg);
    uart_puts("\n");
    
    // 打印调试信息
    uart_printf("PC: 0x%lx\n", read_pc());
    uart_printf("SP: 0x%lx\n", read_sp());
    
    // 停止执行
    while(1) {
        asm volatile("wfi");
    }
}

void debug_print_memory_map(void) {
    uart_puts("Memory Map:\n");
    uart_printf("  Boot sector: 0x80000000 - 0x80000200\n");
    uart_printf("  Bootloader:  0x80001000 - 0x80010000\n");
    uart_printf("  Kernel:      0x80010000 - 0x80200000\n");
    uart_printf("  Free memory: 0x80200000 - 0x88000000\n");
}
```

#### 任务15：性能优化
- 优化磁盘读取性能（批量读取）
- 减少bootloader大小
- 优化启动时间

## 关键实现细节

### 链接脚本设计

```ld
/* bootloader/boot.ld */
OUTPUT_ARCH("riscv")
ENTRY(_start)

SECTIONS {
    . = 0x80000000;
    
    .text : {
        *(.text)
    }
    
    .data : {
        *(.data)
    }
    
    .bss : {
        *(.bss)
    }
}
```

### 磁盘布局设计

```
扇区0:     Boot Sector (512字节)
扇区1-63:  Bootloader主程序 (最大32KB)
扇区64起:  内核ELF文件
扇区2048起: 文件系统镜像
```

### 调试策略

1. **串口输出调试**：在关键点添加调试信息
2. **GDB调试**：设置断点调试bootloader
3. **内存转储**：出错时转储关键内存区域
4. **单步执行**：逐步验证每个功能模块

## 验收标准

### 基本功能
- [ ] 能够从磁盘启动显示bootloader信息
- [ ] 能够加载并启动内核
- [ ] 内核能够正常运行用户程序
- [ ] 串口输出正常工作

### 高级功能  
- [ ] 内存大小正确检测和报告
- [ ] ELF内核正确解析和加载
- [ ] 错误情况下有适当的错误信息
- [ ] 启动时间在合理范围内（<5秒）

### 兼容性
- [ ] 与原有内核功能完全兼容
- [ ] 支持QEMU virtio设备
- [ ] 支持多核启动

这个实现计划提供了详细的步骤指导和代码示例，可以按照这个计划逐步实现bootloader功能。
