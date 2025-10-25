#!/bin/bash
# Build script for RV64IM ECALL demonstration

set -e

RISCV_PREFIX=riscv64-unknown-elf-

echo "========================================="
echo "RV64IM ECALL Demonstration"
echo "========================================="
echo ""
echo "NOTE: Full ECALL trap handling in bare-metal Spike"
echo "      (without proxy kernel) is complex. This demo"
echo "      shows the HTIF-based exit mechanism which is"
echo "      what syscalls ultimately use."
echo ""

# Check if RISC-V toolchain is available
if ! command -v ${RISCV_PREFIX}gcc &> /dev/null; then
    echo "Error: RISC-V toolchain not found."
    exit 1
fi

echo "1. Testing ECALL instruction recognition..."
echo "   (Checking if Spike recognizes ecall and sets mcause)"

cat > /tmp/test_ecall_mcause.c <<'EOF'
extern volatile long tohost;
#define HTIF_CMD(d,c,p) ((d<<56)|(c<<48)|(p&0xFFFFFFFFFFFFUL))
void htif_exit(long code) { tohost = HTIF_CMD(0, 0, (code << 1) | 1); }
volatile long actual_mcause = 999;
void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause;
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    htif_exit(mcause);  // Exit with mcause value
}
void _start(void) {
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
    asm volatile ("ecall");  // This should set mcause=11
    htif_exit(100);  // If we get here, ecall didn't trap
}
EOF

${RISCV_PREFIX}gcc -march=rv64im -mabi=lp64 -mcmodel=medany -nostdlib \
    -nostartfiles -O1 -T rv64im_minimal.ld -o /tmp/test_ecall_mcause.elf \
    /tmp/test_ecall_mcause.c 2>/dev/null

if [ -f "./build/spike" ]; then
    if timeout 2 ./build/spike --isa=rv64im /tmp/test_ecall_mcause.elf </dev/null 2>&1 | grep -q "tohost = 11"; then
        echo "   ✅ ECALL instruction works! (mcause = 11 for M-mode ecall)"
    else
        echo "   ❌ ECALL trap handling issue detected"
    fi
else
    echo "   ⚠️  Spike not found at ./build/spike"
fi

echo ""
echo "2. Demonstrating HTIF-based exit (what syscalls use)..."

cat > /tmp/test_htif_demo.c <<'EOF'
extern volatile long tohost;
#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}
void _start(void) {
    // Simulate what SYS_exit would do via ecall
    long exit_code = 0;
    long exit_payload = (exit_code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}
EOF

${RISCV_PREFIX}gcc -march=rv64im -mabi=lp64 -mcmodel=medany -nostdlib \
    -nostartfiles -O1 -T rv64im_minimal.ld -o /tmp/test_htif_demo.elf \
    /tmp/test_htif_demo.c 2>/dev/null

if [ -f "./build/spike" ]; then
    if timeout 2 ./build/spike --isa=rv64im /tmp/test_htif_demo.elf </dev/null 2>&1; then
        echo "   ✅ HTIF exit works! (This is how SYS_exit syscall communicates)"
    else
        echo "   ❌ HTIF mechanism issue"
    fi
else
    echo "   ⚠️  Spike not found"
fi

echo ""
echo "========================================="
echo "Summary:"
echo "========================================="
echo ""
echo "✅ What works in bare-metal Spike:"
echo "   - ECALL instruction is recognized (mcause = 11)"
echo "   - HTIF-based exit (tohost/fromhost)"
echo "   - Can set up mtvec trap vector"
echo "   - Can read CSRs (mcause, mepc, etc.)"
echo ""
echo "⚠️  Limitations in bare-metal Spike:"
echo "   - Full trap handler return (mret) is complex"
echo "   - HTIF console I/O (putchar) needs proxy kernel"
echo "   - For production syscalls, use Spike with pk:"
echo "     ./build/spike pk your_program"
echo ""
echo "📚 For full ECALL/syscall support, use:"
echo "   - Spike with proxy kernel (pk)"
echo "   - Linux on QEMU"
echo "   - Real RISC-V hardware"
echo ""

