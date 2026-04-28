/*
 * Bug #192: 8 distinct __directpage globals. Verifies the linker
 * assigns each a distinct DP address and that writes to one don't
 * leak into another.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage uint8_t a = 0x01;
static __directpage uint8_t b = 0x02;
static __directpage uint8_t c = 0x03;
static __directpage uint8_t d = 0x04;
static __directpage uint8_t e = 0x05;
static __directpage uint8_t f = 0x06;
static __directpage uint8_t g = 0x07;
static __directpage uint8_t h = 0x08;

int main(void)
{
    /* All distinct initial values? */
    if (a != 0x01 || b != 0x02 || c != 0x03 || d != 0x04 ||
        e != 0x05 || f != 0x06 || g != 0x07 || h != 0x08) {
        printf("FAIL initial: %02x %02x %02x %02x %02x %02x %02x %02x\n",
               (unsigned)a, (unsigned)b, (unsigned)c, (unsigned)d,
               (unsigned)e, (unsigned)f, (unsigned)g, (unsigned)h);
        return 1;
    }

    /* Mutate each in turn; verify no neighbour was disturbed. */
    a = 0xAA; b = 0xBB; c = 0xCC; d = 0xDD;
    e = 0xEE; f = 0xFF; g = 0x99; h = 0x88;

    if (a != 0xAA || b != 0xBB || c != 0xCC || d != 0xDD ||
        e != 0xEE || f != 0xFF || g != 0x99 || h != 0x88) {
        printf("FAIL after writes: %02x %02x %02x %02x %02x %02x %02x %02x\n",
               (unsigned)a, (unsigned)b, (unsigned)c, (unsigned)d,
               (unsigned)e, (unsigned)f, (unsigned)g, (unsigned)h);
        return 1;
    }

    /* Address-uniqueness check via summed identity */
    unsigned sum = (unsigned)a + (unsigned)b + (unsigned)c + (unsigned)d +
                   (unsigned)e + (unsigned)f + (unsigned)g + (unsigned)h;
    /* 0xAA+0xBB+0xCC+0xDD+0xEE+0xFF+0x99+0x88 = 0x61C */
    if (sum != 0x61C) {
        printf("FAIL sum: 0x%x\n", sum);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
