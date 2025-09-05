#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "memlayout.h"
#include "spinlock.h"
#include "proc.h"

// Test device的控制命令
#define TEST_FINISHER_FAIL    0x3333
#define TEST_FINISHER_PASS    0x5555
#define TEST_FINISHER_RESET   0x7777

uint64
sys_shutdown(void)
{
  printf("System shutdown requested...\n");
  
  // 向QEMU的Test device写入关机命令
  // 0x5555表示正常关机(PASS)，QEMU会退出并返回状态码0
  volatile uint32 *test_dev = (volatile uint32 *)TEST_DEVICE;
  *test_dev = TEST_FINISHER_PASS;
  
  // 如果上面的方法失败，执行无限循环作为备用方案
  // 这种情况下用户需要手动停止QEMU
  printf("Shutdown failed, entering infinite loop...\n");
  while(1) {
    // 让CPU进入低功耗状态
    asm volatile("wfi");
  }
  
  return 0;  // not reached
}