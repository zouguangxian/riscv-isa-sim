# Nested Syscalls - Problem and Solutions

This document explains the nested syscall problem and presents multiple solutions for handling syscalls that call other syscalls.

## Table of Contents
1. [The Problem](#the-problem)
2. [Why Current Implementation Fails](#why-current-implementation-fails)
3. [Solution 1: Don't Allow Nesting (Current)](#solution-1-dont-allow-nesting-current)
4. [Solution 2: Full Context Save](#solution-2-full-context-save)
5. [Solution 3: Detect and Prevent Nesting](#solution-3-detect-and-prevent-nesting)
6. [Solution 4: Separate Trap Stack](#solution-4-separate-trap-stack)
7. [Solution 5: Recursive Trap Handler](#solution-5-recursive-trap-handler)
8. [Comparison Table](#comparison-table)
9. [Real-World Examples](#real-world-examples)

---

## The Problem

### What is a Nested Syscall?

A nested syscall occurs when a syscall implementation calls another syscall:

```
User Code
  │
  ├─► ecall (SYS_write)          ← First syscall
  │     │
  │     └─► sys_write()
  │           │
  │           └─► write()
  │                 │
  │                 └─► ecall (SYS_read)  ← Nested syscall!
  │                       │
  │                       └─► sys_read()
  │                             │
  │                             └─► Return
  │                       │
  │                       └─► Return (but to where?)
  │     │
  │     └─► Return (corrupted!)
  │
  └─► Crash! 💥
```

### Example Scenario

```c
// Syscall implementation that nests
reg_t sys_write(reg_t fd, const char *buf, reg_t count) {
    if (fd == SPECIAL_FD) {
        // Need to read config first
        char config[64];
        read(CONFIG_FD, config, 64);  // ← This does ecall!
        // Process and write
    }
    // Write data
    htif_putchar(buf[i]);
    return count;
}
```

---

## Why Current Implementation Fails

### Current Trap Handler (Minimal 16-byte Stack)

```asm
trap_handler:
    # Stack frame: 16 bytes
    addi    sp, sp, -16
    sd      ra, 8(sp)        # Save return address
    sd      mepc, 0(sp)      # Save exception PC
    
    # Shuffle args and call C handler
    mv      a3, a0
    mv      a4, a1
    mv      a5, a2
    csrr    a0, mcause
    addi    a1, sp, 0
    mv      a2, a7
    call    trap_handler_c
    
    # Restore and return
    ld      t0, 0(sp)
    csrw    mepc, t0
    ld      ra, 8(sp)
    addi    sp, sp, 16
    mret
```

### What Gets Saved
- ✓ `ra` (return address)
- ✓ `mepc` (exception PC)
- ✗ `a0-a7` (syscall arguments) - **NOT saved!**
- ✗ `t0-t6` (temporaries) - **NOT saved!**
- ✗ `s0-s11` (saved registers) - **NOT saved!**

### Execution Flow with Nesting

```
Step 1: First ecall (SYS_write)
  Stack: [mepc₁=0x1000] [ra₁=...] ← sp
  a0=1 (fd), a1=buf, a2=22 (count), a7=64 (SYS_write)

Step 2: Inside sys_write, call read() → ecall
  Stack: [mepc₂=0x2000] [ra₂=...] ← sp (OVERWRITES!)
  a0=3 (fd), a1=config, a2=64, a7=63 (SYS_read)
  
  Problem: mepc₁ is LOST! Original arguments are LOST!

Step 3: Return from nested ecall
  Returns to mepc₂ = 0x2000 (wrong!)
  
Step 4: Try to return from first ecall
  mepc₁ is gone → CRASH! 💥
```

### Why It Fails: Insufficient Context

| What | Saved? | Problem |
|------|--------|---------|
| `mepc` | ✓ | Overwritten by nested trap |
| `ra` | ✓ | Overwritten by nested trap |
| `a0-a7` | ✗ | Lost when nested trap changes them |
| `t0-t6` | ✗ | Lost when trap handler uses them |
| `s0-s11` | Rely on ABI | May be corrupted |

---

## Solution 1: Don't Allow Nesting (Current)

### Principle
**Syscall implementations must NOT call other syscalls.**

### Implementation

**Current code already does this!**

```c
// ✓ GOOD: Direct implementation
reg_t sys_write(reg_t fd, const char *buf, reg_t count) {
    if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {
        // Direct implementation - no ecall
        for (reg_t i = 0; i < count; i++) {
            htif_putchar(buf[i]);  // Direct HTIF, no ecall
        }
    }
    return count;
}

// ✗ BAD: Would cause nesting
reg_t sys_write_bad(reg_t fd, const char *buf, reg_t count) {
    return write(fd, buf, count);  // write() does ecall!
}
```

### Documentation

Add comments to make the limitation clear:

```c
// ============================================================================
// SYSCALL IMPLEMENTATIONS
// ============================================================================
//
// IMPORTANT: These functions are called from the trap handler.
//            They MUST NOT call ecall (directly or indirectly).
//            Nesting is not supported with the minimal trap handler.
//
// Safe to call:
//   ✓ htif_putchar()     - Direct HTIF communication
//   ✓ htif_exit()        - Direct HTIF communication
//   ✓ Regular functions  - As long as they don't ecall
//
// DO NOT call:
//   ✗ write(), read()    - These use ecall!
//   ✗ Any wrapper that does ecall
// ============================================================================

reg_t sys_write(reg_t fd, const char *buf, reg_t count) {
    // Implementation here (no ecall!)
}
```

### Pros & Cons

**Pros:**
- ✓ Simple
- ✓ Fast (16-byte stack)
- ✓ Easy to understand
- ✓ No overhead
- ✓ Already implemented!

**Cons:**
- ✗ Limited flexibility
- ✗ Can't implement complex syscalls
- ✗ Easy to violate accidentally

### When to Use
- ✓ Bare-metal firmware
- ✓ Simple embedded systems
- ✓ Testing/validation code
- ✓ Bootloaders
- ✓ Performance-critical paths

---

## Solution 2: Full Context Save

### Principle
**Save ALL registers so nested calls work perfectly.**

### Implementation

```c
// RV64 version (256 bytes stack)
asm(
    ".align 4\n"
    ".global trap_handler_full\n"
    "trap_handler_full:\n"
    
    // Allocate large frame for all registers
    "    addi    sp, sp, -256\n"
    
    // Save ALL general-purpose registers (x1-x31)
    "    sd      x1,  8(sp)\n"      // ra
    "    sd      x2,  16(sp)\n"     // sp (for debugging)
    "    sd      x3,  24(sp)\n"     // gp
    "    sd      x4,  32(sp)\n"     // tp
    "    sd      x5,  40(sp)\n"     // t0
    "    sd      x6,  48(sp)\n"     // t1
    "    sd      x7,  56(sp)\n"     // t2
    "    sd      x8,  64(sp)\n"     // s0/fp
    "    sd      x9,  72(sp)\n"     // s1
    "    sd      x10, 80(sp)\n"     // a0
    "    sd      x11, 88(sp)\n"     // a1
    "    sd      x12, 96(sp)\n"     // a2
    "    sd      x13, 104(sp)\n"    // a3
    "    sd      x14, 112(sp)\n"    // a4
    "    sd      x15, 120(sp)\n"    // a5
    "    sd      x16, 128(sp)\n"    // a6
    "    sd      x17, 136(sp)\n"    // a7
    "    sd      x18, 144(sp)\n"    // s2
    "    sd      x19, 152(sp)\n"    // s3
    "    sd      x20, 160(sp)\n"    // s4
    "    sd      x21, 168(sp)\n"    // s5
    "    sd      x22, 176(sp)\n"    // s6
    "    sd      x23, 184(sp)\n"    // s7
    "    sd      x24, 192(sp)\n"    // s8
    "    sd      x25, 200(sp)\n"    // s9
    "    sd      x26, 208(sp)\n"    // s10
    "    sd      x27, 216(sp)\n"    // s11
    "    sd      x28, 224(sp)\n"    // t3
    "    sd      x29, 232(sp)\n"    // t4
    "    sd      x30, 240(sp)\n"    // t5
    "    sd      x31, 248(sp)\n"    // t6
    
    // Save CSRs
    "    csrr    t0, mepc\n"
    "    sd      t0, 0(sp)\n"
    
    // Prepare args for C handler
    "    csrr    a0, mcause\n"
    "    mv      a1, sp\n"          // Pass stack pointer
    "    call    trap_handler_c_full\n"
    
    // Restore ALL registers
    "    ld      t0, 0(sp)\n"
    "    csrw    mepc, t0\n"
    "    ld      x1,  8(sp)\n"      // ra
    // Skip x2 (sp) - will restore at end
    "    ld      x3,  24(sp)\n"     // gp
    "    ld      x4,  32(sp)\n"     // tp
    "    ld      x5,  40(sp)\n"     // t0
    "    ld      x6,  48(sp)\n"     // t1
    "    ld      x7,  56(sp)\n"     // t2
    "    ld      x8,  64(sp)\n"     // s0
    "    ld      x9,  72(sp)\n"     // s1
    "    ld      x10, 80(sp)\n"     // a0
    "    ld      x11, 88(sp)\n"     // a1
    "    ld      x12, 96(sp)\n"     // a2
    "    ld      x13, 104(sp)\n"    // a3
    "    ld      x14, 112(sp)\n"    // a4
    "    ld      x15, 120(sp)\n"    // a5
    "    ld      x16, 128(sp)\n"    // a6
    "    ld      x17, 136(sp)\n"    // a7
    "    ld      x18, 144(sp)\n"    // s2
    "    ld      x19, 152(sp)\n"    // s3
    "    ld      x20, 160(sp)\n"    // s4
    "    ld      x21, 168(sp)\n"    // s5
    "    ld      x22, 176(sp)\n"    // s6
    "    ld      x23, 184(sp)\n"    // s7
    "    ld      x24, 192(sp)\n"    // s8
    "    ld      x25, 200(sp)\n"    // s9
    "    ld      x26, 208(sp)\n"    // s10
    "    ld      x27, 216(sp)\n"    // s11
    "    ld      x28, 224(sp)\n"    // t3
    "    ld      x29, 232(sp)\n"    // t4
    "    ld      x30, 240(sp)\n"    // t5
    "    ld      x31, 248(sp)\n"    // t6
    
    // Restore stack pointer and return
    "    addi    sp, sp, 256\n"
    "    mret\n"
);

// C handler receives stack pointer
reg_t trap_handler_c_full(reg_t mcause, reg_t *saved_regs) {
    // saved_regs[0] = mepc
    // saved_regs[1] = ra (x1)
    // saved_regs[10] = a0 (x10)
    // etc.
    
    reg_t *mepc = &saved_regs[0];
    reg_t a7 = saved_regs[17];  // x17 = a7
    reg_t a0 = saved_regs[10];  // x10 = a0
    reg_t a1 = saved_regs[11];  // x11 = a1
    reg_t a2 = saved_regs[12];  // x12 = a2
    
    // Now safe to nest!
    if ((mcause & ~MCAUSE_INT_BIT) == 11) {
        reg_t result;
        if (a7 == SYS_write) {
            result = sys_write(a0, (const char *)a1, a2);
        }
        
        *mepc += 4;
        saved_regs[10] = result;  // Return value in a0
    }
    
    return 0;
}
```

### Pros & Cons

**Pros:**
- ✓ Supports unlimited nesting
- ✓ Full context preserved
- ✓ Can implement any syscall
- ✓ Standard OS approach

**Cons:**
- ✗ Large stack frame (256 bytes)
- ✗ Slow (many loads/stores)
- ✗ Complex to implement
- ✗ Overkill for simple systems

### When to Use
- ✓ Full OS kernels (Linux, xv6)
- ✓ Complex syscall implementations
- ✓ Production systems
- ✓ When nesting is required

---

## Solution 3: Detect and Prevent Nesting

### Principle
**Detect nested calls and handle them gracefully (error or special handling).**

### Implementation

```c
// Global flag to track nesting
volatile int in_trap_handler = 0;
volatile reg_t nested_trap_count = 0;

reg_t trap_handler_c(reg_t mcause, reg_t *mepc, 
                     reg_t a7, reg_t a0, reg_t a1, reg_t a2) {
    // Check for nested trap
    if (in_trap_handler) {
        nested_trap_count++;
        
        // Option 1: Abort with error
        htif_exit(100 + nested_trap_count);
        
        // Option 2: Log and return error
        // return -EAGAIN;
        
        // Option 3: Queue for later processing
        // queue_deferred_syscall(a7, a0, a1, a2);
        // return -EWOULDBLOCK;
    }
    
    // Mark as in handler
    in_trap_handler = 1;
    
    // Normal trap handling
    reg_t cause_code = mcause & ~MCAUSE_INT_BIT;
    reg_t result = -1;
    
    if (cause_code == 11) {  // ecall
        if (a7 == SYS_write) {
            result = sys_write(a0, (const char *)a1, a2);
        } else if (a7 == SYS_exit) {
            htif_exit(a0);
        }
        *mepc += 4;
    } else {
        htif_exit(50 + cause_code);
    }
    
    // Clear flag before returning
    in_trap_handler = 0;
    
    return result;
}
```

### Enhanced Version with Diagnostics

```c
#define MAX_TRAP_DEPTH 16

typedef struct {
    reg_t mcause;
    reg_t mepc;
    reg_t a7;
    reg_t depth;
} trap_info_t;

volatile int trap_depth = 0;
trap_info_t trap_stack[MAX_TRAP_DEPTH];

reg_t trap_handler_c_safe(reg_t mcause, reg_t *mepc, 
                          reg_t a7, reg_t a0, reg_t a1, reg_t a2) {
    // Check trap depth
    if (trap_depth >= MAX_TRAP_DEPTH) {
        // Stack overflow - dump diagnostics
        htif_putchar('T');
        htif_putchar('R');
        htif_putchar('A');
        htif_putchar('P');
        htif_putchar(' ');
        htif_putchar('O');
        htif_putchar('V');
        htif_putchar('E');
        htif_putchar('R');
        htif_putchar('F');
        htif_putchar('L');
        htif_putchar('O');
        htif_putchar('W');
        htif_putchar('\n');
        
        // Dump trap history
        for (int i = 0; i < trap_depth && i < MAX_TRAP_DEPTH; i++) {
            // Print trap info...
        }
        
        htif_exit(200);
    }
    
    if (trap_depth > 0) {
        // Nested trap detected
        htif_putchar('W');
        htif_putchar('A');
        htif_putchar('R');
        htif_putchar('N');
        htif_putchar(':');
        htif_putchar(' ');
        htif_putchar('N');
        htif_putchar('E');
        htif_putchar('S');
        htif_putchar('T');
        htif_putchar('\n');
        
        // Return error instead of crashing
        return -1;
    }
    
    // Record trap info
    trap_stack[trap_depth].mcause = mcause;
    trap_stack[trap_depth].mepc = *mepc;
    trap_stack[trap_depth].a7 = a7;
    trap_stack[trap_depth].depth = trap_depth;
    trap_depth++;
    
    // Handle trap...
    reg_t result = /* ... */;
    
    // Unwind
    trap_depth--;
    
    return result;
}
```

### Pros & Cons

**Pros:**
- ✓ Prevents crashes
- ✓ Provides diagnostics
- ✓ Graceful error handling
- ✓ Small overhead

**Cons:**
- ✗ Doesn't support nesting (just detects it)
- ✗ Syscalls still can't nest
- ✗ Adds complexity

### When to Use
- ✓ Debugging nested syscall issues
- ✓ Transition from simple to complex handler
- ✓ Systems where nesting might occur accidentally
- ✓ Development/testing

---

## Solution 4: Separate Trap Stack

### Principle
**Use a dedicated stack for trap handling, separate from user stack.**

### Implementation

```c
// Dedicated trap stack
#define TRAP_STACK_SIZE 4096
char trap_stack[TRAP_STACK_SIZE] __attribute__((aligned(16)));

asm(
    ".align 4\n"
    ".global trap_handler_separate_stack\n"
    "trap_handler_separate_stack:\n"
    
    // Save user sp to mscratch, load trap sp
    "    csrrw   sp, mscratch, sp\n"   // sp ↔ mscratch
    
    // Now on trap stack - safe to nest!
    "    addi    sp, sp, -16\n"
    "    sd      ra, 8(sp)\n"
    
    "    csrr    t0, mepc\n"
    "    sd      t0, 0(sp)\n"
    
    // Shuffle args
    "    mv      a3, a0\n"
    "    mv      a4, a1\n"
    "    mv      a5, a2\n"
    "    csrr    a0, mcause\n"
    "    addi    a1, sp, 0\n"
    "    mv      a2, a7\n"
    
    "    call    trap_handler_c\n"
    
    // Restore
    "    ld      t0, 0(sp)\n"
    "    csrw    mepc, t0\n"
    "    ld      ra, 8(sp)\n"
    "    addi    sp, sp, 16\n"
    
    // Restore user sp from mscratch
    "    csrrw   sp, mscratch, sp\n"   // sp ↔ mscratch
    "    mret\n"
);

void setup_trap_handler_separate_stack(void) {
    // Initialize mscratch with trap stack pointer
    reg_t trap_sp = (reg_t)&trap_stack[TRAP_STACK_SIZE];
    asm volatile("csrw mscratch, %0" :: "r"(trap_sp));
    
    // Setup trap handler
    reg_t trap_addr = (reg_t)trap_handler_separate_stack;
    asm volatile("csrw mtvec, %0" :: "r"(trap_addr));
}
```

### How It Works

```
Normal execution:
  sp → User Stack
  mscratch → Trap Stack

Enter trap:
  csrrw sp, mscratch, sp
  ↓
  sp → Trap Stack (was in mscratch)
  mscratch → User Stack (was in sp)

Nested trap:
  csrrw sp, mscratch, sp
  ↓
  sp → User Stack (was in mscratch) - WRONG!
  
Problem: Nested traps break because mscratch
         gets swapped back!
```

### Fixed Version (Track Depth)

```c
volatile int trap_depth = 0;

asm(
    "trap_handler_fixed:\n"
    
    // Check if already in trap
    "    la      t0, trap_depth\n"
    "    lw      t1, 0(t0)\n"
    "    bnez    t1, already_in_trap\n"
    
    // First trap - switch to trap stack
    "    csrrw   sp, mscratch, sp\n"
    
    "already_in_trap:\n"
    // Increment depth
    "    addi    t1, t1, 1\n"
    "    sw      t1, 0(t0)\n"
    
    // ... handle trap ...
    
    // Decrement depth
    "    la      t0, trap_depth\n"
    "    lw      t1, 0(t0)\n"
    "    addi    t1, t1, -1\n"
    "    sw      t1, 0(t0)\n"
    
    // If depth == 0, restore user stack
    "    bnez    t1, still_nested\n"
    "    csrrw   sp, mscratch, sp\n"
    
    "still_nested:\n"
    "    mret\n"
);
```

### Pros & Cons

**Pros:**
- ✓ Clean separation of stacks
- ✓ Can support nesting with depth tracking
- ✓ Protects user stack
- ✓ Standard technique

**Cons:**
- ✗ More complex setup
- ✗ Requires mscratch CSR
- ✗ Needs depth tracking for nesting
- ✗ Tricky to get right

### When to Use
- ✓ Production OS kernels
- ✓ When stack overflow is a concern
- ✓ Multi-threaded systems
- ✓ Advanced embedded systems

---

## Solution 5: Recursive Trap Handler

### Principle
**Design trap handler to be explicitly recursive-safe.**

### Implementation

```c
typedef struct trap_context {
    reg_t mepc;
    reg_t mcause;
    reg_t regs[32];    // All registers
    struct trap_context *prev;
} trap_context_t;

trap_context_t *current_trap = NULL;

reg_t trap_handler_c_recursive(reg_t mcause, reg_t *mepc, 
                               reg_t a7, reg_t a0, reg_t a1, reg_t a2) {
    // Allocate context on stack
    trap_context_t ctx;
    
    // Save current state
    ctx.mepc = *mepc;
    ctx.mcause = mcause;
    ctx.regs[10] = a0;
    ctx.regs[11] = a1;
    ctx.regs[12] = a2;
    ctx.regs[17] = a7;
    ctx.prev = current_trap;
    
    // Make this the current context
    current_trap = &ctx;
    
    // Handle trap (can nest now!)
    reg_t result = -1;
    if ((mcause & ~MCAUSE_INT_BIT) == 11) {
        if (a7 == SYS_write) {
            // This can call other syscalls safely
            result = sys_write(a0, (const char *)a1, a2);
        }
        *mepc += 4;
    }
    
    // Restore previous context
    current_trap = ctx.prev;
    
    return result;
}
```

### Pros & Cons

**Pros:**
- ✓ Supports nesting explicitly
- ✓ Can track trap history
- ✓ Clean unwinding
- ✓ Good for debugging

**Cons:**
- ✗ Still need full context save in assembly
- ✗ Overhead of context management
- ✗ Complex implementation

### When to Use
- ✓ Systems needing trap history
- ✓ Advanced debugging scenarios
- ✓ Educational purposes

---

## Comparison Table

| Solution | Stack Size | Speed | Nesting Support | Complexity | Use Case |
|----------|-----------|-------|-----------------|------------|----------|
| **1. Don't Nest** | 16 bytes | ⚡⚡⚡ | ❌ | ⭐ | Testing, firmware |
| **2. Full Save** | 256 bytes | ⚡ | ✅ Full | ⭐⭐⭐⭐ | OS kernels |
| **3. Detect** | 16 bytes | ⚡⚡ | ❌ (detects) | ⭐⭐ | Debugging |
| **4. Separate Stack** | 16 bytes + trap stack | ⚡⚡ | ✅ (with depth) | ⭐⭐⭐ | Production OS |
| **5. Recursive** | 16 bytes + contexts | ⚡⚡ | ✅ | ⭐⭐⭐⭐ | Advanced use |

### Legend
- Stack Size: Per-trap memory usage
- Speed: ⚡⚡⚡ (fast) to ⚡ (slow)
- Complexity: ⭐ (simple) to ⭐⭐⭐⭐ (complex)

---

## Real-World Examples

### Linux Kernel
**Uses:** Full context save + separate stack
- Saves all registers on entry
- Uses per-CPU kernel stacks
- Supports full nesting
- ~200-300 instructions for trap entry

### xv6 (Educational OS)
**Uses:** Full context save
- Simple full-context save
- ~32 register saves
- Good for learning
- Clear code structure

### FreeRTOS
**Uses:** Separate stack + minimal save
- Task stacks vs interrupt stack
- Saves only necessary registers
- Fast interrupt response
- Context-specific

### Embedded Bootloaders
**Uses:** Don't nest (Solution 1)
- Minimal trap handler
- No nesting needed
- Fast and simple
- Our current approach!

---

## Recommendations

### For Your Current Code (Testing/Firmware)
**✅ Keep Solution 1: Don't Allow Nesting**

**Why:**
- Already implemented
- Fast and simple
- Sufficient for testing
- Clear limitations

**Just document it:**
```c
// IMPORTANT: Syscall implementations MUST NOT call other syscalls.
// The trap handler saves only ra and mepc (16 bytes).
// Nesting would corrupt the saved context.
```

### For Future OS Development
**✅ Migrate to Solution 2 or 4**

**When you need:**
- Complex syscall implementations
- Nesting support
- Production quality

**Migration path:**
1. Start with Solution 1 (current)
2. Add detection (Solution 3) during development
3. Implement full save (Solution 2) or separate stack (Solution 4)
4. Test thoroughly with nested scenarios

### For Learning
**✅ Implement multiple solutions**

**Educational value:**
- Compare performance
- Understand trade-offs
- See complexity scaling

---

## Testing Nested Syscalls

### Test Case 1: Detect Nesting

```c
// Add this to test if nesting occurs
volatile int nested_detected = 0;

reg_t sys_write_test(reg_t fd, const char *buf, reg_t count) {
    // Try to nest (should be detected)
    char test[1];
    if (write(fd, test, 1) >= 0) {  // This does ecall
        nested_detected = 1;  // Should not reach here
    }
    return count;
}
```

### Test Case 2: Measure Performance

```c
void benchmark_trap_overhead(void) {
    uint64_t start, end;
    
    // Measure minimal trap (Solution 1)
    start = rdcycle();
    for (int i = 0; i < 1000; i++) {
        write(1, "x", 1);
    }
    end = rdcycle();
    // Report: ~50-80 cycles per trap
    
    // Measure full save (Solution 2)
    // Report: ~150-200 cycles per trap
}
```

---

## Summary

**Current Implementation:** Solution 1 (Don't Allow Nesting)
- ✅ Works perfectly for testing
- ✅ Fast and simple
- ✅ Well-documented

**If you need nesting:**
- Choose Solution 2 for OS kernels
- Choose Solution 4 for production systems
- Choose Solution 3 for debugging

**Key Insight:** The minimal 16-byte trap handler is optimal for your use case. Don't add complexity unless you need it!

---

## References

- RISC-V Privileged Spec: Trap handling
- xv6-riscv: Simple full-context save
- Linux kernel: arch/riscv/kernel/entry.S
- FreeRTOS: portable/GCC/RISC-V/

