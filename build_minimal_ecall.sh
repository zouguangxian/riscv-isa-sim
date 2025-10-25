#!/bin/bash
# Build script for minimal RV64IM C example with ecall support

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "Building minimal RV64IM C example with ecall support..."

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

echo "Compiling rv64im_minimal_ecall.c..."

# Compile with ecall support
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
    -o rv64im_minimal_ecall.elf \
    rv64im_minimal_ecall.c

echo "Built rv64im_minimal_ecall.elf successfully!"
echo ""
echo "This program:"
echo "  - Uses RV64IM instructions only"
echo "  - Implements ecall instruction for syscalls"
echo "  - Handles SYS_write (syscall 64) via HTIF putchar"
echo "  - Outputs text to stdout when fd=1"
echo "  - Includes a trap handler for ecall exceptions"
echo ""
echo "Expected output:"
echo "  Hello from ecall!"
echo "  Testing SYS_write syscall via ecall instruction"
echo "  Character output: ABCDEFGHIJKLMNOPQRSTUVWXYZ"
echo "  Computation test: 42 * 13 = 546 (calculated)"
echo "  All ecall tests completed successfully!"
echo ""

# Test run with timeout
if [ -f "./build/spike" ]; then
    echo "Running test with 5 second timeout..."
    echo "----------------------------------------"
    # Redirect stdin from /dev/null to prevent blocking
    if timeout 5 ./build/spike --isa=rv64im rv64im_minimal_ecall.elf </dev/null; then
        echo "----------------------------------------"
        echo "Test completed successfully!"
    else
        EXIT_CODE=$?
        if [ $EXIT_CODE -eq 124 ]; then
            echo "----------------------------------------"
            echo "WARNING: Test timed out after 5 seconds"
        else
            echo "----------------------------------------"
            echo "ERROR: Test failed with exit code $EXIT_CODE"
        fi
    fi
else
    echo "Spike not found at ./build/spike"
    echo "To run manually:"
    echo "  ./build/spike --isa=rv64im rv64im_minimal_ecall.elf"
fi

