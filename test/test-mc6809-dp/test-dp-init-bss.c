/*
 * Bug #192: __directpage globals with NO initialiser. Verifies
 * __do_zero_dp_bss() in crt0.c zero-initialises .dp.bss at
 * startup. C requires uninitialised globals to be zero at startup;
 * if the CRT skipped __do_zero_dp_bss the values would be garbage.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage int zero_int;
static __directpage uint8_t zero_byte;
static __directpage uint16_t zero_word;
static __directpage uint8_t zero_array[4];

int main(void)
{
    if (zero_int != 0) {
        printf("FAIL zero_int: %d (0x%04x)\n", zero_int, (unsigned)zero_int);
        return 1;
    }
    if (zero_byte != 0) {
        printf("FAIL zero_byte: 0x%02x\n", (unsigned)zero_byte);
        return 1;
    }
    if (zero_word != 0) {
        printf("FAIL zero_word: 0x%04x\n", (unsigned)zero_word);
        return 1;
    }
    for (int i = 0; i < 4; i++) {
        if (zero_array[i] != 0) {
            printf("FAIL zero_array[%d]: 0x%02x\n", i, (unsigned)zero_array[i]);
            return 1;
        }
    }
    printf("PASS\n");
    return 0;
}
