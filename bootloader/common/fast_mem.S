# RISC-V 汇编优化内存操作
.section .text
.global fast_memcpy
.global fast_memset

# 快速内存复制：fast_memcpy(dest, src, size)
# a0 = dest, a1 = src, a2 = size
fast_memcpy:
    # 边界检查
    beqz a2, 2f          # if size == 0, return
    
    # 8字节对齐复制
    andi t0, a2, 0x7     # t0 = size % 8 (remaining bytes)
    sub t1, a2, t0       # t1 = aligned size
    add t2, a1, t1       # t2 = src + aligned_size (end of 64-bit section)
    
1:  # 64位复制循环
    bge a1, t2, 3f       # if src >= end, goto byte copy
    ld t3, 0(a1)         # load 8 bytes from src
    sd t3, 0(a0)         # store 8 bytes to dest
    addi a1, a1, 8       # src += 8
    addi a0, a0, 8       # dest += 8
    j 1b                 # repeat
    
3:  # 字节复制循环（剩余字节）
    beqz t0, 2f          # if no remaining bytes, done
    add t2, a1, t0       # t2 = src + remaining (end of byte section)
4:
    bge a1, t2, 2f       # if src >= end, done
    lb t3, 0(a1)         # load 1 byte from src
    sb t3, 0(a0)         # store 1 byte to dest
    addi a1, a1, 1       # src += 1
    addi a0, a0, 1       # dest += 1
    j 4b                 # repeat
    
2:  # 返回
    ret

# 快速内存清零：fast_memset(ptr, size)
# a0 = ptr, a1 = size
fast_memset:
    # 边界检查
    beqz a1, 2f          # if size == 0, return
    
    # 8字节对齐清零
    andi t0, a1, 0x7     # t0 = size % 8 (remaining bytes)
    sub t1, a1, t0       # t1 = aligned size
    add t2, a0, t1       # t2 = ptr + aligned_size (end of 64-bit section)
    
1:  # 64位清零循环
    bge a0, t2, 3f       # if ptr >= end, goto byte clear
    sd zero, 0(a0)       # store 8 zero bytes
    addi a0, a0, 8       # ptr += 8
    j 1b                 # repeat
    
3:  # 字节清零循环（剩余字节）
    beqz t0, 2f          # if no remaining bytes, done
    add t2, a0, t0       # t2 = ptr + remaining (end of byte section)
4:
    bge a0, t2, 2f       # if ptr >= end, done
    sb zero, 0(a0)       # store 1 zero byte
    addi a0, a0, 1       # ptr += 1
    j 4b                 # repeat
    
2:  # 返回
    ret
