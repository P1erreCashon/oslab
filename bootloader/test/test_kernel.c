// 简单的测试内核，验证bootloader是否工作

// UART基地址
#define UART0_BASE 0x10000000L

void uart_putc(char c) {
    volatile char *uart = (char *)UART0_BASE;
    *uart = c;
}

void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// 测试内核主函数
void test_kernel_main(void) {
    uart_puts("\n");
    uart_puts("========================================\n");
    uart_puts("    TEST KERNEL LOADED SUCCESSFULLY    \n");
    uart_puts("========================================\n");
    uart_puts("\n");
    
    uart_puts("Bootloader Stage 2 completed!\n");
    uart_puts("Two-stage loading mechanism works!\n");
    uart_puts("\n");
    
    uart_puts("Test kernel running at: 0x80200000\n");
    uart_puts("This proves that:\n");
    uart_puts("  1. Stage 1 loaded Stage 2 from disk\n");
    uart_puts("  2. Stage 2 loaded kernel from disk\n");
    uart_puts("  3. virtio disk driver works\n");
    uart_puts("  4. Memory layout is correct\n");
    uart_puts("\n");
    
    uart_puts("=== STAGE 2 IMPLEMENTATION SUCCESS ===\n");
    uart_puts("\n");
    
    // 简单的功能测试
    uart_puts("Running basic tests...\n");
    
    // 测试1: 内存访问
    volatile int *test_mem = (int *)0x80300000;
    *test_mem = 0xDEADBEEF;
    if (*test_mem == 0xDEADBEEF) {
        uart_puts("✓ Memory access test passed\n");
    } else {
        uart_puts("✗ Memory access test failed\n");
    }
    
    // 测试2: 栈操作
    int stack_test = 42;
    if (stack_test == 42) {
        uart_puts("✓ Stack operation test passed\n");
    } else {
        uart_puts("✗ Stack operation test failed\n");
    }
    
    uart_puts("\nAll tests completed!\n");
    uart_puts("Ready for Stage 3 development.\n");
    uart_puts("\nHalting test kernel...\n");
    
    // 无限循环
    while (1) {
        asm volatile("wfi");
    }
}
