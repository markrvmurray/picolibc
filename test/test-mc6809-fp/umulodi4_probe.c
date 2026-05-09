/* Bug #239: __umulodi4 — 64-bit unsigned multiply with overflow.
 *
 * Cross-checks low-64 result and overflow flag across:
 *   - zero / identity
 *   - results that fit in i64 (no overflow)
 *   - results that overflow i64
 *   - boundary cases at 2^32 and 2^63
 */
#include <stdio.h>
#include <stdint.h>

extern uint64_t __umulodi4(uint64_t a, uint64_t b, int *overflow);

static int fail;

#define CHK(a, b, want_lo, want_of)                                       \
    do {                                                                  \
        int of_got = -1;                                                  \
        uint64_t lo_got = __umulodi4((a), (b), &of_got);                  \
        int      of_w = !!(want_of);                                      \
        if (lo_got != (want_lo) || of_got != of_w) {                      \
            printf("FAIL %llx * %llx: lo=%llx of=%d want lo=%llx of=%d\n",\
                   (unsigned long long)(a), (unsigned long long)(b),      \
                   (unsigned long long)lo_got, of_got,                    \
                   (unsigned long long)(want_lo), of_w);                  \
            fail = 1;                                                     \
        }                                                                 \
    } while (0)

int main(void) {
    /* Zero / identity */
    CHK(0ULL, 0ULL, 0ULL, 0);
    CHK(0ULL, 0xFFFFFFFFFFFFFFFFULL, 0ULL, 0);
    CHK(0xFFFFFFFFFFFFFFFFULL, 0ULL, 0ULL, 0);
    CHK(1ULL, 0x123456789ABCDEFULL, 0x123456789ABCDEFULL, 0);
    CHK(0x123456789ABCDEFULL, 1ULL, 0x123456789ABCDEFULL, 0);

    /* Small fits */
    CHK(2ULL, 3ULL, 6ULL, 0);
    CHK(0xFFFFULL, 0xFFFFULL, 0xFFFE0001ULL, 0);
    CHK(0x10000ULL, 0x10000ULL, 0x100000000ULL, 0);

    /* 32x32 fits in 64 — never overflows */
    CHK(0xFFFFFFFFULL, 0xFFFFFFFFULL, 0xFFFFFFFE00000001ULL, 0);

    /* (UINT32_MAX + 1)^2 = 2^64 — overflow, low 64 = 0 */
    CHK(0x100000000ULL, 0x100000000ULL, 0ULL, 1);

    /* 2 * 2^63 = 2^64 — overflow, low 64 = 0 */
    CHK(0x8000000000000000ULL, 2ULL, 0ULL, 1);
    CHK(2ULL, 0x8000000000000000ULL, 0ULL, 1);

    /* 2^63 * 2^63 = 2^126 — overflow, low 64 = 0 */
    CHK(0x8000000000000000ULL, 0x8000000000000000ULL, 0ULL, 1);

    /* UINT64_MAX * 1 — fits */
    CHK(0xFFFFFFFFFFFFFFFFULL, 1ULL, 0xFFFFFFFFFFFFFFFFULL, 0);

    /* UINT64_MAX * 2 — overflows by 1 bit */
    CHK(0xFFFFFFFFFFFFFFFFULL, 2ULL, 0xFFFFFFFFFFFFFFFEULL, 1);

    /* (UINT64_MAX)^2 = UINT64_MAX * UINT64_MAX — overflows; low half == 1 */
    CHK(0xFFFFFFFFFFFFFFFFULL, 0xFFFFFFFFFFFFFFFFULL, 1ULL, 1);

    /* Asymmetric pattern across all bytes */
    CHK(0x0123456789ABCDEFULL, 0xFEDCBA9876543210ULL,
        (uint64_t)((uint64_t)0x0123456789ABCDEFULL *
                   (uint64_t)0xFEDCBA9876543210ULL),
        1 /* (a*b) >> 64 != 0 */);

    /* Mid-range that fits */
    CHK(1000000000ULL, 1000000000ULL, 1000000000000000000ULL, 0);

    /* Mid-range that overflows */
    CHK(10000000000ULL, 10000000000ULL,
        (uint64_t)(10000000000ULL * 10000000000ULL),  /* low 64 of true product */
        1);

    /* Sentinel — divisor 36 (matches mktemp's hot loop divisor) */
    CHK(0x123456789ABCDEFULL, 36ULL,
        (uint64_t)0x123456789ABCDEFULL * 36ULL, 0);

    if (!fail) printf("PASS\n");
    return fail;
}
