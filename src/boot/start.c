#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

void main();
void timerinit();

// entry.S需要为每个CPU分配一个栈空间
// 16字节对齐，总共为NCPU个CPU分配栈空间
__attribute__ ((aligned (16))) char stack0[4096 * NCPU];

// 每个CPU的机器模式定时器中断的临时存储区域
// 每个CPU需要5个64位字的空间来保存中断处理时的上下文
uint64 timer_scratch[NCPU][5];

// kernelvec.S中的汇编代码，用于处理机器模式的定时器中断
extern void timervec();

// entry.S在机器模式下跳转到此处，使用stack0栈空间
void
start()
{
  // 设置M模式下的前一特权级为管理者模式(Supervisor)，供mret指令使用
  // 当mret执行时，会切换到管理者模式继续执行
  unsigned long x = r_mstatus();
  x &= ~MSTATUS_MPP_MASK;  // 清除MPP位域
  x |= MSTATUS_MPP_S;      // 设置MPP为管理者模式
  w_mstatus(x);

  // 设置M模式异常程序计数器指向main函数，供mret指令使用
  // 需要编译时使用gcc -mcmodel=medany选项
  w_mepc((uint64)main);

  // 暂时禁用分页机制
  w_satp(0);

  // 将所有中断和异常委托给管理者模式处理
  w_medeleg(0xffff);  // 异常委托
  w_mideleg(0xffff);  // 中断委托
  // 启用管理者模式的外部中断、定时器中断和软件中断
  w_sie(r_sie() | SIE_SEIE | SIE_STIE | SIE_SSIE);

  // 配置物理内存保护(PMP)，给予管理者模式访问全部物理内存的权限
  w_pmpaddr0(0x3fffffffffffffull);  // 设置PMP地址范围
  w_pmpcfg0(0xf);                   // 设置PMP配置(读写执行权限)

  // 请求时钟中断服务
  timerinit();

  // 将当前CPU的hartid保存到tp寄存器中，供cpuid()函数使用
  // 在进入管理者模式中, mhartid寄存器不可用
  int id = r_mhartid();
  w_tp(id);

  // 切换到管理者模式并跳转到main()函数
  asm volatile("mret");
}

// 安排接收定时器中断
// 中断将以机器模式到达kernelvec.S中的timervec函数
// timervec会将其转换为软件中断，交由trap.c中的devintr()处理
void
timerinit()
{
  // 每个CPU都有独立的定时器中断源
  int id = r_mhartid();

  // 向CLINT(核心本地中断控制器)请求定时器中断
  int interval = 1000000; // 周期数；在QEMU中大约是1/10秒
  *(uint64*)CLINT_MTIMECMP(id) = *(uint64*)CLINT_MTIME + interval;

  // 在scratch[]中为timervec准备信息
  // scratch[0..2] : timervec保存寄存器的空间
  // scratch[3] : CLINT MTIMECMP寄存器地址
  // scratch[4] : 定时器中断之间期望的间隔(周期数)
  uint64 *scratch = &timer_scratch[id][0];
  scratch[3] = CLINT_MTIMECMP(id);
  scratch[4] = interval;
  w_mscratch((uint64)scratch);

  // 设置机器模式的陷阱处理程序
  w_mtvec((uint64)timervec);

  // 启用机器模式中断
  w_mstatus(r_mstatus() | MSTATUS_MIE);

  // 启用机器模式定时器中断
  w_mie(r_mie() | MIE_MTIE);
}
