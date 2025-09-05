#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "spinlock.h"
#include "proc.h"
#include "syscall.h"

// File descriptor constants
#define STDIN_FILENO  0
#define STDOUT_FILENO 1
#define STDERR_FILENO 2

//
// System call: read
// Read data from a file descriptor
// For now, only support reading from stdin (console)
//
uint64
sys_read(void)
{
  int fd;
  uint64 buf;
  int count;

  // Get arguments from user space
  argint(0, &fd);
  argaddr(1, &buf);
  argint(2, &count);

  // Only support reading from stdin (console)
  if (fd == STDIN_FILENO) {
    return consoleread(1, buf, count);
  }

  // For other file descriptors, return error since no file system
  return -1;
}

//
// System call: write
// Write data to a file descriptor
// For now, only support writing to stdout/stderr (console)
//
uint64
sys_write(void)
{
  int fd;
  uint64 buf;
  int count;

  // Get arguments from user space
  argint(0, &fd);
  argaddr(1, &buf);
  argint(2, &count);

  // Only support writing to stdout and stderr (console)
  if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    return consolewrite(1, buf, count);
  }

  // For other file descriptors, return error since no file system
  return -1;
}
