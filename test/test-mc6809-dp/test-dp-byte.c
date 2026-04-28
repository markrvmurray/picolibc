/*
 * Bug #192: single byte __directpage global. Verifies basic
 * DP-mode load/store cycles end-to-end on usim.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage uint8_t counter = 42;

int main(void)
{
    if (counter != 42) {
        printf("FAIL initial: %u\n", (unsigned)counter);
        return 1;
    }
    counter++;
    if (counter != 43) {
        printf("FAIL after inc: %u\n", (unsigned)counter);
        return 1;
    }
    counter = 0xAA;
    if (counter != 0xAA) {
        printf("FAIL after store: 0x%02x\n", (unsigned)counter);
        return 1;
    }
    counter -= 1;
    if (counter != 0xA9) {
        printf("FAIL after dec: 0x%02x\n", (unsigned)counter);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
