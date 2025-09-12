        #
        # 内核中断和异常处理入口
        # 在内核态中的中断和异常来到这里。
        #
        # 与用户态陷阱的区别：
        # - 当前栈是内核栈（不需要切换栈）
        # - 不需要切换页表（已经在内核页表中）
        # - 只需要保存寄存器，调用处理函数，然后恢复
        #
        # 当前栈是内核栈。
        # 推送所有寄存器，调用 kerneltrap()。
        # 当 kerneltrap() 返回时，恢复寄存器，返回。
        #
.globl kerneltrap
.globl kernelvec
.align 4
kernelvec:
        # 内核中断/异常处理入口点
        # 为保存寄存器腾出空间。
        # 在栈上分配 256 字节空间来保存所有寄存器
        # RISC-V 有 32 个寄存器，每个 8 字节，共需要 256 字节
        addi sp, sp, -256

        # 保存所有通用寄存器到栈上
        # 这样 C 代码就可以自由使用这些寄存器
        # 保存寄存器。
        sd ra, 0(sp)
        sd sp, 8(sp)
        sd gp, 16(sp)
        sd tp, 24(sp)
        sd t0, 32(sp)
        sd t1, 40(sp)
        sd t2, 48(sp)
        sd s0, 56(sp)
        sd s1, 64(sp)
        sd a0, 72(sp)
        sd a1, 80(sp)
        sd a2, 88(sp)
        sd a3, 96(sp)
        sd a4, 104(sp)
        sd a5, 112(sp)
        sd a6, 120(sp)
        sd a7, 128(sp)
        sd s2, 136(sp)
        sd s3, 144(sp)
        sd s4, 152(sp)
        sd s5, 160(sp)
        sd s6, 168(sp)
        sd s7, 176(sp)
        sd s8, 184(sp)
        sd s9, 192(sp)
        sd s10, 200(sp)
        sd s11, 208(sp)
        sd t3, 216(sp)
        sd t4, 224(sp)
        sd t5, 232(sp)
        sd t6, 240(sp)

        # 调用 C 语言的陷阱处理函数
        # 调用 trap.c 中的 C 陷阱处理程序
        # 这个函数会识别中断类型并进行相应处理
        call kerneltrap

        # 从 C 函数返回后，恢复所有寄存器
        # 恢复寄存器。
        ld ra, 0(sp)
        ld sp, 8(sp)
        ld gp, 16(sp)
        # 特别注意：不恢复 tp（包含 hartid），以防 CPU 变更
        # tp 寄存器包含当前 CPU 核心的 ID，如果在处理过程中进程被调度到其他核心，
        # 我们不应该恢复旧的 tp 值
        ld t0, 32(sp)
        ld t1, 40(sp)
        ld t2, 48(sp)
        ld s0, 56(sp)
        ld s1, 64(sp)
        ld a0, 72(sp)
        ld a1, 80(sp)
        ld a2, 88(sp)
        ld a3, 96(sp)
        ld a4, 104(sp)
        ld a5, 112(sp)
        ld a6, 120(sp)
        ld a7, 128(sp)
        ld s2, 136(sp)
        ld s3, 144(sp)
        ld s4, 152(sp)
        ld s5, 160(sp)
        ld s6, 168(sp)
        ld s7, 176(sp)
        ld s8, 184(sp)
        ld s9, 192(sp)
        ld s10, 200(sp)
        ld s11, 208(sp)
        ld t3, 216(sp)
        ld t4, 224(sp)
        ld t5, 232(sp)
        ld t6, 240(sp)

        # 恢复栈指针，释放之前分配的 256 字节空间
        addi sp, sp, 256

        # 返回到被中断的内核代码
        # 返回到我们在内核中正在做的任何事情。
        # sret 会恢复之前的执行状态
        sret

        #
        # 机器模式定时器中断处理
        # 机器模式定时器中断。
        # 这个函数在机器模式下运行，特权级别比管理员模式更高
        #
.globl timervec
.align 4
timervec:
        # 机器模式定时器中断处理程序
        # start.c 已经设置了 mscratch 指向的内存：
        # scratch[0,8,16] : 寄存器保存区域。
        # scratch[24] : CLINT 的 MTIMECMP 寄存器地址。
        # scratch[32] : 中断之间的期望间隔。
        #
        # CLINT (Core Local Interruptor) 是 RISC-V 的定时器硬件
        # MTIMECMP 是定时器比较寄存器，当 mtime >= mtimecmp 时产生中断
        
        # 保存寄存器到 scratch 区域（机器模式下的临时存储）
        csrrw a0, mscratch, a0
        sd a1, 0(a0)
        sd a2, 8(a0)
        sd a3, 16(a0)

        # 设置下一次定时器中断
        # 通过将间隔添加到 mtimecmp 来调度下一个定时器中断。
        ld a1, 24(a0) # CLINT_MTIMECMP(hart) - 加载定时器比较寄存器地址
        ld a2, 32(a0) # interval - 加载时间间隔
        ld a3, 0(a1)  # 读取当前的 mtimecmp 值
        add a3, a3, a2 # 加上间隔，得到下一次中断时间
        sd a3, 0(a1)   # 写回 mtimecmp 寄存器

        # 触发软件中断给管理员模式处理
        # 在此处理程序返回后触发一个软件中断。
        # 这样管理员模式的内核可以处理定时器事件
        li a1, 2
        csrw sip, a1  # 设置管理员模式软件中断位

        # 恢复寄存器并返回
        ld a3, 16(a0)
        ld a2, 8(a0)
        ld a1, 0(a0)
        csrrw a0, mscratch, a0

        # 从机器模式中断返回
        mret
