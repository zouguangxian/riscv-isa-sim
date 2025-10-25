// Debug version to test SYS_write step by step
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL
#define SYS_write           64
#define STDOUT_FILENO       1

static inline long htif_cmd(long d, long c, long p) {
    return (d<<56) | (c<<48) | (p&0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, (code << 1) | 1);
}

volatile long trap_called = 0;

void trap_handler(void) __attribute__((aligned(4)));
void trap_handler(void) {
    long mcause, mepc, a7;
    
    asm volatile ("csrr %0, mcause" : "=r"(mcause));
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    asm volatile ("mv %0, a7" : "=r"(a7));
    
    if (mcause == 11) {
        trap_called++;
        
        if (a7 == SYS_write) {
            // Just return 10 to indicate write was called
            asm volatile ("li a0, 10");
        }
        
        mepc += 4;
        asm volatile ("csrw mepc, %0" :: "r"(mepc));
        asm volatile ("mret");
        __builtin_unreachable();
    } else {
        htif_exit(99);
    }
}

void _start(void) {
    // Setup trap handler
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
    
    // Test one SYS_write ecall
    register long a7 asm("a7") = SYS_write;
    register long a0 asm("a0") = STDOUT_FILENO;
    register long a1 asm("a1") = 0;  // null pointer (won't use it)
    register long a2 asm("a2") = 5;
    register long ret asm("a0");
    
    asm volatile ("ecall" : "=r"(ret) : "r"(a7), "r"(a0), "r"(a1), "r"(a2) : "memory");
    
    // Check if it worked
    if (trap_called == 1 && ret == 10) {
        htif_exit(0);  // Success
    } else {
        htif_exit(1);  // Failed
    }
}

