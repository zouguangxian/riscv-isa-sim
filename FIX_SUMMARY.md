# Fix Summary: Build Scripts with Exit Code Handling

## Problem

When running `./build_minimal.sh`, the script appeared to fail silently and not execute the Spike test, even though the manual command worked:

```bash
# This worked and showed output:
$ timeout 5 ./build/spike --isa=rv64im rv64im_minimal.elf
*** FAILED *** (tohost = 1)

# But this didn't show the output:
$ ./build_minimal.sh
```

## Root Cause

The build script had `set -e` which means **"exit immediately if any command returns non-zero"**.

When the test program exits with code 1 (to test the failure case), Spike also returns exit code 1. This caused the script to terminate immediately due to `set -e`, before it could:
1. Display the `*** FAILED ***` message (which was buffered)
2. Print the exit code
3. Complete gracefully

## Solution

Changed from:
```bash
set -e
...
timeout 5 ./build/spike --isa=rv64im rv64im_minimal.elf
echo "Exit code: $?"  # Never executed!
```

To:
```bash
set -e
...
# Use if statement to handle exit code gracefully
if timeout 5 ./build/spike --isa=rv64im rv64im_minimal.elf; then
    echo "Exit code: 0 (success)"
else
    EXIT_CODE=$?
    if [ $EXIT_CODE -eq 124 ]; then
        echo "WARNING: Test timed out after 5 seconds"
    else
        echo "Exit code: $EXIT_CODE (failure)"
    fi
fi
```

## Why This Works

In bash, `if` statements don't trigger `set -e` behavior. The exit code is captured in the condition, and we can handle it gracefully in the `else` clause.

## Now Working

```bash
$ ./build_minimal.sh
Building minimal RV64IM C example...
Compiling rv64im_minimal.c...
Built rv64im_minimal.elf successfully!

This program:
  - Uses fixed addresses for tohost/fromhost (avoids symbol issues)
  - Uses only RV64IM instructions
  - Performs integer arithmetic and M-extension operations
  - Signals completion via tohost at address 0x80001000

To run:
  ./build/spike --isa=rv64im rv64im_minimal.elf

Expected: Program runs and exits cleanly
Running...
*** FAILED *** (tohost = 1)
Exit code: 1 (failure)
```

✅ Perfect!

## Files Fixed

- `build_minimal.sh` - Now properly handles non-zero exit codes
- `build_minimal_ecall.sh` - Already had this fix applied
- `rv64im_minimal.c` - Updated comment to clarify intentional failure test

## Testing Different Exit Codes

To test success (exit code 0):
```c
// In rv64im_minimal.c, change:
signal_exit(1);  // failure
// to:
signal_exit(0);  // success
```

Then rebuild and run:
```bash
$ ./build_minimal.sh
# Should show no error message and "Exit code: 0 (success)"
```

## Related Fixes

This issue was discovered while fixing the HTIF exit mechanism. The complete set of fixes includes:

1. **8-byte alignment** - tohost/fromhost must be 8-byte aligned (fixed in `rv64im_minimal.ld`)
2. **Exit code handling** - Build scripts must handle non-zero exits gracefully (this fix)
3. **Timeout integration** - All build scripts now use `timeout 5` to prevent hanging

## References

- `README_HTIF_EXIT.md` - Complete guide to HTIF exit codes
- `test_exit_codes.sh` - Comprehensive test of exit codes 0, 1, 42
- `ECALL_SYSCALLS.md` - Guide to syscall implementation (ecall instruction)

