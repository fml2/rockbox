#define ASM
#include "config.h"
#include "cpu.h"
#include "crypto-s5l8702.h"     /* IM3HDR_SZ */

ENTRY(start)
#ifdef ROCKBOX_LITTLE_ENDIAN
OUTPUT_FORMAT(elf32-littlearm)
#else
OUTPUT_FORMAT(elf32-bigarm)
#endif
OUTPUT_ARCH(arm)
STARTUP(target/arm/s5l8702/crt0.o)

#define MAX_LOADSIZE 8M  /* reserved for loading Rockbox binary */

#if defined(IPOD_6G) || defined(IPOD_NANO3G) || defined(IPOD_NANO4G)
MEMORY
{
    DRAM  : ORIGIN = DRAM_ORIG, LENGTH = DRAM_SIZE
    IRAM  : ORIGIN = IRAM_ORIG, LENGTH = IRAM_SIZE

    /* s5l8702/s5l8720 maps address 0 to ROM, IRAM or DRAM */
    VECT_AREA : ORIGIN = 0, LENGTH = 1K

    /* IRAM region where loaded IM3 body will be moved and executed,
       preserving the loaded IM3 header (0x600 or 0x800 bytes) at IRAM1_ORIG */
    MOVE_AREA : ORIGIN = IRAM1_ORIG + IM3HDR_SZ,
                LENGTH = IRAM1_SIZE - IM3HDR_SZ

    /* DRAM region for BSS */
    BSS_AREA  : ORIGIN = DRAM_ORIG + MAX_LOADSIZE,
                LENGTH = DRAM_SIZE - MAX_LOADSIZE - TTB_SIZE
}
#define LOAD_AREA   MOVE_AREA
#else
#error No target defined!
#endif


SECTIONS
{
  _movestart = LOADADDR(.text) ;
  _moveend = LOADADDR(.data) + SIZEOF(.data) ;

#ifdef NEEDS_INTVECT_COPYING
  .intvect : {
    _intvectstart = . ;
    *(.intvect)
    _intvectend = _newstart ;
  } >VECT_AREA AT> LOAD_AREA
  _intvectcopy = LOADADDR(.intvect) ;
#endif

  .text : {
#ifndef NEEDS_INTVECT_COPYING
    *(.intvect)
#endif
    *(.init.text)
    *(.text*)
    *(.glue_7*)
  } > LOAD_AREA

  .rodata : {
    *(.rodata*)
    . = ALIGN(0x4);
  } > LOAD_AREA

  .data : {
    _datastart = . ;
    *(.irodata*)
    *(.icode*)
    *(.idata*)
    *(.data*)
    *(.ncdata*);
    . = ALIGN(0x20);  /* align move size */
    _dataend = . ;
  } > LOAD_AREA

  .stack (NOLOAD) :
  {
     *(.stack)
     _stackbegin = .;
     stackbegin = .;
     . += 0x2000;
     _stackend = .;
     stackend = .;
     _irqstackbegin = .;
     . += 0x400;
     _irqstackend = .;
     _fiqstackbegin = .;
     . += 0x400;
     _fiqstackend = .;
  } > LOAD_AREA

  .bss (NOLOAD) : {
     _edata = .;
     *(.bss*);
     *(.ibss*);
     *(.ncbss*);
     *(COMMON);
    . = ALIGN(0x20);  /* align bzero size */
     _end = .;
  } > BSS_AREA
}
