//
// Minimal process management - simplified for bootloader
//

#include "types.h"
#include "param.h"
#include "riscv.h"
#include "defs.h"

// Return this CPU's id
int
cpuid()
{
  int id = r_tp();
  return id;
}
