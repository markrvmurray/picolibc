/* Bug ba5485d: HD6309 DIVQ fast paths in __divsi3 / __modsi3.
 * Cross-checks signed i32 div/mod across positive/negative combos. */
#include <stdio.h>
#include <stdint.h>

extern int32_t __divsi3(int32_t a, int32_t b);
extern int32_t __modsi3(int32_t a, int32_t b);

static int fail;

static void chk_div(int32_t a, int32_t b, int32_t want) {
    int32_t got = __divsi3(a, b);
    if (got != want) {
        printf("FAIL %ld / %ld: got %ld want %ld\n",
               (long)a, (long)b, (long)got, (long)want);
        fail = 1;
    }
}
static void chk_mod(int32_t a, int32_t b, int32_t want) {
    int32_t got = __modsi3(a, b);
    if (got != want) {
        printf("FAIL %ld %% %ld: got %ld want %ld\n",
               (long)a, (long)b, (long)got, (long)want);
        fail = 1;
    }
}

int main(void) {
    /* Positive / positive — fast path. */
    chk_div(100, 7, 14);   chk_mod(100, 7, 2);
    chk_div(1000000, 100, 10000); chk_mod(1000000, 100, 0);
    chk_div(0x12345678, 5, 0x12345678/5); chk_mod(0x12345678, 5, 0x12345678%5);

    /* Negative / positive — fast path with sign. */
    chk_div(-100, 7, -14);   chk_mod(-100, 7, -2);
    chk_div(-1000000, 100, -10000); chk_mod(-1000000, 100, 0);
    chk_div(-0x12345678, 5, -0x12345678/5); chk_mod(-0x12345678, 5, -0x12345678%5);

    /* Positive / negative. */
    chk_div(100, -7, -14);   chk_mod(100, -7, 2);
    chk_div(1000000, -100, -10000); chk_mod(1000000, -100, 0);

    /* Negative / negative. */
    chk_div(-100, -7, 14);   chk_mod(-100, -7, -2);
    chk_div(-1000000, -100, 10000); chk_mod(-1000000, -100, 0);

    /* divisor == 1 — fast path edge */
    chk_div(0x12345678, 1, 0x12345678); chk_mod(0x12345678, 1, 0);
    chk_div(-0x12345678, 1, -0x12345678); chk_mod(-0x12345678, 1, 0);

    /* divisor outside [2, 0x7FFF] — slow path. */
    chk_div(1000000, 0x10000, 1000000 / 0x10000);
    chk_mod(1000000, 0x10000, 1000000 % 0x10000);
    chk_div(-1000000, 0x10000, -1000000 / 0x10000);
    chk_div(0x12345678, -0x10000, 0x12345678 / -0x10000);

    /* zero dividend */
    chk_div(0, 5, 0); chk_mod(0, 5, 0);
    chk_div(0, -5, 0); chk_mod(0, -5, 0);

    /* edge: INT_MIN / -1 (overflow case in some impls) — at C-level UB,
     * skip to avoid testing implementation-defined behaviour. */

    if (!fail) printf("PASS\n");
    return fail;
}
