/* Simple linker script for RV64IM example */
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
    
    .rodata : {
        *(.rodata)
        *(.rodata.*)
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
    
    /* Place tohost and fromhost at a well-known location */
    . = ALIGN(8);
    tohost = .;
    . = . + 8;
    fromhost = .;
    . = . + 8;
    
    /* Stack at end of memory */
    PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
}