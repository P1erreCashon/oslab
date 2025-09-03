//
// 支持涉及文件描述符的系统调用的函数。
//

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "fs.h"
#include "spinlock.h"
#include "sleeplock.h"
#include "file.h"
#include "stat.h"
#include "proc.h"

struct devsw devsw[NDEV];
struct {
  struct spinlock lock;
  struct file file[NFILE];
} ftable;  //这是系统的全局打开文件表

/// @brief 初始化文件表
void
fileinit(void)
{
  // 初始化文件表锁
  initlock(&ftable.lock, "ftable");
}

/// @brief 分配一个文件结构体。
struct file*
filealloc(void)
{
  struct file *f;

  // 获取文件表锁
  acquire(&ftable.lock);
  // 遍历文件表寻找空闲的文件结构体
  for(f = ftable.file; f < ftable.file + NFILE; f++){
    if(f->ref == 0){
      // 找到空闲文件结构体，设置引用计数为1
      f->ref = 1;
      release(&ftable.lock);
      return f;
    }
  }
  // 没有找到空闲的文件结构体
  release(&ftable.lock);
  return 0;
}

/// @brief 增加文件f的引用计数。
struct file*
filedup(struct file *f)
{
  acquire(&ftable.lock);
  // 检查文件引用计数的有效性
  if(f->ref < 1)
    panic("filedup");
  // 增加引用计数
  f->ref++;
  release(&ftable.lock);
  return f;
}

/// @brief 关闭文件f。（减少引用计数，当引用计数达到0时关闭。）
void
fileclose(struct file *f)
{
  struct file ff;

  acquire(&ftable.lock);
  // 检查文件引用计数的有效性
  if(f->ref < 1)
    panic("fileclose");
  // 减少引用计数，如果仍有其他引用则直接返回
  if(--f->ref > 0){
    release(&ftable.lock);
    return;
  }
  // 保存文件信息的副本
  ff = *f;
  // 清除文件表项
  f->ref = 0;
  f->type = FD_NONE;
  release(&ftable.lock);

  // 根据文件类型进行相应的清理工作
  if(ff.type == FD_PIPE){
    // 关闭管道
    pipeclose(ff.pipe, ff.writable);
  } else if(ff.type == FD_INODE || ff.type == FD_DEVICE){
    // 释放inode引用
    //对于涉及文件系统的操作，还需要begin_op和end_op
    //在进行一个inode操作前后，必须添加这种资源获取与释放的对应操作
    //详情参看实验指导书8.6代码：日志（https://xv6.dgs.zone/tranlate_books/book-riscv-rev1/c8/s6.html）
    begin_op();
    iput(ff.ip);
    end_op();
  }
}

/// @brief 获取文件f的元数据。
/// addr是用户虚拟地址，指向struct stat。
int
filestat(struct file *f, uint64 addr)
{
  struct proc *p = myproc();
  struct stat st;
  
  // 只有inode和设备文件支持stat操作
  if(f->type == FD_INODE || f->type == FD_DEVICE){
    // 锁定inode并获取stat信息
    ilock(f->ip);
    stati(f->ip, &st);
    iunlock(f->ip);
    // 将stat信息复制到用户空间
    if(copyout(p->pagetable, addr, (char *)&st, sizeof(st)) < 0)
      return -1;
    return 0;
  }
  return -1;
}

/// @brief 从文件f读取数据。
/// addr是用户虚拟地址。
int
fileread(struct file *f, uint64 addr, int n)
{
  int r = 0;

  // 检查文件是否可读
  if(f->readable == 0)
    return -1;

  // 根据文件类型执行不同的读取操作
  if(f->type == FD_PIPE){
    // 从管道读取
    r = piperead(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    // 从设备读取
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].read)
      return -1;
    r = devsw[f->major].read(1, addr, n);
  } else if(f->type == FD_INODE){
    // 从inode文件读取
    ilock(f->ip);
    // 从当前偏移量处读取数据
    if((r = readi(f->ip, 1, addr, f->off, n)) > 0)
      f->off += r; // 更新文件偏移量
    iunlock(f->ip);
  } else {
    panic("fileread");
  }

  return r;
}

/// @brief 向文件f写入数据。
/// addr是用户虚拟地址。
int
filewrite(struct file *f, uint64 addr, int n)
{
  int r, ret = 0;

  // 检查文件是否可写
  if(f->writable == 0)
    return -1;

  // 根据文件类型执行不同的写入操作
  if(f->type == FD_PIPE){
    // 向管道写入
    ret = pipewrite(f->pipe, addr, n);
  } else if(f->type == FD_DEVICE){
    // 向设备写入
    if(f->major < 0 || f->major >= NDEV || !devsw[f->major].write)
      return -1;
    ret = devsw[f->major].write(1, addr, n);
  } else if(f->type == FD_INODE){
    // 向inode文件写入
    // 一次写入几个块以避免超过最大日志事务大小，
    // 包括i-node、间接块、分配块，
    // 以及2个块的不对齐写入余量。
    // 这实际上应该在更低层，因为writei()
    // 可能正在写入像控制台这样的设备。
    int max = ((MAXOPBLOCKS-1-1-2) / 2) * BSIZE;
    int i = 0;
    // 分批写入数据
    while(i < n){
      int n1 = n - i;
      if(n1 > max)
        n1 = max;

      // 开始操作事务
      begin_op();
      ilock(f->ip);
      // 写入数据并更新文件偏移量
      if ((r = writei(f->ip, 1, addr + i, f->off, n1)) > 0)
        f->off += r;
      iunlock(f->ip);
      // 结束操作事务
      end_op();

      // 检查写入是否成功
      if(r != n1){
        // writei出错
        break;
      }
      i += r;
    }
    // 如果全部写入成功返回n，否则返回-1
    ret = (i == n ? n : -1);
  } else {
    panic("filewrite");
  }

  return ret;
}

