.section .text
.global _start

_start:
    # 设置第二阶段栈
    li sp, 0x80020000
    
    # 调用C主函数
    jal ra, bootloader_main
    
    # 如果返回则停机 (正常情况不应该返回)
halt:
    wfi
    j halt
