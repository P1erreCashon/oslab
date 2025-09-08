#include "types.h"
#include "param.h"
#include "memlayout.h"
#include "riscv.h"
#include "defs.h"

volatile static int started = 0;

// start()函数在管理者模式下跳转到此处，所有CPU都会执行
void
main()
{
    // 只有CPU 0(引导处理器)执行系统初始化
    consoleinit();       // 初始化控制台
    printfinit();        // 初始化printf功能
    printf("\n");
    printf("xv6 kernel is booting\n");
    printf("\n");
    kinit();             // 物理页面分配器初始化
    kvminit();           // 创建内核页表
    kvminithart();       // 开启分页机制

    trapinit();          // 陷阱向量初始化
    trapinithart();      // 安装内核陷阱向量
    plicinit();          // 设置中断控制器
    plicinithart();      // 向PLIC请求设备中断
    intr_on();
    while(1);
}
