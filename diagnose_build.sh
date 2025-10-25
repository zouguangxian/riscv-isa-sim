#!/bin/bash
# Diagnostic script to identify why build_minimal.sh might be timing out

echo "=== Diagnostic Information ==="
echo ""

echo "1. Current directory:"
pwd
echo ""

echo "2. Checking if spike exists:"
ls -la ./build/spike 2>&1 | head -1
echo ""

echo "3. Checking current rv64im_minimal.elf:"
if [ -f rv64im_minimal.elf ]; then
    ls -la rv64im_minimal.elf
    echo "tohost address:"
    riscv64-unknown-elf-nm rv64im_minimal.elf | grep tohost
else
    echo "rv64im_minimal.elf does not exist"
fi
echo ""

echo "4. Testing manual timeout command:"
time timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf
RESULT=$?
echo "Manual test exit code: $RESULT"
echo ""

echo "5. Rebuilding from scratch:"
rm -f rv64im_minimal.elf
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
echo "6. Testing freshly built binary:"
time timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf
RESULT=$?
echo "Fresh build test exit code: $RESULT"
echo ""

echo "7. Testing with bash -c:"
time bash -c 'timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf'
RESULT=$?
echo "Bash -c test exit code: $RESULT"
echo ""

echo "8. Testing script's if statement pattern:"
if timeout 2 ./build/spike --isa=rv64im rv64im_minimal.elf; then
    echo "If test: SUCCESS (exit 0)"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo "If test: TIMEOUT (exit 124)"
    else
        echo "If test: FAILURE (exit $EXIT_CODE)"
    fi
fi
echo ""

echo "=== End of Diagnostics ==="

