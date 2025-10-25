#!/bin/bash
# Test script to diagnose user's timeout issue

set -x  # Enable command tracing

cd /workspaces/riscv-isa-sim || exit 1

echo "=== Step 1: Remove old ELF ==="
rm -f rv64im_minimal.elf
ls -la rv64im_minimal.elf 2>&1

echo ""
echo "=== Step 2: Compile fresh ==="
riscv64-unknown-elf-gcc \
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

echo ""
echo "=== Step 3: Check tohost alignment ==="
riscv64-unknown-elf-nm rv64im_minimal.elf | grep -E '(tohost|fromhost)'

echo ""
echo "=== Step 4: Test with 2 second timeout ==="
echo "Starting test at: $(date +%H:%M:%S.%N)"
timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf
EXIT_CODE=$?
echo "Finished test at: $(date +%H:%M:%S.%N)"
echo "Exit code: $EXIT_CODE"

echo ""
echo "=== Step 5: Test with if statement (like build script) ==="
echo "Starting test at: $(date +%H:%M:%S.%N)"
if timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf; then
    echo "Success"
else
    EXIT_CODE=$?
    echo "Failed with exit code: $EXIT_CODE"
    if [ $EXIT_CODE -eq 124 ]; then
        echo "TIMEOUT DETECTED"
    fi
fi
echo "Finished test at: $(date +%H:%M:%S.%N)"

echo ""
echo "=== Step 6: Check which timeout command ==="
which timeout
type timeout
timeout --version 2>&1 | head -3

