/*
 * Bug #192: function-local `static __directpage` globals. Verifies
 * Sema accepts them (local statics have static storage duration)
 * and that they survive across calls — i.e. they live in DP, not
 * on the stack.
 */
#include <stdio.h>
#include <stdint.h>

/* `volatile` defeats the optimizer's tendency to downgrade
 * function-local statics to .dp.bss when their initial value can
 * be proven dead — we want to TEST the .dp.data init path
 * end-to-end, so the storage and reads MUST survive optimisation. */
static int bump(void)
{
    static volatile __directpage int n;   /* zero-init by .dp.bss */
    n += 3;
    return n;
}

static uint8_t set_and_get(uint8_t v)
{
    static volatile __directpage uint8_t hold = 0xCD;   /* init by .dp.data */
    uint8_t prev = hold;
    hold = v;
    return prev;
}

int main(void)
{
    /* Three calls in a row — n must remember its value. */
    if (bump() != 3) { printf("FAIL bump 1\n"); return 1; }
    if (bump() != 6) { printf("FAIL bump 2\n"); return 1; }
    if (bump() != 9) { printf("FAIL bump 3\n"); return 1; }

    /* set_and_get: first call returns the .dp.data initialiser. */
    if (set_and_get(0xAA) != 0xCD) {
        printf("FAIL set_and_get init\n");
        return 1;
    }
    if (set_and_get(0xBB) != 0xAA) {
        printf("FAIL set_and_get round-trip\n");
        return 1;
    }
    if (set_and_get(0x00) != 0xBB) {
        printf("FAIL set_and_get final\n");
        return 1;
    }

    printf("PASS\n");
    return 0;
}
