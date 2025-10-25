// RV64IM C Example with ECALL and Syscall Support
// Tests how Spike handles ecall instructions and implements SYS_write via HTIF

// HTIF symbols - will be defined by linker script
extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_SYSCALL      0x00UL
#define HTIF_CMD_PUTCHAR      0x01UL

// RISC-V Syscall numbers (matching Linux ABI)
#define SYS_write             64
#define SYS_exit              93
#define SYS_exit_group        94

// File descriptors
#define STDOUT_FILENO         1
#define STDERR_FILENO         2

// Helper function to format HTIF command
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

// Wait for tohost to be acknowledged (cleared by host)
static inline void wait_tohost_ack(void) {
    while (tohost != 0) {
        asm volatile ("" ::: "memory");
    }
}

// HTIF putchar - output a single character to console
void htif_putchar(char ch) {
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, (unsigned char)ch);
    wait_tohost_ack();
}

// Exit via HTIF
void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    while (1) {
        asm volatile ("wfi");
    }
}

// Simple strlen implementation
static long my_strlen(const char *s) {
    long len = 0;
    while (s[len]) len++;
    return len;
}

// Syscall handler for SYS_write
// Arguments: fd (a0), buf (a1), count (a2)
// Returns: number of bytes written or -1 on error
static long sys_write(long fd, const char *buf, long count) {
    // Only support stdout and stderr via HTIF putchar
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        for (long i = 0; i < count; i++) {
            htif_putchar(buf[i]);
        }
        return count;
    }
    return -1; // Unsupported file descriptor
}

// Main syscall dispatcher
// Called from trap handler with syscall number and arguments
static long handle_syscall(long syscall_num, long a0, long a1, long a2, 
                          long a3, long a4, long a5) {
    switch (syscall_num) {
        case SYS_write:
            return sys_write(a0, (const char *)a1, a2);
        
        case SYS_exit:
        case SYS_exit_group:
            htif_exit(a0);
            return 0; // Never reached
        
        default:
            // Unsupported syscall
            return -1;
    }
}

// Trap handler for ecall
// This is called when an ecall instruction is executed
void trap_handler(void) {
    long mcause, mepc;
    
    // Read mcause to determine trap type
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    
    // Check if it's an ecall from M-mode (mcause = 11) or U-mode (mcause = 8)
    if (mcause == 11 || mcause == 8) {
        // Save registers (syscall arguments)
        register long a0 asm("a0");
        register long a1 asm("a1");
        register long a2 asm("a2");
        register long a3 asm("a3");
        register long a4 asm("a4");
        register long a5 asm("a5");
        register long a7 asm("a7"); // syscall number
        
        long syscall_args[6];
        asm volatile (
            "mv %0, a0\n"
            "mv %1, a1\n"
            "mv %2, a2\n"
            "mv %3, a3\n"
            "mv %4, a4\n"
            "mv %5, a5\n"
            : "=r"(syscall_args[0]), "=r"(syscall_args[1]), "=r"(syscall_args[2]),
              "=r"(syscall_args[3]), "=r"(syscall_args[4]), "=r"(syscall_args[5])
        );
        
        long syscall_num;
        asm volatile ("mv %0, a7" : "=r"(syscall_num));
        
        // Handle the syscall
        long result = handle_syscall(syscall_num, syscall_args[0], syscall_args[1],
                                    syscall_args[2], syscall_args[3], 
                                    syscall_args[4], syscall_args[5]);
        
        // Return value goes in a0
        asm volatile ("mv a0, %0" :: "r"(result));
        
        // Move to next instruction (ecall is 4 bytes)
        mepc += 4;
        asm volatile ("csrw mepc, %0" :: "r"(mepc));
    } else {
        // Unexpected trap - exit with error
        htif_exit(-1);
    }
}

// Wrapper to make syscall via ecall instruction
static inline long syscall(long num, long a0, long a1, long a2, 
                          long a3, long a4, long a5) {
    register long ret asm("a0");
    register long syscall_num asm("a7") = num;
    register long arg0 asm("a0") = a0;
    register long arg1 asm("a1") = a1;
    register long arg2 asm("a2") = a2;
    register long arg3 asm("a3") = a3;
    register long arg4 asm("a4") = a4;
    register long arg5 asm("a5") = a5;
    
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(syscall_num), "r"(arg0), "r"(arg1), "r"(arg2), 
          "r"(arg3), "r"(arg4), "r"(arg5)
        : "memory"
    );
    
    return ret;
}

// Convenience wrapper for write syscall
static long write(int fd, const void *buf, long count) {
    return syscall(SYS_write, fd, (long)buf, count, 0, 0, 0);
}

// Setup trap vector
static void setup_trap_handler(void) {
    // Set mtvec to point to our trap handler
    // Using direct mode (lowest 2 bits = 0)
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// Entry point
void _start(void) {
    htif_exit(1);
    
    // Setup trap handler first
    setup_trap_handler();
    
    // Test 1: Write "Hello from ecall!\n" to stdout using ecall
    const char *msg1 = "Hello from ecall!\n";
    write(STDOUT_FILENO, msg1, my_strlen(msg1));
    
    // Test 2: Write another message
    const char *msg2 = "Testing SYS_write syscall via ecall instruction\n";
    write(STDOUT_FILENO, msg2, my_strlen(msg2));
    
    // Test 3: Write individual characters
    const char *msg3 = "Character output: ";
    write(STDOUT_FILENO, msg3, my_strlen(msg3));
    
    // Write each letter individually
    for (char c = 'A'; c <= 'Z'; c++) {
        write(STDOUT_FILENO, &c, 1);
    }
    write(STDOUT_FILENO, "\n", 1);
    
    // Test 4: Do some calculations and report
    long x = 42;
    long y = 13;
    long result = x * y; // Uses M extension multiply
    
    const char *msg4 = "Computation test: 42 * 13 = 546 (calculated)\n";
    write(STDOUT_FILENO, msg4, my_strlen(msg4));
    
    // Test 5: Final message
    const char *msg5 = "All ecall tests completed successfully!\n";
    write(STDOUT_FILENO, msg5, my_strlen(msg5));
    
    // Exit via ecall with success code
    syscall(SYS_exit, 0, 0, 0, 0, 0, 0);
    
    // Should never reach here
    while (1) {
        asm volatile ("wfi");
    }
}

