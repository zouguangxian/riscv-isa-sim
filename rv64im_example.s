# Simple RISC-V RV64IM example
# This is a minimal program that uses only integer and multiplication instructions
# and can run with: spike --isa=rv64im program.elf

.section .text
.global _start
.global tohost
.global fromhost

_start:
    # Simple computation using only RV64IM instructions
    li t0, 10        # Load immediate 10
    li t1, 20        # Load immediate 20
    add t2, t0, t1   # t2 = 10 + 20 = 30
    
    # Test multiplication (M extension)
    li t3, 5
    mul t4, t2, t3   # t4 = 30 * 5 = 150
    
    # Test some 64-bit operations (RV64I)
    li t5, 0x100000000  # Load a 64-bit value
    addw t6, t4, zero   # 32-bit add (sign-extend result) - t6 = 150
    
    # Simple loop to demonstrate branching
    li s0, 0         # counter
    li s1, 5         # loop limit
    
loop:
    addi s0, s0, 1   # increment counter
    blt s0, s1, loop # if counter < 5, continue loop
    
    # Store result and exit
    # For RV64IM, we just need to signal completion via tohost
    li t0, 1         # Success code
    la t1, tohost
    sd t0, 0(t1)     # Store to tohost to signal completion
    
    # Infinite loop (in case tohost doesn't work)
infinite_loop:
    j infinite_loop

# HTIF communication symbols
.section .data
.align 8
tohost:
    .8byte 0
fromhost:
    .8byte 0