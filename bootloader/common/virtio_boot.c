#include "boot_types.h"
#include "virtio_boot.h"

// 寄存器访问宏 
#define R(addr, r) ((volatile uint32 *)((addr) + (r)))

static struct boot_disk disk;
static uint64 virtio_base = 0;

// 扫描virtio设备
static uint64 scan_virtio_block_device(void) {
    // QEMU virt机器virtio设备地址范围
    uint64 virtio_addrs[] = {
        0x10001000, 0x10002000, 0x10003000, 0x10004000,
        0x10005000, 0x10006000, 0x10007000, 0x10008000,
        0
    };
    
    uart_puts("Scanning for virtio block devices...\n");
    
    for(int i = 0; virtio_addrs[i] != 0; i++) {
        uint64 addr = virtio_addrs[i];
        volatile uint32 *base = (volatile uint32 *)addr;
        
        uint32 magic = base[VIRTIO_MMIO_MAGIC_VALUE/4];
        uint32 version = base[VIRTIO_MMIO_VERSION/4];  
        uint32 device_id = base[VIRTIO_MMIO_DEVICE_ID/4];
        uint32 vendor_id = base[VIRTIO_MMIO_VENDOR_ID/4];
        
        uart_puts("  Device ");
        uart_put_dec(i);
        uart_puts(": ID=");
        uart_put_dec(device_id);
        uart_puts(", Magic=");
        uart_put_hex(magic);
        uart_puts("\n");
        
        if(magic == 0x74726976 && (version == 1 || version == 2) && 
           vendor_id == 0x554d4551 && device_id == 2) {
            uart_puts("Found virtio block device at ");
            uart_put_hex(addr);
            uart_puts("\n");
            return addr;
        }
    }
    
    uart_puts("No virtio block device found\n");
    return 0;
}

int virtio_disk_boot_init(void) {
    uint32 status = 0;
    
    debug_print("Initializing virtio disk...");
    
    // 扫描找到virtio块设备
    virtio_base = scan_virtio_block_device();
    if(virtio_base == 0) {
        debug_print("No virtio block device found");
        return BOOT_ERROR_DISK;
    }
    
    // 验证virtio设备存在
    if(*R(virtio_base, VIRTIO_MMIO_MAGIC_VALUE) != 0x74726976) {
        debug_print("Bad virtio magic value");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(virtio_base, VIRTIO_MMIO_VERSION) != 2 && *R(virtio_base, VIRTIO_MMIO_VERSION) != 1) {
        uart_puts("Bad virtio version: ");
        uart_put_dec(*R(virtio_base, VIRTIO_MMIO_VERSION));
        uart_puts(" (expected 1 or 2)\n");
        debug_print("Bad virtio version");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(virtio_base, VIRTIO_MMIO_DEVICE_ID) != 2) {
        uart_puts("Device ID: ");
        uart_put_dec(*R(virtio_base, VIRTIO_MMIO_DEVICE_ID));
        uart_puts(" (expected 2 for block device)\n");
        debug_print("Not a virtio block device");
        return BOOT_ERROR_DISK;
    }
    
    if(*R(virtio_base, VIRTIO_MMIO_VENDOR_ID) != 0x554d4551) {
        debug_print("Bad virtio vendor");
        return BOOT_ERROR_DISK;
    }
    
    // 重置设备
    *R(virtio_base, VIRTIO_MMIO_STATUS) = 0;
    
    // 设备初始化序列 (按照virtio规范)
    status |= VIRTIO_CONFIG_S_ACKNOWLEDGE;
    *R(virtio_base, VIRTIO_MMIO_STATUS) = status;
    debug_print("Device acknowledged");
    
    status |= VIRTIO_CONFIG_S_DRIVER;
    *R(virtio_base, VIRTIO_MMIO_STATUS) = status;
    debug_print("Driver ready");
    
    // 特性协商 - 版本1只需要简单协商
    uint32 features = 0;
    if(*R(virtio_base, VIRTIO_MMIO_VERSION) == 2) {
        features = *R(virtio_base, VIRTIO_MMIO_DEVICE_FEATURES);
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
        
        *R(virtio_base, VIRTIO_MMIO_DRIVER_FEATURES) = features;
    } else {
        // 版本1：跳过特性协商
        uart_puts("Virtio version 1: skipping feature negotiation\n");
    }
    
    uart_puts("Driver features: ");
    uart_put_hex(features);
    uart_puts("\n");
    
    // 添加FEATURES_OK状态
    status |= VIRTIO_CONFIG_S_FEATURES_OK;
    uart_puts("Setting status to: ");
    uart_put_hex(status);
    uart_puts("\n");
    
    *R(virtio_base, VIRTIO_MMIO_STATUS) = status;
    
    // 验证特性协商成功 - 再次读取状态
    uint32 read_status = *R(virtio_base, VIRTIO_MMIO_STATUS);
    uart_puts("Status after features: ");
    uart_put_hex(read_status);
    uart_puts("\n");
    
    if(!(read_status & VIRTIO_CONFIG_S_FEATURES_OK)) {
        uart_puts("Warning: Features bit not set, trying to continue anyway\n");
        // 对于版本1设备，可能不严格检查这个状态位
        if(*R(virtio_base, VIRTIO_MMIO_VERSION) != 1) {
            debug_print("Features negotiation failed");
            return BOOT_ERROR_DISK;
        }
    }
    
    debug_print("Features negotiated");
    
    // 初始化队列0
    *R(virtio_base, VIRTIO_MMIO_QUEUE_SEL) = 0;
    
    // 确保队列未准备好
    if(*R(virtio_base, VIRTIO_MMIO_QUEUE_READY)) {
        debug_print("Queue should not be ready");
        return BOOT_ERROR_DISK;
    }
    
    // 检查队列大小
    uint32 max = *R(virtio_base, VIRTIO_MMIO_QUEUE_NUM_MAX);
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
    
    uart_puts("Queue memory allocated:\n");
    uart_puts("  desc: ");
    uart_put_hex((uint64)disk.desc);
    uart_puts("\n  avail: ");
    uart_put_hex((uint64)disk.avail);
    uart_puts("\n  used: ");
    uart_put_hex((uint64)disk.used);
    uart_puts("\n");
    
    // 清零队列内存
    memset(disk.desc, 0, 4096);
    memset(disk.avail, 0, 4096);
    memset(disk.used, 0, 4096);
    
    // 设置队列大小
    *R(virtio_base, VIRTIO_MMIO_QUEUE_NUM) = NUM;
    
    // 设置队列物理地址
    *R(virtio_base, VIRTIO_MMIO_QUEUE_DESC_LOW) = (uint64)disk.desc;
    *R(virtio_base, VIRTIO_MMIO_QUEUE_DESC_HIGH) = (uint64)disk.desc >> 32;
    *R(virtio_base, VIRTIO_MMIO_DRIVER_DESC_LOW) = (uint64)disk.avail;
    *R(virtio_base, VIRTIO_MMIO_DRIVER_DESC_HIGH) = (uint64)disk.avail >> 32;
    *R(virtio_base, VIRTIO_MMIO_DEVICE_DESC_LOW) = (uint64)disk.used;
    *R(virtio_base, VIRTIO_MMIO_DEVICE_DESC_HIGH) = (uint64)disk.used >> 32;
    
    // 标记队列准备好
    *R(virtio_base, VIRTIO_MMIO_QUEUE_READY) = 1;
    
    // 初始化描述符空闲标记
    for(int i = 0; i < NUM; i++) {
        disk.free[i] = 1;
        disk.info[i].in_use = 0;
    }
    
    disk.used_idx = 0;
    
    // 完成初始化
    status |= VIRTIO_CONFIG_S_DRIVER_OK;
    *R(virtio_base, VIRTIO_MMIO_STATUS) = status;
    
    debug_print("Virtio disk initialized successfully");
    return BOOT_SUCCESS;
}

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
    *R(virtio_base, VIRTIO_MMIO_QUEUE_NOTIFY) = 0;
    
    // 轮询等待完成 (简化版，不使用中断)
    int timeout = 1000000;  // 超时计数
    while(disk.used_idx == disk.used->idx && timeout > 0) {
        timeout--;
        // 添加短暂延迟
        for(int i = 0; i < 100; i++) {
            asm volatile("nop");
        }
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
    
    // 使用固定地址先检查第一个设备
    uint64 addr = VIRTIO0;  // 0x10001000
    uart_puts("Magic: ");
    uart_put_hex(*R(addr, VIRTIO_MMIO_MAGIC_VALUE));
    uart_puts("\nVersion: ");
    uart_put_dec(*R(addr, VIRTIO_MMIO_VERSION));
    uart_puts("\nDevice ID: ");
    uart_put_dec(*R(addr, VIRTIO_MMIO_DEVICE_ID));
    uart_puts("\nVendor ID: ");
    uart_put_hex(*R(addr, VIRTIO_MMIO_VENDOR_ID));
    uart_puts("\nStatus: ");
    uart_put_hex(*R(addr, VIRTIO_MMIO_STATUS));
    uart_puts("\nQueue Ready: ");
    uart_put_dec(*R(addr, VIRTIO_MMIO_QUEUE_READY));
    uart_puts("\n");
}
