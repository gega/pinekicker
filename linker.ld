MEMORY
{
  FLASH (rx) : ORIGIN = 0x00000000, LENGTH = 0x6000
  RAM (rwx) : ORIGIN = 0x20000000, LENGTH = 0x10000
}

/* disable mynewt image header */
_imghdr_size = 0x0;

SECTIONS
{
  /* RAM function for the flash writer function */
  .ramfunc :
  {
    . = ALIGN(8);
    __ramfunc_start__ = .;
    KEEP(*(.ramfunc .ramfunc.*))
    __ramfunc_end__ = .;
  } > RAM AT > FLASH

  __ramfunc_load_start__ = LOADADDR(.ramfunc);
  __ramfunc_load_end__   = __ramfunc_load_start__ + SIZEOF(.ramfunc);
} INSERT AFTER .text;
