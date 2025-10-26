// Minimal test to check if Spike clears tohost for HTIF putchar
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_PUTCHAR      0x01UL
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_CMD_SYSCALL      0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    while (1) { asm volatile ("" ::: "memory"); }
}

// Test 1: Write putchar, check if tohost gets cleared
void test_tohost_clear(void) {
    // Write putchar command for 'H'
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, 'H');
    
    // Wait a bit (give Spike time to process)
    for (volatile int i = 0; i < 1000; i++) {
        asm volatile ("" ::: "memory");
    }
    
    // Check if tohost was cleared
    if (tohost == 0) {
        htif_exit(0);  // SUCCESS: tohost was cleared!
    } else {
        htif_exit(1);  // FAIL: tohost not cleared
    }
}

void _start(void) {
    test_tohost_clear();
}

