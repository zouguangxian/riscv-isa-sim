// Portable minimal ecall handler for RISC-V (RV32 and RV64)
// 
// Automatically detects RV32 vs RV64 and adapts:
// - Register sizes (32-bit vs 64-bit)
// - Load/store instructions (lw/sw vs ld/sd)
// - Stack frame layout
// - Interrupt bit position (bit 31 vs bit 63)
//
// Build for RV64: riscv64-unknown-elf-gcc -march=rv64im ...
// Build for RV32: riscv32-unknown-elf-gcc -march=rv32im ...

#include <stdint.h>

// ============================================================================
// PORTABILITY LAYER - Automatically adapts to RV32 or RV64
// ============================================================================

#if __riscv_xlen == 64
    // RV64 configuration
    typedef int64_t reg_t;
    typedef uint64_t ureg_t;
    #define MCAUSE_INT_BIT (1L << 63)
    #define REG_LOAD "ld"
    #define REG_STORE "sd"
    #define STACK_FRAME 16
    #define RA_OFFSET 8
    #define MEPC_OFFSET 0
    #define XLEN 64
#elif __riscv_xlen == 32
    // RV32 configuration
    typedef int32_t reg_t;
    typedef uint32_t ureg_t;
    #define MCAUSE_INT_BIT (1 << 31)
    #define REG_LOAD "lw"
    #define REG_STORE "sw"
    #define STACK_FRAME 16  // Keep 16 for alignment
    #define RA_OFFSET 12
    #define MEPC_OFFSET 8
    #define XLEN 32
#else
    #error "Unsupported XLEN"
#endif

// Helper macros for stringification
#define XSTR(s) STR(s)
#define STR(s) #s

// ============================================================================
// CONSTANTS
// ============================================================================

#define STDOUT_FILENO 1
#define STDERR_FILENO 2

// RISC-V syscall numbers (same for RV32/RV64)
#define SYS_write 64
#define SYS_exit  93

// HTIF device/command constants
#define HTIF_DEVICE_SYSCALL 0
#define HTIF_DEVICE_CONSOLE 1
#define HTIF_CMD_SYSCALL    0
#define HTIF_CMD_PUTCHAR    1

// ============================================================================
// HTIF INTERFACE
// ============================================================================

extern volatile reg_t tohost;
extern volatile reg_t fromhost;

static inline reg_t htif_cmd(reg_t device, reg_t cmd, reg_t payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

static inline void wait_tohost_ack(void) {
    while (tohost != 0) {
        asm volatile ("" ::: "memory");
    }
}

void htif_putchar(char ch) {
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, (unsigned char)ch);
    wait_tohost_ack();
}

void htif_exit(reg_t code) {
    reg_t exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    while (1) {
        asm volatile ("" ::: "memory");
    }
}

// ============================================================================
// SYSCALL IMPLEMENTATIONS
// ============================================================================

reg_t sys_write(reg_t fd, const char *buf, reg_t count) {
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        for (reg_t i = 0; i < count; i++) {
            htif_putchar(buf[i]);
        }
    }
    return count;
}

// ============================================================================
// TEST GLOBALS
// ============================================================================

volatile reg_t ecall_was_called = 0;
volatile reg_t trap_mcause = 0;
volatile reg_t syscall_number = 0;

// ============================================================================
// PORTABLE C TRAP HANDLER
// ============================================================================

reg_t trap_handler_c(reg_t mcause, reg_t *mepc, 
                     reg_t a7, reg_t a0, reg_t a1, reg_t a2) {
    trap_mcause = mcause;
    
    // Extract interrupt flag and cause code
    // RV64: bit 63 is interrupt flag
    // RV32: bit 31 is interrupt flag
    reg_t is_interrupt = mcause & MCAUSE_INT_BIT;
    reg_t cause_code = mcause & ~MCAUSE_INT_BIT;
    
    if (is_interrupt) {
        // Interrupt handling (not supported in this example)
        htif_exit(80 + cause_code);
        __builtin_unreachable();
    } else {
        // Exception handling
        if (cause_code == 11) {  // M-mode ecall
            ecall_was_called++;
            syscall_number = a7;
            
            reg_t result;
            if (a7 == SYS_write) {
                result = sys_write(a0, (const char *)a1, a2);
            } else if (a7 == SYS_exit) {
                htif_exit(a0);
                __builtin_unreachable();
            } else {
                result = -1;  // Unsupported syscall
            }
            
            *mepc += 4;  // Skip ecall instruction (4 bytes on both RV32/RV64)
            return result;
        } else {
            // Other exception
            htif_exit(50 + cause_code);
            __builtin_unreachable();
        }
    }
}

// ============================================================================
// PORTABLE ASSEMBLY TRAP HANDLER
// ============================================================================
//
// This uses preprocessor macros to generate the correct assembly for
// RV32 (lw/sw) or RV64 (ld/sd)
//
// Stack frame:
//   RV64: 16 bytes (ra at +8, mepc at +0)
//   RV32: 16 bytes (ra at +12, mepc at +8)
// ============================================================================

asm(
    ".align 4\n"
    ".global trap_handler\n"
    "trap_handler:\n"
    
    // Allocate stack frame (16 bytes for both RV32/RV64)
    "    addi    sp, sp, -" XSTR(STACK_FRAME) "\n"
    
    // Save return address (RV32: sw at +12, RV64: sd at +8)
    "    " REG_STORE " ra, " XSTR(RA_OFFSET) "(sp)\n"
    
    // Read and save mepc (RV32: sw at +8, RV64: sd at +0)
    "    csrr    t0, mepc\n"
    "    " REG_STORE " t0, " XSTR(MEPC_OFFSET) "(sp)\n"
    
    // Shuffle args: preserve a0-a2 before reading mcause into a0
    "    mv      a3, a0\n"
    "    mv      a4, a1\n"
    "    mv      a5, a2\n"
    
    // Read mcause and prepare arguments for C handler
    "    csrr    a0, mcause\n"              // arg0: mcause
    "    addi    a1, sp, " XSTR(MEPC_OFFSET) "\n"  // arg1: &mepc
    "    mv      a2, a7\n"                  // arg2: syscall number
    // a3-a5 already set above               // arg3-5: syscall args
    
    // Call C handler
    "    call    trap_handler_c\n"
    
    // Load updated mepc and commit to CSR
    "    " REG_LOAD " t0, " XSTR(MEPC_OFFSET) "(sp)\n"
    "    csrw    mepc, t0\n"
    
    // Restore ra and deallocate stack
    "    " REG_LOAD " ra, " XSTR(RA_OFFSET) "(sp)\n"
    "    addi    sp, sp, " XSTR(STACK_FRAME) "\n"
    
    // Return from trap (a0 already has result)
    "    mret\n"
);

extern void trap_handler(void);

// ============================================================================
// TRAP HANDLER SETUP
// ============================================================================

void setup_trap_handler(void) {
    reg_t trap_addr = (reg_t)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// ============================================================================
// SYSCALL WRAPPER
// ============================================================================

reg_t do_syscall(reg_t syscall_num, reg_t arg0, reg_t arg1, reg_t arg2) {
    register reg_t a7 asm("a7") = syscall_num;
    register reg_t a0 asm("a0") = arg0;
    register reg_t a1 asm("a1") = arg1;
    register reg_t a2 asm("a2") = arg2;
    register reg_t result asm("a0");
    
    asm volatile (
        "ecall"
        : "=r"(result)
        : "r"(a7), "r"(a0), "r"(a1), "r"(a2)
        : "memory"
    );
    
    return result;
}

reg_t write(int fd, const void *buf, reg_t count) {
    return do_syscall(SYS_write, fd, (reg_t)buf, count);
}

// ============================================================================
// BOOT CODE
// ============================================================================

asm(
    ".section .text.boot\n"
    ".global _start\n"
    "_start:\n"
    "    la sp, _stack_top\n"
    "    j _start_c\n"
);

void _start_c(void) {
    // Test 1: Direct HTIF output
    htif_putchar('R');
    htif_putchar('V');
#if __riscv_xlen == 64
    htif_putchar('6');
    htif_putchar('4');
#else
    htif_putchar('3');
    htif_putchar('2');
#endif
    htif_putchar('\n');
    
    // Test 2: Setup trap handler
    setup_trap_handler();
    
    // Test 3: SYS_write via ecall
#if __riscv_xlen == 64
    const char *msg = "Hello from RV64!\n";
    reg_t len = 17;
#else
    const char *msg = "Hello from RV32!\n";
    reg_t len = 17;
#endif
    reg_t result = write(STDOUT_FILENO, msg, len);
    
    // Validate
    if (ecall_was_called != 1) {
        htif_exit(10);
    } else if (trap_mcause != 11) {
        htif_exit(11);
    } else if (result != len) {
        htif_exit(12);
    } else {
        htif_exit(0);  // Success!
    }
}

