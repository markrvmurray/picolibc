#include <stdio.h>
#include <stdint.h>

extern uint32_t __udivsi3(uint32_t a, uint32_t b);
extern uint32_t __umodsi3(uint32_t a, uint32_t b);

static void put_hex_byte(uint8_t b) {
    static const char tab[] = "0123456789abcdef";
    putchar(tab[(b >> 4) & 0xf]);
    putchar(tab[b & 0xf]);
}
static void put_hex_u32(uint32_t v) {
    put_hex_byte((uint8_t)(v >> 24));
    put_hex_byte((uint8_t)(v >> 16));
    put_hex_byte((uint8_t)(v >> 8));
    put_hex_byte((uint8_t)v);
}

#define CHK_DIV(a, b, want) do { \
    uint32_t q = __udivsi3((a), (b)); \
    fputs(#a "/" #b "=", stdout); \
    put_hex_u32(q); \
    fputs(" want=", stdout); \
    put_hex_u32(want); \
    fputs((q == (want)) ? " OK\n" : " FAIL\n", stdout); \
} while (0)

int main(void) {
    CHK_DIV(7UL, 10UL, 0UL);
    CHK_DIV(10UL, 10UL, 1UL);
    CHK_DIV(100UL, 10UL, 10UL);
    CHK_DIV(0x12345UL, 10UL, 7456UL);
    CHK_DIV(0x10000UL, 16UL, 0x1000UL);
    fputs("DONE\n", stdout);
    return 0;
}
