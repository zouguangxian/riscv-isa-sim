# RV64IM Examples for Spike

This repository contains several ways to run examples with `spike --isa=rv64im`, but most require external toolchains or pk (proxy kernel).

## Available Examples in the Repository

### 1. Simple Assembly Example (Created)

I've created a minimal `rv64im_example.s` that demonstrates:
- Basic integer operations (RV64I)
- Multiplication instructions (M extension)  
- 64-bit operations
- Branching and loops
- HTIF tohost/fromhost communication

**Files created:**
- `rv64im_example.s` - Assembly source
- `rv64im_example.ld` - Linker script
- `build_rv64im_example.sh` - Build script

**To build and run (requires RISC-V toolchain):**
```bash
# Install RISC-V toolchain first
./build_rv64im_example.sh

# Run with spike
spike --isa=rv64im rv64im_example.elf

# Or run bare-metal without pk
spike --isa=rv64im --pc=0x80000000 rv64im_example.elf
```

### 2. Existing CI Test Examples

The repository has test examples in `ci-tests/` but they require:
- `riscv64-linux-gnu-gcc` toolchain
- Proxy kernel (pk) for system calls
- Higher ISA extensions (most use printf/floating-point)

**Examples that could work with modifications:**
- `ci-tests/dummy-slliuw.c` - Simple instruction test (remove printf)
- `ci-tests/atomics.c` - Atomic operations test (requires 'A' extension)

### 3. Snippy-Generated Tests

The `ci-tests/snippy-tests/` directory contains YAML configurations for generating random instruction tests:
- `basic.yaml` - Basic integer instructions
- `boot-code.s` - Bare-metal boot stub with tohost/fromhost

**To use snippy tests:**
```bash
# Requires snippy tool (external download)
cd ci-tests
./generate-snippy-tests.sh 
./run-snippy-tests.sh snippy-tests spike
```

## Instructions Only Available in rv64im

The `rv64im` ISA includes:
- **RV64I**: Base 64-bit integer instruction set
- **M**: Integer multiplication and division

**Key instructions:**
- `mul, mulh, mulhsu, mulhu` - Multiplication
- `div, divu, rem, remu` - Division and remainder  
- `mulw, divw, divuw, remw, remuw` - 32-bit variants
- All standard RV64I instructions (add, sub, load, store, branch, etc.)

## What's NOT available in rv64im

- **A**: Atomic instructions
- **F/D**: Floating-point (single/double precision)
- **C**: Compressed instructions
- **V**: Vector instructions
- **Zicsr**: CSR instructions (Control and Status Registers)
- **Zifencei**: Instruction fence

## Running Without Toolchain

If you don't have the RISC-V toolchain installed, you can:

1. **Use Docker with RISC-V tools:**
   ```bash
   docker run --rm -v $(pwd):/work -w /work riscv/riscv-gnu-toolchain
   ```

2. **Install RISC-V toolchain:**
   ```bash
   # Ubuntu/Debian
   sudo apt install gcc-riscv64-unknown-elf
   
   # Or build from source
   git clone https://github.com/riscv/riscv-gnu-toolchain
   ```

3. **Use existing snippy framework:**
   The repository's CI system uses snippy to generate random test programs.

## Example Program Behavior

The `rv64im_example.s` program:
1. Performs basic arithmetic (10 + 20 = 30)
2. Tests multiplication (30 * 5 = 150)  
3. Runs a simple loop 5 times
4. Signals completion via tohost = 1
5. Enters infinite loop

**Expected spike output:**
```bash
$ spike --isa=rv64im rv64im_example.elf
# Program runs and exits cleanly when tohost is written
```

## Limitations

Most examples in this repository assume:
- Full `rv64gc` ISA (including floating-point, atomics, compressed)
- Proxy kernel for system calls
- C standard library functions

For pure `rv64im` testing, you'll need either:
- Custom assembly programs (like the one provided)
- Modified C programs that avoid unsupported features
- Bare-metal firmware-style code