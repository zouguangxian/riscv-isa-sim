# C Programming for RV64IM ISA

Yes, you can absolutely write C programs that compile to only use RV64IM instructions! Here are the key requirements and examples.

## Key Requirements for RV64IM C Programs

### 1. **Compiler Flags**
```bash
riscv64-unknown-elf-gcc \
    -march=rv64im \          # Use only RV64IM ISA
    -mabi=lp64 \             # 64-bit ABI without floating-point
    -nostdlib \              # Don't link standard library
    -nostartfiles \          # Don't use standard startup files
    -ffreestanding \         # Freestanding environment
    -fno-builtin \           # Don't use builtin functions
    -O1                      # Optimize (but not too aggressively)
```

### 2. **What You CAN Use**
- **Integer arithmetic**: `+`, `-`, `*`, `/`, `%`
- **Bitwise operations**: `&`, `|`, `^`, `~`, `<<`, `>>`
- **Comparisons**: `==`, `!=`, `<`, `>`, `<=`, `>=`
- **Control flow**: `if`, `while`, `for`, `switch`
- **Pointers and arrays**: Direct memory access
- **Basic data types**: `int`, `long`, `char`, pointers
- **Inline assembly**: For special instructions like `wfi`

### 3. **What You CANNOT Use**
- **Standard library functions**: `printf`, `malloc`, `strcpy`, etc.
- **Floating-point**: `float`, `double` (requires F/D extensions)
- **Atomic operations**: `__atomic_*` functions (requires A extension)
- **CSR operations**: Special compiler intrinsics (requires Zicsr)

## Examples Provided

### 1. **Simple Example (`rv64im_simple.c`)**
```c
void _start(void) {
    long x = 10, y = 5;
    long sum = x + y;           // Basic arithmetic
    long product = x * y;       // Multiplication (M extension)
    long quotient = x / y;      // Division (M extension)
    
    tohost = 1;                 // Signal completion
    while (1) asm volatile ("wfi");
}
```

**Build and run:**
```bash
./build_simple.sh
spike --isa=rv64im rv64im_simple.elf
```

### 2. **Complex Example (`rv64im_example.c`)**
More sophisticated example with:
- Function calls
- Arrays and loops
- Memory operations
- Factorial and Fibonacci calculations
- Multiple arithmetic operations

**Build and run:**
```bash
./build_rv64im_c_example.sh
spike --isa=rv64im rv64im_example_c.elf
```

## Programming Patterns for RV64IM

### Memory Operations
```c
// Direct memory access (no malloc needed)
volatile long* data = (volatile long*)0x80100000;
data[0] = 0x123456789ABCDEF0L;  // 64-bit store
data[1] = data[0] + 1;          // Load and store
```

### Custom String Operations
```c
// Since no stdlib, implement your own
int my_strlen(const char* str) {
    int len = 0;
    while (str[len]) len++;
    return len;
}

void my_strcpy(char* dest, const char* src) {
    while ((*dest++ = *src++));
}
```

### Mathematical Functions
```c
// Multiplication and division are available (M extension)
long power(long base, int exp) {
    long result = 1;
    for (int i = 0; i < exp; i++) {
        result *= base;  // Uses MUL instruction
    }
    return result;
}

long gcd(long a, long b) {
    while (b != 0) {
        long temp = a % b;  // Uses REM instruction
        a = b;
        b = temp;
    }
    return a;
}
```

### HTIF Communication
```c
// Essential for spike communication
extern volatile long tohost;
extern volatile long fromhost;

void exit_success() {
    tohost = 1;  // Signal success to spike
}

void exit_failure() {
    tohost = 2;  // Signal failure to spike
}
```

## Common Pitfalls

### 1. **Accidental Floating-Point**
```c
// WRONG - creates floating-point instructions
double x = 3.14;
float y = 2.5f;

// RIGHT - use fixed-point arithmetic
long x_fixed = 314;  // Represents 3.14 * 100
long y_fixed = 250;  // Represents 2.5 * 100
```

### 2. **Library Function Usage**
```c
// WRONG - requires standard library
printf("Hello\n");
int len = strlen(str);

// RIGHT - implement your own or use direct methods
tohost = 42;  // Signal result directly
int len = my_strlen(str);  // Custom implementation
```

### 3. **Unaligned Access**
```c
// Be careful with packed structures
struct __attribute__((packed)) data {
    char c;
    long l;  // Might be unaligned
};

// Better to ensure alignment
struct data {
    long l;
    char c;
} __attribute__((aligned(8)));
```

## Verification

The build scripts include instruction verification:
```bash
# Check that only RV64IM instructions are used
riscv64-unknown-elf-objdump -d program.elf | grep -E "(fadd|flw|lr\.|c\.)"
# Should return nothing if program is RV64IM-compliant
```

## Advanced Techniques

### Inline Assembly for Special Cases
```c
void wait_for_interrupt() {
    asm volatile ("wfi");  // Wait for interrupt
}

void memory_barrier() {
    asm volatile ("fence" ::: "memory");
}

long read_cycle_counter() {
    long cycles;
    asm volatile ("rdcycle %0" : "=r"(cycles));
    return cycles;
}
```

### Custom Data Structures
```c
// Implement your own data structures
typedef struct {
    long data[100];
    int size;
} simple_array;

void array_push(simple_array* arr, long value) {
    if (arr->size < 100) {
        arr->data[arr->size++] = value;
    }
}
```

This approach gives you the full power of C while ensuring compatibility with the minimal RV64IM ISA!