# Trap Handler Implementation: A Critical Lesson

## The Problem

When implementing an `ecall` trap handler in C to handle syscalls like `SYS_write`, we encountered a subtle but critical bug where console output was not appearing despite the test passing all validation checks.

## Root Cause: Register Corruption by C Code

### The Buggy C Implementation

```c
void trap_handler(void) {
    long mcause, mepc, a0_val, a1_val, a2_val;
    
    // BUG: Compiler chooses which register to use for CSR reads
    asm volatile ("csrr %0, mcause" : "=r"(mcause));  // Compiler picked a0!
    asm volatile ("csrr %0, mepc" : "=r"(mepc));
    asm volatile ("mv %0, a0" : "=r"(a0_val));        // Saved mcause, not fd!
    asm volatile ("mv %0, a1" : "=r"(a1_val));
    asm volatile ("mv %0, a2" : "=r"(a2_val));
    
    if (mcause == 11) {  // M-mode ecall
        result = sys_write(a0_val, (const char *)a1_val, a2_val);
        // Called sys_write(11, ...) instead of sys_write(1, ...)!
    }
}
```

### What Happened in Assembly

```asm
csrr  a0, mcause       # ← Overwrote a0 (fd=1) with mcause (11)!
csrr  s0, mepc         # mepc to s0
mv    s1, a0           # s1 = a0 = 11 (mcause, NOT original fd!)
mv    s2, a1           # s2 = a1 (buf pointer)
mv    s3, a2           # s3 = a2 (count)
...
mv    a0, s1           # a0 = 11 (BUG!)
mv    a1, s2           # a1 = buf pointer (OK)
mv    a2, s3           # a2 = count (OK)
jal   sys_write        # Called sys_write(11, buf, count)
```

Inside `sys_write`:
```c
if (fd == STDOUT_FILENO || fd == STDERR_FILENO) {  // 11 != 1, so FALSE!
    // Never executed!
    htif_putchar(buf[i]);
}
```

## The Solution: Separated Concerns Architecture

Key insight from another AI: **Use temporary registers (`t0-t6`) for CSR reads, NOT `a0-a7`!**

The best approach separates responsibilities:
1. **Assembly trap_handler**: Only handles context save/restore
2. **C trap_handler_c**: Handles dispatching logic

### The Correct Implementation

#### Assembly: Context Management Only
```asm
trap_handler:
    # 1. Save context FIRST before reading CSRs
    addi sp, sp, -80
    sd   ra, 72(sp)
    sd   s0, 64(sp)
    sd   a0, 56(sp)       # Save original a0 (fd=1) ← CRITICAL!
    sd   a1, 48(sp)       # Save original a1 (buf)
    sd   a2, 40(sp)       # Save original a2 (count)
    sd   a7, 32(sp)       # Save original a7 (syscall number)
    
    # 2. Read CSRs into TEMPORARY registers
    csrr t0, mcause       # t0 = mcause (NOT a0!)
    csrr t1, mepc         # t1 = mepc (NOT a1!)
    sd   t1, 24(sp)       # Store mepc on stack
    
    # 3. Prepare arguments and call C handler
    mv   a0, t0           # arg0: mcause
    addi a1, sp, 24       # arg1: &mepc (so C can modify it)
    ld   a2, 32(sp)       # arg2: saved a7 (syscall number)
    ld   a3, 56(sp)       # arg3: saved a0 (fd=1) ← THE FIX!
    ld   a4, 48(sp)       # arg4: saved a1 (buf)
    ld   a5, 40(sp)       # arg5: saved a2 (count)
    call trap_handler_c   # Call C dispatcher
    
    # 4. Restore context and return
    mv   s0, a0           # Save result
    ld   t1, 24(sp)       # Load updated mepc
    csrw mepc, t1         # Write to CSR
    mv   a0, s0           # Return value
    ld   ra, 72(sp)
    ld   s0, 64(sp)
    addi sp, sp, 80
    mret
```

#### C: Dispatching Logic
```c
long trap_handler_c(long mcause, long *mepc, long a7, long a0, long a1, long a2) {
    trap_mcause = mcause;
    
    if (mcause == 11) {  // M-mode ecall
        ecall_was_called++;
        syscall_number = a7;
        
        long result;
        if (a7 == SYS_write) {
            result = sys_write(a0, (const char *)a1, a2);
        } else if (a7 == SYS_exit) {
            htif_exit(a0);
        } else {
            result = -1;
        }
        
        *mepc += 4;  // Skip ecall instruction
        return result;
    } else {
        htif_exit(50 + mcause);
    }
}
```

This separation provides:
- **Clean assembly**: Only 16 instructions for context management
- **Maintainable C**: Easy to add new syscalls
- **Correct register preservation**: Assembly ensures a0-a7 are saved before any processing

## Verification

After the fix:

```bash
$ ./build/spike --isa=rv64im rv64im_minimal_ecall.elf
DIRECT
Hello from SYS_write!
```

Success! Both outputs work:
1. **"DIRECT"** - Direct `htif_putchar()` calls
2. **"Hello from SYS_write!"** - Via `ecall` → `trap_handler` → `sys_write` → `htif_putchar()`

## Key Lessons

### 1. **Register Preservation is Critical**
When entering a trap handler, the `a0-a7` registers contain syscall arguments. These MUST be preserved before doing anything else.

### 2. **Use Temporary Registers for CSRs**
Always read CSRs into temporary registers (`t0-t6`), NEVER into argument registers (`a0-a7`) or saved registers (`s0-s11`) unless you've already saved them.

### 3. **C Trap Handlers are Dangerous**
C code gives the compiler freedom to choose registers. For trap handlers, this can lead to subtle bugs. Use inline assembly or pure assembly for trap handlers.

### 4. **RISC-V Register Convention**
- `a0-a7`: Function arguments (caller-saved)
- `t0-t6`: Temporary registers (caller-saved, safe to clobber)
- `s0-s11`: Saved registers (callee-saved)
- Using `t` registers for temporary values in trap handlers ensures we don't corrupt arguments

### 5. **Standard Trap Handler Pattern (Separated Concerns)**

**Assembly (context management only):**
```asm
trap_handler:
    # 1. Save all context
    addi sp, sp, -FRAME_SIZE
    sd   ra, ...
    sd   s0, ...
    sd   a0-a7, ...    # Save syscall arguments FIRST!
    
    # 2. Read trap information (use t registers!)
    csrr t0, mcause
    csrr t1, mepc
    csrr t2, mtval
    sd   t1, MEPC_OFFSET(sp)
    
    # 3. Prepare arguments and call C dispatcher
    mv   a0, t0                    # mcause
    addi a1, sp, MEPC_OFFSET       # &mepc
    ld   a2-a7, ...                # Saved syscall arguments
    call trap_handler_c
    
    # 4. Restore context and return
    ld   t1, MEPC_OFFSET(sp)       # C may have updated mepc
    csrw mepc, t1
    ld   ra, ...
    addi sp, sp, FRAME_SIZE
    mret
```

**C (dispatching logic):**
```c
long trap_handler_c(long mcause, long *mepc, long a7, long a0, ...) {
    if (mcause == CAUSE_ECALL) {
        // Dispatch syscalls
        long result = handle_syscall(a7, a0, a1, a2, ...);
        *mepc += 4;  // Skip ecall instruction
        return result;
    } else {
        // Handle other traps
        return handle_exception(mcause, *mepc);
    }
}
```

**Benefits:**
- Assembly is minimal and predictable (no compiler surprises)
- C code is readable and easy to extend
- Clear separation of concerns

## Files Involved

- `rv64im_minimal_ecall.c` - Demonstrates both buggy C and fixed assembly trap handlers
- `build_minimal_ecall.sh` - Build and test script
- `rv64im_minimal_ecall.ld` - Linker script with proper symbol placement

## Testing

```bash
./build_minimal_ecall.sh
```

Expected output:
- ✅ ECALL TEST PASSED
- Console output: "DIRECT" and "Hello from SYS_write!"
- Exit code: 0

## References

- RISC-V Privileged Spec: CSR descriptions
- RISC-V ABI: Register calling convention
- Spike simulator: HTIF protocol implementation

