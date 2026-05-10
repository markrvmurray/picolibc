/* Bug #225 / Bug #255: __clzsi2 uses LLVM-MC6809's i32-sret libcall
 * ABI. Direct C-ABI calls to __clzsi2(uint32_t) → int do not work.
 *
 * Bug #262: must use __builtin_clzl (NOT __builtin_clz) to reach
 * __clzsi2 — on MC6809 int is 16 bits and long is 32 bits, so
 * __builtin_clz(uint32_t) silently truncates to 16 bits and lowers
 * to __clzhi2 (the 16-bit libcall) instead of __clzsi2. That gave
 * the wrong answer for any input where the truncated low 16 bits
 * differ in leading-zero count from the original 32-bit value.
 * __builtin_clzl takes unsigned long (= uint32_t on MC6809) and
 * lowers to __clzsi2, exercising the path this test exists to cover. */
#include <stdio.h>
#include <stdint.h>

static int fail;
static void chk(uint32_t a, int want) {
    int got = __builtin_clzl(a);
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
