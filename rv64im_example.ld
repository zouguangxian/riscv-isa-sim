/* Linker script for RV64IM example */
OUTPUT_ARCH("riscv")
ENTRY(_start)

MEMORY
{
    RAM : ORIGIN = 0x80000000, LENGTH = 128M
}

SECTIONS
{
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
}