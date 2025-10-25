#!/bin/bash
# Build script for simple RV64IM C example

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building simple RV64IM C example..."

# Check if RISC-V toolchain is available
if ! command -v ${RISCV_PREFIX}gcc &> /dev/null; then
    echo "Error: RISC-V toolchain not found."
    echo ""
    echo "Install options:"
    echo "1. Package manager: sudo apt install gcc-riscv64-unknown-elf"
    echo "2. Docker: docker run --rm -v \$(pwd):/work -w /work riscv/riscv-gnu-toolchain"
    echo "3. Build from source: https://github.com/riscv/riscv-gnu-toolchain"
    exit 1
fi

echo "Compiling rv64im_simple.c..."

# Compile with very strict settings to ensure only RV64IM
${RISCV_PREFIX}gcc \
    -march=rv64im \
    -mabi=lp64 \
    -mcmodel=medany \
    -nostdlib \
    -nostartfiles \
    -ffreestanding \
    -fno-builtin \
    -O1 \
    -T rv64im_simple.ld \
    -o rv64im_simple.elf \
    rv64im_simple.c

echo "Built rv64im_simple.elf successfully!"
echo ""
echo "This program:"
echo "  - Uses only RV64IM instructions (no floating-point, atomics, etc.)"
echo "  - Performs basic integer arithmetic and multiplication/division"
echo "  - Tests 64-bit operations"
echo "  - Signals completion via tohost"
echo ""
echo "To run:"
echo "  ./build/spike --isa=rv64im rv64im_simple.elf"
echo ""
echo "Expected output: Program should run and exit cleanly"