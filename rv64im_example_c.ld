/* Linker script for RV64IM C example */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
    /* Code section starts at the beginning */
    .text : {
        *(.text)
        *(.text.*)
    } > RAM
    
    /* Read-only data */
    .rodata : {
        *(.rodata)
        *(.rodata.*)
    } > RAM
    
    /* Initialized data */
    .data : {
        *(.data)
        *(.data.*)
    } > RAM
    
    /* HTIF communication section - tohost and fromhost symbols */
    .htif : {
        PROVIDE(tohost = .);
        . = . + 8;
        PROVIDE(fromhost = .);
        . = . + 8;
    } > RAM
    
    /* Uninitialized data */
    .bss : {
        *(.bss)
        *(.bss.*)
        *(COMMON)
    } > RAM
    
    /* Stack grows down from end of RAM */
    PROVIDE(_stack_top = ORIGIN(RAM) + LENGTH(RAM));
}