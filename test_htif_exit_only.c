// Ultra-minimal test - just exit
extern volatile long tohost;

#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_CMD_SYSCALL      0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void _start(void) {
    // Exit immediately with code 42
    long exit_payload = (42 << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    
    // Loop forever
    while (1) {
        asm volatile ("wfi" ::: "memory");
    }
}

