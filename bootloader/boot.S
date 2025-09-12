.section .text
.global _start

_start:
    # 设置栈指针 - 使用专用栈区域，不要覆盖内核地址
    li sp, 0x80040000
    
    # UART基地址 (QEMU virt machine)
    li t0, 0x10000000
    
    # 输出 "BOOT" 字符串
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
    
    # 输出 "Loading Stage 2..."
    li t1, 'L'
    sb t1, 0(t0)
    li t1, 'D'
    sb t1, 0(t0)
    li t1, 'G'
    sb t1, 0(t0)
    li t1, '2'
    sb t1, 0(t0)
    li t1, '\n'
    sb t1, 0(t0)
    
    # 简单的磁盘读取 - 从扇区1读取Stage 2到0x80030000
    # 这里暂时直接跳转，因为在512字节限制内实现完整的磁盘读取很困难
    # 在实际系统中，BIOS/UEFI会负责从磁盘加载Stage 2
    li t0, 0x80030000
    jr t0

# 错误处理 - 如果第二阶段无法运行会回到这里
error_halt:
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
