// Final working ECALL test for Spike (bare-metal, M-mode)
// Demonstrates: trap handler setup, ecall execution, syscall dispatching

extern volatile long tohost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, (code << 1) | 1);
}

// Syscall counter
volatile long ecall_executed = 0;

// Trap handler (4-byte aligned)
void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause, mepc, a7_val, a0_val;
    
    // Read trap info
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    
    // Must be M-mode ecall (mcause = 11)
    if (mcause != 11) {
        htif_exit(50 + mcause);  // Unexpected mcause
    }
    
    // Read syscall number and argument
    asm volatile ("mv %0, a7" : "=r"(a7_val));
    asm volatile ("mv %0, a0" : "=r"(a0_val));
    
    ecall_executed++;
    
    // Simple syscall: return (syscall_num * 10) + arg
    long result = (a7_val * 10) + a0_val;
    
    // Write result to a0
    asm volatile ("mv a0, %0" :: "r"(result));
    
    // Skip past ecall (4 bytes)
    mepc += 4;
    asm volatile ("csrw mepc, %0" :: "r"(mepc));
    
    // Return from trap using mret instruction
    asm volatile ("mret");
    
    // Never reached
    __builtin_unreachable();
}

// Execute ecall
long do_syscall(long num, long arg) {
    register long a7 asm("a7") = num;
    register long a0 asm("a0") = arg;
    register long ret asm("a0");
    
    asm volatile ("ecall" : "=r"(ret) : "r"(a7), "r"(a0) : "memory");
    
    return ret;
}

void _start(void) {
    // 1. Setup trap handler
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
    
    // 2. Test ecall: syscall(5, 3) should return 5*10 + 3 = 53
    long result1 = do_syscall(5, 3);
    
    // 3. Test another ecall: syscall(7, 2) should return 7*10 + 2 = 72
    long result2 = do_syscall(7, 2);
    
    // 4. Verify results
    if (ecall_executed == 2 && result1 == 53 && result2 == 72) {
        htif_exit(0);  // SUCCESS!
    } else {
        // Failure - return diagnostic code
        if (ecall_executed != 2) {
            htif_exit(10 + ecall_executed);  // Wrong count
        } else if (result1 != 53) {
            htif_exit(20);  // Wrong result1
        } else {
            htif_exit(30);  // Wrong result2
        }
    }
}

