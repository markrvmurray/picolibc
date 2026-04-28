#!/bin/sh
#
# Bug #192 negative test — verify that exceeding the DP region
# size produces a clear diagnostic.
#
# Two layers of defense:
#   (a) Clang's type system rejects a single addrspace(1) object
#       larger than 255 bytes at compile time (the addrspace's
#       8-bit pointers can't address more).
#   (b) The picolibc linker script's ASSERT catches the case where
#       the cumulative size of all .dp.* sections exceeds the
#       budget at link time, even when each individual object fits.
#
# This script verifies (a) — Clang rejects a 300-byte
# __directpage array at compile time. Meson can't validate
# build-failure tests directly, so this script is run manually
# from the bug-#192 verification battery, NOT from the meson
# test suite.
#
# Exit 0 on success (Clang correctly rejected the overflow).
# Exit 1 if either the compile succeeded (BUG — overflow not
#         caught) or it failed for an unexpected reason.

set -u

PICO=${PICO:-/Users/markmurray/GitHub/picolibc}
BUILDDIR=${BUILDDIR:-$PICO/builddir-mc6809-Os}
LLVM_BIN=${LLVM_BIN:-/Users/markmurray/GitHub/llvm-mc6809/llvm/cmake-build-debug-system/bin}
TMPDIR=${TMPDIR:-/tmp}

SRC=$TMPDIR/dp-overflow-$$.c
OBJ=$TMPDIR/dp-overflow-$$.o
ELF=$TMPDIR/dp-overflow-$$.elf
ERR=$TMPDIR/dp-overflow-$$.err

cleanup() { rm -f "$SRC" "$OBJ" "$ELF" "$ERR"; }
trap cleanup EXIT

# Generate a TU with 250 single-byte __directpage globals — well
# above the 223-byte budget at $21-$FF, so the link MUST fail with
# the ASSERT diagnostic from the cross-file's directpage_block.
cat > "$SRC" <<'CEOF'
/* 300-byte __directpage array — exceeds the addrspace(1) maximum
 * of 255 bytes (since DP pointers are 8-bit). Clang's type system
 * MUST reject this at compile time. */
extern void exit(int);
static __directpage volatile unsigned char big[300];
int main(void) { return big[0]; }
CEOF

# Try to compile. Clang's type system should reject the 300-byte
# addrspace(1) object at compile time.
"$LLVM_BIN/clang" --target=mc6809 -nostdlib -O0 -c "$SRC" -o "$OBJ" 2>"$ERR"
COMPILE_RC=$?

if [ "$COMPILE_RC" -eq 0 ]; then
    echo "FAIL: compile succeeded but should have rejected the overflow"
    echo "      (300-byte __directpage object exceeds addrspace(1)'s 255-byte limit)"
    exit 1
fi

if grep -q "is too large for the address space" "$ERR"; then
    echo "PASS: Clang correctly rejected DP overflow at compile time"
    exit 0
fi

echo "FAIL: compile failed (rc=$COMPILE_RC) but stderr lacks the expected"
echo "      'too large for the address space' diagnostic. Full stderr:"
cat "$ERR"
exit 1
