// RV64IM ECALL Test - Demonstrates trap handling and syscalls
// This version works in bare-metal Spike without proxy kernel

extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_CMD_SYSCALL      0x00UL

// Syscall numbers (RISC-V Linux ABI)
#define SYS_exit              93

// Helper function to format HTIF command
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

// Exit via HTIF
void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}

// Global variable to track if ecall was executed
volatile long ecall_count = 0;
volatile long last_syscall_num = 0;

// Syscall handler - called from trap handler
static long handle_syscall(long syscall_num, long a0, long a1, long a2) {
    last_syscall_num = syscall_num;
    ecall_count++;
    
    switch (syscall_num) {
        case SYS_exit:
            // Exit via HTIF
            htif_exit(a0);
            return 0; // Never reached
        
        default:
            // Return the syscall number as a test value
            return syscall_num + 1000;
    }
}

// Trap handler for ecall
// Must be 4-byte aligned
void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause, mepc;
    long a0_val, a1_val, a2_val, a7_val;
    
    // Read trap cause and exception PC
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    
    // Check if it's an ecall (mcause = 8 from U-mode, or 11 from M-mode)
    if (mcause == 8 || mcause == 11) {
        // Read syscall arguments from registers
        asm volatile ("mv %0, a0" : "=r"(a0_val));
        asm volatile ("mv %0, a1" : "=r"(a1_val));
        asm volatile ("mv %0, a2" : "=r"(a2_val));
        asm volatile ("mv %0, a7" : "=r"(a7_val));
        
        // Handle the syscall
        long result = handle_syscall(a7_val, a0_val, a1_val, a2_val);
        
        // Write return value to a0
        asm volatile ("mv a0, %0" :: "r"(result));
        
        // Move to next instruction (ecall is 4 bytes)
        mepc += 4;
        asm volatile ("csrw mepc, %0" :: "r"(mepc));
    } else {
        // Unexpected trap - exit with error
        htif_exit(99);
    }
}

// Setup trap vector
static void setup_trap_handler(void) {
    // Set mtvec to point to our trap handler (direct mode)
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// Make a syscall via ecall instruction
static inline long syscall3(long num, long a0, long a1, long a2) {
    register long ret asm("a0");
    register long syscall_num asm("a7") = num;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(syscall_num), "r"(arg0), "r"(arg1), "r"(arg2)
        : "memory"
    );
    
    return ret;
}

// Test computation using M extension
static long test_multiply(long a, long b) {
    return a * b;
}

static long test_divide(long a, long b) {
    return a / b;
}

// Entry point
void _start(void) {
    // Setup trap handler first
    setup_trap_handler();
    
    // Test 1: Basic computation (no ecall)
    long x = 10;
    long y = 3;
    long product = test_multiply(x, y);  // 30
    long quotient = test_divide(product, y);  // 10
    
    // Test 2: Make a test syscall (syscall 42 - not implemented)
    // Should return 42 + 1000 = 1042
    long result1 = syscall3(42, 100, 200, 300);
    
    // Test 3: Make another test syscall (syscall 7)
    // Should return 7 + 1000 = 1007
    long result2 = syscall3(7, 11, 22, 33);
    
    // Test 4: Verify results
    // If everything worked:
    // - ecall_count should be 2
    // - result1 should be 1042
    // - result2 should be 1007
    
    long success = 0;
    if (ecall_count == 2 && result1 == 1042 && result2 == 1007) {
        success = 1;
    }
    
    // Test 5: Exit via ecall with appropriate code
    if (success) {
        // Exit with code 0 (success)
        syscall3(SYS_exit, 0, 0, 0);
    } else {
        // Exit with code 1 (failure) 
        syscall3(SYS_exit, 1, 0, 0);
    }
    
    // Should never reach here
    htif_exit(99);
}

