# Bootloader 第二阶段实施计划

**阶段名称**: 磁盘I/O与两阶段加载  
**预计时间**: 4-5天  
**目标**: 实现从磁盘启动，完成两阶段加载机制  
**基于**: 第一阶段已完成的基础框架

## 阶段概述

第二阶段将实现bootloader的核心功能：从磁盘读取数据和两阶段加载。我们将复用kernel中的virtio驱动代码，并实现一个简化但功能完整的bootloader主程序。

### 技术目标
1. **磁盘I/O功能**: 实现virtio磁盘读取
2. **两阶段加载**: boot.S加载main.c程序
3. **真正的磁盘启动**: 从virtio磁盘启动而非内核模式
4. **错误处理**: 基础的启动错误诊断

## 详细实施计划

## 任务1: 复用并简化virtio驱动代码 (第1天)

### 1.1 创建bootloader专用头文件
**文件**: `bootloader/boot_types.h`

```c
// 复用kernel中的基本类型定义
typedef unsigned char uint8;
typedef unsigned short uint16;
typedef unsigned int uint32;
typedef unsigned long uint64;

// 简化的内存分配 - bootloader使用固定内存区域
#define BOOTLOADER_BASE 0x80000000
#define BOOTLOADER_HEAP 0x80010000
#define BOOTLOADER_STACK 0x80020000

// virtio相关常量 (来自kernel/virtio.h)
#define VIRTIO0 0x10001000
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1
#define NUM 8  // virtio描述符数量

// 错误码定义
#define BOOT_SUCCESS 0
#define BOOT_ERROR_DISK -1
#define BOOT_ERROR_MEMORY -2
#define BOOT_ERROR_FORMAT -3
```

### 1.2 简化virtio数据结构
**文件**: `bootloader/virtio_boot.h`

```c
// 直接复用kernel/virtio.h中的结构体定义
// 但简化内存管理部分

struct virtq_desc {
  uint64 addr;
  uint32 len; 
  uint16 flags;
  uint16 next;
};

struct virtio_blk_req {
  uint32 type;
  uint32 reserved;
  uint64 sector;
};

// 简化的磁盘控制结构
struct boot_disk {
  struct virtq_desc *desc;
  struct virtq_avail *avail;
  struct virtq_used *used;
  char free[NUM];
  uint16 used_idx;
};
```

### 1.3 实现简化的内存管理
**文件**: `bootloader/memory.c`

```c
#include "boot_types.h"

static char *heap_ptr = (char *)BOOTLOADER_HEAP;

// 简单的内存分配器 - 只分配不回收
void *boot_alloc(int size) {
    void *ptr = heap_ptr;
    heap_ptr += (size + 7) & ~7;  // 8字节对齐
    
    // 清零内存
    char *p = (char *)ptr;
    for (int i = 0; i < size; i++) {
        p[i] = 0;
    }
    
    return ptr;
}

// 简单的memset实现
void *memset(void *dst, int c, uint len) {
    char *d = dst;
    while (len--) *d++ = c;
    return dst;
}
```

## 任务2: 实现bootloader专用virtio驱动 (第1-2天)

### 2.1 virtio设备初始化
**文件**: `bootloader/virtio_boot.c`

```c
#include "boot_types.h"
#include "virtio_boot.h"

// 复用kernel/virtio_disk.c的寄存器访问宏
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct boot_disk disk;

int virtio_disk_boot_init(void) {
    uint32 status = 0;
    
    // 验证virtio设备 (复用kernel逻辑)
    if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976 ||
       *R(VIRTIO_MMIO_VERSION) != 2 ||
       *R(VIRTIO_MMIO_DEVICE_ID) != 2 ||
       *R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551){
        return BOOT_ERROR_DISK;
    }
    
    // 设备初始化序列 (简化kernel/virtio_disk.c)
    *R(VIRTIO_MMIO_STATUS) = 0;
    
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    // 特性协商 (复用kernel的特性屏蔽逻辑)
    uint64 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    // ... (其他特性屏蔽)
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
    
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    // 队列初始化 (使用boot_alloc代替kalloc)
    disk.desc = (struct virtq_desc *)boot_alloc(4096);
    disk.avail = (struct virtq_avail *)boot_alloc(4096);
    disk.used = (struct virtq_used *)boot_alloc(4096);
    
    if (!disk.desc || !disk.avail || !disk.used) {
        return BOOT_ERROR_MEMORY;
    }
    
    // 队列配置 (复用kernel逻辑)
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
    // ... (其他队列配置)
    
    // 初始化描述符
    for(int i = 0; i < NUM; i++) {
        disk.free[i] = 1;
    }
    
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    return BOOT_SUCCESS;
}
```

### 2.2 磁盘读取功能
```c
// 简化的磁盘读取 - 同步版本，不使用中断
int virtio_disk_read_sync(uint64 sector, void *buf) {
    // 基于kernel/virtio_disk.c的virtio_disk_rw函数
    // 但简化为同步操作
    
    // 分配描述符
    int idx[3];
    if (alloc3_desc(idx) != 0) {
        return BOOT_ERROR_DISK;
    }
    
    // 构建请求 (复用kernel逻辑)
    struct virtio_blk_req *req = &disk.ops[idx[0]];
    req->type = VIRTIO_BLK_T_IN;
    req->reserved = 0;
    req->sector = sector;
    
    // 设置描述符链 (复用kernel逻辑)
    disk.desc[idx[0]].addr = (uint64)req;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];
    
    disk.desc[idx[1]].addr = (uint64)buf;
    disk.desc[idx[1]].len = 512;
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];
    
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    
    // 提交请求并等待完成 (轮询方式)
    disk.avail->ring[disk.avail->idx % NUM] = idx[0];
    disk.avail->idx++;
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
    
    // 等待完成 (简化版)
    while (disk.used_idx == disk.used->idx) {
        // 轮询等待
    }
    
    // 检查结果
    if (disk.info[idx[0]].status != 0) {
        free_chain(idx[0]);
        return BOOT_ERROR_DISK;
    }
    
    free_chain(idx[0]);
    disk.used_idx++;
    
    return BOOT_SUCCESS;
}
```

## 任务3: 扩展boot.S支持两阶段加载 (第2天)

### 3.1 修改boot.S实现磁盘加载
**文件**: `bootloader/boot.S`

```assembly
.section .text
.global _start

_start:
    # 设置栈指针
    li sp, BOOTLOADER_STACK
    
    # 初始化UART输出
    li t0, 0x10000000
    li t1, 'B'
    sb t1, 0(t0)
    li t1, 'O'
    sb t1, 0(t0)
    li t1, 'O'
    sb t1, 0(t0)
    li t1, 'T'
    sb t1, 0(t0)
    li t1, '\n'
    sb t1, 0(t0)
    
    # 调用C函数加载第二阶段
    jal ra, load_stage2
    
    # 如果加载成功，跳转到第二阶段
    li t0, BOOTLOADER_BASE + 0x1000  # 第二阶段加载地址
    jr t0
    
    # 如果失败，显示错误并停机
error_halt:
    # 输出 "ERROR"
    li t0, 0x10000000
    li t1, 'E'
    sb t1, 0(t0)
    li t1, 'R'
    sb t1, 0(t0)
    li t1, 'R'
    sb t1, 0(t0)
    li t1, 'O'
    sb t1, 0(t0)
    li t1, 'R'
    sb t1, 0(t0)
    li t1, '\n'
    sb t1, 0(t0)
    
halt:
    wfi
    j halt
```

### 3.2 添加C函数接口
**文件**: `bootloader/boot_stage1.c`

```c
#include "boot_types.h"
#include "virtio_boot.h"

// UART输出函数
void uart_putc(char c) {
    volatile char *uart = (char *)0x10000000;
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// 第二阶段加载函数
int load_stage2(void) {
    uart_puts("Loading stage2...\n");
    
    // 初始化virtio磁盘
    if (virtio_disk_boot_init() != BOOT_SUCCESS) {
        uart_puts("Disk init failed\n");
        return -1;
    }
    
    // 从扇区1开始读取32KB的第二阶段程序
    char *stage2_addr = (char *)(BOOTLOADER_BASE + 0x1000);
    
    for (int i = 0; i < 64; i++) {  // 64个扇区 = 32KB
        if (virtio_disk_read_sync(1 + i, stage2_addr + i * 512) != BOOT_SUCCESS) {
            uart_puts("Stage2 load failed\n");
            return -1;
        }
    }
    
    uart_puts("Stage2 loaded successfully\n");
    return 0;
}
```

## 任务4: 实现bootloader主程序框架 (第2-3天)

### 4.1 创建第二阶段主程序
**文件**: `bootloader/main.c`

```c
#include "boot_types.h"
#include "virtio_boot.h"

// UART函数声明
void uart_puts(const char *s);
extern int virtio_disk_read_sync(uint64 sector, void *buf);

// 简单的ELF头检查
struct elf_header {
    uint32 magic;
    uint8 class;
    uint8 data;
    uint8 version;
    uint8 pad[9];
    uint16 type;
    uint16 machine;
    uint32 version2;
    uint64 entry;
    uint64 phoff;
    uint64 shoff;
    uint32 flags;
    uint16 ehsize;
    uint16 phentsize;
    uint16 phnum;
    uint16 shentsize;
    uint16 shnum;
    uint16 shstrndx;
};

#define ELF_MAGIC 0x464C457F

// 第二阶段主函数
void bootloader_main(void) {
    uart_puts("\n=== Bootloader Stage 2 ===\n");
    uart_puts("Loading kernel...\n");
    
    // 读取内核ELF头 (从扇区64开始)
    struct elf_header elf;
    if (virtio_disk_read_sync(64, &elf) != BOOT_SUCCESS) {
        uart_puts("Failed to read kernel\n");
        goto error;
    }
    
    // 验证ELF魔数
    if (elf.magic != ELF_MAGIC) {
        uart_puts("Invalid kernel format\n");
        goto error;
    }
    
    uart_puts("Valid ELF kernel found\n");
    uart_puts("Kernel entry point: 0x");
    print_hex(elf.entry);
    uart_puts("\n");
    
    // 简单加载：直接将内核复制到0x80200000
    char *kernel_addr = (char *)0x80200000;
    int kernel_sectors = 1024;  // 假设内核不超过512KB
    
    for (int i = 0; i < kernel_sectors; i++) {
        if (virtio_disk_read_sync(64 + i, kernel_addr + i * 512) != BOOT_SUCCESS) {
            uart_puts("Kernel load failed at sector ");
            print_hex(64 + i);
            uart_puts("\n");
            goto error;
        }
        
        // 每加载64个扇区显示进度
        if ((i & 63) == 0) {
            uart_puts(".");
        }
    }
    
    uart_puts("\nKernel loaded successfully\n");
    uart_puts("Jumping to kernel...\n");
    
    // 跳转到内核入口点
    void (*kernel_entry)(void) = (void (*)(void))elf.entry;
    kernel_entry();
    
error:
    uart_puts("Boot failed, halting...\n");
    while (1) {
        asm volatile("wfi");
    }
}

// 辅助函数：打印十六进制数
void print_hex(uint64 val) {
    char digits[] = "0123456789ABCDEF";
    for (int i = 60; i >= 0; i -= 4) {
        uart_putc(digits[(val >> i) & 0xF]);
    }
}
```

### 4.2 第二阶段入口汇编
**文件**: `bootloader/stage2_start.S`

```assembly
.section .text
.global _start

_start:
    # 设置第二阶段栈
    li sp, 0x80020000
    
    # 调用C主函数
    jal ra, bootloader_main
    
    # 如果返回则停机
halt:
    wfi
    j halt
```

## 任务5: 构建系统与链接脚本 (第3天)

### 5.1 第二阶段链接脚本
**文件**: `bootloader/stage2.ld`

```ld
ENTRY(_start)

SECTIONS
{
  . = 0x80001000;
  
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

### 5.2 更新bootloader Makefile
**文件**: `bootloader/Makefile`

```makefile
CC = riscv64-unknown-elf-gcc
LD = riscv64-unknown-elf-ld  
OBJCOPY = riscv64-unknown-elf-objcopy

CFLAGS = -march=rv64g -mabi=lp64 -static -mcmodel=medany -fno-common -nostdlib -mno-relax
CFLAGS += -I. -O2

# 第一阶段目标
boot.bin: boot.S boot_stage1.o memory.o virtio_boot.o
	$(CC) $(CFLAGS) -c boot.S -o boot.o
	$(LD) -Ttext 0x80000000 -o boot.elf boot.o boot_stage1.o memory.o virtio_boot.o
	$(OBJCOPY) -O binary boot.elf boot.bin
	@echo "Stage 1 size: $$(wc -c < boot.bin) bytes"

# 第二阶段目标
stage2.bin: stage2_start.S main.o memory.o virtio_boot.o
	$(CC) $(CFLAGS) -c stage2_start.S -o stage2_start.o
	$(LD) -T stage2.ld -o stage2.elf stage2_start.o main.o memory.o virtio_boot.o
	$(OBJCOPY) -O binary stage2.elf stage2.bin
	@echo "Stage 2 size: $$(wc -c < stage2.bin) bytes"

# C文件编译
%.o: %.c
	$(CC) $(CFLAGS) -c $< -o $@

# 完整bootloader镜像
bootdisk_stage2.img: boot.bin stage2.bin
	dd if=/dev/zero of=bootdisk_stage2.img bs=1M count=64
	dd if=boot.bin of=bootdisk_stage2.img bs=512 count=1 conv=notrunc
	dd if=stage2.bin of=bootdisk_stage2.img bs=512 seek=1 conv=notrunc

clean:
	rm -f *.o *.elf *.bin *.img

.PHONY: clean
```

## 任务6: 测试与调试 (第4天)

### 6.1 创建测试脚本
**文件**: `bootloader/test_stage2.sh`

```bash
#!/bin/bash

echo "=== Building bootloader stage 2 ==="
make clean
make bootdisk_stage2.img

if [ $? -eq 0 ]; then
    echo "=== Testing bootloader ==="
    echo "Starting QEMU (Ctrl+A X to exit)..."
    qemu-system-riscv64 -machine virt -bios none \
        -drive file=bootdisk_stage2.img,format=raw,if=virtio \
        -m 128M -nographic
else
    echo "Build failed!"
    exit 1
fi
```

### 6.2 集成到主Makefile
在主项目`Makefile`中添加：

```makefile
# 第二阶段bootloader目标
bootloader/stage2.bin:
	$(MAKE) -C bootloader stage2.bin

bootdisk_stage2.img: bootloader/boot.bin bootloader/stage2.bin
	$(MAKE) -C bootloader bootdisk_stage2.img

# 测试第二阶段bootloader
qemu-stage2: bootdisk_stage2.img
	qemu-system-riscv64 -machine virt -bios none \
		-drive file=bootloader/bootdisk_stage2.img,format=raw,if=virtio \
		-m 128M -nographic
```

## 任务7: 创建简单内核用于测试 (第4-5天)

### 7.1 测试内核
**文件**: `bootloader/test_kernel.c`

```c
// 简单的测试内核，验证bootloader是否工作
void test_kernel_main(void) {
    volatile char *uart = (char *)0x10000000;
    
    char *msg = "\n=== TEST KERNEL LOADED ===\n";
    while (*msg) {
        *uart = *msg++;
    }
    
    char *success = "Bootloader Stage 2 SUCCESS!\n";
    while (*success) {
        *uart = *success++;
    }
    
    // 无限循环
    while (1) {
        asm volatile("wfi");
    }
}
```

### 7.2 测试内核链接脚本
**文件**: `bootloader/test_kernel.ld`

```ld
ENTRY(test_kernel_main)

SECTIONS
{
  . = 0x80200000;
  
  .text : {
    *(.text)
  }
}
```

## 验收标准

### 功能验收
- [ ] bootloader能够从virtio磁盘启动
- [ ] 第一阶段能够加载第二阶段
- [ ] 第二阶段能够读取并加载测试内核
- [ ] 两阶段启动流程完整工作
- [ ] 基本的错误诊断功能

### 性能指标
- [ ] 第一阶段代码 < 512字节
- [ ] 第二阶段代码 < 32KB  
- [ ] 启动时间 < 5秒
- [ ] 内存占用 < 1MB

### 技术指标
- [ ] virtio磁盘驱动正常工作
- [ ] ELF格式基本验证
- [ ] 串口输出调试信息完整
- [ ] 错误处理覆盖主要失败场景

## 风险缓解策略

### 高风险项目
1. **virtio驱动复杂性**: 采用kernel代码复用策略
2. **内存布局冲突**: 详细的内存地址规划
3. **链接脚本错误**: 分阶段验证每个组件

### 应对措施
1. **充分复用已验证的kernel代码**
2. **分步骤验证，每个组件独立测试**
3. **完善的调试输出和错误处理**
4. **详细的构建和测试文档**

## 下一阶段预览

第三阶段将实现完整的ELF内核加载器，包括：
- 程序段解析和加载
- 内存映射管理
- 内核参数传递
- 与原版xv6的完全兼容

---
**文档版本**: v1.0  
**创建时间**: 2025年9月2日  
**预期完成**: 2025年9月6日
