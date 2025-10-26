# Trap Handler Improvements

This document describes the improvements made to the trap handler based on feedback and best practices.

## Summary of Improvements

All suggestions from the code review have been addressed:

### ✅ 1. Stack Alignment (16-byte aligned)

**Before:** 80 bytes (still aligned, but suboptimal)
**After:** 96 bytes (6 × 16)

```asm
# Before
addi sp, sp, -80

# After
addi sp, sp, -96    # Perfectly 16-byte aligned
```

**Why it matters:**
- RISC-V ABI requires 16-byte stack alignment
- Some compilers/libraries may assume this for SIMD operations
- Better for cache line alignment

### ✅ 2. Register Usage Optimization

**Before:** Used `s0` to hold return value across mepc load
```asm
call trap_handler_c
mv   s0, a0         # Save to s0
ld   t1, 24(sp)     # Load mepc
csrw mepc, t1
mv   a0, s0         # Move back to a0
```

**After:** Use `t0` (temporary register) instead
```asm
call trap_handler_c
mv   t0, a0         # Save to t0 (more appropriate)
ld   t1, 24(sp)     # Load mepc
csrw mepc, t1
mv   a0, t0         # Move back to a0
```

**Why it matters:**
- Temporary registers (`t0-t6`) are meant for this purpose
- Saved registers (`s0-s11`) are callee-saved, so need saving/restoring
- More efficient and follows RISC-V conventions

**Additional:** Now save `s1` and `s2` as well in case C handler uses them:
```asm
sd s1, 72(sp)
sd s2, 64(sp)
```

### ✅ 3. Interrupt vs Exception Distinction

**Before:** Only checked exact mcause value
```c
if (mcause == 11) {  // M-mode ecall
    // ...
}
```

**After:** Properly distinguish interrupts from exceptions
```c
// RISC-V mcause format:
// Bit 63: Interrupt flag (1 = interrupt, 0 = exception)
// Bits 62-0: Exception/Interrupt code
long is_interrupt = mcause & (1L << 63);
long cause_code = mcause & ~(1L << 63);

if (is_interrupt) {
    // INTERRUPT HANDLING
    htif_exit(80 + cause_code);
} else {
    // EXCEPTION HANDLING
    if (cause_code == 11) {  // M-mode ecall
        // ...
    }
}
```

**Why it matters:**
- Interrupts and exceptions need different handling
- Interrupts may not need mepc adjustment
- Proper OS/kernel trap handlers must distinguish these

**mcause bit layout for RV64:**
```
Bit 63: Interrupt flag
   ┌─┐
   │1│ = Interrupt
   │0│ = Exception
   └─┘
Bits 62-0: Cause code
   ┌────────────────────────┐
   │  Exception/Int. Code   │
   └────────────────────────┘
```

### ✅ 4. Calling Convention / Portability

**Added:** Portability notes for RV32 vs RV64
```c
// PORTABILITY NOTE:
//   This implementation uses 'long' which is 64-bit on RV64.
//   For RV32, you'd want to use 'int' or 'int32_t' for register values,
//   and the interrupt bit would be bit 31 instead of bit 63.
```

**Type usage:**
- RV64: `long` (64-bit) ✓
- RV32: Would use `int` or `int32_t` (32-bit)

**For truly portable code, you could use:**
```c
#include <stdint.h>

#if __riscv_xlen == 64
typedef int64_t reg_t;
#define MCAUSE_INT_BIT (1L << 63)
#else
typedef int32_t reg_t;
#define MCAUSE_INT_BIT (1L << 31)
#endif
```

### ✅ 5. Safety / Error Handling

**Added:** Safety checks and better error handling

```c
// Safety check: ensure mepc is valid (non-null)
if (!mepc) {
    htif_exit(99);  // Fatal: null mepc pointer
    __builtin_unreachable();
}
```

**Error codes:**
- Exit 99: Fatal error (null mepc pointer)
- Exit 80+N: Interrupt code N occurred (not supported)
- Exit 50+N: Exception code N occurred (not ecall)

**Why it matters:**
- Prevents undefined behavior from null pointer dereference
- Provides clear exit codes for debugging
- Makes the code more defensive

## Before and After Comparison

### Assembly Changes

| Aspect | Before | After | Improvement |
|--------|--------|-------|-------------|
| Stack size | 80 bytes | 96 bytes | Better alignment |
| Saved regs | ra, s0 | ra, s0, s1, s2 | More robust |
| Result storage | s0 (saved reg) | t0 (temp reg) | More appropriate |
| Instructions | ~20 | ~22 | Slightly more, but safer |

### C Handler Changes

| Feature | Before | After |
|---------|--------|-------|
| Interrupt handling | ❌ | ✅ |
| Safety checks | ❌ | ✅ |
| Portability notes | ❌ | ✅ |
| Error handling | Basic | Comprehensive |

## Stack Frame Layout (New)

```
High Address
┌─────────────┐
│   ra        │  +88(sp)  ← Link register
├─────────────┤
│   s0        │  +80(sp)  ← Saved register 0
├─────────────┤
│   s1        │  +72(sp)  ← Saved register 1
├─────────────┤
│   s2        │  +64(sp)  ← Saved register 2
├─────────────┤
│   a0 (fd)   │  +56(sp)  ← Original syscall arg 0
├─────────────┤
│   a1 (buf)  │  +48(sp)  ← Original syscall arg 1
├─────────────┤
│   a2 (count)│  +40(sp)  ← Original syscall arg 2
├─────────────┤
│   a7 (num)  │  +32(sp)  ← Syscall number
├─────────────┤
│   mepc      │  +24(sp)  ← Exception PC (modifiable)
├─────────────┤
│  (padding)  │  +16(sp)  ← 16-byte alignment
├─────────────┤
│  (padding)  │  +8(sp)
├─────────────┤
│  (padding)  │   0(sp)
└─────────────┘ ← sp (16-byte aligned)
Low Address

Total: 96 bytes (6 × 16) ✓
```

## Testing

All tests pass with the improvements:

```bash
$ ./build_minimal_ecall.sh
Building minimal RV64IM C example with ecall support...
Compiling rv64im_minimal_ecall.c...
Built rv64im_minimal_ecall.elf successfully!

Running ecall test...
========================================
DIRECT
Hello from SYS_write!
========================================
✅ ECALL TEST PASSED!
```

## Performance Impact

The improvements have **minimal performance impact**:

- **Stack frame:** +16 bytes (from 80 to 96)
  - Cost: 2 extra instructions (save/restore s1, s2)
  - Benefit: Better ABI compliance and safety

- **Register usage:** Changed s0 → t0 for temp storage
  - Cost: None (same number of instructions)
  - Benefit: More semantically correct

- **Interrupt check:** Added bit masking
  - Cost: ~3 extra instructions in C
  - Benefit: Proper handling of all trap types

## Recommendations for Production Use

For a production OS/kernel trap handler, consider:

1. **Full context save:**
   ```asm
   # Save ALL registers (full context switch)
   sd x1-x31, ...
   ```

2. **Additional CSRs:**
   ```asm
   csrr t0, mtval      # Trap value (faulting address, etc.)
   csrr t1, mstatus    # Status register
   ```

3. **Nested trap support:**
   ```asm
   # Save mstatus before enabling interrupts
   csrr t0, mstatus
   csrs mstatus, MIE   # Enable interrupts
   ```

4. **Per-CPU data:**
   ```c
   struct cpu_context *ctx = get_current_cpu_context();
   ```

5. **Preemption support:**
   ```c
   if (should_preempt()) {
       schedule();  // Context switch
   }
   ```

## See Also

- `ARCHITECTURE.md` - Overall trap handler architecture
- `TRAP_HANDLER_LESSON.md` - Why register preservation matters
- `REFACTORING_SUMMARY.md` - Separation of concerns benefits
- `rv64im_minimal_ecall.c` - Implementation with all improvements

