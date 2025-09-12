        #
        # 处理从用户空间到内核的陷阱的底层代码，
        # 以及从内核返回到用户空间的代码。
        #
        # 重要概念：
        # - 陷阱(trap): 包括系统调用、中断、异常等需要从用户态转到内核态的事件
        # - TRAMPOLINE: 这是一个特殊的页面，同时映射在用户空间和内核空间的相同虚拟地址
        # - 页表切换: 从用户页表切换到内核页表的关键过程
        #
        # 内核将包含此代码的页面映射到相同的虚拟地址（TRAMPOLINE）
        # 在用户空间和内核空间中，这样当切换页表时，
        # 代码可以继续正常工作。
        # kernel.ld 使此代码从页边界开始。
        #

#include "riscv.h"
#include "memlayout.h"

.section trampsec
.globl trampoline
trampoline:
.align 4
.globl uservec
uservec:    
	#
        # 用户陷阱向量 - 所有从用户空间来的陷阱都会跳转到这里
        # trap.c 设置 stvec 指向这里，所以
        # 来自用户空间的陷阱从这里开始，
        # 处于管理员模式，但使用用户页表。
        #
        # 此时的状态：
        # - CPU 已自动切换到管理员模式(supervisor mode)
        # - 但仍在使用用户页表
        # - 用户的 PC 保存在 sepc 寄存器中
        # - 陷阱原因保存在 scause 寄存器中
        #

        # 第一步：保护用户的 a0 寄存器
        # 将用户 a0 保存在 sscratch 中，
        # 这样 a0 就可以用来访问 TRAPFRAME。
        # sscratch 是专门用于在陷阱时临时保存数据的寄存器
        csrw sscratch, a0

        # 第二步：获取 TRAPFRAME 地址
        # 每个进程都有一个单独的 p->trapframe 内存区域，
        # 但它被映射到每个进程用户页表中的相同虚拟地址（TRAPFRAME）。
        # TRAPFRAME 是一个固定的虚拟地址，用于保存/恢复寄存器
        li a0, TRAPFRAME
        
        # 第三步：保存所有用户寄存器到 TRAPFRAME
        # 这些数字(40, 48, 56...)是 TRAPFRAME 结构中各个字段的偏移量
        # 将用户寄存器保存在 TRAPFRAME 中
        sd ra, 40(a0)
        sd sp, 48(a0)
        sd gp, 56(a0)
        sd tp, 64(a0)
        sd t0, 72(a0)
        sd t1, 80(a0)
        sd t2, 88(a0)
        sd s0, 96(a0)
        sd s1, 104(a0)
        sd a1, 120(a0)
        sd a2, 128(a0)
        sd a3, 136(a0)
        sd a4, 144(a0)
        sd a5, 152(a0)
        sd a6, 160(a0)
        sd a7, 168(a0)
        sd s2, 176(a0)
        sd s3, 184(a0)
        sd s4, 192(a0)
        sd s5, 200(a0)
        sd s6, 208(a0)
        sd s7, 216(a0)
        sd s8, 224(a0)
        sd s9, 232(a0)
        sd s10, 240(a0)
        sd s11, 248(a0)
        sd t3, 256(a0)
        sd t4, 264(a0)
        sd t5, 272(a0)
        sd t6, 280(a0)

	# 第四步：保存真正的用户 a0 寄存器值
	# 将用户 a0 保存在 p->trapframe->a0 中
        # 从 sscratch 中取出之前保存的用户 a0 值
        csrr t0, sscratch
        sd t0, 112(a0)

        # 第五步：设置内核执行环境
        # 从这里开始，我们需要从 trapframe 中加载内核的执行环境
        
        # 初始化内核栈指针，来自 p->trapframe->kernel_sp
        # 每个进程都有自己的内核栈
        ld sp, 8(a0)

        # 让 tp 保存当前 hartid，来自 p->trapframe->kernel_hartid
        # hartid 是硬件线程ID，用于多核系统中识别当前CPU核心
        ld tp, 32(a0)

        # 加载 usertrap() 的地址，来自 p->trapframe->kernel_trap
        # 这是即将跳转到的 C 函数地址
        ld t0, 16(a0)

        # 第六步：切换页表
        # 获取内核页表地址，来自 p->trapframe->kernel_satp。
        # satp 寄存器控制页表的基地址
        ld t1, 0(a0)

        # 内存屏障：等待之前的内存操作完成，以便它们使用用户页表。
        # 这确保所有之前的内存访问都在页表切换前完成
        sfence.vma zero, zero

        # 关键步骤：安装内核页表。
        # 从这一刻起，所有内存访问都将使用内核页表
        csrw satp, t1

        # 刷新TLB中现在过时的用户条目。
        # TLB(Translation Lookaside Buffer)是页表缓存，需要清除旧的映射
        sfence.vma zero, zero

        # 第七步：跳转到内核 C 代码
        # 跳转到 usertrap()，它不会返回
        # 从这里开始，执行权转移给 C 代码
        jr t0

.globl userret
userret:
        # 用户返回函数 - 从内核返回到用户空间
        # userret(pagetable)
        # 由 trap.c 中的 usertrapret() 调用
        # 从内核切换到用户。
        # a0: 用户页表，用于 satp。
        #
        # 此时的状态：
        # - 我们在内核中，使用内核页表
        # - 所有内核工作已完成
        # - 需要恢复用户状态并返回用户空间
        #

        # 第一步：切换回用户页表
        # 切换到用户页表。
        # 内存屏障确保切换是原子的
        sfence.vma zero, zero
        csrw satp, a0
        sfence.vma zero, zero

        # 第二步：准备恢复用户寄存器
        # 重新加载 TRAPFRAME 地址到 a0
        li a0, TRAPFRAME

        # 第三步：恢复所有用户寄存器
        # 从 TRAPFRAME 恢复除 a0 之外的所有寄存器
        # 注意：a0 最后恢复，因为我们还需要用它来访问 TRAPFRAME
        ld ra, 40(a0)
        ld sp, 48(a0)
        ld gp, 56(a0)
        ld tp, 64(a0)
        ld t0, 72(a0)
        ld t1, 80(a0)
        ld t2, 88(a0)
        ld s0, 96(a0)
        ld s1, 104(a0)
        ld a1, 120(a0)
        ld a2, 128(a0)
        ld a3, 136(a0)
        ld a4, 144(a0)
        ld a5, 152(a0)
        ld a6, 160(a0)
        ld a7, 168(a0)
        ld s2, 176(a0)
        ld s3, 184(a0)
        ld s4, 192(a0)
        ld s5, 200(a0)
        ld s6, 208(a0)
        ld s7, 216(a0)
        ld s8, 224(a0)
        ld s9, 232(a0)
        ld s10, 240(a0)
        ld s11, 248(a0)
        ld t3, 256(a0)
        ld t4, 264(a0)
        ld t5, 272(a0)
        ld t6, 280(a0)

	# 第四步：恢复用户 a0 寄存器
	# 恢复用户 a0
        # 这是最后恢复的寄存器，因为我们需要用它来访问 TRAPFRAME
        ld a0, 112(a0)
        
        # 第五步：返回用户模式
        # 返回到用户模式和用户 pc。
        # usertrapret() 设置了 sstatus 和 sepc。
        # sret 指令会：
        # 1. 恢复之前的特权级别（用户模式）
        # 2. 跳转到 sepc 寄存器指向的地址（用户程序的下一条指令）
        # 3. 根据 sstatus 设置中断状态
        sret
