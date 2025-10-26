// Simple test - write to tohost immediately like rv64im_minimal
extern volatile long tohost;

#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_PUTCHAR      0x01UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void _start(void) {
    // Write putchar command for 'H' immediately
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, 'H');
    
    // Loop forever with WFI
    while (1) {
        asm volatile ("wfi" ::: "memory");
    }
}

