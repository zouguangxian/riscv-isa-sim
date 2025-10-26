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
        *(.text.boot)    /* Boot code first */
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
    
    /* Align to 8 bytes before HTIF region */
    . = ALIGN(8);
    
    /* Place tohost and fromhost dynamically after other sections */
    .htif : ALIGN(8) {
        tohost = .;
        . = . + 8;
        fromhost = .;
        . = . + 8;
    } > RAM
    
    /* Stack at end of memory */
    PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
}

