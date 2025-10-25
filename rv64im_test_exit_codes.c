// Test different HTIF exit codes to see Spike's output
// This demonstrates the difference between success (0) and failure (non-zero)

extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_SYSCALL      0x00UL
#define HTIF_CMD_PUTCHAR      0x01UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_putchar(char ch) {
    // Wait for any previous command to complete
    while (tohost != 0) {
        asm volatile ("" ::: "memory");
    }
    
    // Write the putchar command
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, (unsigned char)ch);
    
    // Manually clear tohost (Spike may not do this automatically in bare-metal)
    // In some configurations, we need to read fromhost first
    while (tohost != 0) {
        // Check if there's a response
        if (fromhost != 0) {
            fromhost = 0;  // Clear fromhost
        }
        // Try clearing tohost manually
        tohost = 0;
    }
}

void htif_puts(const char *s) {
    while (*s) {
        htif_putchar(*s);
        s++;
    }
}

void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    // Don't wait - Spike will terminate immediately on exit command
}

void _start(void) {
    htif_puts("Testing HTIF exit code 1 (failure)\n");
    htif_puts("Spike should output: *** FAILED *** (tohost = 1)\n");
    htif_puts("Note: Actual tohost value written is (1 << 1) | 1 = 3\n");
    htif_puts("      But Spike displays the exit code, not raw tohost\n");
    htif_puts("\nExiting with code 1...\n");
    
    // Exit with failure code - should trigger "*** FAILED ***" message
    htif_exit(1);
}

