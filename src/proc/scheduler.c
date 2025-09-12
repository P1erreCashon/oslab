#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"


// 每个CPU的进程调度器
// 每个CPU在完成自身设置后调用scheduler()
// 调度器永不返回，它持续循环执行：
//  - 选择一个进程运行
//  - 通过swtch切换到该进程开始运行
//  - 最终该进程通过swtch将控制权交回给调度器
void
scheduler(void)
{
  struct proc *p;
  struct cpu *c = mycpu();
  
  c->proc = 0;
  for(;;){
    // 通过确保设备能够产生中断来避免死锁
    intr_on();

    // 遍历进程表，寻找可运行的进程
    for(p = proc; p < &proc[NPROC]; p++) {
      acquire(&p->lock);
      if(p->state == RUNNABLE) {
        // 切换到选中的进程。进程有责任释放其锁
        // 然后在跳回调度器之前重新获取锁
        p->state = RUNNING;
        c->proc = p;
        swtch(&c->context, &p->context);  // 上下文切换到进程

        // 进程暂时运行完毕
        // 它应该在返回之前改变了p->state
        c->proc = 0;
      }
      release(&p->lock);
    }
  }
}

// 切换到调度器。必须只持有p->lock锁
// 并且已经改变了proc->state。
// 因为intena是这个内核线程的属性，而不是这个CPU的属性。
// 因此此处需要保存和恢复intena
void
sched(void)
{
  int intena;
  struct proc *p = myproc();

  if(!holding(&p->lock))
    panic("sched p->lock");
  if(mycpu()->noff != 1)
    panic("sched locks");
  if(p->state == RUNNING)
    panic("sched running");
  if(intr_get())
    panic("sched interruptible");

  intena = mycpu()->intena;
  swtch(&p->context, &mycpu()->context);  // 切换到调度器上下文
  mycpu()->intena = intena;
}

// 用于进程放弃CPU, 重新进入调度
void
yield(void)
{
  struct proc *p = myproc();
  acquire(&p->lock);     // 获取进程锁
  p->state = RUNNABLE;   // 将进程状态设为可运行
  sched();               // 调用sched()切换到调度器
  release(&p->lock);     // 释放进程锁
}