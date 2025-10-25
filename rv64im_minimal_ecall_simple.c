// Simplified RV64IM C Example with ECALL
// Uses Spike's built-in proxy kernel syscall handling

// HTIF symbols - will be defined by linker script
extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_SYSCALL      0x00UL
#define HTIF_CMD_PUTCHAR      0x01UL

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

// HTIF puts - output a string
void htif_puts(const char *s) {
    while (*s) {
        htif_putchar(*s);
        s++;
    }
}

// Exit via HTIF - this is what Spike recognizes
void htif_exit(long code) {
    // Format: (exit_code << 1) | 1
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    // Spike will terminate immediately when it sees this tohost write
}

// Test function to demonstrate different outputs
void test_htif_output(void) {
    htif_puts("Test 1: Hello from HTIF!\n");
    htif_puts("Test 2: Character output: ");
    
    // Output alphabet
    for (char c = 'A'; c <= 'Z'; c++) {
        htif_putchar(c);
    }
    htif_putchar('\n');
    
    htif_puts("Test 3: Numbers via putchar: ");
    for (char c = '0'; c <= '9'; c++) {
        htif_putchar(c);
    }
    htif_putchar('\n');
}

// Entry point
void _start(void) {
    // Test HTIF output
    test_htif_output();
    
    htif_puts("\n=== Testing HTIF Exit Codes ===\n");
    htif_puts("About to exit with code 0 (success)\n");
    
    // Test successful exit - should show no error
    // To test failure, change this to: htif_exit(1);
    htif_exit(0);
    
    // Never reached
}

