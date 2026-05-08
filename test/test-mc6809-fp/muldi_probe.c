/* Bug aacd0b7: HD6309 MULD path in __muldi3.
 * Cross-checks signed and unsigned 64x64->64 multiply across edge
 * cases — sign-bit-set operands (which the unsigned correction
 * MUST get right), large bit patterns, and zero/identity cases. */
#include <stdio.h>
#include <stdint.h>

extern int64_t __muldi3(int64_t a, int64_t b);

static int fail;

static void chk(int64_t a, int64_t b, int64_t want) {
    int64_t got = __muldi3(a, b);
    if (got != want) {
        printf("FAIL %llx * %llx: got %llx want %llx\n",
               (unsigned long long)a, (unsigned long long)b,
               (unsigned long long)got, (unsigned long long)want);
        fail = 1;
    }
}

int main(void) {
    /* Zero / identity */
    chk(0, 0x123456789abcdefLL, 0);
    chk(0x123456789abcdefLL, 0, 0);
    chk(1, 0x123456789abcdefLL, 0x123456789abcdefLL);
    chk(0x123456789abcdefLL, 1, 0x123456789abcdefLL);

    /* Small positives — exercises only low-weight partials */
    chk(2, 3, 6);
    chk(0xFFFF, 0xFFFF, 0xFFFE0001LL);
    chk(0x10000LL, 0x10000LL, 0x100000000LL);
    chk(0x10001LL, 0x10001LL, 0x100020001LL);

    /* 32-bit corners — cross all four pair partials */
    chk(0x12345678LL, 0xABCDEF01LL,
        (int64_t)((uint64_t)0x12345678ULL * (uint64_t)0xABCDEF01ULL));

    /* Sign-bit-set in lower 16 of each pair — exercises unsigned correction */
    chk(0x8000LL, 0x8000LL, 0x40000000LL);
    chk(0x8000LL, 2LL, 0x10000LL);
    chk(0xFFFF8000LL, 2LL, (int64_t)0x1FFFF0000LL);

    /* Random-looking 64-bit values; ground truth via long-long arithmetic
     * computed at compile time on the host. */
    chk(0x0123456789ABCDEFLL, 0xFEDCBA9876543210LL,
        (int64_t)((uint64_t)0x0123456789ABCDEFULL *
                  (uint64_t)0xFEDCBA9876543210ULL));

    chk(0xDEADBEEFCAFEBABELL, 0x1234567890ABCDEFLL,
        (int64_t)((uint64_t)0xDEADBEEFCAFEBABEULL *
                  (uint64_t)0x1234567890ABCDEFULL));

    /* INT64_MIN squared (truncated) */
    chk((int64_t)0x8000000000000000LL, (int64_t)0x8000000000000000LL, 0LL);

    /* All-ones squared truncated — well-known pattern */
    chk(-1LL, -1LL, 1LL);
    chk(-1LL, 0x123456789abcdefLL, -0x123456789abcdefLL);

    /* Stress every nibble in cross-pair multiplications */
    chk(0xFFFFFFFFLL, 0xFFFFFFFFLL,
        (int64_t)((uint64_t)0xFFFFFFFFULL * (uint64_t)0xFFFFFFFFULL));
    chk(0xFFFFFFFFFFFFFFFFLL, 0xFFFFFFFFFFFFFFFFLL, 1LL);

    if (!fail) printf("PASS\n");
    return fail;
}
