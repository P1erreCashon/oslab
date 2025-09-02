# 技术实现细节

## 1. RISC-V汇编实现分析

### 1.1 启动入口点
```assembly
.section .text
.global _start

_start:
    # 设置栈指针到安全地址
    li sp, 0x80000000
```

**技术要点：**
- 使用`li`指令加载立即数到栈指针
- 选择`0x80000000`作为栈基址，这是QEMU RISC-V virt machine的RAM起始地址
- 栈向下增长，这个地址提供了足够的栈空间

### 1.2 UART串口通信
```assembly
    # UART基地址 (QEMU virt machine)
    li t0, 0x10000000
    
    # 输出字符
    li t1, 'B'
    sb t1, 0(t0)
```

**技术要点：**
- QEMU virt machine的UART0映射在物理地址`0x10000000`
- 使用`sb`指令（store byte）直接写入UART数据寄存器
- 每次写入一个字符，UART会自动发送到串口

### 1.3 等待指令与循环
```assembly
halt:
    wfi      # Wait For Interrupt
    j halt   # 无条件跳转，形成无限循环
```

**技术要点：**
- `wfi`指令让CPU进入低功耗等待状态
- `j halt`形成无限循环，防止程序继续执行
- 这是bootloader的临时结束，后续会替换为跳转到第二阶段

## 2. 构建系统分析

### 2.1 编译过程
```makefile
$(CC) $(CFLAGS) -c boot.S -o boot.o
```

**编译参数详解：**
- `-march=rv64g`: 指定RISC-V 64位指令集，包含IMAFD扩展
- `-mabi=lp64`: 使用LP64 ABI（Long and Pointer 64-bit）
- `-static`: 生成静态链接的二进制文件
- `-mcmodel=medany`: 使用medium any代码模型，支持任意地址
- `-fno-common`: 不使用common段，提高兼容性
- `-nostdlib`: 不链接标准C库
- `-mno-relax`: 禁用链接时指令优化

### 2.2 链接过程
```makefile
$(LD) -Ttext 0x80000000 -o boot.elf boot.o
```

**链接参数详解：**
- `-Ttext 0x80000000`: 设置代码段起始地址
- 这个地址是QEMU RISC-V虚拟机的物理RAM起始位置
- ELF格式包含符号信息，便于调试

### 2.3 二进制转换
```makefile
$(OBJCOPY) -O binary boot.elf boot.bin
```

**转换说明：**
- 从ELF格式转换为纯二进制格式
- 移除所有元数据，只保留代码和数据
- 结果是60字节的纯机器码

## 3. QEMU虚拟机配置

### 3.1 硬件配置
```bash
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic
```

**参数详解：**
- `-machine virt`: 使用QEMU的虚拟RISC-V机器
- `-bios none`: 不加载BIOS，直接执行内核
- `-kernel bootloader/boot.bin`: 将bootloader作为"内核"加载
- `-m 128M`: 分配128MB内存
- `-nographic`: 无图形界面，使用串口控制台

### 3.2 内存映射
QEMU virt machine的标准内存布局：
```
0x00000000 - 0x00000FFF : 设备树信息
0x00001000 - 0x00000FFF : Boot ROM
0x02000000 : CLINT (Core Local Interruptor)
0x0C000000 : PLIC (Platform Level Interrupt Controller)  
0x10000000 : UART0 控制寄存器
0x10001000 : VirtIO MMIO
0x80000000 : 物理RAM起始地址 (我们的bootloader加载在这里)
```

## 4. 汇编指令分析

### 4.1 立即数加载
```assembly
li sp, 0x80000000    # 伪指令，实际编译为多条指令
```

编译器会将大立即数拆分为：
```assembly
lui sp, 0x80000      # 加载高20位
addi sp, sp, 0       # 加载低12位（本例中为0）
```

### 4.2 内存访问
```assembly
sb t1, 0(t0)         # store byte: 将t1的低8位存储到地址t0+0
```

这是RISC-V的基本加载/存储指令格式：
- `sb`: store byte (8位)
- `sh`: store halfword (16位)  
- `sw`: store word (32位)
- `sd`: store doubleword (64位)

### 4.3 控制流
```assembly
j halt               # 无条件跳转到标签halt
wfi                  # 等待中断指令
```

- `j`是无条件跳转伪指令，实际编译为`jal x0, offset`
- `wfi`让CPU进入低功耗状态，等待中断唤醒

## 5. 调试技巧

### 5.1 使用objdump查看汇编
```bash
riscv64-unknown-elf-objdump -d bootloader/boot.elf
```

### 5.2 查看符号表
```bash  
riscv64-unknown-elf-objdump -t bootloader/boot.elf
```

### 5.3 查看十六进制内容
```bash
hexdump -C bootloader/boot.bin
```

### 5.4 GDB调试
```bash
# 启动QEMU等待调试器
qemu-system-riscv64 -machine virt -bios none -kernel bootloader/boot.bin -m 128M -nographic -s -S

# 另一终端启动GDB
riscv64-unknown-elf-gdb bootloader/boot.elf
(gdb) target remote localhost:1234
(gdb) info registers
(gdb) x/10i $pc
```

## 6. 常见问题排查

### 6.1 没有输出
**可能原因：**
- UART地址错误
- 指令集不匹配
- 内存地址冲突

**排查方法：**
- 检查UART基地址是否为`0x10000000`
- 确认使用`rv64g`指令集
- 验证加载地址`0x80000000`

### 6.2 构建失败
**可能原因：**
- 工具链未安装或版本不匹配
- 编译参数错误

**解决方案：**
- 安装RISC-V GNU工具链
- 检查PATH环境变量
- 验证工具链版本兼容性

### 6.3 QEMU无法启动
**可能原因：**
- QEMU RISC-V支持未安装
- 内存配置不当

**解决方案：**
- 安装qemu-system-riscv64
- 检查QEMU版本和RISC-V支持
