#!/bin/bash
# Test script to demonstrate HTIF exit codes

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "=== Testing HTIF Exit Codes ==="
echo ""

# Test 1: Exit with failure (code 1)
echo "Test 1: Building program that exits with code 1 (failure)..."
cat > /tmp/test_exit_1.c <<'EOF'
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void _start(void) {
    // Exit with failure code
    long exit_payload = (1 << 1) | 1;  // code=1
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}
EOF

${RISCV_PREFIX}gcc -march=rv64im -mabi=lp64 -mcmodel=medany -nostdlib \
    -nostartfiles -ffreestanding -fno-builtin -O1 \
    -T rv64im_minimal.ld -o /tmp/test_exit_1.elf /tmp/test_exit_1.c

echo "Running... (should show '*** FAILED *** (tohost = 1)')"
timeout 2 ./build/spike --isa=rv64im /tmp/test_exit_1.elf || echo "Exit code: $?"
echo ""

# Test 2: Exit with success (code 0)
echo "Test 2: Building program that exits with code 0 (success)..."
cat > /tmp/test_exit_0.c <<'EOF'
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void _start(void) {
    // Exit with success code
    long exit_payload = (0 << 1) | 1;  // code=0
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}
EOF

${RISCV_PREFIX}gcc -march=rv64im -mabi=lp64 -mcmodel=medany -nostdlib \
    -nostartfiles -ffreestanding -fno-builtin -O1 \
    -T rv64im_minimal.ld -o /tmp/test_exit_0.elf /tmp/test_exit_0.c

echo "Running... (should exit silently with code 0)"
timeout 2 ./build/spike --isa=rv64im /tmp/test_exit_0.elf && echo "SUCCESS - No error message"
echo "Exit code: $?"
echo ""

# Test 3: Exit with arbitrary code
echo "Test 3: Building program that exits with code 42..."
cat > /tmp/test_exit_42.c <<'EOF'
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void _start(void) {
    // Exit with code 42
    long exit_payload = (42 << 1) | 1;  // code=42
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}
EOF

${RISCV_PREFIX}gcc -march=rv64im -mabi=lp64 -mcmodel=medany -nostdlib \
    -nostartfiles -ffreestanding -fno-builtin -O1 \
    -T rv64im_minimal.ld -o /tmp/test_exit_42.elf /tmp/test_exit_42.c

echo "Running... (should show '*** FAILED *** (tohost = 42)')"
timeout 2 ./build/spike --isa=rv64im /tmp/test_exit_42.elf || echo "Exit code: $?"
echo ""

echo "=== All tests completed ==="

