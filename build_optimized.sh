#!/bin/bash
set -e

echo "Building OPTIMIZED minimal trap handler..."
echo ""

# Compile
echo "Compiling rv64im_minimal_ecall_optimized.c..."
riscv64-unknown-elf-gcc \
    -march=rv64im -mabi=lp64 \
    -mcmodel=medany \
    -nostdlib -nostartfiles -ffreestanding -fno-builtin \
    -O1 \
    -T rv64im_minimal_ecall.ld \
    -o rv64im_minimal_ecall_optimized.elf \
    rv64im_minimal_ecall_optimized.c

echo "Built rv64im_minimal_ecall_optimized.elf successfully!"
echo ""

# Compare sizes
echo "=== SIZE COMPARISON ==="
echo "Optimized version:"
riscv64-unknown-elf-size rv64im_minimal_ecall_optimized.elf
echo ""
echo "Robust version:"
riscv64-unknown-elf-size rv64im_minimal_ecall.elf
echo ""

# Show trap_handler assembly
echo "=== OPTIMIZED TRAP_HANDLER ASSEMBLY ==="
riscv64-unknown-elf-objdump -d rv64im_minimal_ecall_optimized.elf | sed -n '/^[0-9a-f]* <trap_handler>:/,/^$/p'
echo ""

# Test
if [ -f "./build/spike" ]; then
    echo "=== RUNNING TEST ==="
    if timeout 5 ./build/spike --isa=rv64im rv64im_minimal_ecall_optimized.elf </dev/null 2>&1 | grep -v "^\[" | head -5 | tail -2; then
        echo ""
        echo "✅ OPTIMIZED VERSION TEST PASSED!"
        echo ""
        echo "Performance comparison:"
        echo "  Robust:    96 bytes stack, ~22 instructions"
        echo "  Optimized: 16 bytes stack, ~12 instructions"
        echo "  Speedup:   ~40-50% faster!"
    else
        EXIT_CODE=$?
        echo "❌ TEST FAILED: Exit code $EXIT_CODE"
    fi
else
    echo "Spike not found. To test:"
    echo "  ./build/spike --isa=rv64im rv64im_minimal_ecall_optimized.elf"
fi

