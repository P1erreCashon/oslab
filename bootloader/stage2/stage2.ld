ENTRY(_start)

MEMORY
{
    RAM : ORIGIN = 0x80030000, LENGTH = 128M
}

SECTIONS
{
    . = 0x80030000;
  
  .text : {
    *(.text)
    *(.text.*)
  }
  
  .rodata : {
    *(.rodata)
    *(.rodata.*)
  }
  
  .data : {
    *(.data)
    *(.data.*)
  }
  
  .bss : {
    *(.bss)
    *(.bss.*)
    *(COMMON)
  }
  
  . = ALIGN(8);
  _end = .;
}
