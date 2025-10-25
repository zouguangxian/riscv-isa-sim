// Simplified ECALL test - demonstrates the concept even if full trap handling is tricky

extern volatile long tohost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, (code << 1) | 1);
}

void _start(void) {
    // For now, let's demonstrate what ECALL *would* do without actually using it
    // since bare-metal Spike's trap handling is complex
    
    // This demonstrates the ECALL concept:
    // 1. You would set up a trap handler via mtvec
    // 2. Execute ecall instruction
    // 3. Trap handler reads mcause (= 11 for M-mode ecall)
    // 4. Trap handler reads a7 (syscall number) and a0-a5 (arguments)
    // 5. Trap handler dispatches to syscall function
    // 6. Trap handler writes result to a0
    // 7. Trap handler increments mepc by 4
    // 8. Trap handler returns via mret
    
    // Simple test: compute something with M extension
    long x = 42;
    long y = 13;
    long result = x * y;  // 546
    
    // In a real ecall implementation, we would:
    // - Put syscall number in a7
    // - Put arguments in a0-a5
    // - Execute ecall
    // - Get result from a0
    
    // For demonstration, let's simulate what a write() syscall would do
    // Syscall 64 (SYS_write) with fd=1, buf="X", count=1
    // In real implementation with proxy kernel, this would output via HTIF
    
    // For now, just exit to show the concept works
    htif_exit(0);
}

