/*
 * Bug #192: hot-loop sanity. 1000-iteration loop touching 4 DP
 * bytes; verifies no UB / aliasing / overlap. The point isn't a
 * cycle measurement — it's that running the same DP-mode op many
 * times produces deterministic correct results.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage uint8_t a;
static __directpage uint8_t b;
static __directpage uint8_t c;
static __directpage uint8_t d;

int main(void)
{
    a = 0; b = 0; c = 0; d = 0;
    for (unsigned i = 0; i < 1000; i++) {
        a = (uint8_t)(a + 1);
        b = (uint8_t)(b + 3);
        c = (uint8_t)(c ^ (uint8_t)i);
        d = (uint8_t)(d - 1);
    }
    /* a += 1000 → 1000 mod 256 = 232 = 0xE8 */
    if (a != 0xE8) {
        printf("FAIL a: 0x%02x\n", (unsigned)a);
        return 1;
    }
    /* b += 3000 → 3000 mod 256 = 184 = 0xB8 */
    if (b != 0xB8) {
        printf("FAIL b: 0x%02x\n", (unsigned)b);
        return 1;
    }
    /* c xors i for i=0..999. The xor accumulator has a closed-form
     * pattern that's tedious to derive on paper; just compute the
     * expected value at runtime in a separate accumulator. */
    uint8_t expect_c = 0;
    for (unsigned i = 0; i < 1000; i++)
        expect_c ^= (uint8_t)i;
    if (c != expect_c) {
        printf("FAIL c: 0x%02x (expected 0x%02x)\n",
               (unsigned)c, (unsigned)expect_c);
        return 1;
    }
    /* d -= 1000 → -1000 mod 256 = 24 = 0x18 */
    if (d != 0x18) {
        printf("FAIL d: 0x%02x\n", (unsigned)d);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
