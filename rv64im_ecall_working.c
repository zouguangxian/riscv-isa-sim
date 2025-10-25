// Working ECALL test for bare-metal Spike
// Spike starts in M-mode, so ecalls from M-mode have mcause=11

extern volatile long tohost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, (code << 1) | 1);
}

// Track trap handler execution
volatile long trap_was_called = 0;
volatile long trap_mcause = 0;
volatile long syscall_result = 0;

// Trap handler - must be aligned to 4 bytes
void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause, mepc, a0, a7;
    
    trap_was_called = 1;
    
    // Read trap information
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    
    trap_mcause = mcause;
    
    // Check if it's an M-mode ecall (mcause = 11)
    if (mcause == 11) {
        // Read syscall number and argument
        asm volatile ("mv %0, a7" : "=r"(a7));
        asm volatile ("mv %0, a0" : "=r"(a0));
        
        // Simple syscall handler: return (syscall_num * 100) + arg0
        long result = (a7 * 100) + a0;
        syscall_result = result;
        
        // Write result to a0
        asm volatile ("mv a0, %0" :: "r"(result));
        
        // Skip past ecall instruction
        mepc += 4;
        asm volatile ("csrw mepc, %0" :: "r"(mepc));
    } else {
        // Unexpected trap
        htif_exit(88);
    }
}

// Setup trap handler
void setup_traps(void) {
    long trap_addr = (long)trap_handler;
    // Set mtvec to trap_handler address (direct mode, bits[1:0] = 00)
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// Make an ecall
long do_ecall(long syscall_num, long arg) {
    register long a0 asm("a0") = arg;
    register long a7 asm("a7") = syscall_num;
    register long ret asm("a0");
    
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(a7), "r"(a0)
        : "memory"
    );
    
    return ret;
}

void _start(void) {
    // Setup trap handler
    setup_traps();
    
    // Verify mtvec was set
    long mtvec_check;
    asm volatile ("csrr %0, mtvec" : "=r"(mtvec_check));
    
    // Test ecall: syscall 5 with argument 7
    // Expected result: (5 * 100) + 7 = 507
    long result = do_ecall(5, 7);
    
    // Check if trap was called and result is correct
    if (trap_was_called && result == 507 && trap_mcause == 11) {
        // Success!
        htif_exit(0);
    } else {
        // Failure - use different codes to debug
        if (!trap_was_called) {
            htif_exit(1);  // Trap handler never called
        } else if (result != 507) {
            htif_exit(2);  // Wrong result
        } else if (trap_mcause != 11) {
            htif_exit(3);  // Wrong mcause
        } else {
            htif_exit(4);  // Unknown error
        }
    }
}

