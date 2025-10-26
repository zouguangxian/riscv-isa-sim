# How Spike Handles ECALL and Invokes Trap Handler

## Complete Flow from ECALL Instruction to Trap Handler

### Step 1: ECALL Instruction Execution

**File:** `riscv/insns/ecall.h`

```c
switch (STATE.prv)
{
  case PRV_U: throw trap_user_ecall();           // U-mode: mcause = 8
  case PRV_S:
    if (STATE.v)
      throw trap_virtual_supervisor_ecall();     // VS-mode: mcause = 10
    else
      throw trap_supervisor_ecall();             // S-mode: mcause = 9
  case PRV_M: throw trap_machine_ecall();        // M-mode: mcause = 11 ✓
  default: abort();
}
```

**What happens:**
- Spike decodes the `ecall` instruction (opcode 0x73)
- Checks current privilege mode (`STATE.prv`)
- In our bare-metal case: `PRV_M` (machine mode)
- Throws `trap_machine_ecall()` exception

**Trap cause constant** (`riscv/encoding.h`):
```c
#define CAUSE_MACHINE_ECALL 0xb  // = 11 decimal
```

---

### Step 2: Exception Caught in Main Loop

**File:** `riscv/execute.cc:329-332`

```cpp
try {
    // Execute instruction
    pc = execute_insn_fast(this, pc, fetch);
    ...
}
catch(trap_t& t)  // ← Catches the trap_machine_ecall exception
{
    take_trap(t, pc);  // ← Invoke trap handling
    n = instret;
    ...
}
```

**What happens:**
- Main execution loop catches the `trap_t` exception
- Calls `take_trap()` with the trap object and current PC
- The `pc` value becomes `epc` (exception PC)

---

### Step 3: Take Trap - Setup CSRs

**File:** `riscv/processor.cc:390-536` (key parts)

```cpp
void processor_t::take_trap(trap_t& t, reg_t epc)
{
    unsigned max_xlen = isa.get_max_xlen();
    
    // Debug output (if enabled)
    if (debug) {
        std::stringstream s;
        s << "core " << id << ": exception " << t.name() 
          << ", epc 0x" << std::hex << epc << std::endl;
        debug_output_log(&s);
    }
    
    // ... delegation checks omitted for clarity ...
    
    // For M-mode trap (our case):
    
    // Calculate trap handler address from mtvec
    reg_t bit = t.cause();  // = 11 for machine ecall
    const reg_t vector = (state.mtvec->read() & 1) && interrupt ? 4 * bit : 0;
    const reg_t trap_handler_address = (state.mtvec->read() & ~(reg_t)1) + vector;
    
    // Set PC to trap handler ← THIS IS THE KEY!
    state.pc = trap_handler_address;
    
    // Set exception CSRs
    state.mepc->write(epc);                    // mepc = PC of ecall instruction
    state.mcause->write(t.cause());            // mcause = 11
    state.mtval->write(t.get_tval());          // mtval = 0 (not used for ecall)
    
    // Update mstatus
    reg_t s = state.mstatus->read();
    s = set_field(s, MSTATUS_MPIE, get_field(s, MSTATUS_MIE));  // MPIE ← MIE
    s = set_field(s, MSTATUS_MPP, state.prv);                    // MPP ← current prv
    s = set_field(s, MSTATUS_MIE, 0);                            // MIE ← 0 (disable interrupts)
    state.mstatus->write(s);
    
    // Set privilege to M-mode
    set_privilege(PRV_M, false);
}
```

**What happens:**

1. **Calculate trap handler address:**
   ```
   trap_handler_address = (mtvec & ~1) + vector
   
   For our case (direct mode, not vectored):
   - mtvec = 0x80000094 (address of our trap_handler)
   - vector = 0 (not an interrupt)
   - trap_handler_address = 0x80000094
   ```

2. **Set CSRs:**
   - `mepc = 0x800001a0` (PC of ecall instruction)
   - `mcause = 11` (CAUSE_MACHINE_ECALL)
   - `mtval = 0` (not used for ecall)

3. **Update mstatus:**
   - Save current `MIE` to `MPIE`
   - Save current privilege to `MPP`
   - Clear `MIE` (disable interrupts)

4. **Set PC:**
   - `state.pc = 0x80000094` ← **Jump to trap handler!**

---

### Step 4: Resume Execution at Trap Handler

**File:** `riscv/execute.cc:235-327`

```cpp
while (n > 0) {
    size_t instret = 0;
    reg_t pc = state.pc;  // ← Now pc = 0x80000094 (trap handler)
    
    try {
        // ... execution continues at trap handler ...
        insn_fetch_t fetch = mmu->load_insn(pc);
        pc = execute_insn_logged(this, pc, fetch);
        ...
    }
```

**What happens:**
- Next loop iteration starts with `pc = trap_handler_address`
- CPU begins executing trap handler code
- Trap handler can read CSRs:
  - `csrr a0, mcause` → reads 11
  - `csrr s0, mepc` → reads 0x800001a0

---

### Step 5: Return from Trap Handler

When trap handler executes `mret`:

**File:** `riscv/insns/mret.h`

```c
// Restore PC from mepc
STATE.pc = STATE.mepc->read();

// Restore interrupt enable from MPIE
reg_t s = STATE.mstatus->read();
s = set_field(s, MSTATUS_MIE, get_field(s, MSTATUS_MPIE));
s = set_field(s, MSTATUS_MPIE, 1);

// Restore privilege from MPP  
reg_t prev_prv = get_field(s, MSTATUS_MPP);
s = set_field(s, MSTATUS_MPP, PRV_U);
STATE.mstatus->write(s);

set_privilege(prev_prv, false);
```

---

## Complete Timeline

```
Time | PC          | Action
-----|-------------|--------------------------------------------------------
  0  | 0x800001a0  | Execute: ecall
     |             | ↓ throw trap_machine_ecall()
     |             | ↓ catch(trap_t& t)
     |             | ↓ take_trap(t, 0x800001a0)
     |             |   • mepc ← 0x800001a0
     |             |   • mcause ← 11
     |             |   • mstatus.MPIE ← mstatus.MIE
     |             |   • mstatus.MIE ← 0
     |             |   • state.pc ← 0x80000094 (mtvec value)
-----|-------------|--------------------------------------------------------
  1  | 0x80000094  | Execute: trap_handler entry
     |             |   addi sp, sp, -16
  2  | 0x80000098  |   sd ra, 8(sp)
  3  | 0x8000009c  |   csrr a0, mcause    → a0 = 11
     |             |   ... handle the trap ...
  N  | 0x80000xxx  | Execute: mret
     |             | ↓ state.pc ← mepc (0x800001a0)
     |             | ↓ Restore mstatus.MIE from MPIE
     |             | ↓ Restore privilege from MPP
-----|-------------|--------------------------------------------------------
 N+1 | 0x800001a4  | Resume: instruction after ecall
     |             | (mepc was incremented by trap handler: mepc += 4)
```

---

## Key Design Points

1. **Trap is an Exception (C++ throw/catch)**
   - Spike uses C++ exceptions for traps
   - Clean separation between normal execution and trap handling
   - Automatic unwinding of instruction execution

2. **State Changes are Atomic**
   - All CSR updates happen in `take_trap()`
   - PC change is the last step
   - No partial trap state visible

3. **No Special Handling for M-mode ECALL**
   - Spike treats all ecalls the same way
   - Only difference is the `mcause` value
   - Trap handler must be set up correctly via `mtvec`

4. **HTIF Loop is Independent**
   - Trap handling happens during `step(n)` execution
   - HTIF loop runs in `idle()` between instruction batches
   - Writing to `tohost` during trap handler works fine!
   - `tohost` will be read/cleared in the next HTIF loop iteration

---

## Why htif_putchar Should Work from Trap Handlers

**The HTIF loop structure:**

```cpp
while (!should_exit()) {
    uint64_t tohost;
    
    // Read tohost
    if ((tohost = mem.read_uint64(tohost_addr)) != 0) {
        mem.write_uint64(tohost_addr, 0);  // Clear it
    }
    
    // Execute some instructions
    if (tohost != 0) {
        device_list.handle_command(cmd);
    } else {
        idle();  // ← step(5000) executes here
    }
}
```

**During instruction execution:**
- `idle()` calls `step(5000)` 
- ecall happens during `step()`
- Trap handler writes to `tohost`
- `step()` returns
- **Next HTIF loop iteration reads and clears tohost!** ✓

So theoretically, `htif_putchar` from trap handler **should work**! The real issue is likely the compiler bug passing wrong arguments.

---

## Summary

Spike's trap handling:
1. ✅ Detects `ecall` based on privilege mode
2. ✅ Throws C++ exception with correct cause (11)
3. ✅ Catches exception in main loop
4. ✅ Calls `take_trap()` to set up CSRs
5. ✅ Sets `PC = mtvec` to jump to trap handler
6. ✅ Continues execution at trap handler
7. ✅ Trap handler can write to `tohost`
8. ✅ HTIF loop processes `tohost` after `step()` returns

The `mcause=11` is correct and not the problem!

