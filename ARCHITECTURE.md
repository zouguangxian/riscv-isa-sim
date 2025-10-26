# Trap Handler Architecture

This document describes the clean separation-of-concerns architecture used for trap handling in `rv64im_minimal_ecall.c`.

## Architecture Overview

```
┌─────────────┐
│   ecall     │  User code executes ecall instruction
└──────┬──────┘
       │
       ▼
┌─────────────────────────────────────┐
│  trap_handler (Assembly)            │  Context save/restore ONLY
│                                     │
│  1. Save registers to stack         │  • Save ra, s0
│     - Save ra, s0                   │  • Save a0-a7 (syscall args)
│     - Save a0-a7 (FIRST!)           │  • Save a7 (syscall number)
│                                     │
│  2. Read CSRs (use t registers!)    │  • csrr t0, mcause
│     - csrr t0, mcause               │  • csrr t1, mepc
│     - csrr t1, mepc                 │  • Store mepc on stack
│                                     │
│  3. Call C dispatcher               │  • Prepare args (a0-a5)
│     - mv a0, t0         (mcause)    │  • call trap_handler_c
│     - addi a1, sp, 24   (&mepc)     │
│     - ld a2-a5 from stack           │
│     - call trap_handler_c           │
│                                     │
│  4. Restore and return              │  • Load updated mepc
│     - Load updated mepc             │  • csrw mepc
│     - csrw mepc, t1                 │  • Restore registers
│     - Restore registers             │  • mret
│     - mret                           │
└──────┬──────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  trap_handler_c (C)                 │  High-level logic ONLY
│                                     │
│  Signature:                         │
│    long trap_handler_c(             │
│        long mcause,                 │
│        long *mepc,                  │
│        long a7, // syscall number   │
│        long a0, // arg0             │
│        long a1, // arg1             │
│        long a2) // arg2             │
│                                     │
│  Logic:                             │
│    if (mcause == 11) { // ecall     │
│        if (a7 == SYS_write)         │
│            return sys_write(...)    │
│        else if (a7 == SYS_exit)     │
│            htif_exit(...)           │
│        else                         │
│            return -1                │
│        *mepc += 4; // skip ecall    │
│    } else {                         │
│        htif_exit(50 + mcause)       │
│    }                                │
└──────┬──────────────────────────────┘
       │
       ▼
┌─────────────────────────────────────┐
│  sys_write, htif_exit, etc.         │  Syscall implementations
└─────────────────────────────────────┘
```

## Key Design Principles

### 1. Clear Separation of Concerns

**Assembly trap_handler:**
- **Responsibility**: Context management only
- **Size**: ~16 instructions
- **Benefits**: 
  - Predictable behavior (no compiler optimizations)
  - Easy to audit for correctness
  - Minimal changes needed

**C trap_handler_c:**
- **Responsibility**: Dispatching logic only
- **Benefits**:
  - Easy to read and maintain
  - Simple to add new syscalls
  - Can use normal C control flow (if/switch)

### 2. Register Preservation

**Critical Rule**: Save `a0-a7` BEFORE reading any CSRs!

```asm
# CORRECT ORDER:
sd   a0, 56(sp)       # ← Save FIRST
sd   a1, 48(sp)
sd   a2, 40(sp)
sd   a7, 32(sp)
csrr t0, mcause       # ← Read CSRs into t registers

# WRONG ORDER:
csrr a0, mcause       # ← BUG: Overwrites a0!
sd   a0, 56(sp)       # ← Saves mcause, not original a0
```

### 3. Temporary Register Usage

**Always use `t0-t6` for CSR reads:**
- `t0-t6`: Temporary registers, safe to clobber
- `a0-a7`: Function arguments, must preserve for syscalls
- `s0-s11`: Saved registers, callee must preserve

```asm
csrr t0, mcause      # ✓ CORRECT
csrr t1, mepc        # ✓ CORRECT
csrr a0, mcause      # ✗ WRONG - corrupts syscall arg!
```

### 4. Stack Frame Layout

```
High Address
┌─────────────┐
│   ra        │  +72(sp)
├─────────────┤
│   s0        │  +64(sp)
├─────────────┤
│   a0 (fd)   │  +56(sp) ← Original syscall arguments
├─────────────┤
│   a1 (buf)  │  +48(sp)
├─────────────┤
│   a2 (count)│  +40(sp)
├─────────────┤
│   a7 (num)  │  +32(sp)
├─────────────┤
│   mepc      │  +24(sp) ← Can be modified by C handler
├─────────────┤
│  (unused)   │  +16(sp)
├─────────────┤
│  (unused)   │  +8(sp)
├─────────────┤
│  (unused)   │   0(sp)
└─────────────┘ ← sp
Low Address
```

## Implementation Details

### Assembly Entry Point

```asm
trap_handler:
    # Stack frame: 80 bytes
    addi sp, sp, -80
    
    # Save link register and frame pointer
    sd   ra, 72(sp)
    sd   s0, 64(sp)
    
    # Save syscall arguments (CRITICAL!)
    sd   a0, 56(sp)
    sd   a1, 48(sp)
    sd   a2, 40(sp)
    sd   a7, 32(sp)
    
    # Read CSRs into temporaries
    csrr t0, mcause
    csrr t1, mepc
    sd   t1, 24(sp)    # Store for C handler to modify
    
    # Prepare C function arguments
    mv   a0, t0        # mcause
    addi a1, sp, 24    # &mepc
    ld   a2, 32(sp)    # a7 (syscall number)
    ld   a3, 56(sp)    # a0 (first arg)
    ld   a4, 48(sp)    # a1 (second arg)
    ld   a5, 40(sp)    # a2 (third arg)
    
    # Call C dispatcher
    call trap_handler_c
    
    # Restore and return
    mv   s0, a0        # Save return value
    ld   t1, 24(sp)    # Load possibly-modified mepc
    csrw mepc, t1      # Update CSR
    mv   a0, s0        # Return value
    ld   ra, 72(sp)
    ld   s0, 64(sp)
    addi sp, sp, 80
    mret
```

### C Dispatcher

```c
long trap_handler_c(long mcause, long *mepc, 
                    long a7, long a0, long a1, long a2) {
    trap_mcause = mcause;
    
    if (mcause == 11) {  // M-mode ecall
        ecall_was_called++;
        syscall_number = a7;
        
        long result;
        
        // Dispatch syscalls
        if (a7 == SYS_write) {
            // a0=fd, a1=buf, a2=count
            result = sys_write(a0, (const char *)a1, a2);
        } else if (a7 == SYS_exit) {
            // a0=exit_code
            htif_exit(a0);
            __builtin_unreachable();
        } else {
            // Unsupported
            result = -1;
        }
        
        // Skip ecall instruction
        *mepc += 4;
        
        return result;
    } else {
        // Non-ecall trap
        htif_exit(50 + mcause);
        __builtin_unreachable();
    }
}
```

## Adding New Syscalls

Adding a new syscall is simple - just modify the C dispatcher:

```c
long trap_handler_c(...) {
    // ...
    if (a7 == SYS_write) {
        result = sys_write(a0, (const char *)a1, a2);
    } else if (a7 == SYS_exit) {
        htif_exit(a0);
    } else if (a7 == SYS_read) {  // NEW!
        result = sys_read(a0, (char *)a1, a2);
    } else {
        result = -1;
    }
    // ...
}
```

No assembly changes needed!

## Benefits

1. **Correctness**: Assembly ensures syscall arguments are never corrupted
2. **Maintainability**: High-level logic in C is easy to understand and modify
3. **Extensibility**: Adding new syscalls doesn't require touching assembly
4. **Debuggability**: Clear separation makes it easy to trace execution
5. **Performance**: Minimal overhead (just one function call)

## Testing

```bash
./build_minimal_ecall.sh
```

Expected output:
```
DIRECT
Hello from SYS_write!
✅ ECALL TEST PASSED!
```

## See Also

- `TRAP_HANDLER_LESSON.md` - Detailed explanation of the register corruption bug
- `BUGFIX_SUMMARY.md` - Before/after comparison
- `rv64im_minimal_ecall.c` - Complete implementation

