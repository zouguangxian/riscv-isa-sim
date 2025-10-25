/* Minimal linker script for RV64IM minimal example */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x80000000;
    
    .text : {
        *(.text)
        *(.text.*)
    } > RAM
    
    .data : {
        *(.data)
        *(.data.*)
    } > RAM
    
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > RAM
    
    /* Place tohost and fromhost at a fixed location for HTIF */
    . = 0x80001000;
    .htif : {
        tohost = .;
        . = . + 8;
        fromhost = .;
        . = . + 8;
    } > RAM
    
    /* Stack at end of memory */
    PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
}