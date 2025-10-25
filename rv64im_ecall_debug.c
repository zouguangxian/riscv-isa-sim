// ECALL debug test - shows what mcause we get
extern volatile long tohost;

#define HTIF_CMD(d,c,p) ((d<<56)|(c<<48)|(p&0xFFFFFFFFFFFFUL))

void htif_exit(long code) {
    tohost = HTIF_CMD(0, 0, (code << 1) | 1);
}

volatile long actual_mcause = 999;

void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause;
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    actual_mcause = mcause;
    
    // Exit with the mcause value so we can see what it is
    // (exit code will show in Spike output)
    htif_exit(mcause);
}

void _start(void) {
    // Setup trap handler
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
    
    // Try to execute ecall
    asm volatile ("ecall");
    
    // If we get here, ecall didn't trap
    htif_exit(100);
}

