# HTIF Putchar Guide - Console I/O in Spike

## TL;DR

**HTIF console I/O (putchar) does NOT work in bare-metal Spike.**  
**Use Spike with proxy kernel (pk) for console output.**

## Why Putchar Doesn't Work in Bare-Metal

### The Problem

HTIF console device (`HTIF_DEVICE_CONSOLE`) requires bidirectional communication:
1. Target writes to `tohost` with putchar command
2. Host (Spike) processes the character
3. Host **clears** `tohost` to acknowledge
4. Target waits for `tohost` to be cleared before continuing

In bare-metal mode, step 3 **doesn't happen automatically** - Spike doesn't clear `tohost` for console I/O without the proxy kernel.

### What Happens

```c
void htif_putchar(char ch) {
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, ch);
    
    // Wait for acknowledgment
    while (tohost != 0) {
        // HANGS FOREVER in bare-metal mode!
        // Spike never clears tohost
    }
}
```

Result: **Infinite loop, timeout.**

## Solutions

### Option 1: Use Proxy Kernel (Recommended)

The proxy kernel (`pk`) handles HTIF communication properly.

**Install pk:**
```bash
# Clone and build riscv-pk
git clone https://github.com/riscv/riscv-pk.git
cd riscv-pk
mkdir build && cd build
../configure --prefix=$RISCV --host=riscv64-unknown-elf
make
make install
```

**Write a program with putchar:**
```c
#include <unistd.h>

void _start(void) {
    const char *msg = "Hello from pk!\n";
    write(1, msg, 15);
    
    // Exit
    asm volatile ("li a0, 0; li a7, 93; ecall");
}
```

**Compile and run:**
```bash
riscv64-unknown-elf-gcc -march=rv64im -mabi=lp64 -nostdlib \
    -o program.elf program.c

# Run with proxy kernel
./build/spike pk program.elf
```

Output:
```
Hello from pk!
```

### Option 2: Use Full Newlib/Glibc

Compile with standard C library:

```c
#include <stdio.h>

int main() {
    printf("Hello, World!\n");
    return 0;
}
```

```bash
riscv64-unknown-elf-gcc -o program program.c
./build/spike pk program
```

### Option 3: Use QEMU Instead

QEMU handles console I/O in bare-metal mode:

```bash
qemu-system-riscv64 -machine virt -nographic -bios none \
    -kernel your_program.elf
```

### Option 4: Exit Code Communication (Current Approach)

What we do in `rv64im_minimal_ecall.c`:

```c
// Instead of printing messages, use exit codes to communicate results
if (test_passed) {
    htif_exit(0);  // Success
} else {
    htif_exit(error_code);  // Specific error
}
```

Then your build script interprets the exit code:
```bash
if timeout 5 ./build/spike --isa=rv64im program.elf; then
    echo "✅ Test passed!"
else
    case $? in
        10) echo "❌ Error: No ecalls executed";;
        13) echo "❌ Error: Wrong result";;
    esac
fi
```

## Code Reference

### Included But Not Used

In `rv64im_minimal_ecall.c`, we include putchar functions for reference:

```c
// HTIF putchar - output a single character
// NOTE: Console I/O doesn't work reliably in bare-metal Spike
// This is for demonstration purposes only
void htif_putchar(char ch) {
    tohost = htif_cmd(HTIF_DEVICE_CONSOLE, HTIF_CMD_PUTCHAR, ch);
    tohost = 0;  // Manual clear (doesn't help in bare-metal)
}

void htif_puts(const char *s) {
    while (*s) {
        htif_putchar(*s);
        s++;
    }
}
```

These functions are **not called** in `_start()` because they would cause the program to hang or produce no output.

### Example With Proxy Kernel

If you want to test putchar properly, create this file:

```c
// rv64im_with_pk.c - Requires proxy kernel
extern volatile long tohost;

#define HTIF_CMD(d,c,p) ((d<<56)|(c<<48)|(p&0xFFFFFFFFFFFFUL))

void putchar_pk(char ch) {
    tohost = HTIF_CMD(1, 1, ch);  // Console device, putchar cmd
    while (tohost != 0);  // Wait for pk to clear it
}

void puts_pk(const char *s) {
    while (*s) putchar_pk(*s++);
}

void _start(void) {
    puts_pk("Hello from proxy kernel!\n");
    tohost = HTIF_CMD(0, 0, 1);  // Exit with code 0
}
```

Build and run:
```bash
riscv64-unknown-elf-gcc -march=rv64im -mabi=lp64 -nostdlib -nostartfiles \
    -T rv64im_minimal.ld -o rv64im_with_pk.elf rv64im_with_pk.c

./build/spike pk rv64im_with_pk.elf
```

Output:
```
Hello from proxy kernel!
```

## Summary

| Method | Console Output | Complexity | Use Case |
|--------|----------------|------------|----------|
| Bare-metal Spike | ❌ No | Simple | Testing basic instructions |
| Spike + pk | ✅ Yes | Medium | Testing syscalls with output |
| Spike + newlib | ✅ Yes | Medium | Standard C programs |
| QEMU bare-metal | ✅ Yes | Medium | Alternative to Spike |
| Exit codes | ✅ Indirect | Simple | What we use now ✅ |

## What We Demonstrated

In `rv64im_minimal_ecall.c`:
- ✅ ECALL instruction works
- ✅ Trap handler is invoked  
- ✅ Arguments are passed
- ✅ Results are returned
- ✅ Execution resumes
- ❌ Console output (requires pk)

## Files

- `rv64im_minimal_ecall.c` - Working example (no console output)
- `build_minimal_ecall.sh` - Build script with exit code interpretation
- `PUTCHAR_GUIDE.md` - This file

## Next Steps

To add console output to your ECALL tests:
1. Install proxy kernel (`pk`)
2. Run: `./build/spike pk rv64im_minimal_ecall.elf`
3. Modify code to use `putchar_pk()` function
4. Enjoy console output! 🎉

