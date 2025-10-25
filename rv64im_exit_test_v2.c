// Test HTIF exit with properly section-placed tohost/fromhost

// Use extern declarations like the working example
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_CMD_SYSCALL      0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
}

void _start(void) {
    // Do some simple computation
    long x = 10;
    long y = 3;
    long result = x * y;  // 30
    
    // Exit with failure code to test "*** FAILED ***" output
    // Change to htif_exit(0) for success
    htif_exit(1);
}

