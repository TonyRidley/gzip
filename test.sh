#!/bin/bash

# Each test creates a temp file, runs compress → decompress, then diffs the original against the restored output.
# Test 10 checks the gzip magic bytes with xxd.
# Test 11 verifies the output is compatible with the system's gunzip.

set -e

BINARY="./gzip"
PASS=0
FAIL=0
TMPDIR=$(mktemp -d)

trap "rm -rf $TMPDIR" EXIT

green() { printf "\033[32m%s\033[0m\n" "$1"; }
red()   { printf "\033[31m%s\033[0m\n" "$1"; }

run_test() {
    local name="$1"
    local input_file="$2"
    local compressed="$TMPDIR/compressed.gz"
    local restored="$TMPDIR/restored"

    printf "%-40s" "$name"

    # Compress
    if ! $BINARY compress "$input_file" "$compressed" > /dev/null 2>&1; then
        red "FAIL (compress failed)"
        FAIL=$((FAIL + 1))
        return
    fi

    # Decompress
    if ! $BINARY decompress "$compressed" "$restored" > /dev/null 2>&1; then
        red "FAIL (decompress failed)"
        FAIL=$((FAIL + 1))
        return
    fi

    # Compare
    if diff -q "$input_file" "$restored" > /dev/null 2>&1; then
        green "PASS"
        PASS=$((PASS + 1))
    else
        red "FAIL (output mismatch)"
        FAIL=$((FAIL + 1))
    fi

    rm -f "$compressed" "$restored"
}

echo ""
echo "===== Gzip Round-Trip Tests ====="
echo ""

# --- Test 1: Empty file ---
touch "$TMPDIR/empty.txt"
run_test "Empty file" "$TMPDIR/empty.txt"

# --- Test 2: Single character ---
printf "A" > "$TMPDIR/single.txt"
run_test "Single character" "$TMPDIR/single.txt"

# --- Test 3: Short string ---
echo "Hello, World!" > "$TMPDIR/hello.txt"
run_test "Short string" "$TMPDIR/hello.txt"

# --- Test 4: Repeated character ---
python3 -c "print('A' * 1000, end='')" > "$TMPDIR/repeat.txt"
run_test "1000 repeated chars" "$TMPDIR/repeat.txt"

# --- Test 5: Repeated pattern ---
python3 -c "print('abcdef ' * 500, end='')" > "$TMPDIR/pattern.txt"
run_test "Repeated pattern" "$TMPDIR/pattern.txt"

# --- Test 6: All byte values (binary) ---
python3 -c "
import sys
sys.stdout.buffer.write(bytes(range(256)))
" > "$TMPDIR/allbytes.bin"
run_test "All 256 byte values" "$TMPDIR/allbytes.bin"

# --- Test 7: Larger text ---
for i in $(seq 1 200); do
    echo "Line $i: The quick brown fox jumps over the lazy dog."
done > "$TMPDIR/large.txt"
run_test "200-line text file" "$TMPDIR/large.txt"

# --- Test 8: Random binary data ---
dd if=/dev/urandom of="$TMPDIR/random.bin" bs=1024 count=4 2>/dev/null
run_test "4KB random binary" "$TMPDIR/random.bin"

# --- Test 9: Compress an actual source file ---
if [ -f "src/main.cpp" ]; then
    run_test "src/main.cpp" "src/main.cpp"
fi

# --- Test 10: Verify gzip magic bytes ---
printf "%-40s" "Gzip magic bytes (1F 8B)"
echo "test" > "$TMPDIR/magic_test.txt"
$BINARY compress "$TMPDIR/magic_test.txt" "$TMPDIR/magic_test.gz" > /dev/null 2>&1
MAGIC=$(xxd -p -l 2 "$TMPDIR/magic_test.gz")
if [ "$MAGIC" = "1f8b" ]; then
    green "PASS"
    PASS=$((PASS + 1))
else
    red "FAIL (got $MAGIC)"
    FAIL=$((FAIL + 1))
fi

# --- Test 11: System gunzip compatibility ---
printf "%-40s" "Compatible with system gunzip"
echo "Hello from scratch gzip!" > "$TMPDIR/compat.txt"
$BINARY compress "$TMPDIR/compat.txt" "$TMPDIR/compat.gz" > /dev/null 2>&1
if command -v gunzip &> /dev/null; then
    cp "$TMPDIR/compat.gz" "$TMPDIR/compat_sys.gz"
    if gunzip -f "$TMPDIR/compat_sys.gz" 2>/dev/null && \
       diff -q "$TMPDIR/compat.txt" "$TMPDIR/compat_sys" > /dev/null 2>&1; then
        green "PASS"
        PASS=$((PASS + 1))
    else
        red "FAIL"
        FAIL=$((FAIL + 1))
    fi
else
    echo "SKIP (gunzip not found)"
fi

echo ""
echo "============================="
echo "Results: $PASS passed, $FAIL failed"
echo ""

[ "$FAIL" -eq 0 ] && exit 0 || exit 1