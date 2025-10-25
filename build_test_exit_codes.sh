#!/bin/bash
# Build and test HTIF exit codes

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building HTIF exit code test..."

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
    -o rv64im_test_exit_codes.elf \
    rv64im_test_exit_codes.c

echo "Built successfully!"
echo ""
echo "Testing with failure exit code (1)..."
echo "========================================"
if [ -f "./build/spike" ]; then
    timeout 5 ./build/spike --isa=rv64im rv64im_test_exit_codes.elf
    echo "Exit code: $?"
else
    echo "Spike not found. Run manually:"
    echo "  ./build/spike --isa=rv64im rv64im_test_exit_codes.elf"
fi

