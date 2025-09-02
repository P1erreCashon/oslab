#ifndef ELF_LOADER_H
#define ELF_LOADER_H

#include "boot_types.h"

// ELF魔数
#define ELF_MAGIC 0x464C457F

// ELF文件头结构
struct elf_header {
    uint32 e_magic;     // ELF魔数
    uint8 e_ident[12];  // ELF标识
    uint16 e_type;      // 文件类型
    uint16 e_machine;   // 目标架构
    uint32 e_version;   // 文件版本
    uint64 e_entry;     // 入口点地址
    uint64 e_phoff;     // 程序头表偏移
    uint64 e_shoff;     // 节头表偏移
    uint32 e_flags;     // 处理器相关标志
    uint16 e_ehsize;    // ELF头大小
    uint16 e_phentsize; // 程序头大小
    uint16 e_phnum;     // 程序头数量
    uint16 e_shentsize; // 节头大小
    uint16 e_shnum;     // 节头数量
    uint16 e_shstrndx;  // 字符串表索引
};

// 程序头结构
struct program_header {
    uint32 p_type;      // 段类型
    uint32 p_flags;     // 段标志
    uint64 p_offset;    // 文件偏移
    uint64 p_vaddr;     // 虚拟地址
    uint64 p_paddr;     // 物理地址
    uint64 p_filesz;    // 文件中大小
    uint64 p_memsz;     // 内存中大小
    uint64 p_align;     // 对齐
};

// 段类型
#define PT_NULL     0   // 未使用
#define PT_LOAD     1   // 可加载段
#define PT_DYNAMIC  2   // 动态链接信息
#define PT_INTERP   3   // 解释器
#define PT_NOTE     4   // 注释信息

// 段标志
#define PF_X        1   // 可执行
#define PF_W        2   // 可写
#define PF_R        4   // 可读

// ELF加载状态
typedef enum {
    ELF_SUCCESS = 0,
    ELF_ERROR_INVALID_MAGIC = -1,
    ELF_ERROR_INVALID_ARCH = -2,
    ELF_ERROR_INVALID_PHNUM = -3,
    ELF_ERROR_LOAD_FAILED = -4,
    ELF_ERROR_MEMORY = -5
} elf_error_t;

// ELF加载信息
struct elf_load_info {
    uint64 entry_point;     // 内核入口点
    uint64 load_base;       // 加载基址
    uint64 load_size;       // 加载大小
    uint32 segment_count;   // 段数量
};

// 函数声明
elf_error_t elf_validate_header(struct elf_header *elf);
elf_error_t elf_load_kernel(char *elf_data, struct elf_load_info *info);
elf_error_t elf_load_segment(struct program_header *ph, char *elf_data);
void elf_clear_bss(struct program_header *ph);

#endif /* ELF_LOADER_H */
