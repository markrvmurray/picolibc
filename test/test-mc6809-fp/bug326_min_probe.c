/* Bug #326 minimal: ONE __builtin_memcpy of known length with two pointer globals. */
#include <stdio.h>
#include <stdint.h>

char dst[12] = { 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99, 0x99 };
char src[8]  = { 0x11, 0x22, 0x33, 0x44, 0x55, 0x66, 0x77, 0x88 };

int main(void) {
    __builtin_memcpy(dst, src, 8);
    printf("dst[0..11]:");
    for (int i = 0; i < 12; i++) printf(" %02x", (uint8_t)dst[i]);
    printf("\n");
    int ok = 1;
    for (int i = 0; i < 8; i++)
        if ((uint8_t)dst[i] != (uint8_t)src[i]) ok = 0;
    for (int i = 8; i < 12; i++)
        if ((uint8_t)dst[i] != 0x99) ok = 0;
    printf("%s\n", ok ? "OK" : "FAIL");
    return ok ? 0 : 1;
}
