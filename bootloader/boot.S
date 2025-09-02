.section .text
.global _start

_start:
    # 设置栈指针
    li sp, 0x80000000
    
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
    
    # 无限循环
halt:
    wfi
    j halt
