/* Linker script for RV64IM minimal example with ecall support */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    . = 0x80000000;
    
    .text : ALIGN(16) {
        *(.text)
        *(.text.*)
    } > RAM
    
    .rodata : ALIGN(16) {
        *(.rodata)
        *(.rodata.*)
    } > RAM
    
    .data : ALIGN(16) {
        *(.data)
        *(.data.*)
    } > RAM
    
    .bss : ALIGN(16) {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > RAM
    
    /* Place tohost and fromhost at a fixed location for HTIF */
    .htif 0x80001000 : {
        *(.htif)
        tohost = 0x80001000;
        LONG(0);
        LONG(0);
        fromhost = 0x80001008;
        LONG(0);
        LONG(0);
    } > RAM
    
    /* Stack at end of memory */
    PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
}

