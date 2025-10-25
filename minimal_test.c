// Absolute minimal test - just exit immediately
extern volatile long tohost;

void _start(void) {
    // Exit immediately with code 1
    tohost = (0UL << 56) | (0UL << 48) | ((1 << 1) | 1);
}

