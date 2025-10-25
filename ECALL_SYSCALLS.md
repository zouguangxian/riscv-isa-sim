# RISC-V ECALL Syscalls Reference

This document describes the syscalls implemented in `rv64im_minimal_ecall.c` and how they interact with Spike's HTIF interface.

## Overview

The example demonstrates how to:
1. Set up a trap handler for `ecall` instructions
2. Dispatch syscalls based on the syscall number in register `a7`
3. Use HTIF (Host-Target Interface) to communicate with Spike
4. Implement standard Linux syscalls in bare-metal RISC-V code

## HTIF Protocol

HTIF uses memory-mapped I/O with two special locations:
- `tohost` (0x80001000): Write commands from target to host
- `fromhost` (0x80001008): Read responses from host to target

### HTIF Command Format

Commands are 64-bit values with this layout:
```
[63:56] device (8 bits)
[55:48] command (8 bits)
[47:0]  payload (48 bits)
```

### HTIF Devices

| Device | Value | Description |
|--------|-------|-------------|
| HTIF_DEVICE_SYSCALL | 0x00 | System calls (exit) |
| HTIF_DEVICE_CONSOLE | 0x01 | Console I/O (putchar/getchar) |

### HTIF Commands

#### For SYSCALL device (0x00):
- `HTIF_CMD_SYSCALL` (0x00): Exit command
  - Payload: `(exit_code << 1) | 1`

#### For CONSOLE device (0x01):
- `HTIF_CMD_GETCHAR` (0x00): Read a character
  - Payload: 0
- `HTIF_CMD_PUTCHAR` (0x01): Write a character
  - Payload: character value (0-255)

## Implemented Syscalls

### SYS_write (64)

**Arguments:**
- `a0`: File descriptor (int fd)
- `a1`: Buffer pointer (const char *buf)
- `a2`: Byte count (size_t count)

**Return value:**
- `a0`: Number of bytes written, or -1 on error

**Implementation:**
- Supports `STDOUT_FILENO` (1) and `STDERR_FILENO` (2)
- Uses HTIF putchar to write each character individually
- Each character write waits for acknowledgment (tohost cleared)

**Example:**
```c
const char *msg = "Hello, World!\n";
long bytes_written = write(1, msg, 14);
```

**Assembly:**
```asm
li a7, 64          # SYS_write
li a0, 1           # fd = stdout
la a1, msg         # buf = address of string
li a2, 14          # count = 14 bytes
ecall              # invoke syscall
# a0 now contains bytes written
```

### SYS_exit (93)

**Arguments:**
- `a0`: Exit code (int status)

**Return value:**
- Does not return

**Implementation:**
- Uses HTIF syscall device with exit payload
- Payload format: `(exit_code << 1) | 1`
- Spike terminates simulation when this is written to tohost

**Example:**
```c
syscall(SYS_exit, 0, 0, 0, 0, 0, 0);  // Exit with code 0
```

**Assembly:**
```asm
li a7, 93          # SYS_exit
li a0, 0           # status = 0 (success)
ecall              # invoke syscall
# Does not return
```

### SYS_exit_group (94)

**Arguments:**
- `a0`: Exit code (int status)

**Return value:**
- Does not return

**Implementation:**
- Same as SYS_exit (in bare-metal there's no distinction)

## Available Syscalls (Not Yet Implemented)

You can extend the syscall handler to support additional syscalls. Here are some common ones:

### File Operations

| Syscall | Number | Description |
|---------|--------|-------------|
| SYS_read | 63 | Read from file descriptor |
| SYS_open | 56 | Open file |
| SYS_close | 57 | Close file descriptor |
| SYS_lseek | 62 | Reposition file offset |

### Process Operations

| Syscall | Number | Description |
|---------|--------|-------------|
| SYS_getpid | 172 | Get process ID |
| SYS_gettid | 178 | Get thread ID |
| SYS_brk | 214 | Change data segment size |

### Time Operations

| Syscall | Number | Description |
|---------|--------|-------------|
| SYS_gettimeofday | 169 | Get time of day |
| SYS_clock_gettime | 113 | Get time |

## How to Add New Syscalls

1. **Define the syscall number** in the C file:
```c
#define SYS_read 63
```

2. **Implement the syscall handler**:
```c
static long sys_read(long fd, char *buf, long count) {
    // Implementation here
    return bytes_read;
}
```

3. **Add to syscall dispatcher**:
```c
static long handle_syscall(long syscall_num, long a0, long a1, long a2, 
                          long a3, long a4, long a5) {
    switch (syscall_num) {
        case SYS_write:
            return sys_write(a0, (const char *)a1, a2);
        
        case SYS_read:
            return sys_read(a0, (char *)a1, a2);
        
        // ... other syscalls
    }
}
```

4. **Create a wrapper function** (optional, for convenience):
```c
static long read(int fd, void *buf, long count) {
    return syscall(SYS_read, fd, (long)buf, count, 0, 0, 0);
}
```

## Trap Handler Details

The trap handler is set up by writing to the `mtvec` CSR (Machine Trap Vector):

```c
void setup_trap_handler(void) {
    long trap_addr = (long)trap_handler;
    asm volatile ("csrw mtvec, %0" :: "r"(trap_addr));
}
```

When an `ecall` is executed:
1. CPU jumps to `trap_handler`
2. Handler reads `mcause` to determine trap type
3. For ecall, `mcause` is 8 (from U-mode) or 11 (from M-mode)
4. Handler extracts syscall number from `a7` and arguments from `a0-a5`
5. Handler calls `handle_syscall()` to dispatch
6. Result is placed in `a0`
7. `mepc` (Machine Exception PC) is incremented by 4 to skip ecall instruction
8. Handler returns, CPU resumes execution after ecall

## Testing the Implementation

Run the example:
```bash
./build_minimal_ecall.sh
./build/spike --isa=rv64im rv64im_minimal_ecall.elf
```

Expected output:
```
Hello from ecall!
Testing SYS_write syscall via ecall instruction
Character output: ABCDEFGHIJKLMNOPQRSTUVWXYZ
Computation test: 42 * 13 = 546 (calculated)
All ecall tests completed successfully!
```

## Debugging Tips

1. **View disassembly** to verify ecall instructions:
```bash
riscv64-unknown-elf-objdump -d rv64im_minimal_ecall.elf | less
```

2. **Run with debug output** (if Spike supports it):
```bash
./build/spike -l --isa=rv64im rv64im_minimal_ecall.elf
```

3. **Check trap handler address**:
```bash
riscv64-unknown-elf-nm rv64im_minimal_ecall.elf | grep trap_handler
```

4. **Verify HTIF locations**:
```bash
riscv64-unknown-elf-nm rv64im_minimal_ecall.elf | grep -E '(tohost|fromhost)'
```

## References

- [RISC-V Privileged Spec](https://riscv.org/technical/specifications/)
- [RISC-V Linux Syscall ABI](https://github.com/riscv/riscv-elf-psabi-doc)
- [Spike HTIF Protocol](https://github.com/riscv-software-src/riscv-isa-sim)

