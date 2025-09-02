#include "elf_loader.h"
#include "boot_types.h"

// 验证ELF文件头
elf_error_t elf_validate_header(struct elf_header *elf) {
    // 检查ELF魔数
    if (elf->e_magic != ELF_MAGIC) {
        debug_print("Invalid ELF magic number");
        return ELF_ERROR_INVALID_MAGIC;
    }
    
    // 检查架构 (RISC-V 64位)
    if (elf->e_machine != 0xF3) { // EM_RISCV
        debug_print("Invalid architecture, not RISC-V");
        return ELF_ERROR_INVALID_ARCH;
    }
    
    // 检查程序头数量
    if (elf->e_phnum == 0 || elf->e_phnum > 16) {
        debug_print("Invalid program header count");
        return ELF_ERROR_INVALID_PHNUM;
    }
    
    return ELF_SUCCESS;
}

// 清零BSS段（未初始化数据）
void elf_clear_bss(struct program_header *ph) {
    if (ph->p_memsz <= ph->p_filesz) {
        return; // 没有BSS段
    }
    
    uint64 bss_start = ph->p_vaddr + ph->p_filesz;
    uint64 bss_size = ph->p_memsz - ph->p_filesz;
    
    uart_puts("Clearing BSS: ");
    uart_put_hex(bss_start);
    uart_puts(" size ");
    uart_put_hex(bss_size);
    uart_puts("\n");
    
    // 临时限制BSS清除大小进行测试
    if (bss_size > 1024) {
        uart_puts("WARNING: Large BSS, clearing only 1KB for test\n");
        bss_size = 1024;
    }
    
    uart_puts("Starting BSS clear...\n");
    // 快速清零
    char *bss_ptr = (char *)bss_start;
    for (uint64 i = 0; i < bss_size; i++) {
        bss_ptr[i] = 0;
        if ((i & 0xFF) == 0) { // 每256字节显示进度
            uart_putc('.');
        }
    }
    uart_puts("BSS cleared\n");
}

// 加载单个程序段
elf_error_t elf_load_segment(struct program_header *ph, char *elf_data) {
    if (ph->p_type != PT_LOAD) {
        return ELF_SUCCESS; // 忽略非加载段
    }
    
    uart_puts("Loading segment: ");
    uart_put_hex(ph->p_vaddr);
    uart_puts(" size ");
    uart_put_hex(ph->p_filesz);
    uart_puts(" mem ");
    uart_put_hex(ph->p_memsz);
    uart_puts("\n");
    
    // 检查地址合理性
    if (ph->p_vaddr < 0x80000000 || ph->p_vaddr >= 0x88000000) {
        uart_puts("WARNING: Segment address outside expected range\n");
    }
    
    // 复制文件数据到内存 - 测试版本（限制大小）
    char *dest = (char *)ph->p_vaddr;
    char *src = elf_data + ph->p_offset;
    
    uint64 copy_size = ph->p_filesz;
    // 临时限制：只复制前4KB来测试
    if (copy_size > 4096) {
        uart_puts("WARNING: Large segment, copying only 4KB for test\n");
        copy_size = 4096;
    }
    
    uart_puts("Copying ");
    uart_put_hex(copy_size);
    uart_puts(" bytes...\n");

    // 简单字节复制
    for (uint64 i = 0; i < copy_size; i++) {
        dest[i] = src[i];
        if ((i & 0x3FF) == 0) { // 每1KB显示进度
            uart_putc('.');
        }
    }
    uart_puts(" done\n");
    
    // 清零BSS部分
    elf_clear_bss(ph);
    
    return ELF_SUCCESS;
}

// 加载完整的ELF内核
elf_error_t elf_load_kernel(char *elf_data, struct elf_load_info *info) {
    struct elf_header *elf = (struct elf_header *)elf_data;
    
    uart_puts("=== ELF Kernel Loader ===\n");
    
    // 验证ELF头
    elf_error_t err = elf_validate_header(elf);
    if (err != ELF_SUCCESS) {
        uart_puts("ELF validation failed: ");
        uart_put_dec(err);
        uart_puts("\n");
        return err;
    }
    
    uart_puts("Valid ELF file detected\n");
    uart_puts("Entry point: ");
    uart_put_hex(elf->e_entry);
    uart_puts("\n");
    uart_puts("Program headers: ");
    uart_put_dec(elf->e_phnum);
    uart_puts(" at offset ");
    uart_put_hex(elf->e_phoff);
    uart_puts("\n");
    
    // 初始化加载信息
    info->entry_point = elf->e_entry;
    info->load_base = 0xFFFFFFFFFFFFFFFF; // 最大值，后续更新为最小值
    info->load_size = 0;
    info->segment_count = 0;
    
    // 获取程序头表
    struct program_header *ph_table = (struct program_header *)(elf_data + elf->e_phoff);
    
    // 第一次遍历：收集信息
    for (int i = 0; i < elf->e_phnum; i++) {
        struct program_header *ph = &ph_table[i];
        
        if (ph->p_type == PT_LOAD) {
            info->segment_count++;
            
            // 更新加载范围
            if (ph->p_vaddr < info->load_base) {
                info->load_base = ph->p_vaddr;
            }
            
            uint64 segment_end = ph->p_vaddr + ph->p_memsz;
            uint64 current_end = info->load_base + info->load_size;
            if (segment_end > current_end) {
                info->load_size = segment_end - info->load_base;
            }
        }
    }
    
    uart_puts("Load info: base ");
    uart_put_hex(info->load_base);
    uart_puts(" size ");
    uart_put_hex(info->load_size);
    uart_puts(" segments ");
    uart_put_dec(info->segment_count);
    uart_puts("\n");
    
    // 第二次遍历：实际加载
    for (int i = 0; i < elf->e_phnum; i++) {
        struct program_header *ph = &ph_table[i];
        
        err = elf_load_segment(ph, elf_data);
        if (err != ELF_SUCCESS) {
            uart_puts("Failed to load segment ");
            uart_put_dec(i);
            uart_puts("\n");
            return err;
        }
    }
    
    uart_puts("Kernel loaded successfully\n");
    return ELF_SUCCESS;
}
