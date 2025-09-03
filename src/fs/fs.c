
// 本os的文件系统实现。分为五个层次：
//   + 块：原始磁盘块的分配器。
//   + 日志：用于多步骤更新的崩溃恢复。 （本实验中不涉及到）
//   + 文件（files）：inode 分配器、读取、写入、元数据。
//   + 目录（directories）：具有特殊内容的 inode（其他 inode 的列表！）
//   + 名称：像 /usr/rtm/xv6/fs.c 这样的路径，方便命名。
//
// 本文件包含低级别的文件系统操作例程，即底层的文件操作实现。
// （更高级的）操作接口实现为系统调用，位于 sysfile.c。

#include "types.h"
#include "riscv.h"
#include "defs.h"
#include "param.h"
#include "stat.h"
#include "spinlock.h"
#include "proc.h"
#include "sleeplock.h"
#include "fs.h"
#include "buf.h"
#include "file.h"

#define min(a, b) ((a) < (b) ? (a) : (b))
// there should be one superblock per disk device, but we run with
// only one device
struct superblock sb;

/// @brief 读取超级块
static void
readsb(int dev, struct superblock *sb)
{
  struct buf *bp;

  // 从设备读取第1个块（超级块）
  bp = bread(dev, 1);
  memmove(sb, bp->data, sizeof(*sb));
  // 释放缓冲区
  brelse(bp);
}

/// @brief 初始化文件系统
void fsinit(int dev)
{
  // 读取超级块
  readsb(dev, &sb);
  // 检查文件系统魔数是否正确
  if (sb.magic != FSMAGIC)
    panic("invalid file system");
  // 初始化日志系统
  initlog(dev, &sb);
}

/** Block层操作
 *  下面的操作是针对块的。块是文件系统的基本单位。
 *  块设备（如硬盘）由块号寻址。
 *  块大小为 BSIZE 字节。
 *  块号从 0 开始。
 */

/// @brief 此函数用于清空指定设备和块号的块内容，将其全部设置为零。
/// 这通常用于初始化新分配的块，以确保其不包含任何旧数据
/// @param dev 指定的设备号
/// @param bno 块号
static void
bzero(int dev, int bno)
{
  struct buf *bp;
  // 从设备读取相应的缓冲区，此处bread会获取缓冲区锁
  bp = bread(dev, bno);
  // 使用memset设置成0
  memset(bp->data, 0, BSIZE);
  // 将修改后的缓冲区内容写回设备，log_write替代了本应有的bwrite
  log_write(bp);
  // 释放缓冲区。
  brelse(bp);
}

/// @brief 分配一个新的磁盘块，并将其内容初始化为零。
/// @param dev 指定的设备号
/// @return 返回分配的块号，如果没有可用的块则返回0
static uint
balloc(uint dev)
{
  int b, bi, m;
  struct buf *bp;

  bp = 0;
  for (b = 0; b < sb.size; b += BPB)
  {
    bp = bread(dev, BBLOCK(b, sb));
    // 这个循环用于在位图块中查找一个空闲的块
    for (bi = 0; bi < BPB && b + bi < sb.size; bi++)
    {
      m = 1 << (bi % 8);
      if ((bp->data[bi / 8] & m) == 0)
      {                        // Is block free?
        bp->data[bi / 8] |= m; // 在data的相应位置上设置为1，表示该块已被分配
        // 对块进行修改后记得bget对应的写回和资源释放
        log_write(bp);
        brelse(bp);
        bzero(dev, b + bi);
        return b + bi;
      }
    }
    // 记得bget对应的资源释放
    brelse(bp);
  }
  printf("balloc: out of blocks\n");
  return 0;
}

/// @brief 释放一个不再使用的磁盘块，将其标记为可用。
/// @param dev 设备号
/// @param b 要释放的块号
static void
bfree(int dev, uint b)
{
  struct buf *bp;
  int bi, m;
  // 通过位图找到对应的块
  bp = bread(dev, BBLOCK(b, sb));
  bi = b % BPB;
  m = 1 << (bi % 8);
  // 判断该块是否已经是空闲的
  if ((bp->data[bi / 8] & m) == 0)
    panic("freeing free block");
  // 将对应位置清0，表示该块已被释放
  bp->data[bi / 8] &= ~m;
  // 写回
  log_write(bp);
  brelse(bp);
}

/**以下是Inode层操作 */
// 一个 inode 描述了一个没有名字的文件。
// inode 磁盘结构保存了元数据：文件的类型、
// 文件大小、引用该文件的链接数，以及
// 存储文件内容的块列表。
//
// inode 按顺序排列在磁盘上的 sb.inodestart 位置，即存储在sb中。
// 每个 inode 都有一个编号，表示它在磁盘上的位置。
//
// 内核在内存中保持一个正在使用的 inode 表，
// 以便同步多个进程访问共享 inode。
// 内存中的 inode 包含一些不在磁盘上存储的
// 管理信息：ip->ref 和 ip->valid。
//
// inode 及其内存表示会经过一系列的状态转换，
// 才能被文件系统的其他代码使用。
//
// * 分配：当 inode 的类型（磁盘上的）非零时，
//   inode 就会被分配。ialloc() 分配 inode，
//   当引用计数和链接计数降为零时，iput() 释放 inode。
//
// * 表中引用：inode 表中的一个条目如果 ip->ref 为零，
//   就是空闲的。否则，ip->ref 跟踪指向该条目的
//   内存指针数量（打开的文件和当前目录）。
//   iget() 查找或创建一个表条目并递增其引用计数；
//   iput() 则递减引用计数。
//
// * 有效：inode 表条目中的信息（类型、大小等）
//   只有在 ip->valid 为 1 时才是正确的。
//   ilock() 从磁盘读取 inode 并设置 ip->valid，
//   而 iput() 在 ip->ref 降为零时清除 ip->valid。
//
// * 锁定：文件系统代码只有在首先锁定 inode 后，
//   才能检查和修改 inode 及其内容中的信息。
//
// 因此，一个典型的操作序列是：
//   ip = iget(dev, inum)
//   ilock(ip)
//   ... 检查和修改 ip->xxx ...
//   iunlock(ip)
//   iput(ip)
//
// ilock() 与 iget() 分开，以便系统调用可以
// 获取一个长时间的 inode 引用（例如打开的文件），
// 并且只在短时间内锁定它（例如在 read() 中）。
// 这种分离还有助于避免死锁和路径查找中的竞争条件。
// iget() 递增 ip->ref，确保 inode 保持在表中，
// 并且指向它的指针保持有效。
//
// 许多内部文件系统函数要求调用者在调用时
// 已经锁定了相关 inode；这使得调用者可以
// 创建多步骤的原子操作。
//
// itable.lock 自旋锁保护 itable 条目的分配。
// 由于 ip->ref 表示一个条目是否空闲，
// 而 ip->dev 和 ip->inum 表示条目所持有的 inode，
// 因此在使用这些字段时，必须持有 itable.lock。
//
// ip->lock 睡眠锁保护除 ref、dev 和 inum 外的所有 ip-> 字段。
// 必须持有 ip->lock 才能读取或写入 inode 的
// ip->valid、ip->size、ip->type 等字段。

// 内核维护的 inode 表
struct
{
  struct spinlock lock;
  struct inode inode[NINODE];
} itable;

/// @brief 初始化 inode 表，主要是初始化锁
void iinit()
{
  int i = 0;

  // 初始化inode表锁
  initlock(&itable.lock, "itable");
  // 为每个inode初始化睡眠锁
  for (i = 0; i < NINODE; i++)
  {
    initsleeplock(&itable.inode[i].lock, "inode");
  }
}

static struct inode *iget(uint dev, uint inum);
/// @brief 在设备dev上分配一个inode。通过给它指定类型type将其标记为已分配。
/// 返回一个未锁定但已分配且被引用的inode，如果没有空闲inode则返回NULL。
struct inode *
ialloc(uint dev, short type)
{
  int inum;
  struct buf *bp;
  struct dinode *dip;

  // 遍历所有可能的inode编号
  for (inum = 1; inum < sb.ninodes; inum++)
  {
    // 读取包含该inode的磁盘块
    bp = bread(dev, IBLOCK(inum, sb));
    // 获取磁盘inode指针
    dip = (struct dinode *)bp->data + inum % IPB;
    // 检查是否为空闲inode
    if (dip->type == 0)
    { // a free inode
      memset(dip, 0, sizeof(*dip));
      dip->type = type;
      log_write(bp); // mark it allocated on the disk
      // 释放缓冲区
      brelse(bp);
      return iget(dev, inum);
    }
    brelse(bp);
  }
  printf("ialloc: no inodes\n");
  return 0;
}

/// @brief 将修改过的内存inode复制到磁盘。
/// 必须在对磁盘上存在的ip->xxx字段进行任何更改后调用。
/// 调用者必须持有ip->lock。
void iupdate(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  // 读取包含该inode的磁盘块
  bp = bread(ip->dev, IBLOCK(ip->inum, sb));
  // 获取磁盘inode指针
  dip = (struct dinode *)bp->data + ip->inum % IPB;
  dip->type = ip->type;
  dip->major = ip->major;
  dip->minor = ip->minor;
  dip->nlink = ip->nlink;
  dip->size = ip->size;
  memmove(dip->addrs, ip->addrs, sizeof(ip->addrs));
  // 写回磁盘
  log_write(bp);
  // 释放缓冲区
  brelse(bp);
}

/// @brief 在设备 dev 上查找编号为 inum 的 inode，并返回其内存副本，进入一次文件系统事务。
/// 此操作不会锁定 inode，也不会从磁盘读取 inode。
/// 对应的操作为iput
/// @param dev 设备号
/// @param inum inode编号
/// @return 返回指向内存中对应inode的指针，如果没有找到则返回NULL
static struct inode *
iget(uint dev, uint inum)
{
  struct inode *ip, *empty;

  acquire(&itable.lock);

  // 检查inode是否已经在表中
  empty = 0;
  for (ip = &itable.inode[0]; ip < &itable.inode[NINODE]; ip++)
  {
    if (ip->ref > 0 && ip->dev == dev && ip->inum == inum)
    {
      // 找到匹配的inode，增加引用计数
      ip->ref++;
      release(&itable.lock);
      return ip;
    }
    // 记住空闲槽位
    if (empty == 0 && ip->ref == 0) // Remember empty slot.
      empty = ip;
  }

  // 回收一个inode表项
  if (empty == 0)
    panic("iget: no inodes");

  // 初始化新的inode表项
  ip = empty;
  ip->dev = dev;
  ip->inum = inum;
  ip->ref = 1;
  ip->valid = 0;
  release(&itable.lock);

  return ip;
}

/// @brief 增加ip的引用计数。
/// 返回ip以支持ip = idup(ip1)的习惯用法。
struct inode *
idup(struct inode *ip)
{
  acquire(&itable.lock);
  ip->ref++;
  release(&itable.lock);
  return ip;
}

/// @brief 锁定给定的inode。如果需要，从磁盘读取inode。
void ilock(struct inode *ip)
{
  struct buf *bp;
  struct dinode *dip;

  if (ip == 0 || ip->ref < 1)
    panic("ilock");

  acquiresleep(&ip->lock);

  // 如果inode无效，从磁盘读取
  if (ip->valid == 0)
  {
    bp = bread(ip->dev, IBLOCK(ip->inum, sb));
    dip = (struct dinode *)bp->data + ip->inum % IPB;
    // 从磁盘inode复制到内存inode
    ip->type = dip->type;
    ip->major = dip->major;
    ip->minor = dip->minor;
    ip->nlink = dip->nlink;
    ip->size = dip->size;
    memmove(ip->addrs, dip->addrs, sizeof(ip->addrs));
    // 释放缓冲区
    brelse(bp);
    // 标记为有效
    ip->valid = 1;
    if (ip->type == 0)
      panic("ilock: no type");
  }
}

/// @brief 解锁给定的inode。
void iunlock(struct inode *ip)
{
  if (ip == 0 || !holdingsleep(&ip->lock) || ip->ref < 1)
    panic("iunlock");

  releasesleep(&ip->lock);
}

/// @brief 释放对内存inode的引用。
/// 如果这是最后一个引用，inode表项可以被回收。
/// 如果这是最后一个引用且inode没有指向它的链接，
/// 则在磁盘上释放inode（及其内容）。
/// 所有对iput()的调用必须在事务内进行，以防需要释放inode。
void iput(struct inode *ip)
{
  acquire(&itable.lock);

  // 如果引用计数为1且有效且没有链接，需要截断并释放
  if (ip->ref == 1 && ip->valid && ip->nlink == 0)
  {
    // inode has no links and no other references: truncate and free.

    // ip->ref == 1意味着没有其他进程锁定ip，
    // 所以这个acquiresleep()不会阻塞（或死锁）。
    acquiresleep(&ip->lock);

    release(&itable.lock);

    // 截断inode，释放其数据块
    itrunc(ip);
    ip->type = 0;
    // 更新磁盘inode
    iupdate(ip);
    ip->valid = 0;

    releasesleep(&ip->lock);

    acquire(&itable.lock);
  }

  // 减少引用计数
  ip->ref--;
  release(&itable.lock);
}

/// @brief 一套inode操作的释放连招
/// 常用习惯用法：解锁，然后放置。
void iunlockput(struct inode *ip)
{
  iunlock(ip);
  iput(ip);
}

// Inode内容
//
// 与每个inode关联的内容（数据）存储在磁盘的块中。
// 前NDIRECT个块号列在ip->addrs[]中。
// 接下来的NINDIRECT个块列在块ip->addrs[NDIRECT]中。

/// @brief 返回inode ip中第n个块的磁盘块地址。
/// 如果没有这样的块，bmap会分配一个。
/// 如果磁盘空间不足则返回0。
static uint
bmap(struct inode *ip, uint bn)
{
  uint addr, *a;
  struct buf *bp;

  // 直接块
  if (bn < NDIRECT)
  {
    // 如果该直接块未分配，分配一个
    if ((addr = ip->addrs[bn]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[bn] = addr;
    }
    return addr;
  }
  bn -= NDIRECT;

  // 间接块
  if (bn < NINDIRECT)
  {
    // 加载间接块，如果需要则分配
    if ((addr = ip->addrs[NDIRECT]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr == 0)
        return 0;
      ip->addrs[NDIRECT] = addr;
    }
    // 读取间接块
    bp = bread(ip->dev, addr);
    a = (uint *)bp->data;
    // 检查间接块中的目标块是否已分配
    if ((addr = a[bn]) == 0)
    {
      addr = balloc(ip->dev);
      if (addr)
      {
        a[bn] = addr;
        log_write(bp);
      }
    }
    // 释放间接块缓冲区
    brelse(bp);
    return addr;
  }

  panic("bmap: out of range");
}

/// @brief 截断inode（丢弃内容）。
/// 调用者必须持有ip->lock。
void itrunc(struct inode *ip)
{
  int i, j;
  struct buf *bp;
  uint *a;

  // 释放所有直接块
  for (i = 0; i < NDIRECT; i++)
  {
    if (ip->addrs[i])
    {
      bfree(ip->dev, ip->addrs[i]);
      ip->addrs[i] = 0;
    }
  }

  // 如果有间接块，释放间接块中的所有块
  if (ip->addrs[NDIRECT])
  {
    bp = bread(ip->dev, ip->addrs[NDIRECT]);
    a = (uint *)bp->data;
    // 释放间接块中指向的所有块
    for (j = 0; j < NINDIRECT; j++)
    {
      if (a[j])
        bfree(ip->dev, a[j]);
    }
    // 释放间接块缓冲区
    brelse(bp);
    // 释放间接块本身
    bfree(ip->dev, ip->addrs[NDIRECT]);
    ip->addrs[NDIRECT] = 0;
  }

  // 重置文件大小并更新inode
  ip->size = 0;
  iupdate(ip);
}

/// @brief 从inode复制stat信息。
/// 调用者必须持有ip->lock。
void stati(struct inode *ip, struct stat *st)
{
  st->dev = ip->dev;
  st->ino = ip->inum;
  st->type = ip->type;
  st->nlink = ip->nlink;
  st->size = ip->size;
}

/// @brief 从inode读取数据。
/// 调用者必须持有ip->lock。
/// 如果user_dst==1，则dst是用户虚拟地址；否则，dst是内核地址。
int readi(struct inode *ip, int user_dst, uint64 dst, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  // 检查偏移量和大小是否有效
  if (off > ip->size || off + n < off)
    return 0;
  // 调整读取大小以不超过文件大小
  if (off + n > ip->size)
    n = ip->size - off;

  // 逐块读取数据
  for (tot = 0; tot < n; tot += m, off += m, dst += m)
  {
    // 获取当前块地址
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    // 读取块数据
    bp = bread(ip->dev, addr);
    // 计算本次读取的字节数
    m = min(n - tot, BSIZE - off % BSIZE);
    // 复制数据到目标地址
    if (either_copyout(user_dst, dst, bp->data + (off % BSIZE), m) == -1)
    {
      brelse(bp);
      tot = -1;
      break;
    }
    // 释放缓冲区
    brelse(bp);
  }
  return tot;
}

/// @brief 向inode写入数据。
/// 调用者必须持有ip->lock。
/// 如果user_src==1，则src是用户虚拟地址；否则，src是内核地址。
/// 返回成功写入的字节数。
/// 如果返回值小于请求的n，则发生了某种错误。
int writei(struct inode *ip, int user_src, uint64 src, uint off, uint n)
{
  uint tot, m;
  struct buf *bp;

  // 检查偏移量和大小是否有效
  if (off > ip->size || off + n < off)
    return -1;
  // 检查是否超过最大文件大小
  if (off + n > MAXFILE * BSIZE)
    return -1;

  // 逐块写入数据
  for (tot = 0; tot < n; tot += m, off += m, src += m)
  {
    // 获取或分配当前块地址
    uint addr = bmap(ip, off / BSIZE);
    if (addr == 0)
      break;
    // 读取块数据
    bp = bread(ip->dev, addr);
    // 计算本次写入的字节数
    m = min(n - tot, BSIZE - off % BSIZE);
    // 从源地址复制数据到缓冲区
    if (either_copyin(bp->data + (off % BSIZE), user_src, src, m) == -1)
    {
      brelse(bp);
      break;
    }
    // 写回磁盘
    log_write(bp);
    // 释放缓冲区
    brelse(bp);
  }

  // 如果写入位置超过原文件大小，更新文件大小
  if (off > ip->size)
    ip->size = off;

  // 写回inode到磁盘，即使大小没有改变
  // 因为上面的循环可能调用了bmap()并向ip->addrs[]添加了新块。
  iupdate(ip);

  return tot;
}

// 目录

int namecmp(const char *s, const char *t)
{
  return strncmp(s, t, DIRSIZ);
}

/// @brief 在目录中查找目录项。
/// 如果找到，将*poff设置为目录项的字节偏移量。
struct inode *
dirlookup(struct inode *dp, char *name, uint *poff)
{
  uint off, inum;
  struct dirent de;

  // 检查是否为目录
  if (dp->type != T_DIR)
    panic("dirlookup not DIR");

  // 遍历目录中的所有目录项
  for (off = 0; off < dp->size; off += sizeof(de))
  {
    // 读取目录项
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlookup read");
    // 跳过空目录项
    if (de.inum == 0)
      continue;
    // 比较名称
    if (namecmp(name, de.name) == 0)
    {
      // 条目匹配路径元素
      if (poff)
        *poff = off;
      inum = de.inum;
      return iget(dp->dev, inum);
    }
  }

  return 0;
}

/// @brief 向目录dp写入新的目录项(name, inum)。
/// 成功时返回0，失败时返回-1（例如磁盘块不足）。
int dirlink(struct inode *dp, char *name, uint inum)
{
  int off;
  struct dirent de;
  struct inode *ip;

  // 检查名称是否已存在
  if ((ip = dirlookup(dp, name, 0)) != 0)
  {
    iput(ip);
    return -1;
  }

  // 查找空的目录项
  for (off = 0; off < dp->size; off += sizeof(de))
  {
    if (readi(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
      panic("dirlink read");
    if (de.inum == 0)
      break;
  }

  // 设置新目录项
  strncpy(de.name, name, DIRSIZ);
  de.inum = inum;
  // 写入目录项
  if (writei(dp, 0, (uint64)&de, off, sizeof(de)) != sizeof(de))
    return -1;

  return 0;
}

// 路径（Path）相关操作

/// @brief 从路径中复制下一个路径元素到name。
/// 返回指向复制元素后面元素的指针。
/// 返回的路径没有前导斜杠，
/// 所以调用者可以检查*path=='\0'来看名称是否是最后一个。
/// 如果没有名称要移除，返回0。
///
/// 示例：
///   skipelem("a/bb/c", name) = "bb/c", setting name = "a"
///   skipelem("///a//bb", name) = "bb", setting name = "a"
///   skipelem("a", name) = "", setting name = "a"
///   skipelem("", name) = skipelem("////", name) = 0
static char *
skipelem(char *path, char *name)
{
  char *s;
  int len;

  // 跳过前导斜杠
  while (*path == '/')
    path++;
  // 如果路径为空，返回0
  if (*path == 0)
    return 0;
  s = path;
  // 找到下一个斜杠或字符串结尾
  while (*path != '/' && *path != 0)
    path++;
  len = path - s;
  // 复制名称，确保不超过DIRSIZ
  if (len >= DIRSIZ)
    memmove(name, s, DIRSIZ);
  else
  {
    memmove(name, s, len);
    name[len] = 0;
  }
  // 跳过尾随斜杠
  while (*path == '/')
    path++;
  return path;
}

/// @brief 查找并返回路径名对应的inode。
/// 如果parent != 0，返回父目录的inode并将最终路径元素复制到name中，
/// name必须有DIRSIZ字节的空间。
/// 必须在文件系统事务内（iget后）调用，因为它调用iput()，会释放inode的引用。
static struct inode *
namex(char *path, int nameiparent, char *name)
{
  struct inode *ip, *next;

  // 根据路径是否以'/'开头，选择起始inode
  if (*path == '/')
    ip = iget(ROOTDEV, ROOTINO);
  else
    ip = idup(myproc()->cwd);

  // 逐个处理路径元素
  while ((path = skipelem(path, name)) != 0)
  {
    ilock(ip);
    // 检查当前inode是否为目录
    if (ip->type != T_DIR)
    {
      iunlockput(ip);
      return 0;
    }
    // 如果需要返回父目录且已到达最后一个元素
    if (nameiparent && *path == '\0')
    {
      // Stop one level early.
      iunlock(ip);
      return ip;
    }
    // 在当前目录中查找下一个路径元素
    if ((next = dirlookup(ip, name, 0)) == 0)
    {
      iunlockput(ip);
      return 0;
    }
    iunlockput(ip);
    ip = next;
  }
  // 如果需要返回父目录但已遍历完所有路径元素
  if (nameiparent)
  {
    iput(ip);
    return 0;
  }
  return ip;
}

/// @brief 查找路径名对应的inode。
/// 返回该inode的引用（未锁定）。
struct inode *
namei(char *path)
{
  char name[DIRSIZ];
  return namex(path, 0, name);
}

/// @brief 查找路径名对应的父目录的inode，并将最终路径元素复制到name中。
/// name必须有DIRSIZ字节的空间。
/// 返回父目录的inode引用（未锁定）。
struct inode *
nameiparent(char *path, char *name)
{
  return namex(path, 1, name);
}
