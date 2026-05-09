/* Bug #245: hand-written __ctzsi2 in compiler-rt/lib/builtins/mc6809/.
 * Mirrors clzsi2_test.c — exercises the byte-scan + bit-shift loop.
 *
 * Bug #255: __ctzsi2 now uses LLVM-MC6809's i32-sret libcall ABI (caller
 * passes 4-byte buffer in X, reads result from offset 2). Direct C-ABI
 * calls to __ctzsi2(uint32_t) → int no longer work. Use __builtin_ctz
 * which clang lowers via the libcall path.
 *
 * (a == 0 is undefined for __builtin_ctz, so the explicit zero case is
 * dropped — the underlying .S still defensively returns 32.) */
#include <stdio.h>
#include <stdint.h>

static int fail;
static void chk(uint32_t a, int want) {
    int got = __builtin_ctz(a);
    if (got != want) {
        printf("FAIL __ctzsi2(0x%08lx): got %d want %d\n",
               (unsigned long)a, got, want);
        fail = 1;
    }
}

int main(void) {
    chk(0x00000001, 0);
    chk(0x00000002, 1);
    chk(0x00000080, 7);
    chk(0x00000100, 8);
    chk(0x00010000, 16);
    chk(0x00800000, 23);
    chk(0x80000000, 31);
    chk(0xFFFFFFFF, 0);
    chk(0xFFFF0000, 16);
    chk(0xFF000000, 24);
    chk(0x00000003, 0);
    chk(0x00000005, 0);
    if (!fail) printf("PASSED\n");
    return fail;
}
