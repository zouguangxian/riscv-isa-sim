// Simplest possible ecall test
extern volatile long tohost;

#define HTIF_CMD(d,c,p) ((d<<56)|(c<<48)|(p&0xFFFFFFFFFFFFUL))

void htif_exit(long code) {
    tohost = HTIF_CMD(0, 0, (code << 1) | 1);
}

// Test if we can even read/write CSRs
void _start(void) {
    long mtvec_val = 0;
    long mcause_val = 0;
    
    // Try to read mtvec
    asm volatile ("csrr %0, mtvec" : "=r"(mtvec_val));
    
    // If we got here, we can read CSRs
    // Try to read mcause
    asm volatile ("csrr %0, mcause" : "=r"(mcause_val));
    
    // If both reads succeeded, exit with success
    // (mtvec might be 0 if not set, that's okay)
    htif_exit(0);
}

