// RV64IM C Example
// This C program uses only instructions available in RV64IM ISA
// Compile with: riscv64-unknown-elf-gcc -march=rv64im -mabi=lp64 -nostdlib -nostartfiles -O2

// External symbols for HTIF communication
extern volatile long tohost;
extern volatile long fromhost;

// Simple functions that compile to RV64IM instructions only

// Integer multiplication and division (M extension)
long multiply(long a, long b) {
    return a * b;
}

long divide(long a, long b) {
    return a / b;
}

long remainder(long a, long b) {
    return a % b;
}

// Simple loop with integer operations
long factorial(int n) {
    long result = 1;
    for (int i = 1; i <= n; i++) {
        result = multiply(result, i);
    }
    return result;
}

// Fibonacci sequence using only integer arithmetic
long fibonacci(int n) {
    if (n <= 1) return n;
    
    long a = 0, b = 1, temp;
    for (int i = 2; i <= n; i++) {
        temp = a + b;
        a = b;
        b = temp;
    }
    return b;
}

// Simple array operations
void array_operations(long* arr, int size) {
    // Initialize array
    for (int i = 0; i < size; i++) {
        arr[i] = multiply(i, i);  // arr[i] = i^2
    }
    
    // Sum all elements
    long sum = 0;
    for (int i = 0; i < size; i++) {
        sum += arr[i];
    }
    
    // Store result in first element
    arr[0] = sum;
}

// Memory operations using only basic loads/stores
void memory_test() {
    // Use a fixed memory location for our data
    volatile long* data = (volatile long*)0x80100000;
    
    // Test basic memory operations
    data[0] = 0x123456789ABCDEF0L;  // 64-bit store
    data[1] = data[0] + 1;          // Load and store
    data[2] = multiply(data[1], 2); // Arithmetic with memory
}

// Entry point - equivalent to _start in assembly
void _start() {
    // Test basic arithmetic
    long a = 10;
    long b = 20;
    long c = a + b;  // c = 30
    
    // Test multiplication (M extension)
    long result = multiply(c, 5);  // result = 150
    
    // Test division (M extension)
    long div_result = divide(result, 3);  // div_result = 50
    long mod_result = remainder(result, 7); // mod_result = 3
    
    // Test factorial calculation
    long fact5 = factorial(5);  // fact5 = 120
    
    // Test fibonacci
    long fib10 = fibonacci(10);  // fib10 = 55
    
    // Test array operations
    long array[10];
    array_operations(array, 10);
    
    // Test memory operations
    memory_test();
    
    // Combine all results for a final computation
    long final_result = result + div_result + mod_result + fact5 + fib10 + array[0];
    
    // Signal completion to host with our result
    // We'll just use 1 for success, but in a real test you might want to
    // validate the final_result value
    tohost = (final_result > 0) ? 1 : 0;
    
    // Infinite loop in case tohost doesn't terminate simulation
    while (1) {
        // Use inline assembly for wait-for-interrupt
        asm volatile ("wfi");
    }
}

// Define the HTIF communication variables
// These must be at specific addresses that spike can find
volatile long tohost __attribute__((section(".htif"))) = 0;
volatile long fromhost __attribute__((section(".htif"))) = 0;