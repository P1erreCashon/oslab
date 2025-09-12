        # QEMU通过 -kernel 参数将内核加载到物理地址0x80000000
        # 并使每个硬件线程(hart，即CPU核心)跳转到此处执行
        # 链接脚本kernel.ld确保以下代码被放置在0x80000000地址
.section .text
.global _entry
_entry:
        # 为C语言代码设置栈空间
        # stack0声明在start.c中，每个CPU分配4096字节的栈空间
        # 计算公式: sp = stack0基地址 + (硬件线程ID * 4096)
        la sp, stack0        # 加载stack0的基地址到栈指针sp
        li a0, 1024*4        # 加载4096(每个CPU的栈大小)到a0
        csrr a1, mhartid     # 读取当前硬件线程ID到a1
        addi a1, a1, 1       # hartid+1(栈指针初始化到该CPU栈的栈顶)
        mul a0, a0, a1       # 计算偏移量: 4096 * (hartid+1)
        add sp, sp, a0       # 设置当前CPU的栈顶指针
        # 跳转到start.c中的start()函数继续初始化
        call start
spin:
        # 如果start()函数返回，则进入无限循环(正常情况下不会执行到这里)
        j spin
