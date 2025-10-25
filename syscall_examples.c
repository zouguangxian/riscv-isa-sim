// Example syscall usage patterns for RISC-V ecall instruction
// This file shows how to use different syscalls (not a complete program)

// ============================================================================
// Example 1: Simple write to stdout
// ============================================================================
void example_simple_write(void) {
    const char *msg = "Hello, World!\n";
    
    // Method 1: Using wrapper function
    write(STDOUT_FILENO, msg, 14);
    
    // Method 2: Direct syscall
    syscall(SYS_write, 1, (long)msg, 14, 0, 0, 0);
    
    // Method 3: Raw assembly
    asm volatile (
        "li a7, 64\n"        // SYS_write = 64
        "li a0, 1\n"         // fd = stdout
        "mv a1, %0\n"        // buf = msg
        "li a2, 14\n"        // count = 14
        "ecall\n"
        :
        : "r"(msg)
        : "a0", "a1", "a2", "a7", "memory"
    );
}

// ============================================================================
// Example 2: Write multiple messages
// ============================================================================
void example_multiple_writes(void) {
    write(STDOUT_FILENO, "Line 1\n", 7);
    write(STDOUT_FILENO, "Line 2\n", 7);
    write(STDOUT_FILENO, "Line 3\n", 7);
}

// ============================================================================
// Example 3: Write with error checking
// ============================================================================
void example_write_with_error_check(void) {
    const char *msg = "Test message\n";
    long result = write(STDOUT_FILENO, msg, 13);
    
    if (result < 0) {
        // Error occurred
        const char *err = "Write failed!\n";
        write(STDERR_FILENO, err, 14);
    } else if (result != 13) {
        // Partial write
        const char *warn = "Partial write!\n";
        write(STDERR_FILENO, warn, 15);
    } else {
        // Success - wrote all bytes
    }
}

// ============================================================================
// Example 4: Character-by-character output
// ============================================================================
void example_char_output(const char *str) {
    while (*str) {
        write(STDOUT_FILENO, str, 1);
        str++;
    }
}

// ============================================================================
// Example 5: Print number (simple itoa and print)
// ============================================================================
void print_number(long num) {
    char buf[32];
    int i = 0;
    
    // Handle negative numbers
    int is_negative = 0;
    if (num < 0) {
        is_negative = 1;
        num = -num;
    }
    
    // Convert to string (reversed)
    do {
        buf[i++] = '0' + (num % 10);
        num /= 10;
    } while (num > 0);
    
    if (is_negative) {
        buf[i++] = '-';
    }
    
    // Write in reverse order
    while (i > 0) {
        i--;
        write(STDOUT_FILENO, &buf[i], 1);
    }
}

// ============================================================================
// Example 6: Exit with status code
// ============================================================================
void example_exit_success(void) {
    // Exit with code 0 (success)
    syscall(SYS_exit, 0, 0, 0, 0, 0, 0);
}

void example_exit_failure(void) {
    // Exit with code 1 (failure)
    syscall(SYS_exit, 1, 0, 0, 0, 0, 0, 0);
}

// ============================================================================
// Example 7: Formatted output (simple printf-like function)
// ============================================================================
void simple_print(const char *format, long value) {
    const char *p = format;
    
    while (*p) {
        if (*p == '%' && *(p+1) == 'd') {
            // Print number
            print_number(value);
            p += 2;
        } else {
            // Print character
            write(STDOUT_FILENO, p, 1);
            p++;
        }
    }
}

// Usage:
// simple_print("Result: %d\n", 42);  // Outputs: "Result: 42\n"

// ============================================================================
// Example 8: Direct HTIF putchar (bypassing syscall)
// ============================================================================
void example_direct_htif_putchar(char ch) {
    // This bypasses the syscall mechanism and goes directly to HTIF
    long cmd = (0x01UL << 56) |  // HTIF_DEVICE_CONSOLE
               (0x01UL << 48) |  // HTIF_CMD_PUTCHAR
               (ch & 0xFF);
    
    tohost = cmd;
    
    // Wait for acknowledgment
    while (tohost != 0) {
        asm volatile ("" ::: "memory");
    }
}

// ============================================================================
// Example 9: Test all alphabet characters
// ============================================================================
void example_alphabet_test(void) {
    const char *prefix = "Alphabet: ";
    write(STDOUT_FILENO, prefix, 10);
    
    for (char c = 'A'; c <= 'Z'; c++) {
        write(STDOUT_FILENO, &c, 1);
    }
    
    write(STDOUT_FILENO, "\n", 1);
    
    for (char c = 'a'; c <= 'z'; c++) {
        write(STDOUT_FILENO, &c, 1);
    }
    
    write(STDOUT_FILENO, "\n", 1);
}

// ============================================================================
// Example 10: Performance test (many small writes vs one large write)
// ============================================================================
void example_performance_comparison(void) {
    const char *msg = "This is a test message for performance comparison\n";
    long len = 51;
    
    // Method 1: One large write (faster)
    write(STDOUT_FILENO, msg, len);
    
    // Method 2: Many small writes (slower due to HTIF overhead)
    for (long i = 0; i < len; i++) {
        write(STDOUT_FILENO, &msg[i], 1);
    }
}

// ============================================================================
// Example 11: Custom syscall with all arguments
// ============================================================================
void example_custom_syscall(void) {
    // Example showing all 6 argument registers
    long result = syscall(
        999,         // syscall number (custom)
        1,           // a0: arg1
        2,           // a1: arg2
        3,           // a2: arg3
        4,           // a3: arg4
        5,           // a4: arg5
        6            // a5: arg6
    );
    
    // result will be in a0 after ecall returns
}

// ============================================================================
// Example 12: Write to stderr instead of stdout
// ============================================================================
void example_stderr_output(void) {
    const char *error_msg = "ERROR: Something went wrong!\n";
    write(STDERR_FILENO, error_msg, 29);
}

// ============================================================================
// Example 13: Conditional output based on computation
// ============================================================================
void example_conditional_output(long a, long b) {
    if (a > b) {
        write(STDOUT_FILENO, "a is greater than b\n", 20);
    } else if (a < b) {
        write(STDOUT_FILENO, "a is less than b\n", 17);
    } else {
        write(STDOUT_FILENO, "a equals b\n", 11);
    }
}

// ============================================================================
// Example 14: Loop with syscalls
// ============================================================================
void example_loop_with_syscalls(void) {
    const char *prefix = "Iteration ";
    const char *newline = "\n";
    
    for (int i = 0; i < 10; i++) {
        write(STDOUT_FILENO, prefix, 10);
        print_number(i);
        write(STDOUT_FILENO, newline, 1);
    }
}

// ============================================================================
// Example 15: Using ecall in inline assembly with all details
// ============================================================================
long example_inline_asm_ecall(int fd, const char *buf, long count) {
    register long a0_reg asm("a0") = fd;
    register long a1_reg asm("a1") = (long)buf;
    register long a2_reg asm("a2") = count;
    register long a7_reg asm("a7") = SYS_write;
    register long ret asm("a0");
    
    asm volatile (
        "ecall"
        : "=r"(ret)                          // output: return value in a0
        : "r"(a7_reg),                       // input: syscall number in a7
          "r"(a0_reg),                       // input: arg0 in a0
          "r"(a1_reg),                       // input: arg1 in a1
          "r"(a2_reg)                        // input: arg2 in a2
        : "memory"                           // clobbers: memory
    );
    
    return ret;
}

// ============================================================================
// Notes on syscall calling convention:
// ============================================================================
// 
// RISC-V Linux syscall ABI:
// - a7 (x17): syscall number
// - a0 (x10): 1st argument, also return value
// - a1 (x11): 2nd argument
// - a2 (x12): 3rd argument
// - a3 (x13): 4th argument
// - a4 (x14): 5th argument
// - a5 (x15): 6th argument
//
// To invoke a syscall:
// 1. Load syscall number into a7
// 2. Load arguments into a0-a5
// 3. Execute ecall instruction
// 4. Read return value from a0
//
// The trap handler will:
// 1. Save the return address (mepc)
// 2. Dispatch to the appropriate syscall handler
// 3. Place result in a0
// 4. Increment mepc by 4 (skip ecall instruction)
// 5. Return from trap
//

