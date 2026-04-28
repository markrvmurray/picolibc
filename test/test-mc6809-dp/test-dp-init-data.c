/*
 * Bug #192: __directpage global with non-zero initialiser. Verifies
 * __do_copy_dp_data() in crt0.c correctly copies the initial value
 * from the .dp.data load image to the runtime DP location.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage int counter = 0xABCD;
static __directpage uint8_t flag = 0x5A;
static __directpage int signed_v = -1234;

int main(void)
{
    /* Read the initialised values WITHOUT writing first — proves
     * __do_copy_dp_data ran at startup. */
    if (counter != 0xABCD) {
        printf("FAIL counter: 0x%04x\n", (unsigned)counter);
        return 1;
    }
    if (flag != 0x5A) {
        printf("FAIL flag: 0x%02x\n", (unsigned)flag);
        return 1;
    }
    if (signed_v != -1234) {
        printf("FAIL signed_v: %d\n", signed_v);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
