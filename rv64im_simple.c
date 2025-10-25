// Minimal RV64IM C Example
// This is a very simple C program that definitely only uses RV64IM instructions

// HTIF symbols - use external symbols defined by linker script  
extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_CMD_SYSCALL      0x00UL

// Helper function to format HTIF command
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

// Simple computation functions
static long add(long a, long b) {
    return a + b;
}

static long multiply(long a, long b) {
    return a * b;  // Uses MUL instruction from M extension
}

static long divide(long a, long b) {
    return a / b;  // Uses DIV instruction from M extension
}

// Entry point
void _start(void) {
    // Simple calculations using only integer arithmetic
    long x = 10;
    long y = 5;
    
    // Test basic operations
    long sum = add(x, y);           // 15
    long product = multiply(x, y);  // 50
    long quotient = divide(x, y);   // 2
    
    // Test 64-bit operations
    long big_num = 0x123456789ABCDEF0L;
    long result = big_num + sum + product + quotient;
    
    // Simple loop
    long counter = 0;
    for (int i = 0; i < 10; i++) {
        counter = add(counter, i);
    }
    
    // Signal success to spike (tohost = 1 means success)
    // Use proper HTIF protocol: syscall device, exit command with code 0
    long exit_payload = (0 << 1) | 1;  // exit code 0 = success
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    
    // Infinite loop (spike should exit when tohost is written)
    while (1) {
        // Wait for interrupt instruction
        asm volatile ("wfi");
    }
}

// Remove the local definitions - these will be provided by linker script