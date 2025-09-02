// virtio设备扫描函数
int scan_virtio_devices(void) {
    // QEMU virt机器有多个virtio设备地址
    uint64 virtio_addrs[] = {
        0x10001000, // virtio0
        0x10002000, // virtio1  
        0x10003000, // virtio2
        0x10004000, // virtio3
        0x10005000, // virtio4
        0x10006000, // virtio5
        0x10007000, // virtio6
        0x10008000, // virtio7
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
        
        uart_puts("Virtio device ");
        uart_put_dec(i);
        uart_puts(" at ");
        uart_put_hex(addr);
        uart_puts(":\n");
        uart_puts("  Magic: ");
        uart_put_hex(magic);
        uart_puts("\n  Version: ");
        uart_put_dec(version);
        uart_puts("\n  Device ID: ");
        uart_put_dec(device_id);
        uart_puts("\n  Vendor ID: ");
        uart_put_hex(vendor_id);
        uart_puts("\n");
        
        if(magic == 0x74726976 && (version == 1 || version == 2) && 
           vendor_id == 0x554d4551 && device_id == 2) {
            uart_puts("Found virtio block device!\n");
            return addr;
        }
    }
    
    uart_puts("No virtio block device found\n");
    return 0;
}
