# 第二阶段详细任务分解

## 📅 时间安排总览

| 天数 | 主要任务 | 输出产物 | 验收标准 |
|------|----------|----------|----------|
| 第1天 | 代码复用与基础框架 | 基础头文件、内存管理 | 编译通过 |
| 第2天 | virtio驱动简化实现 | 磁盘I/O功能 | 读取单扇区成功 |
| 第3天 | 两阶段加载逻辑 | boot.S扩展、stage2框架 | 第二阶段能启动 |
| 第4天 | 构建系统和测试 | Makefile、测试脚本 | 完整流程工作 |
| 第5天 | 调试优化和文档 | 错误处理、使用文档 | 稳定可靠运行 |

## 📋 第1天：代码复用与基础框架

### 上午任务 (9:00-12:00)
#### 任务1.1: 分析现有kernel代码
**目标**: 理解kernel中可复用的virtio代码结构

**具体步骤**:
1. 阅读`kernel/virtio.h`，提取需要的常量定义
2. 分析`kernel/virtio_disk.c`的初始化流程
3. 识别依赖的kernel函数：`kalloc`, `memset`, `panic`等
4. 确定需要简化的部分

**输出**:
- 代码复用分析报告
- 需要替换的函数清单

#### 任务1.2: 创建bootloader基础类型
**文件**: `bootloader/boot_types.h`

**详细代码**:
```c
#ifndef BOOT_TYPES_H
#define BOOT_TYPES_H

// 基础类型定义 (复用kernel/types.h)
typedef unsigned char  uint8;
typedef unsigned short uint16;
typedef unsigned int   uint32;
typedef unsigned long  uint64;

typedef uint64 pte_t;
typedef uint64 *pagetable_t;

// bootloader专用内存布局
#define BOOTLOADER_BASE     0x80000000  // 第一阶段加载地址
#define BOOTLOADER_STAGE2   0x80001000  // 第二阶段加载地址
#define BOOTLOADER_HEAP     0x80010000  // 堆内存起始
#define BOOTLOADER_STACK    0x80020000  // 栈内存起始
#define BOOTLOADER_BUFFER   0x80030000  // I/O缓冲区

// 磁盘布局定义
#define SECTOR_SIZE         512
#define STAGE2_START_SECTOR 1      // 第二阶段起始扇区
#define STAGE2_SECTORS      64     // 第二阶段占用扇区数 (32KB)
#define KERNEL_START_SECTOR 64     // 内核起始扇区
#define KERNEL_MAX_SECTORS  1024   // 内核最大扇区数 (512KB)

// 错误码定义
#define BOOT_SUCCESS         0
#define BOOT_ERROR_DISK     -1
#define BOOT_ERROR_MEMORY   -2
#define BOOT_ERROR_FORMAT   -3
#define BOOT_ERROR_TIMEOUT  -4

// 调试开关
#define BOOT_DEBUG          1

#if BOOT_DEBUG
#define debug_print(msg) uart_puts("[DEBUG] " msg "\n")
#else
#define debug_print(msg)
#endif

#endif
```

### 下午任务 (14:00-18:00)
#### 任务1.3: 实现简化内存管理
**文件**: `bootloader/memory.c`

**详细实现**:
```c
#include "boot_types.h"

// 简单的线性分配器状态
static char *heap_current = (char *)BOOTLOADER_HEAP;
static char *heap_end = (char *)BOOTLOADER_BUFFER;
static uint64 allocated_bytes = 0;

// bootloader专用的内存分配
void *boot_alloc(int size) {
    // 8字节对齐
    size = (size + 7) & ~7;
    
    if (heap_current + size > heap_end) {
        return 0;  // 内存不足
    }
    
    void *ptr = heap_current;
    heap_current += size;
    allocated_bytes += size;
    
    return ptr;
}

// 页对齐的内存分配 (4KB)
void *boot_alloc_page(void) {
    // 确保页对齐
    uint64 addr = ((uint64)heap_current + 4095) & ~4095;
    heap_current = (char *)(addr + 4096);
    
    if (heap_current > heap_end) {
        return 0;
    }
    
    allocated_bytes += 4096;
    return (void *)addr;
}

// 获取分配统计
uint64 boot_memory_used(void) {
    return allocated_bytes;
}

// 简化的内存清零函数
void *memset(void *dst, int c, uint64 n) {
    char *cdst = (char *)dst;
    for (uint64 i = 0; i < n; i++) {
        cdst[i] = c;
    }
    return dst;
}

// 内存复制函数
void *memmove(void *dst, const void *src, uint64 n) {
    const char *csrc = src;
    char *cdst = dst;
    
    if (src < dst && csrc + n > cdst) {
        // 反向复制避免重叠
        csrc += n;
        cdst += n;
        while (n-- > 0) {
            *--cdst = *--csrc;
        }
    } else {
        // 正向复制
        for (uint64 i = 0; i < n; i++) {
            cdst[i] = csrc[i];
        }
    }
    return dst;
}
```

#### 任务1.4: UART输出功能
**文件**: `bootloader/uart.c`

**详细实现**:
```c
#include "boot_types.h"

#define UART0_BASE 0x10000000L

// 基础字符输出
void uart_putc(char c) {
    volatile uint8 *uart = (uint8 *)UART0_BASE;
    *uart = c;
}

// 字符串输出
void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// 十六进制输出 (调试用)
void uart_put_hex(uint64 val) {
    const char digits[] = "0123456789ABCDEF";
    uart_puts("0x");
    
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8 digit = (val >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            uart_putc(digits[digit]);
            started = 1;
        }
    }
}

// 数字输出
void uart_put_dec(uint64 val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    
    char buf[32];
    int i = 0;
    
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}
```

**验收标准**:
- [ ] 所有文件编译通过
- [ ] 内存分配器基本功能正确
- [ ] UART输出功能正常
- [ ] 代码风格与kernel保持一致

---

## 📋 第2天：virtio驱动实现

### 上午任务 (9:00-12:00)
#### 任务2.1: virtio数据结构定义
**文件**: `bootloader/virtio_boot.h`

**详细实现**:
```c
#ifndef VIRTIO_BOOT_H
#define VIRTIO_BOOT_H

#include "boot_types.h"

// 从kernel/virtio.h复用的常量定义
#define VIRTIO0 0x10001000

// virtio MMIO寄存器偏移
#define VIRTIO_MMIO_MAGIC_VALUE     0x000
#define VIRTIO_MMIO_VERSION         0x004
#define VIRTIO_MMIO_DEVICE_ID       0x008
#define VIRTIO_MMIO_VENDOR_ID       0x00c
#define VIRTIO_MMIO_DEVICE_FEATURES 0x010
#define VIRTIO_MMIO_DRIVER_FEATURES 0x020
#define VIRTIO_MMIO_QUEUE_SEL       0x030
#define VIRTIO_MMIO_QUEUE_NUM_MAX   0x034
#define VIRTIO_MMIO_QUEUE_NUM       0x038
#define VIRTIO_MMIO_QUEUE_READY     0x044
#define VIRTIO_MMIO_QUEUE_NOTIFY    0x050
#define VIRTIO_MMIO_INTERRUPT_STATUS 0x060
#define VIRTIO_MMIO_INTERRUPT_ACK   0x064
#define VIRTIO_MMIO_STATUS          0x070
#define VIRTIO_MMIO_QUEUE_DESC_LOW  0x080
#define VIRTIO_MMIO_QUEUE_DESC_HIGH 0x084
#define VIRTIO_MMIO_DRIVER_DESC_LOW 0x090
#define VIRTIO_MMIO_DRIVER_DESC_HIGH 0x094
#define VIRTIO_MMIO_DEVICE_DESC_LOW 0x0a0
#define VIRTIO_MMIO_DEVICE_DESC_HIGH 0x0a4

// 设备状态位
#define VIRTIO_CONFIG_S_ACKNOWLEDGE 1
#define VIRTIO_CONFIG_S_DRIVER      2
#define VIRTIO_CONFIG_S_DRIVER_OK   4
#define VIRTIO_CONFIG_S_FEATURES_OK 8

// 设备特性位
#define VIRTIO_BLK_F_RO              5
#define VIRTIO_BLK_F_SCSI            7
#define VIRTIO_BLK_F_CONFIG_WCE     11
#define VIRTIO_BLK_F_MQ             12
#define VIRTIO_F_ANY_LAYOUT         27
#define VIRTIO_RING_F_INDIRECT_DESC 28
#define VIRTIO_RING_F_EVENT_IDX     29

// 描述符数量
#define NUM 8

// virtio描述符结构 (复用kernel/virtio.h)
struct virtq_desc {
    uint64 addr;
    uint32 len;
    uint16 flags;
    uint16 next;
};

#define VRING_DESC_F_NEXT  1
#define VRING_DESC_F_WRITE 2

// 可用环结构
struct virtq_avail {
    uint16 flags;
    uint16 idx;
    uint16 ring[NUM];
    uint16 unused;
};

// 已用环条目
struct virtq_used_elem {
    uint32 id;
    uint32 len;
};

struct virtq_used {
    uint16 flags;
    uint16 idx;
    struct virtq_used_elem ring[NUM];
};

// 磁盘操作类型
#define VIRTIO_BLK_T_IN  0
#define VIRTIO_BLK_T_OUT 1

// virtio块设备请求结构
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
    
    // 请求状态跟踪
    struct {
        uint8 status;
        int in_use;
    } info[NUM];
    
    // 请求数据
    struct virtio_blk_req ops[NUM];
};

// 函数声明
int virtio_disk_boot_init(void);
int virtio_disk_read_sync(uint64 sector, void *buf);
void virtio_show_status(void);

#endif
```

#### 任务2.2: virtio设备初始化
**文件**: `bootloader/virtio_boot.c` (第一部分)

**详细实现**:
```c
#include "boot_types.h"
#include "virtio_boot.h"

// 寄存器访问宏 (复用kernel/virtio_disk.c)
#define R(r) ((volatile uint32 *)(VIRTIO0 + (r)))

static struct boot_disk disk;

int virtio_disk_boot_init(void) {
    uint32 status = 0;
    
    debug_print("Initializing virtio disk...");
    
    // 验证virtio设备存在 (复用kernel逻辑)
    if(*R(VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976) {
        debug_print("Bad virtio magic value");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(VIRTIO_MMIO_VERSION) != 2) {
        debug_print("Bad virtio version");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(VIRTIO_MMIO_DEVICE_ID) != 2) {
        debug_print("Not a virtio block device");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        debug_print("Bad virtio vendor");
        return BOOT_ERROR_DISK;
    }
    
    // 重置设备
    *R(VIRTIO_MMIO_STATUS) = 0;
    
    // 设备初始化序列 (按照virtio规范)
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(VIRTIO_MMIO_STATUS) = status;
    debug_print("Device acknowledged");
    
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(VIRTIO_MMIO_STATUS) = status;
    debug_print("Driver ready");
    
    // 特性协商 (禁用不需要的特性)
    uint32 features = *R(VIRTIO_MMIO_DEVICE_FEATURES);
    uart_puts("Device features: ");
    uart_put_hex(features);
    uart_puts("\n");
    
    features &= ~(1 << VIRTIO_BLK_F_RO);
    features &= ~(1 << VIRTIO_BLK_F_SCSI);
    features &= ~(1 << VIRTIO_BLK_F_CONFIG_WCE);
    features &= ~(1 << VIRTIO_BLK_F_MQ);
    features &= ~(1 << VIRTIO_F_ANY_LAYOUT);
    features &= ~(1 << VIRTIO_RING_F_EVENT_IDX);
    features &= ~(1 << VIRTIO_RING_F_INDIRECT_DESC);
    
    *R(VIRTIO_MMIO_DRIVER_FEATURES) = features;
    uart_puts("Driver features: ");
    uart_put_hex(features);
    uart_puts("\n");
    
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    // 验证特性协商成功
    status = *R(VIRTIO_MMIO_STATUS);
    if(!(status & VIRTIO_CONFIG_S_FEATURES_OK)) {
        debug_print("Features negotiation failed");
        return BOOT_ERROR_DISK;
    }
    
    debug_print("Features negotiated");
    return BOOT_SUCCESS;
}
```

### 下午任务 (14:00-18:00)
#### 任务2.3: 队列初始化
**文件**: `bootloader/virtio_boot.c` (第二部分)

**在virtio_disk_boot_init函数中继续添加**:
```c
    // 初始化队列0
    *R(VIRTIO_MMIO_QUEUE_SEL) = 0;
    
    // 确保队列未准备好
    if(*R(VIRTIO_MMIO_QUEUE_READY)) {
        debug_print("Queue should not be ready");
        return BOOT_ERROR_DISK;
    }
    
    // 检查队列大小
    uint32 max = *R(VIRTIO_MMIO_QUEUE_NUM_MAX);
    if(max == 0) {
        debug_print("No queue available");
        return BOOT_ERROR_DISK;
    }
    if(max < NUM) {
        debug_print("Queue too small");
        return BOOT_ERROR_DISK;
    }
    
    uart_puts("Max queue size: ");
    uart_put_dec(max);
    uart_puts("\n");
    
    // 分配队列内存 (使用页对齐分配)
    disk.desc = (struct virtq_desc *)boot_alloc_page();
    disk.avail = (struct virtq_avail *)boot_alloc_page();
    disk.used = (struct virtq_used *)boot_alloc_page();
    
    if(!disk.desc || !disk.avail || !disk.used) {
        debug_print("Queue memory allocation failed");
        return BOOT_ERROR_MEMORY;
    }
    
    // 清零队列内存
    memset(disk.desc, 0, 4096);
    memset(disk.avail, 0, 4096);
    memset(disk.used, 0, 4096);
    
    // 设置队列大小
    *R(VIRTIO_MMIO_QUEUE_NUM) = NUM;
    
    // 设置队列物理地址
    *R(VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
    *R(VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
    *R(VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
    *R(VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
    *R(VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
    *R(VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;
    
    // 标记队列准备好
    *R(VIRTIO_MMIO_QUEUE_READY) = 1;
    
    // 初始化描述符空闲标记
    for(int i = 0; i < NUM; i++) {
        disk.free[i] = 1;
        disk.info[i].in_use = 0;
    }
    
    disk.used_idx = 0;
    
    // 完成初始化
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(VIRTIO_MMIO_STATUS) = status;
    
    debug_print("Virtio disk initialized successfully");
    return BOOT_SUCCESS;
}
```

#### 任务2.4: 同步磁盘读取实现
**继续在virtio_boot.c中添加**:

```c
// 分配描述符
static int alloc_desc(void) {
    for(int i = 0; i < NUM; i++) {
        if(disk.free[i]) {
            disk.free[i] = 0;
            return i;
        }
    }
    return -1;
}

// 释放描述符
static void free_desc(int i) {
    if(i >= NUM || disk.free[i]) {
        debug_print("Invalid descriptor free");
        return;
    }
    
    disk.desc[i].addr = 0;
    disk.desc[i].len = 0;
    disk.desc[i].flags = 0;
    disk.desc[i].next = 0;
    disk.free[i] = 1;
    disk.info[i].in_use = 0;
}

// 分配三个描述符的链
static int alloc3_desc(int *idx) {
    for(int i = 0; i < 3; i++) {
        idx[i] = alloc_desc();
        if(idx[i] < 0) {
            for(int j = 0; j < i; j++) {
                free_desc(idx[j]);
            }
            return -1;
        }
    }
    return 0;
}

// 同步磁盘读取 (轮询方式，不使用中断)
int virtio_disk_read_sync(uint64 sector, void *buf) {
    int idx[3];
    
    // 分配描述符
    if(alloc3_desc(idx) != 0) {
        debug_print("No free descriptors");
        return BOOT_ERROR_DISK;
    }
    
    // 构建请求
    struct virtio_blk_req *req = &disk.ops[idx[0]];
    req->type = VIRTIO_BLK_T_IN;  // 读取
    req->reserved = 0;
    req->sector = sector;
    
    // 设置描述符链
    // 描述符0: 请求头
    disk.desc[idx[0]].addr = (uint64)req;
    disk.desc[idx[0]].len = sizeof(struct virtio_blk_req);
    disk.desc[idx[0]].flags = VRING_DESC_F_NEXT;
    disk.desc[idx[0]].next = idx[1];
    
    // 描述符1: 数据缓冲区
    disk.desc[idx[1]].addr = (uint64)buf;
    disk.desc[idx[1]].len = SECTOR_SIZE;
    disk.desc[idx[1]].flags = VRING_DESC_F_WRITE | VRING_DESC_F_NEXT;
    disk.desc[idx[1]].next = idx[2];
    
    // 描述符2: 状态字节
    disk.desc[idx[2]].addr = (uint64)&disk.info[idx[0]].status;
    disk.desc[idx[2]].len = 1;
    disk.desc[idx[2]].flags = VRING_DESC_F_WRITE;
    
    // 标记请求使用中
    disk.info[idx[0]].in_use = 1;
    disk.info[idx[0]].status = 0xFF;  // 初始状态
    
    // 提交请求到可用环
    disk.avail->ring[disk.avail->idx % NUM] = idx[0];
    disk.avail->idx++;
    
    // 通知设备
    *R(VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
    
    // 轮询等待完成 (简化版，不使用中断)
    int timeout = 1000000;  // 超时计数
    while(disk.used_idx == disk.used->idx && timeout > 0) {
        timeout--;
    }
    
    if(timeout == 0) {
        debug_print("Disk read timeout");
        free_desc(idx[0]);
        free_desc(idx[1]);
        free_desc(idx[2]);
        return BOOT_ERROR_TIMEOUT;
    }
    
    // 检查结果
    if(disk.info[idx[0]].status != 0) {
        uart_puts("Disk read error, status: ");
        uart_put_hex(disk.info[idx[0]].status);
        uart_puts("\n");
        
        free_desc(idx[0]);
        free_desc(idx[1]);
        free_desc(idx[2]);
        return BOOT_ERROR_DISK;
    }
    
    // 清理
    free_desc(idx[0]);
    free_desc(idx[1]);
    free_desc(idx[2]);
    
    disk.used_idx++;
    
    return BOOT_SUCCESS;
}

// 显示virtio状态 (调试用)
void virtio_show_status(void) {
    uart_puts("\n=== Virtio Status ===\n");
    uart_puts("Magic: ");
    uart_put_hex(*R(VIRTIO_MMIO_MAGIC_VALUE));
    uart_puts("\nVersion: ");
    uart_put_dec(*R(VIRTIO_MMIO_VERSION));
    uart_puts("\nDevice ID: ");
    uart_put_dec(*R(VIRTIO_MMIO_DEVICE_ID));
    uart_puts("\nVendor ID: ");
    uart_put_hex(*R(VIRTIO_MMIO_VENDOR_ID));
    uart_puts("\nStatus: ");
    uart_put_hex(*R(VIRTIO_MMIO_STATUS));
    uart_puts("\nQueue Ready: ");
    uart_put_dec(*R(VIRTIO_MMIO_QUEUE_READY));
    uart_puts("\n");
}
```

**验收标准**:
- [ ] virtio设备初始化成功
- [ ] 能够读取单个扇区数据
- [ ] 错误处理机制工作
- [ ] 调试输出信息完整

---

*后续第3-5天的详细任务分解将在实际实施中根据前期进展情况进行调整和完善。*
