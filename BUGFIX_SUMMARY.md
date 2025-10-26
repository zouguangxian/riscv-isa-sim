# Bug Fix Summary: Trap Handler Register Corruption

## Issue
`write(1, "Hello from SYS_write!\n", 22)` via `ecall` was not producing console output, despite all validation checks passing (correct return value, ecall count, mcause).

## Root Cause
The C trap handler was reading `mcause` CSR into a variable, and the compiler chose to use the `a0` register for this. This **overwrote** the original `a0` value (fd=1) with `mcause` (11). When `sys_write` was called, it received `fd=11` instead of `fd=1`, causing the output check to fail.

## Before (Buggy Assembly)
```asm
trap_handler:
    csrr  a0, mcause       # ← BUG: Overwrote a0 (fd=1) with mcause (11)
    mv    s1, a0           # s1 = 11 (mcause)
    ...
    mv    a0, s1           # a0 = 11
    jal   sys_write        # sys_write(11, buf, count) - wrong fd!
```

**Result:** `sys_write` received `fd=11`, check `if (fd == STDOUT_FILENO)` failed (11 ≠ 1), no output.

## After (Fixed Assembly)
```asm
trap_handler:
    sd    a0, 24(sp)       # ← FIX: Save original a0 (fd=1) FIRST
    sd    a1, 16(sp)       # Save original a1 (buf)
    sd    a2, 8(sp)        # Save original a2 (count)
    
    csrr  t0, mcause       # ← FIX: Use t0, NOT a0!
    csrr  t1, mepc         # ← FIX: Use t1, NOT a1!
    
    li    t2, 11
    bne   t0, t2, trap_handler_other
    
    ld    a0, 24(sp)       # ← FIX: Restore original a0 (fd=1)
    ld    a1, 16(sp)       # Restore original a1
    ld    a2, 8(sp)        # Restore original a2
    jal   sys_write        # sys_write(1, buf, count) - correct!
```

**Result:** `sys_write` received `fd=1`, check passed (1 == 1), output printed!

## Output Comparison

### Before (No Console Output)
```
========================================
========================================
✅ ECALL TEST PASSED!
```

### After (With Console Output)
```
========================================
DIRECT
Hello from SYS_write!
========================================
✅ ECALL TEST PASSED!
```

## Code Changes

### File: `rv64im_minimal_ecall.c`

**Removed:** C trap handler with inline assembly
**Added:** Pure assembly trap handler that:
1. Saves `a0-a7` to stack BEFORE reading CSRs
2. Uses `t0-t6` (temporary registers) for CSR reads
3. Restores `a0-a7` from stack before calling C functions

## Key Insight from Other AI

> "Use `t0` instead of `a0` for reading `mcause`"

This was the critical insight. Temporary registers (`t0-t6`) are designed for temporary values and won't corrupt function arguments.

## Technical Details

### RISC-V Register Roles
- **`a0-a7`**: Function arguments (syscall arguments in trap context)
- **`t0-t6`**: Temporary registers (safe to use for CSR reads)
- **`s0-s11`**: Saved registers (must be preserved across calls)

### Syscall Argument Layout
When `ecall` is executed:
```
a7 = syscall number (64 for SYS_write)
a0 = arg0 (fd = 1)
a1 = arg1 (buf pointer)
a2 = arg2 (count = 22)
```

These MUST be preserved until we're ready to use them!

## Files Modified
1. `rv64im_minimal_ecall.c` - Rewritten trap handler in pure assembly
2. `build_minimal_ecall.sh` - Updated success messages
3. `TRAP_HANDLER_LESSON.md` - Documentation of the lesson learned

## Verification
```bash
./build_minimal_ecall.sh
```

Expected results:
- ✅ Build successful
- ✅ Test passed
- ✅ Console output: "DIRECT\nHello from SYS_write!\n"
- ✅ Exit code: 0

## Lesson Learned

**Always use temporary registers (`t0-t6`) for CSR reads in trap handlers!**

Never assume the C compiler will preserve specific registers when using inline assembly. For critical low-level code like trap handlers, use pure assembly to have full control over register allocation.

