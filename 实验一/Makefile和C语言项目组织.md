## 理解 Makefile，怎么用 Makefile
在 Linux/Unix 开发中，**Makefile** 是用来自动化编译的工具说明文件，配合 `make` 命令使用。
1. **为什么需要 Makefile**
   * 如果你有很多 `.c` 文件，要手动写命令编译很麻烦：

     ```bash
     gcc -o myprog main.c file1.c file2.c ...
     ```
   * 修改了一个 `.c` 文件后，只需重新编译它，而不是整个工程。
   * Makefile 就是用来**描述依赖关系和编译规则**，让 `make` 工具自动完成增量编译。
2. **Makefile 基本语法**
   ```makefile
   target: dependencies
   	commands
   ```
   * **target**：目标文件（如可执行程序、.o 文件）。
   * **dependencies**：依赖文件（比如 .c、.h）。
   * **commands**：生成目标的命令（必须以 Tab 开头）。
3. **简单示例**
   例子：有 `main.c`、`foo.c`、`foo.h`，要编译成 `app`：
   ```makefile
   app: main.o foo.o
   	gcc -o app main.o foo.o

   main.o: main.c foo.h
   	gcc -c main.c

   foo.o: foo.c foo.h
   	gcc -c foo.c

   clean:
   	rm -f *.o app
   ```
   * 执行 `make` → 会自动编译生成 `app`。
   * 修改 `foo.c` 再执行 `make` → 只会重新编译 `foo.o`，节省时间。
   * 执行 `make clean` → 删除编译结果。
4. **常用内置变量**
   * `$@` → 目标文件名
   * `$^` → 所有依赖文件
   * `$<` → 第一个依赖文件
   例如：
   ```makefile
   app: main.o foo.o
   	gcc -o $@ $^
   ```

## Xv6编译流程

当开发者在 xv6 工程根目录执行 make 或 make qemu 命令时，Make 工具首先读取根目录下的 Makefile 文件。Makefile 可以理解为整个工程的"构建说明书"，其中定义了项目中包含哪些源文件、使用什么编译器、采用哪些编译参数，以及各个文件之间的依赖关系。Make 会根据这些规则自动决定需要执行哪些编译和链接操作，而不需要开发者手动输入复杂的命令。

第一阶段：配置编译环境。 Makefile 首先定义了工程中使用的目录、交叉编译工具链以及编译参数。例如，变量 K 和 U 分别表示 kernel 和 user 两个目录；CC、LD、OBJDUMP 等变量分别表示编译器、链接器和反汇编工具。Makefile 还会自动检测系统中安装的是哪一种 RISC-V 交叉编译工具链（如 riscv64-unknown-elf-gcc 或 riscv64-linux-gnu-gcc），然后将其作为整个工程使用的编译工具。同时，CFLAGS 中设置了统一的编译参数，例如开启警告、生成调试信息、关闭标准库支持以及指定目标架构等，这些参数会作用于所有源文件。

第二阶段：编译内核源文件。 Makefile 根据 OBJS 变量列出的源文件列表，依次将 kernel 目录下的 C 文件和汇编文件编译为目标文件（.o 文件）。例如，main.c 会生成 main.o，vm.c 会生成 vm.o，trap.c 会生成 trap.o。目标文件中已经包含了机器指令，但各个模块仍然彼此独立，尚未组成一个完整的操作系统。由于 Makefile 使用了自动依赖机制（-MD），当某个源文件发生修改时，只需要重新编译对应的目标文件，而无需重新编译整个工程，从而提高了编译效率。

第三阶段：生成内核程序。 当所有目标文件编译完成后，Makefile 调用链接器 ld，按照 kernel/kernel.ld 链接脚本规定的内存布局，将所有目标文件链接成最终的内核程序 kernel/kernel。链接过程中，链接器会解析各个目标文件之间的函数调用关系，为所有符号分配最终地址，并按照链接脚本将 .text、.rodata、.data 和 .bss 等代码段和数据段放置到指定位置。随后，Makefile 还会利用 objdump 自动生成反汇编文件 kernel.asm 和符号表文件 kernel.sym，方便开发者分析生成的内核。

第四阶段：编译用户程序。 除了内核之外，xv6 还需要编译运行在用户态的程序。Makefile 首先生成系统调用接口文件 usys.S，然后编译用户程序公共库（如 ulib.c、printf.c、umalloc.c 等），最后分别编译 cat、echo、ls、sh 等所有用户程序，并将每个程序分别链接成独立的 ELF 可执行文件。例如，cat.c 会生成 _cat，ls.c 会生成 _ls。每个用户程序都可以独立运行，并最终被写入文件系统镜像。

第五阶段：生成文件系统镜像。 用户程序编译完成后，Makefile 会先编译 mkfs 工具，然后执行 mkfs，将所有用户程序以及 README 等文件打包生成文件系统镜像 fs.img。该镜像模拟了 xv6 的磁盘内容，在 QEMU 启动后，操作系统便可以从这个镜像中读取用户程序，实现文件管理和程序加载。

第六阶段：启动 QEMU。 如果执行的是 make qemu，那么在内核和文件系统镜像都生成完成后，Makefile 会自动调用 qemu-system-riscv64。启动命令中指定了内核镜像 kernel/kernel、文件系统镜像 fs.img、内存大小、CPU 数量以及 VirtIO 磁盘设备等参数。QEMU 随后模拟一台完整的 RISC-V 虚拟计算机，并加载刚刚编译完成的 xv6 内核开始运行。如果执行的是 make qemu-gdb，则还会开启 GDB 调试接口，方便开发者进行源码级调试。

## C语言项目组织

在本实验中，你的项目会有许多 .h 文件，.c 文件，.S汇编文件，.ld链接脚本文件。

对于每一个 .c 源文件，通常都会有一个或多个对应的头文件（.h 文件）。源文件中主要实现函数和变量，而头文件则负责声明这些函数、数据结构和宏定义，供其他源文件引用。例如，proc.c 实现了进程管理相关功能，而 proc.h 则定义了进程控制块等数据结构；fs.c 实现文件系统功能，而 fs.h 定义了文件系统使用的数据结构和常量。***当一个源文件需要使用其他模块提供的函数或数据结构时，只需通过 #include 引入相应的头文件，而不需要直接访问其他源文件***。此外，在本实验中，***推荐每个 .c 文件都首先包含自己对应的 .h 文件***，让编译器帮助检查接口的一致性，这也是大型 C 工程普遍遵循的编程规范。

``` 
#print.c库函数include示例
#include <stdarg.h>
#include "lib/print.h"
#include "lib/lock.h"
#include "dev/uart.h"

volatile int panicked = 0;

// 防止占用的lock
static struct {
  struct spinlock print_lk;
  int locking;
} pr;

static char digits[] = "0123456789abcdef";

void print_init(void)
{
  uart_init();
  spinlock_init(&pr.print_lk, "pr");
  pr.locking = 1;
}

// 辅助函数
static void
printint(int xx, int base, int sign)
{
  char buf[16];
  int i;
  uint32 x; // 这里调整，加大位宽

  if(sign && (sign = xx < 0))
    x = -xx;
  else
    x = xx;

  i = 0;
  do {
    buf[i++] = digits[x % base];
  } while((x /= base) != 0);

  if(sign)
    buf[i++] = '-';

  while(--i >= 0)
    uart_putc_sync(buf[i]);
}
······
```

***对于 .h 文件，你需要开发时在 .h 文件中定义你需要的变量、常量和函数，并写好Makefile里头文件的搜索路径。*** Makefile 并不会自动搜索整个工程中的所有源文件，而是通过变量明确指定需要参与编译的文件。例如，在 xv6 的 Makefile 中，OBJS 变量列出了所有需要编译的内核目标文件，UPROGS 变量列出了所有需要生成的用户程序。因此，当开发者新增一个内核模块时，不仅需要在 kernel 目录中添加对应的 .c 和 .h 文件，还需要将新生成的目标文件加入 OBJS；如果新增一个用户程序，则需要将程序加入 UPROGS。这样，Makefile 才会在编译过程中处理这些新增文件。

为了使不同目录中的文件能够互相引用，Makefile 还通过编译参数指定了头文件搜索路径。例如，编译选项中的 -I. 表示编译器会在工程根目录查找头文件，在编译部分文件时还会增加 -Ikernel，表示同时搜索 kernel 目录中的头文件。这样，无论源文件位于哪个目录，只要使用 #include "xxx.h"，编译器就能够找到对应的头文件，而开发者无需关心头文件的具体存放位置。


## 全局变量和函数里变量如何分配

在 C/C++ 程序里，变量根据 **作用域(scope)** 和 **存储类型(storage class)** 会被放在不同的内存区域：
1. **全局变量（global variables）**
   * 定义在函数外的变量（无论是 `int g;` 还是带 `static` 的）属于全局变量。
   * **内存位置**：存放在 **静态存储区（data segment）**。
     * 已初始化的全局变量 → `.data` 段。
     * 未初始化的全局变量 → `.bss` 段。
   * 生命周期：程序运行期间始终存在，从程序启动到结束。
   * 作用域：
     * 普通全局变量 → 整个文件都能访问，外部 `extern` 声明后可在其他文件使用。
     * `static` 全局变量 → 仅限当前文件访问。

2. **函数内的局部变量（local variables）**
   * 在函数内部定义的变量，如 `int x = 5;`。
   * **内存位置**：一般在 **栈（stack）** 上分配。
   * 生命周期：函数调用时创建，函数返回时销毁。
   * 注意：返回局部变量的地址会产生野指针问题。

3. **`static` 局部变量**
   * 定义在函数内但带 `static` 的变量，如：
     ```c
     void foo() {
         static int counter = 0;
         counter++;
         printf("%d\n", counter);
     }
     ```
   * **内存位置**：在 **静态存储区**（和全局变量一样），但作用域只在函数内。
   * 生命周期：程序运行期间一直存在，不会因函数退出而销毁。