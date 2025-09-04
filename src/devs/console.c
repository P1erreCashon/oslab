//
// Console output, to the uart - simplified for bootloader.
//

#include "types.h"
#include "param.h"
#include "defs.h"

#define BACKSPACE 0x100

//
// send one character to the uart.
// called by printf().
//
void
consputc(int c)
{
  if(c == BACKSPACE){
    // if the user typed backspace, overwrite with a space.
    uartputc_sync('\b'); uartputc_sync(' '); uartputc_sync('\b');
  } else {
    uartputc_sync(c);
  }
}

void
consoleinit(void)
{
  uartinit();
}

