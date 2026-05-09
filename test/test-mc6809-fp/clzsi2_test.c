/* Bug #225 / Bug #255: __clzsi2 uses LLVM-MC6809's i32-sret libcall
 * ABI. Direct C-ABI calls to __clzsi2(uint32_t) → int do not work; use
 * __builtin_clz which clang lowers via the libcall path. */
#include <stdio.h>
#include <stdint.h>

static int fail;
static void chk(uint32_t a, int want) {
    int got = __builtin_clz(a);
    if (got != want) {
        printf("FAIL __clzsi2(0x%08lx): got %d want %d\n",
               (unsigned long)a, got, want);
        fail = 1;
    }
}

int main(void) {
    chk(0x80000000, 0);
    chk(0x40000000, 1);
    chk(0x00800000, 8);
    chk(0x00400000, 9);
    chk(0x00008000, 16);
    chk(0x00004000, 17);
    chk(0x00000080, 24);
    chk(0x00000040, 25);
    chk(0x00000001, 31);
    chk(0xFFFFFFFF, 0);
    chk(0x00000003, 30);
    chk(0x00010000, 15);
    chk(0x00000100, 23);
    if (!fail) printf("PASSED\n");
    return fail;
}
