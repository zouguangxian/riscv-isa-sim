// RV64IM ECALL Test - Reproducible demonstration
// Shows ecall instruction working in bare-metal Spike

extern volatile long tohost;
extern volatile long fromhost;

// HTIF protocol constants
#define HTIF_DEVICE_SYSCALL   0x00UL
#define HTIF_DEVICE_CONSOLE   0x01UL
#define HTIF_CMD_SYSCALL      0x00UL
#define HTIF_CMD_PUTCHAR      0x01UL

// Syscall numbers (RISC-V Linux ABI)
#define SYS_write             64
#define SYS_exit              93

// File descriptors
#define STDOUT_FILENO         1
#define STDERR_FILENO         2

// Helper function to format HTIF command
static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

// Wait for tohost acknowledgment (host clears tohost when command is processed)
static inline void wait_tohost_ack(void) {
    // tohost is already declared volatile, so just read it directly
    while (tohost != 0) {
        // Busy wait - check if Spike clears tohost
        asm volatile ("" ::: "memory");
    }
}

// HTIF putchar - output a single character
// NOTE: This follows the correct HTIF protocol from your Rust implementation
// TESTING: Will this work in bare-metal Spike?
void htif_putchar(char ch) __attribute__((noinline));
void htif_putchar(char ch) {
    // Write command to tohost
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, (unsigned char)ch);
    
    // Wait for host to acknowledge (clear tohost)
    // According to Spike source, this should be cleared in htif_t::run() loop
    wait_tohost_ack();
}

// HTIF puts - output a string
void htif_puts(const char *s) {
    while (*s) {
        htif_putchar(*s);
        s++;
    }
}

// Exit via HTIF
void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    // Loop forever - Spike will terminate the process
    while (1) {
        asm volatile ("" ::: "memory");
    }
}

// Track whether ecall was executed
volatile long ecall_was_called = 0;
volatile long trap_mcause = 0;
volatile long syscall_number = 0;
volatile long syscall_arg = 0;

// Syscall handler for SYS_write
// Arguments: fd (a0), buf (a1), count (a2)
// 
// CRITICAL: This function is called from the assembly trap_handler,
// which properly saves a0-a7 to the stack BEFORE reading CSRs.
// This ensures fd=1 (not mcause=11) is passed correctly!
long sys_write(long fd, const char *buf, long count) __attribute__((noinline));
long sys_write(long fd, const char *buf, long count) {
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        // Output each character via HTIF
        for (long i = 0; i < count; i++) {
            htif_putchar(buf[i]);
        }
    }
    
    return count;
}

// Main syscall dispatcher
// Returns: syscall result, or -1 for unsupported
// Special: SYS_exit doesn't return
static long handle_syscall(long syscall_num, long a0, long a1, long a2) {
    switch (syscall_num) {
        case SYS_write:
            return sys_write(a0, (const char *)a1, a2);
        
        case SYS_exit:
            // Exit directly without returning to trap handler
            htif_exit(a0);
            __builtin_unreachable();
        
        default:
            // Unsupported syscall
            return -1;
    }
}

// ============================================================================
// TRAP HANDLER ARCHITECTURE
// ============================================================================
//
// This implementation uses a clean separation-of-concerns architecture:
//
// 1. trap_handler (Assembly):
//    - Saves all context (ra, s0, a0-a7) to stack
//    - Reads CSRs into TEMPORARY registers (t0-t6) to avoid corrupting a0-a7
//    - Calls trap_handler_c with saved arguments
//    - Restores context and executes mret
//
// 2. trap_handler_c (C):
//    - Receives mcause, mepc pointer, and syscall arguments
//    - Dispatches based on mcause and syscall number
//    - Updates mepc if needed (e.g., skip ecall instruction)
//    - Returns syscall result
//
// Benefits:
//   - Assembly is minimal and predictable (no compiler surprises)
//   - C code is readable and easy to extend (add new syscalls here)
//   - Syscall arguments (a0-a7) are properly preserved
//
// See ARCHITECTURE.md for detailed documentation.
// ============================================================================

// C trap handler - dispatches based on mcause and syscall number
// This function is called from assembly with properly preserved registers
//
// IMPROVEMENTS BASED ON FEEDBACK:
// 1. Distinguishes interrupts from exceptions using high bit of mcause
// 2. Uses long (equivalent to intptr_t on RV64)
//    NOTE: For RV32, use int32_t or intptr_t for portability
// 3. Adds safety checks for invalid states
// 4. More robust error handling
//
// PORTABILITY NOTE:
//   This implementation uses 'long' which is 64-bit on RV64.
//   For RV32, you'd want to use 'int' or 'int32_t' for register values,
//   and the interrupt bit would be bit 31 instead of bit 63.
long trap_handler_c(long mcause, long *mepc, long a7, long a0, long a1, long a2) {
    // Store trap information to globals for debugging
    trap_mcause = mcause;
    
    // Safety check: ensure mepc is valid (non-null)
    if (!mepc) {
        htif_exit(99);  // Fatal: null mepc pointer
        __builtin_unreachable();
    }
    
    // RISC-V mcause format:
    // Bit 63: Interrupt flag (1 = interrupt, 0 = exception)
    // Bits 62-0: Exception/Interrupt code
    long is_interrupt = mcause & (1L << 63);
    long cause_code = mcause & ~(1L << 63);  // Mask off interrupt bit
    
    if (is_interrupt) {
        // INTERRUPT HANDLING
        // For now, we don't support interrupts in this bare-metal example
        // In a real system, you'd dispatch to interrupt handlers here
        htif_exit(80 + cause_code);  // Exit with interrupt code
        __builtin_unreachable();
    } else {
        // EXCEPTION HANDLING
        
        // Check if it's M-mode ecall (cause_code = 11)
        if (cause_code == 11) {
            ecall_was_called++;
            syscall_number = a7;
            
            // Dispatch based on syscall number
            long result;
            if (a7 == SYS_write) {
                // SYS_write: a0=fd, a1=buf, a2=count
                result = sys_write(a0, (const char *)a1, a2);
            } else if (a7 == SYS_exit) {
                // SYS_exit: a0=exit_code
                htif_exit(a0);
                __builtin_unreachable();
            } else {
                // Unsupported syscall - return error
                result = -1;
            }
            
            // Skip past ecall instruction (4 bytes) for RISC-V
            *mepc += 4;
            
            return result;
        } else {
            // Other exception (not ecall)
            // Examples: instruction access fault, illegal instruction, etc.
            htif_exit(50 + cause_code);
            __builtin_unreachable();
        }
    }
}

// Assembly trap handler - ONLY handles context save/restore
// Dispatching logic is delegated to trap_handler_c
//
// IMPROVEMENTS BASED ON FEEDBACK:
// 1. Stack alignment: 96 bytes (divisible by 16) for RISC-V ABI compliance
// 2. Register usage: Simplified - no unnecessary use of s0
// 3. Saves s1-s2 in case C handler needs them
// 4. Interrupt-safe: Works for both exceptions and interrupts
asm(
    ".align 4\n"
    ".global trap_handler\n"
    "trap_handler:\n"
    
    // Allocate stack frame (96 bytes = 6 * 16, properly aligned)
    "    addi sp, sp, -96\n"
    
    // Save callee-saved registers (in case C uses them)
    "    sd ra, 88(sp)\n"
    "    sd s0, 80(sp)\n"
    "    sd s1, 72(sp)\n"
    "    sd s2, 64(sp)\n"
    
    // Save original syscall/interrupt argument registers (a0-a7)
    "    sd a0, 56(sp)\n"    // Save original a0 (e.g., fd)
    "    sd a1, 48(sp)\n"    // Save original a1 (e.g., buf)
    "    sd a2, 40(sp)\n"    // Save original a2 (e.g., count)
    "    sd a7, 32(sp)\n"    // Save original a7 (syscall number)
    
    // Read CSRs into TEMPORARY registers (critical: don't overwrite a0-a7!)
    "    csrr t0, mcause\n"  // t0 = mcause (includes interrupt bit)
    "    csrr t1, mepc\n"    // t1 = mepc
    
    // Store mepc on stack so C function can modify it
    "    sd t1, 24(sp)\n"    // mepc at sp+24
    
    // Prepare arguments for trap_handler_c(mcause, &mepc, a7, a0, a1, a2)
    // All mcause bits passed to C, including interrupt flag
    "    mv a0, t0\n"        // arg0: mcause (full value with interrupt bit)
    "    addi a1, sp, 24\n"  // arg1: &mepc (pointer to stack location)
    "    ld a2, 32(sp)\n"    // arg2: saved a7 (syscall number)
    "    ld a3, 56(sp)\n"    // arg3: saved a0 (first syscall arg)
    "    ld a4, 48(sp)\n"    // arg4: saved a1 (second syscall arg)
    "    ld a5, 40(sp)\n"    // arg5: saved a2 (third syscall arg)
    
    // Call C handler - it will update mepc and return result
    "    call trap_handler_c\n"
    
    // trap_handler_c returned with result in a0 - keep it there!
    // (No need to move to s0 and back - just use t0 for mepc loads)
    "    mv t0, a0\n"        // Save result in t0 temporarily
    
    // Load updated mepc and write it to CSR
    "    ld t1, 24(sp)\n"    // Load updated mepc
    "    csrw mepc, t1\n"    // Write to CSR
    
    // Restore callee-saved registers
    "    ld ra, 88(sp)\n"
    "    ld s0, 80(sp)\n"
    "    ld s1, 72(sp)\n"
    "    ld s2, 64(sp)\n"
    
    // Return result in a0 and deallocate stack
    "    mv a0, t0\n"        // Return value in a0
    "    addi sp, sp, 96\n"
    "    mret\n"
);

// Forward declaration of assembly trap handler
extern void trap_handler(void);

// Setup trap handler
void setup_trap_handler(void) {
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}

// Execute ecall instruction with 3 arguments
long do_syscall(long syscall_num, long arg0, long arg1, long arg2) {
    register long a7 asm("a7") = syscall_num;
    register long a0 asm("a0") = arg0;
    register long a1 asm("a1") = arg1;
    register long a2 asm("a2") = arg2;
    register long ret asm("a0");
    
    asm volatile (
        "ecall"
        : "=r"(ret)
        : "r"(a7), "r"(a0), "r"(a1), "r"(a2)
        : "memory"
    );
    
    return ret;
}

// Convenience wrapper for write syscall
static long write(int fd, const void *buf, long count) {
    return do_syscall(SYS_write, fd, (long)buf, count);
}

// Assembly entry point - initializes stack before C code
asm(
    ".section .text.boot\n"
    ".global _start\n"
    "_start:\n"
    "    la sp, _stack_top\n"      // Initialize stack pointer
    "    j _start_c\n"              // Jump to C code
);

// C entry point (called from assembly)
void _start_c(void) {
    // Test 1: Direct HTIF output (not via ecall)
    htif_putchar('D');
    htif_putchar('I');
    htif_putchar('R');
    htif_putchar('E');
    htif_putchar('C');
    htif_putchar('T');
    htif_putchar('\n');
    
    // Test 2: Setup trap handler and test ecall
    setup_trap_handler();
    
    // Test 3: SYS_write via ecall using write() wrapper
    const char *msg = "Hello from SYS_write!\n";
    long result = write(STDOUT_FILENO, msg, 22);
    
    // Verify result with detailed exit codes
    if (ecall_was_called != 1) {
        htif_exit(10);  // Wrong ecall count
    } else if (trap_mcause != 11) {
        htif_exit(11);  // Wrong mcause
    } else if (result != 22) {
        htif_exit(12);  // Wrong return value
    } else {
        htif_exit(0);  // Success!
    }
}
