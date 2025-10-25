#!/bin/bash
# Build script for RV64IM example

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building RV64IM example..."

# Check if RISC-V toolchain is available
if ! command -v ${RISCV_PREFIX}gcc &> /dev/null; then
    echo "Error: RISC-V toolchain not found. Please install riscv64-unknown-elf-gcc"
    exit 1
fi

# Assemble and link
${RISCV_PREFIX}as -march=rv64im -mabi=lp64 -o rv64im_example.o rv64im_example.s
${RISCV_PREFIX}ld -T rv64im_example.ld -o rv64im_example.elf rv64im_example.o

echo "Built rv64im_example.elf"
echo ""
echo "To run with spike:"
echo "  spike --isa=rv64im rv64im_example.elf"
echo ""
echo "Or run without pk (bare metal):"
echo "  spike --isa=rv64im --pc=0x80000000 rv64im_example.elf"