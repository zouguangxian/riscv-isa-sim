#!/bin/bash
# Build script for RV64IM C example

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building RV64IM C example..."

# Check if RISC-V toolchain is available
if ! command -v ${RISCV_PREFIX}gcc &> /dev/null; then
    echo "Error: RISC-V toolchain not found. Please install riscv64-unknown-elf-gcc"
    echo "You can install it with:"
    echo "  sudo apt install gcc-riscv64-unknown-elf"
    echo "Or build from: https://github.com/riscv/riscv-gnu-toolchain"
    exit 1
fi

# Compile with strict RV64IM ISA
echo "Compiling with RV64IM ISA..."
${RISCV_PREFIX}gcc \
    -march=rv64im \
    -mabi=lp64 \
    -nostdlib \
    -nostartfiles \
    -O2 \
    -T rv64im_example_c.ld \
    -o rv64im_example_c.elf \
    rv64im_example.c

echo "Built rv64im_example_c.elf"

# Verify the binary uses only RV64IM instructions
echo ""
echo "Checking for unsupported instructions..."
if command -v ${RISCV_PREFIX}objdump &> /dev/null; then
    # Look for any floating-point, atomic, or compressed instructions
    ${RISCV_PREFIX}objdump -d rv64im_example_c.elf > disasm.txt
    
    # Check for floating-point instructions (should be none)
    if grep -E "(fadd|fsub|fmul|fdiv|flw|fsw|fld|fsd)" disasm.txt > /dev/null; then
        echo "WARNING: Found floating-point instructions!"
        grep -E "(fadd|fsub|fmul|fdiv|flw|fsw|fld|fsd)" disasm.txt
    fi
    
    # Check for atomic instructions (should be none)
    if grep -E "(lr\.|sc\.|amo)" disasm.txt > /dev/null; then
        echo "WARNING: Found atomic instructions!"
        grep -E "(lr\.|sc\.|amo)" disasm.txt
    fi
    
    # Check for compressed instructions (should be none)
    if grep -E "\.2byte|c\." disasm.txt > /dev/null; then
        echo "WARNING: Found compressed instructions!"
        grep -E "\.2byte|c\." disasm.txt
    fi
    
    echo "Instruction check complete. If no warnings appeared, the binary should work with --isa=rv64im"
    rm -f disasm.txt
fi

echo ""
echo "To run with spike:"
echo "  spike --isa=rv64im rv64im_example_c.elf"
echo ""
echo "Or run without pk (bare metal):"
echo "  spike --isa=rv64im --pc=0x80000000 rv64im_example_c.elf"
echo ""
echo "Expected behavior:"
echo "  - Program performs various integer/multiplication operations"
echo "  - Should exit cleanly when tohost is written"
echo "  - No floating-point, atomic, or compressed instructions used"