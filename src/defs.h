// defs.h - simplified for bootloader

struct spinlock;

// console.c
void            consoleinit(void);
void            consputc(int);

// printf.c
void            printf(char*, ...);
void            printfinit(void);

// uart.c
void            uartinit(void);
void            uartputc_sync(int);

// proc.c
int             cpuid(void);
