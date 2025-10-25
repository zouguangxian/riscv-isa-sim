#!/bin/bash
# Build script for minimal RV64IM C example (no symbol issues)

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building minimal RV64IM C example..."

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

echo "Compiling rv64im_minimal.c..."

# Simple compilation without external symbols
${RISCV_PREFIX}gcc \
    -march=rv64im \
    -mabi=lp64 \
    -mcmodel=medany \
    -nostdlib \
    -nostartfiles \
    -ffreestanding \
    -fno-builtin \
    -O1 \
    -T rv64im_minimal.ld \
    -o rv64im_minimal.elf \
    rv64im_minimal.c

echo "Built rv64im_minimal.elf successfully!"
echo ""
echo "This program:"
echo "  - Uses fixed addresses for tohost/fromhost (avoids symbol issues)"
echo "  - Uses only RV64IM instructions"
echo "  - Performs integer arithmetic and M-extension operations"
echo "  - Signals completion via tohost at address 0x80001000"
echo ""
echo "To run:"
echo "  ./build/spike --isa=rv64im rv64im_minimal.elf"
echo ""
echo "Expected: Program runs and exits cleanly"

echo "Running..."

# Capture exit code (disable set -e for this command)
# Redirect stdin from /dev/null to prevent blocking
if timeout 5 ./build/spike --isa=rv64im rv64im_minimal.elf </dev/null; then
    echo "Exit code: 0 (success)"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo "WARNING: Test timed out after 5 seconds"
    else
        echo "Exit code: $EXIT_CODE (failure)"
    fi
fi