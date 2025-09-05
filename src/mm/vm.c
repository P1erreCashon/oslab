#include "param.h"
#include "types.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

/*
 * 内核页表
 */
pagetable_t kernel_pagetable;

extern char etext[]; // kernel.ld 将此设置为内核代码的结尾

extern char trampoline[]; // trampoline.S

// 为内核创建直接映射页表
pagetable_t
kvmmake(void)
{
  pagetable_t kpgtbl;

  kpgtbl = (pagetable_t)kalloc();
  memset(kpgtbl, 0, PGSIZE);

  // Test device for shutdown control
  kvmmap(kpgtbl, TEST_DEVICE, TEST_DEVICE, PGSIZE, PTE_R | PTE_W);

  // uart寄存器
  kvmmap(kpgtbl, UART0, UART0, PGSIZE, PTE_R | PTE_W);

  // virtio mmio磁盘接口
  kvmmap(kpgtbl, VIRTIO0, VIRTIO0, PGSIZE, PTE_R | PTE_W);

  // PLIC
  kvmmap(kpgtbl, PLIC, PLIC, 0x400000, PTE_R | PTE_W);

  // 映射内核代码段为可执行和只读
  kvmmap(kpgtbl, KERNBASE, KERNBASE, (uint64)etext - KERNBASE, PTE_R | PTE_X);

  // 映射内核数据段和我们将使用的物理RAM
  kvmmap(kpgtbl, (uint64)etext, (uint64)etext, PHYSTOP - (uint64)etext, PTE_R | PTE_W);

  // 将trampoline页面映射到trap入口/出口，
  // 位于内核的最高虚拟地址
  kvmmap(kpgtbl, TRAMPOLINE, (uint64)trampoline, PGSIZE, PTE_R | PTE_X);

  // 为每个进程分配并映射一个内核栈
  proc_mapstacks(kpgtbl);

  return kpgtbl;
}

// 初始化内核页表
void kvminit(void)
{
  kernel_pagetable = kvmmake();
}

// 将硬件页表寄存器切换到内核页表，
// 并启用分页
void kvminithart()
{
  // 等待对页表内存的任何先前写入完成
  sfence_vma();

  w_satp(MAKE_SATP(kernel_pagetable));

  // 刷新TLB中的陈旧条目
  sfence_vma();
}

/*
 * 页表操作辅助函数
 */

// 从虚拟地址中提取指定级别的页表索引
// level: 页表级别 (0=叶子级, 1=中间级, 2=顶级)
// va: 虚拟地址
static inline uint64
extract_page_table_index(uint64 va, int level)
{
  // RISC-V Sv39: 每级9位索引，从bit 12开始
  int shift = 12 + (9 * level); // 12 + 9*level
  return (va >> shift) & 0x1FF; // 提取9位 (0x1FF = 511)
}

// 检查PTE是否有效
static inline int
is_pte_valid(pte_t pte)
{
  return (pte & PTE_V) != 0;
}

// 检查PTE是否为叶子节点(包含实际的物理页映射)
static inline int
is_pte_leaf(pte_t pte)
{
  return (pte & (PTE_R | PTE_W | PTE_X)) != 0;
}

// 从PTE获取下一级页表的物理地址
static inline uint64
get_next_page_table_pa(pte_t pte)
{
  return PTE2PA(pte);
}

// 为新分配的页表页面创建PTE
static inline pte_t
create_page_table_pte(uint64 pa)
{
  return PA2PTE(pa) | PTE_V;
}

// 分配并初始化新的页表页面
static pagetable_t
allocate_page_table_page(void)
{
  pagetable_t new_table = (pagetable_t)kalloc();
  if (new_table == 0)
    return 0;
  memset(new_table, 0, PGSIZE);
  return new_table;
}

// 返回页表pagetable中对应于虚拟地址va的PTE地址
// 如果alloc!=0，创建任何需要的页表页面
//
// risc-v Sv39方案有三级页表页面
// 一个页表页面包含512个64位PTE
// 一个64位虚拟地址被分成五个字段：
//   39..63 -- 必须为零
//   30..38 -- 9位二级索引 (level 2)
//   21..29 -- 9位一级索引 (level 1)
//   12..20 -- 9位零级索引 (level 0)
//    0..11 -- 12位页内字节偏移
pte_t *
walk(pagetable_t pagetable, uint64 va, int alloc)
{
  if (va >= MAXVA)
    panic("walk: virtual address too large");

  // 遍历页表层级，从顶级(level 2)到中间级(level 1)
  // 最后返回叶子级(level 0)的PTE地址
  for (int level = 2; level > 0; level--)
  {
    uint64 index = extract_page_table_index(va, level);
    pte_t *pte = &pagetable[index];

    if (is_pte_valid(*pte))
    {
      // PTE有效，获取下一级页表的物理地址
      uint64 next_pa = get_next_page_table_pa(*pte);
      pagetable = (pagetable_t)next_pa;
    }
    else
    {
      // PTE无效，需要分配新的页表页面
      if (!alloc)
      {
        return 0; // 不允许分配，返回失败
      }

      pagetable_t new_table = allocate_page_table_page();
      if (new_table == 0)
      {
        return 0; // 内存分配失败
      }

      // 在当前PTE中设置新页表的地址
      *pte = create_page_table_pte((uint64)new_table);
      pagetable = new_table;
    }
  }

  // 返回叶子级(level 0)的PTE地址
  uint64 leaf_index = extract_page_table_index(va, 0);
  return &pagetable[leaf_index];
}

// 检查PTE是否为用户可访问的有效页面
static inline int
is_user_accessible_page(pte_t pte)
{
  return (pte & PTE_V) && (pte & PTE_U);
}

// 查找虚拟地址，返回物理地址，
// 如果没有映射则返回0
// 只能用于查找用户页面
uint64
walkaddr(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;
  uint64 pa;

  if (va >= MAXVA)
    return 0;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    return 0;

  if (!is_user_accessible_page(*pte))
    return 0;

  pa = PTE2PA(*pte);
  return pa;
}

// 向内核页表添加映射
// 仅在启动时使用
// 不刷新TLB或启用分页
void kvmmap(pagetable_t kpgtbl, uint64 va, uint64 pa, uint64 sz, int perm)
{
  if (mappages(kpgtbl, va, sz, pa, perm) != 0)
    panic("kvmmap");
}

// 创建包含指定权限的PTE
static inline pte_t
create_mapping_pte(uint64 pa, int permissions)
{
  return PA2PTE(pa) | permissions | PTE_V;
}

// 检查页面是否已经被映射
static inline int
is_page_already_mapped(pte_t pte)
{
  return (pte & PTE_V) != 0;
}


// 为从va开始的虚拟地址创建PTE，引用
// 从pa开始的物理地址。va和size可能不是
// 页面对齐的。成功返回0，如果walk()无法
// 分配所需的页表页面则返回-1
int mappages(pagetable_t pagetable, uint64 va, uint64 size, uint64 pa, int perm)
{
  uint64 current_va, last_va;
  pte_t *pte;

  if (size == 0)
    panic("mappages: size cannot be zero");

  current_va = PGROUNDDOWN(va);
  last_va = PGROUNDDOWN(va + size - 1);

  for (;;)
  {
    pte = walk(pagetable, current_va, 1);
    if (pte == 0)
      return -1; // 页表分配失败

    if (is_page_already_mapped(*pte))
      panic("mappages: attempting to remap existing page");

    *pte = create_mapping_pte(pa, perm);

    if (current_va == last_va)
      break;

    current_va += PGSIZE;
    pa += PGSIZE;
  }
  return 0;
}

// 检查虚拟地址是否页对齐
static inline int
is_page_aligned(uint64 addr)
{
  return (addr % PGSIZE) == 0;
}

// 清除PTE，使其无效
static inline void
clear_pte(pte_t *pte)
{
  *pte = 0;
}

// 从PTE获取物理地址并释放对应的物理页面
static inline void
free_physical_page_from_pte(pte_t pte)
{
  uint64 pa = PTE2PA(pte);
  kfree((void *)pa);
}

// 验证页面映射的完整性
static inline void
validate_page_mapping(pte_t pte)
{
  if (!is_pte_valid(pte))
    panic("uvmunmap: page not mapped");
  if (PTE_FLAGS(pte) == PTE_V)
    panic("uvmunmap: not a leaf page");
}

// 从va开始移除npages个映射。va必须是
// 页面对齐的。映射必须存在。
// 可选择释放物理内存
void uvmunmap(pagetable_t pagetable, uint64 va, uint64 npages, int do_free)
{
  uint64 current_va;
  pte_t *pte;

  if (!is_page_aligned(va))
    panic("uvmunmap: address not page aligned");

  for (current_va = va; current_va < va + npages * PGSIZE; current_va += PGSIZE)
  {
    pte = walk(pagetable, current_va, 0);
    if (pte == 0)
      panic("uvmunmap: walk failed");

    validate_page_mapping(*pte);

    if (do_free)
    {
      free_physical_page_from_pte(*pte);
    }

    clear_pte(pte);
  }
}

// 创建一个空的用户页表
// 如果内存不足则返回0
pagetable_t
uvmcreate()
{
  pagetable_t pagetable;
  pagetable = (pagetable_t)kalloc();
  if (pagetable == 0)
    return 0;
  memset(pagetable, 0, PGSIZE);
  return pagetable;
}

// 将用户初始代码加载到页表的地址0，
// 用于第一个进程。
// 动态分配程序段页数的两倍，低地址存程序段，高地址作栈内存
uint64 uvmfirst(pagetable_t pagetable, uchar *src, uint sz)
{
  char *mem;
  printf("sz: %d\n", sz);

  // 计算程序段需要的页面数量（向上取整）
  uint64 prog_pages = PGROUNDUP(sz) / PGSIZE;
  if (prog_pages == 0)
    prog_pages = 1; // 至少分配一页
  
  // 总共分配两倍的页面数，低地址存程序段，高地址作栈内存
  uint64 total_pages = prog_pages * 2;
  uint64 total_size = total_pages * PGSIZE;

  printf("prog_pages: %d, total_pages: %d, total_size: %d\n", prog_pages, total_pages, total_size);

  // 分配程序段页面
  for (uint64 i = 0; i < prog_pages; i++)
  {
    mem = kalloc();
    if (mem == 0)
      panic("uvmfirst: kalloc failed for program pages");
    
    memset(mem, 0, PGSIZE);
    
    if (mappages(pagetable, i * PGSIZE, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_X | PTE_U) != 0)
    {
      kfree(mem);
      panic("uvmfirst: mappages failed for program pages");
    }

    // 复制程序内容
    uint64 src_offset = i * PGSIZE;
    uint64 copy_size = (sz - src_offset > PGSIZE) ? PGSIZE : (sz - src_offset);
    if (copy_size > 0 && src_offset < sz)
    {
      memmove(mem, (void *)((uint64)src + src_offset), copy_size);
    }
  }

  // 分配栈内存页面
  for (uint64 i = prog_pages; i < total_pages; i++)
  {
    mem = kalloc();
    if (mem == 0)
      panic("uvmfirst: kalloc failed for stack pages");
    
    memset(mem, 0, PGSIZE);
    
    // 栈内存只需要读写权限，不需要执行权限
    if (mappages(pagetable, i * PGSIZE, PGSIZE, (uint64)mem, PTE_W | PTE_R | PTE_U) != 0)
    {
      kfree(mem);
      panic("uvmfirst: mappages failed for stack pages");
    }
  }

  return total_size;
}

// 分配PTE和物理内存以将进程从oldsz增长到
// newsz，不需要页面对齐。返回新大小或错误时返回0
uint64
uvmalloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz, int xperm)
{
  char *mem;
  uint64 a;

  if (newsz < oldsz)
    return oldsz;

  oldsz = PGROUNDUP(oldsz);
  for (a = oldsz; a < newsz; a += PGSIZE)
  {
    mem = kalloc();
    if (mem == 0)
    {
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
    memset(mem, 0, PGSIZE);
    if (mappages(pagetable, a, PGSIZE, (uint64)mem, PTE_R | PTE_U | xperm) != 0)
    {
      kfree(mem);
      uvmdealloc(pagetable, a, oldsz);
      return 0;
    }
  }
  return newsz;
}

// 释放用户页面以将进程大小从oldsz减少到
// newsz。oldsz和newsz不需要页面对齐，newsz也
// 不需要小于oldsz。oldsz可以大于实际
// 进程大小。返回新的进程大小
uint64
uvmdealloc(pagetable_t pagetable, uint64 oldsz, uint64 newsz)
{
  if (newsz >= oldsz)
    return oldsz;

  if (PGROUNDUP(newsz) < PGROUNDUP(oldsz))
  {
    int npages = (PGROUNDUP(oldsz) - PGROUNDUP(newsz)) / PGSIZE;
    uvmunmap(pagetable, PGROUNDUP(newsz), npages, 1);
  }

  return newsz;
}

// 检查PTE是否指向下一级页表（而非叶子页面）
static inline int
is_page_table_pointer(pte_t pte)
{
  return is_pte_valid(pte) && !is_pte_leaf(pte);
}

// 页表中PTE的数量（2^9 = 512）
#define PAGE_TABLE_ENTRIES 512

// 递归释放页表页面
// 所有叶子映射必须已经被移除
void freewalk(pagetable_t pagetable)
{
  // 遍历页表中的所有PTE
  for (int i = 0; i < PAGE_TABLE_ENTRIES; i++)
  {
    pte_t pte = pagetable[i];

    if (is_page_table_pointer(pte))
    {
      // 这个PTE指向一个下级页表，递归释放
      uint64 child_pa = get_next_page_table_pa(pte);
      freewalk((pagetable_t)child_pa);
      pagetable[i] = 0;
    }
    else if (is_pte_valid(pte))
    {
      // 发现叶子页面，应该已经被清理
      panic("freewalk: found unexpected leaf page");
    }
  }

  // 释放当前页表页面
  kfree((void *)pagetable);
}

// 计算给定大小需要的页面数量
static inline uint64
calculate_pages_needed(uint64 size)
{
  return PGROUNDUP(size) / PGSIZE;
}

// 释放用户内存页面，然后释放页表页面
void uvmfree(pagetable_t pagetable, uint64 sz)
{
  if (sz > 0)
  {
    uint64 npages = calculate_pages_needed(sz);
    uvmunmap(pagetable, 0, npages, 1);
  }
  freewalk(pagetable);
}

// 复制物理页面内容
static inline int
copy_physical_page(uint64 src_pa, char **dest_mem)
{
  *dest_mem = kalloc();
  if (*dest_mem == 0)
    return -1; // 内存分配失败

  memmove(*dest_mem, (char *)src_pa, PGSIZE);
  return 0;
}

// 清理部分复制的页面（错误处理）
static inline void
cleanup_partial_copy(pagetable_t new_table, uint64 copied_size)
{
  uint64 npages = copied_size / PGSIZE;
  uvmunmap(new_table, 0, npages, 1);
}

// 给定父进程的页表，复制其内存到子进程的页表
// 复制页表和物理内存
// 成功返回0，失败返回-1
// 失败时释放任何已分配的页面
int uvmcopy(pagetable_t old, pagetable_t new, uint64 sz)
{
  pte_t *pte;
  uint64 pa, current_va;
  uint flags;
  char *mem;

  for (current_va = 0; current_va < sz; current_va += PGSIZE)
  {
    pte = walk(old, current_va, 0);
    if (pte == 0)
      panic("uvmcopy: pte should exist");

    if (!is_pte_valid(*pte))
      panic("uvmcopy: page not present");

    pa = PTE2PA(*pte);
    flags = PTE_FLAGS(*pte);

    if (copy_physical_page(pa, &mem) != 0)
      goto err;

    if (mappages(new, current_va, PGSIZE, (uint64)mem, flags) != 0)
    {
      kfree(mem);
      goto err;
    }
  }
  return 0;

err:
  cleanup_partial_copy(new, current_va);
  return -1;
}

// 清除PTE的用户访问位
static inline void
clear_user_access_bit(pte_t *pte)
{
  *pte &= ~PTE_U;
}

// 标记PTE对用户访问无效
// exec用于用户栈保护页面
void uvmclear(pagetable_t pagetable, uint64 va)
{
  pte_t *pte;

  pte = walk(pagetable, va, 0);
  if (pte == 0)
    panic("uvmclear: page table walk failed");

  clear_user_access_bit(pte);
}

// 计算在当前页面中可以复制的字节数
static inline uint64
bytes_to_copy_in_page(uint64 va, uint64 remaining_len)
{
  uint64 page_offset = va - PGROUNDDOWN(va);
  uint64 bytes_in_page = PGSIZE - page_offset;
  return (bytes_in_page > remaining_len) ? remaining_len : bytes_in_page;
}

// 从内核复制到用户
// 将len字节从src复制到给定页表中的虚拟地址dstva
// 成功返回0，错误返回-1
int copyout(pagetable_t pagetable, uint64 dstva, char *src, uint64 len)
{
  uint64 bytes_to_copy, page_va, page_pa;

  while (len > 0)
  {
    page_va = PGROUNDDOWN(dstva);
    page_pa = walkaddr(pagetable, page_va);
    if (page_pa == 0)
      return -1; // 页面映射不存在或不可访问

    bytes_to_copy = bytes_to_copy_in_page(dstva, len);

    uint64 dest_offset = dstva - page_va;
    memmove((void *)(page_pa + dest_offset), src, bytes_to_copy);

    len -= bytes_to_copy;
    src += bytes_to_copy;
    dstva = page_va + PGSIZE; // 移到下一页
  }
  return 0;
}

// 从用户复制到内核
// 将len字节从给定页表中的虚拟地址srcva复制到dst
// 成功返回0，错误返回-1
int copyin(pagetable_t pagetable, char *dst, uint64 srcva, uint64 len)
{
  uint64 bytes_to_copy, page_va, page_pa;

  while (len > 0)
  {
    page_va = PGROUNDDOWN(srcva);
    page_pa = walkaddr(pagetable, page_va);
    if (page_pa == 0)
      return -1; // 页面映射不存在或不可访问

    bytes_to_copy = bytes_to_copy_in_page(srcva, len);

    uint64 src_offset = srcva - page_va;
    memmove(dst, (void *)(page_pa + src_offset), bytes_to_copy);

    len -= bytes_to_copy;
    dst += bytes_to_copy;
    srcva = page_va + PGSIZE; // 移到下一页
  }
  return 0;
}

// 从用户复制一个以null结尾的字符串到内核
// 将字节从给定页表中的虚拟地址srcva复制到dst，
// 直到遇到'\0'或达到max
// 成功返回0，错误返回-1
int copyinstr(pagetable_t pagetable, char *dst, uint64 srcva, uint64 max)
{
  uint64 n, va0, pa0;
  int got_null = 0;

  while (got_null == 0 && max > 0)
  {
    va0 = PGROUNDDOWN(srcva);
    pa0 = walkaddr(pagetable, va0);
    if (pa0 == 0)
      return -1;
    n = PGSIZE - (srcva - va0);
    if (n > max)
      n = max;

    char *p = (char *)(pa0 + (srcva - va0));
    while (n > 0)
    {
      if (*p == '\0')
      {
        *dst = '\0';
        got_null = 1;
        break;
      }
      else
      {
        *dst = *p;
      }
      --n;
      --max;
      p++;
      dst++;
    }

    srcva = va0 + PGSIZE;
  }
  if (got_null)
  {
    return 0;
  }
  else
  {
    return -1;
  }
}
