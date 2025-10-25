# HTIF Exit Codes - How to Use

## Summary

You were correct - `htif_exit(1)` should output `*** FAILED *** (tohost = 1)`, and it now does!

The key issues that were fixed:
1. **8-byte alignment**: `tohost` and `fromhost` must be 8-byte aligned for 64-bit stores
2. **Extern declarations**: Use `extern volatile long tohost` instead of defining with section attributes
3. **Simple exit**: Don't wait in an infinite loop after writing the exit command

## Working Example

```c
// Declare HTIF symbols (defined by linker)
extern volatile long tohost;
extern volatile long fromhost;

#define HTIF_DEVICE_SYSCALL 0x00UL
#define HTIF_CMD_SYSCALL    0x00UL

static inline long htif_cmd(long device, long cmd, long payload) {
    return (device << 56) | (cmd << 48) | (payload & 0xFFFFFFFFFFFFUL);
}

void htif_exit(long code) {
    long exit_payload = (code << 1) | 1;
    tohost = htif_cmd(HTIF_DEVICE_SYSCALL, HTIF_CMD_SYSCALL, exit_payload);
    // Spike terminates immediately - no need to wait
}

void _start(void) {
    // Your code here...
    
    htif_exit(1);  // Exit with failure
}
```

## Expected Output

### Exit Code 0 (Success)
```bash
$ ./build/spike --isa=rv64im program.elf
$ echo $?
0
```
No error message, clean exit with code 0.

### Exit Code 1 (Failure)
```bash
$ ./build/spike --isa=rv64im program.elf
*** FAILED *** (tohost = 1)
$ echo $?
1
```

### Exit Code 42 (Custom)
```bash
$ ./build/spike --isa=rv64im program.elf
*** FAILED *** (tohost = 42)
$ echo $?
42
```

## HTIF Exit Payload Format

The exit payload format is:
```
payload = (exit_code << 1) | 1
```

Examples:
- Exit code 0: `(0 << 1) | 1 = 1`
- Exit code 1: `(1 << 1) | 1 = 3`
- Exit code 42: `(42 << 1) | 1 = 85`

The final `| 1` marks this as an exit command (bit 0 = 1).

Spike extracts the exit code from the payload and:
- If code == 0: exits cleanly
- If code != 0: prints `*** FAILED *** (tohost = <code>)` and exits with that code

## Linker Script Requirements

The linker script must ensure 8-byte alignment:

```ld
SECTIONS {
    . = 0x80000000;
    
    .text : { *(.text*) } > RAM
    .data : { *(.data*) } > RAM
    .bss  : { *(.bss*) *(COMMON) } > RAM
    
    /* CRITICAL: 8-byte alignment */
    . = ALIGN(8);
    
    .htif : ALIGN(8) {
        tohost = .;
        . = . + 8;
        fromhost = .;
        . = . + 8;
    } > RAM
}
```

Without 8-byte alignment, you'll get: `terminate called after throwing an instance of 'std::runtime_error' what():  misaligned address`

## Testing

Run the test script to see all exit codes in action:

```bash
./test_exit_codes.sh
```

This will demonstrate:
- Exit code 0 (success) - no error message
- Exit code 1 (failure) - shows `*** FAILED *** (tohost = 1)`
- Exit code 42 (custom) - shows `*** FAILED *** (tohost = 42)`

## HTIF Protocol Details

### Command Format
HTIF commands are 64-bit values:
```
[63:56] device  (8 bits)  - 0x00 = syscall, 0x01 = console
[55:48] command (8 bits)  - 0x00 = syscall/getchar, 0x01 = putchar
[47:0]  payload (48 bits) - command-specific data
```

### Syscall Device (0x00)
- Command 0x00: Exit/syscall
  - Payload format: `(exit_code << 1) | 1` for exit
  - Spike checks bit 0: if set, it's an exit command
  - Spike extracts exit code: `code = payload >> 1`

### Console Device (0x01)
- Command 0x00: Get character (not fully supported in bare-metal)
- Command 0x01: Put character (requires proxy kernel in bare-metal mode)

**Note**: Console I/O (putchar/getchar) doesn't work reliably in bare-metal mode without the proxy kernel (pk). For simple programs, just use the exit mechanism.

## Build and Run

```bash
# Build your program
riscv64-unknown-elf-gcc \
    -march=rv64im \
    -mabi=lp64 \
    -mcmodel=medany \
    -nostdlib \
    -nostartfiles \
    -ffreestanding \
    -O1 \
    -T rv64im_minimal.ld \
    -o program.elf \
    program.c

# Run with Spike
./build/spike --isa=rv64im program.elf

# With timeout (recommended for testing)
timeout 5 ./build/spike --isa=rv64im program.elf
```

## Common Issues

### 1. Misaligned Address Error
**Problem**: `tohost` is not 8-byte aligned  
**Solution**: Add `. = ALIGN(8);` before the `.htif` section in linker script

### 2. Program Hangs/Timeouts
**Problem**: Waiting for `tohost` to be cleared or using console I/O  
**Solution**: Don't wait after exit, and avoid HTIF console commands in bare-metal

### 3. Wrong tohost Address
**Problem**: Spike can't find `tohost`  
**Solution**: Use `extern volatile long tohost;` and let linker place it (Spike finds it via ELF symbols)

## Files

- `rv64im_minimal.c` - Working example with proper exit
- `rv64im_minimal.ld` - Linker script with correct alignment
- `test_exit_codes.sh` - Comprehensive test script
- `build_minimal.sh` - Build script with 5-second timeout

## References

- RISC-V ISA Spec: https://riscv.org/technical/specifications/
- Spike Simulator: https://github.com/riscv-software-src/riscv-isa-sim
- HTIF Protocol: See Spike source code `fesvr/htif.h`

