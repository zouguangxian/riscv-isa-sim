#!/bin/bash
# Build simple HTIF test (with success exit code)

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building simple HTIF test..."

${RISCV_PREFIX}gcc \
    -march=rv64im \
    -mabi=lp64 \
    -mcmodel=medany \
    -nostdlib \
    -nostartfiles \
    -ffreestanding \
    -fno-builtin \
    -O1 \
    -T rv64im_minimal_ecall.ld \
    -o rv64im_minimal_ecall_simple.elf \
    rv64im_minimal_ecall_simple.c

echo "Built successfully!"
echo ""
echo "Testing with success exit code (0)..."
echo "========================================"
if [ -f "./build/spike" ]; then
    timeout 5 ./build/spike --isa=rv64im rv64im_minimal_ecall_simple.elf
    echo "Exit code: $?"
else
    echo "Spike not found. Run manually:"
    echo "  ./build/spike --isa=rv64im rv64im_minimal_ecall_simple.elf"
fi

