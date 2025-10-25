// RV64IM ECALL Test - Reproducible demonstration
// Shows ecall instruction working in bare-metal Spike

extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_SYSCALL      0x00UL
#define HTIF_CMD_PUTCHAR      0x01UL

// Syscall numbers (RISC-V Linux ABI)
#define SYS_write             64
#define SYS_exit              93

// File descriptors
#define STDOUT_FILENO         1
#define STDERR_FILENO         2

// Helper function to format HTIF command
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

// HTIF putchar - output a single character
// NOTE: Console I/O doesn't work reliably in bare-metal Spike
// This is for demonstration purposes only
void htif_putchar(char ch) {
    // Try to output via HTIF console
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, (unsigned char)ch);
    
    // In bare-metal mode, Spike doesn't automatically clear tohost for console I/O
    // We clear it manually, but output may not appear
    tohost = 0;
}

// HTIF puts - output a string
void htif_puts(const char *s) {
    while (*s) {
        htif_putchar(*s);
        s++;
    }
}

// Exit via HTIF
void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}

// Track whether ecall was executed
volatile long ecall_was_called = 0;
volatile long trap_mcause = 0;
volatile long syscall_number = 0;
volatile long syscall_arg = 0;

// Syscall handler for SYS_write
// Arguments: fd (a0), buf (a1), count (a2)
static long sys_write(long fd, const char *buf, long count) {
    // For bare-metal testing: just return count to validate mechanism
    // To enable actual output via HTIF putchar:
    //   1. Uncomment the loop below
    //   2. Run with: ./build/spike pk rv64im_minimal_ecall.elf
    
    // if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
    //     for (long i = 0; i < count; i++) {
    //         htif_putchar(buf[i]);
    //     }
    // }
    
    return count;  // Always return count for testing
}

// Main syscall dispatcher
// Returns: syscall result, or -1 for unsupported
// Special: SYS_exit doesn't return
static long handle_syscall(long syscall_num, long a0, long a1, long a2) {
    switch (syscall_num) {
        case SYS_write:
            return sys_write(a0, (const char *)a1, a2);
        
        case SYS_exit:
            // Exit directly without returning to trap handler
            htif_exit(a0);
            __builtin_unreachable();
        
        default:
            // Unsupported syscall
            return -1;
    }
}

// Trap handler for ecall - must be 4-byte aligned
void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause, mepc, a7_val, a0_val, a1_val, a2_val;
    
    // Read trap information
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    asm volatile ("mv %0, a7" : "=r"(a7_val));
    asm volatile ("mv %0, a0" : "=r"(a0_val));
    asm volatile ("mv %0, a1" : "=r"(a1_val));
    asm volatile ("mv %0, a2" : "=r"(a2_val));
    
    trap_mcause = mcause;
    
    // Check if it's M-mode ecall (mcause = 11)
    if (mcause == 11) {
        ecall_was_called++;
        syscall_number = a7_val;
        
        long result;
        if (a7_val == SYS_write) {
            // SYS_write: return the count argument
            result = a2_val;
        } else {
            // Return -1 for unsupported syscalls
            result = -1;
        }
        
        // Skip past ecall instruction (4 bytes)
        mepc += 4;
        
        // Return from trap using mret, with result in a0
        asm volatile (
            "csrw mepc, %0\n"
            "mv a0, %1\n"
            "mret"
            :: "r"(mepc), "r"(result) : "memory"
        );
        __builtin_unreachable();
    } else {
        // Unexpected trap - exit with mcause as error code
        htif_exit(50 + mcause);
    }
}

// Setup trap handler
void setup_trap_handler(void) {
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// Execute ecall instruction with 3 arguments
long do_syscall(long syscall_num, long arg0, long arg1, long arg2) {
    register long a7 asm("a7") = syscall_num;
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long ret asm("a0");
    
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(a7), "r"(a0), "r"(a1), "r"(a2)
        : "memory"
    );
    
    return ret;
}

// Convenience wrapper for write syscall
static long write(int fd, const void *buf, long count) {
    return do_syscall(SYS_write, fd, (long)buf, count);
}

// Entry point
void _start(void) {
    // NOTE: HTIF console I/O (putchar) doesn't work in bare-metal Spike
    // This test validates the SYS_write syscall mechanism via ecall
    // To enable actual output, uncomment htif_putchar loop in sys_write
    // and run with: ./build/spike pk rv64im_minimal_ecall.elf
    
    
    // Setup trap handler
    setup_trap_handler();
    
    // Test SYS_write via ecall - using exact working pattern
    const char *msg = "Hello from SYS_write!\n";
    
    register long a7 asm("a7") = SYS_write;
    register long a0 asm("a0") = STDOUT_FILENO;
    register long a1 asm("a1") = (long)msg;
    register long a2 asm("a2") = 22;
    register long ret asm("a0");
    
    asm volatile ("ecall" : "=r"(ret) : "r"(a7), "r"(a0), "r"(a1), "r"(a2) : "memory");
    
    htif_exit(10);
    // Verify result with detailed exit codes
    if (ecall_was_called != 1) {
        htif_exit(10);  // Wrong ecall count
    } else if (trap_mcause != 11) {
        htif_exit(11);  // Wrong mcause
    } else if (ret != 22) {
        htif_exit(12);  // Wrong return value
    } else {
        htif_exit(0);  // Success!
    }
}
