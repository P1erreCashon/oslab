#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"

// start()函数在管理者模式下跳转到此处，所有CPU都会执行
void
main()
{

    // 只有CPU 0(引导处理器)执行系统初始化
    consoleinit();       // 初始化控制台
    printfinit();        // 初始化printf功能
    printf("\n");
    printf("Simple Bootloader is running!\n");
    printf("Hello from RISC-V kernel!\n");
    printf("\n");
    printf("Bootloader finished. System halted.\n");

    // 系统停止，进入无限循环
    for(;;)
        ;
}
