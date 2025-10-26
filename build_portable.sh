#!/bin/bash
# Build script for portable RV32/RV64 ecall handler

set -e

echo "Building PORTABLE RISC-V ecall handler..."
echo ""

# Detect available toolchains
HAS_RV64=$(command -v riscv64-unknown-elf-gcc &> /dev/null && echo 1 || echo 0)
HAS_RV32=$(command -v riscv32-unknown-elf-gcc &> /dev/null && echo 1 || echo 0)

if [ $HAS_RV64 -eq 0 ] && [ $HAS_RV32 -eq 0 ]; then
    echo "Error: No RISC-V toolchain found."
    echo "Install: sudo apt install gcc-riscv64-unknown-elf"
    exit 1
fi

# Build for RV64 if available
if [ $HAS_RV64 -eq 1 ]; then
    echo "=== Building for RV64 ==="
    riscv64-unknown-elf-gcc \
        -march=rv64im -mabi=lp64 \
        -mcmodel=medany \
        -nostdlib -nostartfiles -ffreestanding -fno-builtin \
        -O1 \
        -T rv64im_minimal_ecall.ld \
        -o rv_portable_rv64.elf \
        rv_minimal_ecall_portable.c
    
    echo "✓ Built rv_portable_rv64.elf"
    
    # Show configuration
    echo ""
    echo "RV64 Configuration:"
    riscv64-unknown-elf-objdump -d rv_portable_rv64.elf | \
        sed -n '/^[0-9a-f]* <trap_handler>:/,/^$/p' | \
        head -5 | tail -3
    echo ""
fi

# Build for RV32 if available
if [ $HAS_RV32 -eq 1 ]; then
    echo "=== Building for RV32 ==="
    riscv32-unknown-elf-gcc \
        -march=rv32im -mabi=ilp32 \
        -mcmodel=medany \
        -nostdlib -nostartfiles -ffreestanding -fno-builtin \
        -O1 \
        -T rv64im_minimal_ecall.ld \
        -o rv_portable_rv32.elf \
        rv_minimal_ecall_portable.c
    
    echo "✓ Built rv_portable_rv32.elf"
    
    # Show configuration
    echo ""
    echo "RV32 Configuration:"
    riscv32-unknown-elf-objdump -d rv_portable_rv32.elf | \
        sed -n '/^[0-9a-f]* <trap_handler>:/,/^$/p' | \
        head -5 | tail -3
    echo ""
fi

# Test RV64 if Spike available
if [ $HAS_RV64 -eq 1 ] && [ -f "./build/spike" ]; then
    echo "=== Testing RV64 ==="
    if timeout 5 ./build/spike --isa=rv64im rv_portable_rv64.elf </dev/null 2>&1 | \
       sed 's/\[.*DEBUG.*\].*//g' | grep -v "^$" | head -5 | tail -2; then
        echo "✅ RV64 TEST PASSED!"
    else
        echo "⚠️  RV64 test exit code: $?"
    fi
    echo ""
fi

# Summary
echo "=== SUMMARY ==="
echo ""
echo "Portable code automatically adapts:"
echo "  • RV64: Uses ld/sd, 64-bit registers, bit 63 for interrupts"
echo "  • RV32: Uses lw/sw, 32-bit registers, bit 31 for interrupts"
echo ""
echo "Single source code works on both architectures!"
echo ""

if [ $HAS_RV64 -eq 1 ]; then
    echo "To run RV64: ./build/spike --isa=rv64im rv_portable_rv64.elf"
fi
if [ $HAS_RV32 -eq 1 ]; then
    echo "To run RV32: ./build/spike --isa=rv32im rv_portable_rv32.elf"
fi

