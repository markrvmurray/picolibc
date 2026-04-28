/*
 * Bug #192: single 16-bit __directpage global. Verifies LDDd / STDd
 * (the word DP-mode opcodes).
 */
#include <stdio.h>
#include <stdint.h>

static __directpage uint16_t value = 0x1234;

int main(void)
{
    if (value != 0x1234) {
        printf("FAIL initial: 0x%04x\n", (unsigned)value);
        return 1;
    }
    value = 0xBEEF;
    if (value != 0xBEEF) {
        printf("FAIL after store: 0x%04x\n", (unsigned)value);
        return 1;
    }
    value += 0x0001;
    if (value != 0xBEF0) {
        printf("FAIL after inc: 0x%04x\n", (unsigned)value);
        return 1;
    }
    /* Cross the 16-bit boundary to exercise both bytes. */
    value = 0x00FF;
    value++;
    if (value != 0x0100) {
        printf("FAIL byte-carry: 0x%04x\n", (unsigned)value);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
