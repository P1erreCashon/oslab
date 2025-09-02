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
