// Physical memory layout - simplified for bootloader

// qemu -machine virt is set up like this:
// 10000000 -- uart0 
// 80000000 -- boot ROM jumps here in machine mode
//             -kernel loads the kernel here

// qemu puts UART registers here in physical memory.
#define UART0 0x10000000L
