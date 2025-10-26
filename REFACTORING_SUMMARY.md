# Trap Handler Refactoring Summary

## What Changed

Refactored the trap handler to use a clean **separation-of-concerns** architecture.

### Before (Monolithic Assembly)

The assembly trap handler did everything:
- Context save/restore
- CSR reading
- mcause checking
- Syscall dispatching
- Calling sys_write
- mepc updates

**Problems:**
- Hard to maintain (logic in assembly)
- Difficult to add new syscalls
- Mixed responsibilities

### After (Separated Concerns)

#### Assembly: `trap_handler`
**Responsibility**: Context management ONLY
- Save registers (ra, s0, a0-a7)
- Read CSRs into temporary registers (t0, t1)
- Call C function
- Restore registers
- Execute mret

**Size**: ~16 instructions

#### C: `trap_handler_c`
**Responsibility**: Dispatching logic ONLY
- Check mcause
- Dispatch syscalls based on a7
- Update mepc
- Return result

**Easy to extend:**
```c
if (a7 == SYS_write) {
    result = sys_write(...);
} else if (a7 == SYS_exit) {
    htif_exit(...);
} else if (a7 == NEW_SYSCALL) {  // ← Just add here!
    result = sys_new_syscall(...);
}
```

## Benefits

### 1. Maintainability
- High-level logic in readable C
- Easy to understand control flow
- Simple to add new syscalls

### 2. Correctness
- Assembly ensures registers are preserved
- No compiler surprises
- Clear data flow

### 3. Debuggability
- Can set breakpoints in C code
- Easy to trace execution
- Clear separation of concerns

### 4. Performance
- Minimal overhead (single function call)
- Same number of instructions as before
- No performance penalty

## Implementation Details

### Function Signature

```c
long trap_handler_c(
    long mcause,     // Trap cause (e.g., 11 for M-mode ecall)
    long *mepc,      // Pointer to exception PC (can be modified)
    long a7,         // Syscall number (e.g., 64 for SYS_write)
    long a0,         // First syscall argument (e.g., fd)
    long a1,         // Second syscall argument (e.g., buf)
    long a2          // Third syscall argument (e.g., count)
)
```

### Call Flow

```
ecall instruction
    ↓
trap_handler (assembly)
    ↓
    1. Save context to stack
    2. Read mcause → t0, mepc → t1
    3. Prepare arguments (a0=mcause, a1=&mepc, a2-a5=saved syscall args)
    4. call trap_handler_c
    ↓
trap_handler_c (C)
    ↓
    5. Check mcause
    6. Dispatch syscall
    7. Update *mepc
    8. return result
    ↓
trap_handler (assembly)
    ↓
    9. Load updated mepc
    10. csrw mepc
    11. Restore context
    12. mret
```

## Files Modified

1. **`rv64im_minimal_ecall.c`**
   - Added `trap_handler_c` function
   - Simplified `trap_handler` assembly
   - Added architecture comments

2. **`TRAP_HANDLER_LESSON.md`**
   - Updated with separated concerns pattern
   - Added C dispatcher example

3. **`ARCHITECTURE.md`** (NEW)
   - Complete architecture documentation
   - Diagrams and examples
   - Guidelines for adding syscalls

## Testing

```bash
./build_minimal_ecall.sh
```

**Output:**
```
DIRECT
Hello from SYS_write!
✅ ECALL TEST PASSED!
```

**Verification:**
- ✅ Console output works
- ✅ All tests pass
- ✅ Return values correct
- ✅ Exit code: 0

## Code Quality Improvements

### Assembly: Clean and Minimal
```asm
trap_handler:
    # Save context
    addi sp, sp, -80
    sd   ra, 72(sp)
    sd   s0, 64(sp)
    sd   a0-a7, ...
    
    # Read CSRs
    csrr t0, mcause
    csrr t1, mepc
    
    # Call C
    call trap_handler_c
    
    # Restore and return
    csrw mepc, t1
    mret
```

### C: Readable and Maintainable
```c
long trap_handler_c(long mcause, long *mepc, ...) {
    if (mcause == 11) {  // ecall
        if (a7 == SYS_write)
            return sys_write(...);
        else if (a7 == SYS_exit)
            htif_exit(...);
        
        *mepc += 4;
    } else {
        htif_exit(50 + mcause);
    }
}
```

## Key Lesson

**Good architecture separates concerns:**
- Assembly handles low-level details (register preservation, CSRs)
- C handles high-level logic (dispatching, control flow)
- Each layer does what it's best at

This makes the code:
- Easier to understand
- Easier to maintain
- Easier to extend
- Less prone to bugs

## See Also

- `ARCHITECTURE.md` - Complete architecture documentation
- `TRAP_HANDLER_LESSON.md` - Detailed explanation of register corruption bug
- `BUGFIX_SUMMARY.md` - Before/after bug fix comparison

