#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "spinlock.h"
#include "proc.h"
#include "defs.h"

int
exec(char *path, char **argv)
{
  // 简化的 exec 实现 - 直接返回错误
  // 在没有文件系统的情况下，无法从磁盘加载程序
  return -1;
}
