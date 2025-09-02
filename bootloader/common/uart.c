#include "boot_types.h"

#define UART0_BASE 0x10000000L

// 基础字符输出
void uart_putc(char c) {
    volatile uint8 *uart = (uint8 *)UART0_BASE;
    *uart = c;
}

// 字符串输出
void uart_puts(const char *s) {
    while (*s) {
        uart_putc(*s++);
    }
}

// 十六进制输出 (调试用)
void uart_put_hex(uint64 val) {
    const char digits[] = "0123456789ABCDEF";
    uart_puts("0x");
    
    int started = 0;
    for (int i = 60; i >= 0; i -= 4) {
        uint8 digit = (val >> i) & 0xF;
        if (digit != 0 || started || i == 0) {
            uart_putc(digits[digit]);
            started = 1;
        }
    }
}

// 数字输出
void uart_put_dec(uint64 val) {
    if (val == 0) {
        uart_putc('0');
        return;
    }
    
    char buf[32];
    int i = 0;
    
    while (val > 0) {
        buf[i++] = '0' + (val % 10);
        val /= 10;
    }
    
    while (i > 0) {
        uart_putc(buf[--i]);
    }
}

// 格式化输出内存大小
void uart_put_memsize(uint64 bytes) {
    if (bytes < 1024) {
        uart_put_dec(bytes);
        uart_puts(" bytes");
    } else if (bytes < 1024 * 1024) {
        uart_put_dec(bytes / 1024);
        uart_puts(" KB");
    } else {
        uart_put_dec(bytes / (1024 * 1024));
        uart_puts(" MB");
    }
}
