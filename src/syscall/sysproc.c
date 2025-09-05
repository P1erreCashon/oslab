#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

uint64
sys_exit(void)
{
  int n;
  argint(0, &n);
  exit(n);
  return 0;  // not reached
}

uint64
sys_getpid(void)
{
  return myproc()->pid;
}

uint64
sys_fork(void)
{
  return fork();
}

uint64
sys_wait(void)
{
  uint64 p;
  argaddr(0, &p);
  return wait(p);
}

uint64
sys_sbrk(void)
{
  uint64 addr;
  int n;

  argint(0, &n);
  addr = myproc()->sz;
  if(growproc(n) < 0)
    return -1;
  return addr;
}

uint64
sys_sleep(void)
{
  int n;
  uint ticks0;

  argint(0, &n);
  acquire(&tickslock);
  ticks0 = ticks;
  while(ticks - ticks0 < n){
    if(killed(myproc())){
      release(&tickslock);
      return -1;
    }
    sleep(&ticks, &tickslock);
  }
  release(&tickslock);
  return 0;
}

uint64
sys_kill(void)
{
  int pid;

  argint(0, &pid);
  return kill(pid);
}

// return how many clock tick interrupts have occurred
// since start.
uint64
sys_uptime(void)
{
  uint xticks;

  acquire(&tickslock);
  xticks = ticks;
  release(&tickslock);
  return xticks;
}

uint64
sys_gettimeofday(void)
{
  uint64 tv_addr;
  struct timeval tv;
  
  // 获取用户空间指针
  argaddr(0, &tv_addr);
  
  // 获取当前高精度时间 - 使用 CLINT_MTIME 寄存器
  uint64 cycles = *(uint64*)CLINT_MTIME;
  
  // RISC-V CLINT 时钟频率通常为 10MHz
  // 将时钟周期转换为秒和微秒
  tv.tv_sec = cycles / 10000000;
  tv.tv_usec = (cycles % 10000000) / 10;
  
  // 复制到用户空间
  if(copyout(myproc()->pagetable, tv_addr, (char*)&tv, sizeof(tv)) < 0)
    return -1;
    
  return 0;
}
