/*
 * trap.c - xv6 陷阱处理系统
 *
 * 陷阱处理架构概述：
 * 
 * 1. 用户空间陷阱流程：
 *    用户程序 -> (陷阱发生) -> trampoline.S:uservec -> trap.c:usertrap() 
 *    -> (处理) -> trap.c:usertrapret() -> trampoline.S:userret -> 用户程序
 *
 * 2. 内核空间陷阱流程：
 *    内核代码 -> (中断发生) -> kernelvec.S:kernelvec -> trap.c:kerneltrap() 
 *    -> (处理) -> kernelvec.S -> 内核代码
 *
 * 3. 关键概念：
 *    - 陷阱(trap): 系统调用、中断、异常的统称
 *    - 页表切换: 用户页表 <-> 内核页表
 *    - 特权级切换: 用户模式 <-> 管理员模式
 *    - 寄存器保存/恢复: 在 trapframe 或栈上
 *
 * 4. 重要数据结构：
 *    - trapframe: 保存用户态寄存器和内核信息
 *    - trampoline: 同时映射在用户空间和内核空间的代码页
 */

#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

// 全局时钟变量，用于系统定时
struct spinlock tickslock;  // 保护 ticks 变量的自旋锁
uint ticks;                 // 系统启动以来的时钟滴答数

// 外部汇编函数声明
extern char trampoline[], uservec[], userret[];

// 在 kernelvec.S 中，调用 kerneltrap()。
// 内核中断向量函数，处理内核态的中断和异常
void kernelvec();

extern int devintr();

// 陷阱初始化函数
// 在系统启动时调用，初始化陷阱相关的数据结构
void
trapinit(void)
{
  // 初始化时钟锁，用于保护全局时钟变量
  initlock(&tickslock, "time");
}

// 设置在内核中接受异常和陷阱。
// 每个 CPU 核心都需要调用这个函数来设置陷阱处理
void
trapinithart(void)
{
  // 设置 stvec 寄存器指向 kernelvec 函数
  // 这样所有在内核态发生的陷阱都会跳转到 kernelvec
  w_stvec((uint64)kernelvec);
}

//
// 用户陷阱处理函数 - 处理所有从用户空间来的陷阱
// 处理来自用户空间的中断、异常或系统调用。
// 从 trampoline.S 调用
//
// 陷阱类型包括：
// 1. 系统调用（ecall 指令）
// 2. 页错误
// 3. 非法指令
// 4. 设备中断（如定时器、键盘、磁盘等）
//
void
usertrap(void)
{
  int which_dev = 0;  // 用于标识设备中断类型

  // 安全检查：确保陷阱确实来自用户模式
  // SSTATUS_SPP 位表示先前的特权级别，0=用户模式，1=管理员模式
  if((r_sstatus() & SSTATUS_SPP) != 0)
    panic("usertrap: not from user mode");

  // 重要：重新设置陷阱向量
  // 将中断和异常发送到 kerneltrap()，
  // 因为我们现在在内核中。
  // 如果在处理用户陷阱时发生内核陷阱（如定时器中断），
  // 应该由 kerneltrap 而不是 usertrap 处理
  w_stvec((uint64)kernelvec);

  struct proc *p = myproc();  // 获取当前进程
  
  // 保存用户程序计数器。
  // sepc 寄存器包含发生陷阱时的 PC 值
  p->trapframe->epc = r_sepc();
  
  // 判断陷阱类型并分别处理
  if(r_scause() == 8){
    // 系统调用处理
    // scause == 8 表示这是一个来自用户模式的 ecall 指令

    // 检查进程是否被标记为需要杀死
    if(killed(p))
      exit(-1);

    // 重要：调整返回地址
    // sepc 指向 ecall 指令，
    // 但我们想返回到下一条指令。
    // RISC-V 指令都是 4 字节，所以 +4
    p->trapframe->epc += 4;

    // 启用中断
    // 中断会改变 sepc、scause 和 sstatus，
    // 所以只有在我们完成这些寄存器的操作后才启用中断。
    intr_on();

    // 调用系统调用处理函数
    syscall();
  } else if((which_dev = devintr()) != 0){
    // 设备中断处理
    // devintr() 返回非零值表示这是一个设备中断
    // 正常
  } else {
    // 未知陷阱类型 - 这通常表示程序错误
    printf("usertrap(): unexpected scause %p pid=%d\n", r_scause(), p->pid);
    printf("            sepc=%p stval=%p\n", r_sepc(), r_stval());
    setkilled(p);  // 标记进程需要被杀死
  }

  // 检查进程是否在陷阱处理过程中被杀死
  if(killed(p))
    exit(-1);

  // 进程调度检查
  // 如果这是定时器中断，则让出 CPU。
  // which_dev == 2 表示定时器中断
  // 这是实现抢占式多任务的关键机制
  if(which_dev == 2)
    yield();  // 让出 CPU，调度其他进程

  // 返回用户空间
  usertrapret();
}

//
// 用户陷阱返回函数 - 从内核返回到用户空间
// 返回到用户空间
//
// 这个函数的主要任务：
// 1. 设置用户态的陷阱向量
// 2. 准备 trapframe 中的内核信息
// 3. 设置用户态的特权级别和中断状态
// 4. 切换到用户页表并恢复用户寄存器
//
void
usertrapret(void)
{
  struct proc *p = myproc();

  // 关闭中断，防止在准备过程中被打断
  // 我们即将将陷阱的目标从 kerneltrap() 切换到 usertrap()，
  // 所以关闭中断直到我们回到用户空间，在那里 usertrap() 是正确的。
  intr_off();

  // 设置用户态陷阱向量
  // 将系统调用、中断和异常发送到 trampoline.S 中的 uservec
  // 计算 uservec 在 trampoline 页面中的实际地址
  uint64 trampoline_uservec = TRAMPOLINE + (uservec - trampoline);
  w_stvec(trampoline_uservec);

  // 准备 trapframe，为下次用户陷阱做准备
  // 设置 uservec 在进程下次陷入内核时需要的 trapframe 值。
  p->trapframe->kernel_satp = r_satp();         // 内核页表
  p->trapframe->kernel_sp = p->kstack + PGSIZE; // 进程的内核栈
  p->trapframe->kernel_trap = (uint64)usertrap; // 用户陷阱处理函数地址
  p->trapframe->kernel_hartid = r_tp();         // cpuid() 的 hartid

  // 设置处理器状态，准备返回用户模式
  // 设置 trampoline.S 的 sret 将用来进入用户空间的寄存器。
  
  // 将 S 先前特权模式设置为用户。
  unsigned long x = r_sstatus();
  x &= ~SSTATUS_SPP; // 将 SPP 清零，表示用户模式
  x |= SSTATUS_SPIE; // 在用户模式下启用中断
  w_sstatus(x);

  // 设置返回地址
  // 将 S 异常程序计数器设置为保存的用户 pc。
  // 用户程序将从这个地址继续执行
  w_sepc(p->trapframe->epc);

  // 准备用户页表
  // 告诉 trampoline.S 要切换到的用户页表。
  uint64 satp = MAKE_SATP(p->pagetable);

  // 最后一步：跳转到 trampoline 代码完成用户空间切换
  // 跳转到内存顶部 trampoline.S 中的 userret，
  // 它切换到用户页表、恢复用户寄存器并通过 sret 切换到用户模式。
  uint64 trampoline_userret = TRAMPOLINE + (userret - trampoline);
  ((void (*)(uint64))trampoline_userret)(satp);
}

// 内核陷阱处理函数
// 来自内核代码的中断和异常通过 kernelvec 到达这里，
// 在当前内核栈上。
//
// 与用户陷阱的区别：
// - 不需要保存用户状态（已经在内核中）
// - 不需要切换页表
// - 主要处理设备中断和内核错误
//
void 
kerneltrap()
{
  int which_dev = 0;
  
  // 保存重要的处理器状态寄存器
  // 这些可能在中断处理过程中被修改
  uint64 sepc = r_sepc();     // 异常程序计数器
  uint64 sstatus = r_sstatus(); // 状态寄存器
  uint64 scause = r_scause();   // 异常原因
  
  // 安全检查：确保我们在管理员模式
  if((sstatus & SSTATUS_SPP) == 0)
    panic("kerneltrap: not from supervisor mode");
  // 确保中断被禁用（内核中断处理时不应该嵌套中断）
  if(intr_get() != 0)
    panic("kerneltrap: interrupts enabled");

  // 处理设备中断
  if((which_dev = devintr()) == 0){
    // 如果不是设备中断，那就是内核错误
    printf("scause %p\n", scause);
    printf("sepc=%p stval=%p\n", r_sepc(), r_stval());
    panic("kerneltrap");
  }

  // 内核中的进程调度
  // 如果这是定时器中断，则让出 CPU。
  // 允许在内核执行过程中进行进程切换
  if(which_dev == 2 && myproc() != 0 && myproc()->state == RUNNING)
    yield();

  // 恢复处理器状态
  // yield() 可能导致一些陷阱发生，
  // 所以恢复陷阱寄存器供 kernelvec.S 的 sepc 指令使用。
  w_sepc(sepc);
  w_sstatus(sstatus);
}

// 时钟中断处理函数
// 由定时器中断调用，用于更新系统时间
void
clockintr()
{
  acquire(&tickslock);  // 获取锁，保护全局变量
  ticks++;              // 增加时钟计数
  wakeup(&ticks);       // 唤醒等待时钟的进程
  release(&tickslock);  // 释放锁
}

// 设备中断处理函数
// 检查是否是外部中断或软件中断，
// 并处理它。
// 如果是定时器中断则返回 2，
// 如果是其他设备则返回 1，
// 如果不识别则返回 0。
//
// RISC-V 中断分类：
// - 外部中断：来自外部设备（UART、磁盘等）
// - 软件中断：由软件触发，常用于核间通信
// - 定时器中断：由定时器硬件产生
//
int
devintr()
{
  printf("devintr: scause %p\n", r_scause());
  uint64 scause = r_scause();

  // 检查是否是外部中断
  // scause 的最高位表示是否是中断（1）还是异常（0）
  // 低8位表示中断/异常的具体类型
  if((scause & 0x8000000000000000L) &&
     (scause & 0xff) == 9){
    // 这是一个管理员外部中断，通过 PLIC。
    // PLIC (Platform-Level Interrupt Controller) 管理外部设备中断

    // irq 指示哪个设备产生了中断。
    int irq = plic_claim();  // 获取中断请求号

    // 根据设备类型分发中断处理
    if(irq == UART0_IRQ){
      uartintr();           // 处理串口中断
    } else if(irq){
      printf("unexpected interrupt irq=%d\n", irq);
    }

    // 通知 PLIC 中断处理完成
    // PLIC 允许每个设备一次最多产生一个中断；
    // 告诉 PLIC 现在允许设备再次中断。
    if(irq)
      plic_complete(irq);

    return 1;
  } else if(scause == 0x8000000000000001L){
    // 软件中断处理
    // 来自机器模式定时器中断的软件中断，
    // 由 kernelvec.S 中的 timervec 转发。

    // 只有 CPU 0 负责更新全局时钟
    if(cpuid() == 0){
      clockintr();
    }
    
    // 清除软件中断标志
    // 通过清除 sip 中的 SSIP 位来确认软件中断。
    w_sip(r_sip() & ~2);

    return 2;  // 表示定时器中断
  } else {
    return 0;  // 未识别的中断类型
  }
}

