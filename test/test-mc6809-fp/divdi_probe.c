/* Bug f98d69f: HD6309 DIVQ fast path in __[u]divdi3 / __[u]moddi3 / __[u]divmoddi4.
 * Cross-checks all six i64 div/mod entry points across:
 *   - divisor in [1, 0x7FFF] (fast path)
 *   - divisor outside fast-path range (slow path)
 *   - signed combinations (positive, negative, mixed) */
#include <stdio.h>
#include <stdint.h>

extern uint64_t __udivdi3(uint64_t a, uint64_t b);
extern uint64_t __umoddi3(uint64_t a, uint64_t b);
extern uint64_t __udivmoddi4(uint64_t a, uint64_t b, uint64_t *rem);
extern int64_t  __divdi3(int64_t a, int64_t b);
extern int64_t  __moddi3(int64_t a, int64_t b);
extern int64_t  __divmoddi4(int64_t a, int64_t b, int64_t *rem);

static int fail;

#define CHK_U(fn, a, b, want)                                          \
    do {                                                               \
        uint64_t got = fn(a, b);                                       \
        if (got != (want)) {                                           \
            printf("FAIL %s(%llx,%llx): got %llx want %llx\n",         \
                   #fn, (unsigned long long)(a), (unsigned long long)(b),\
                   (unsigned long long)got, (unsigned long long)(want));\
            fail = 1;                                                  \
        }                                                              \
    } while (0)

#define CHK_S(fn, a, b, want)                                          \
    do {                                                               \
        int64_t got = fn(a, b);                                        \
        if (got != (want)) {                                           \
            printf("FAIL %s(%lld,%lld): got %lld want %lld\n",         \
                   #fn, (long long)(a), (long long)(b),                \
                   (long long)got, (long long)(want));                 \
            fail = 1;                                                  \
        }                                                              \
    } while (0)

int main(void) {
    /* ---- udivdi3 / umoddi3 fast-path divisors ---- */
    CHK_U(__udivdi3, 0x123456789ABCDEF0ULL, 10ULL,
          0x123456789ABCDEF0ULL / 10ULL);
    CHK_U(__umoddi3, 0x123456789ABCDEF0ULL, 10ULL,
          0x123456789ABCDEF0ULL % 10ULL);
    CHK_U(__udivdi3, 0xFFFFFFFFFFFFFFFFULL, 0x7FFFULL,
          0xFFFFFFFFFFFFFFFFULL / 0x7FFFULL);
    CHK_U(__umoddi3, 0xFFFFFFFFFFFFFFFFULL, 0x7FFFULL,
          0xFFFFFFFFFFFFFFFFULL % 0x7FFFULL);
    CHK_U(__udivdi3, 1000000000000ULL, 1000ULL, 1000000000ULL);
    CHK_U(__umoddi3, 1000000000007ULL, 1000ULL, 7ULL);
    CHK_U(__udivdi3, 0x8000000000000000ULL, 2ULL, 0x4000000000000000ULL);
    CHK_U(__umoddi3, 0x8000000000000001ULL, 2ULL, 1ULL);
    /* divisor = 1 (fast path edge) */
    CHK_U(__udivdi3, 0x123456789ABCDEFULL, 1ULL, 0x123456789ABCDEFULL);
    CHK_U(__umoddi3, 0x123456789ABCDEFULL, 1ULL, 0ULL);

    /* ---- udivdi3 / umoddi3 slow-path divisors ---- */
    CHK_U(__udivdi3, 0xFFFFFFFFFFFFFFFFULL, 0x10000ULL,
          0xFFFFFFFFFFFFFFFFULL / 0x10000ULL);
    CHK_U(__umoddi3, 0xFFFFFFFFFFFFFFFFULL, 0x10000ULL,
          0xFFFFFFFFFFFFFFFFULL % 0x10000ULL);
    CHK_U(__udivdi3, 0x123456789ABCDEF0ULL, 0xCAFEBABEULL,
          0x123456789ABCDEF0ULL / 0xCAFEBABEULL);
    CHK_U(__umoddi3, 0x123456789ABCDEF0ULL, 0xCAFEBABEULL,
          0x123456789ABCDEF0ULL % 0xCAFEBABEULL);
    /* divisor with bit 15 set (forces slow path even though high is zero) */
    CHK_U(__udivdi3, 0x12345678ULL, 0x8001ULL, 0x12345678ULL / 0x8001ULL);
    CHK_U(__umoddi3, 0x12345678ULL, 0x8001ULL, 0x12345678ULL % 0x8001ULL);

    /* ---- udivmoddi4 ---- */
    {
        uint64_t r;
        uint64_t q = __udivmoddi4(0x123456789ABCDEF0ULL, 7ULL, &r);
        if (q != 0x123456789ABCDEF0ULL / 7ULL ||
            r != 0x123456789ABCDEF0ULL % 7ULL) {
            printf("FAIL __udivmoddi4 fast: q=%llx r=%llx\n",
                   (unsigned long long)q, (unsigned long long)r);
            fail = 1;
        }
        /* NULL rem */
        q = __udivmoddi4(0x12345678ULL, 100ULL, 0);
        if (q != 0x12345678ULL / 100ULL) {
            printf("FAIL __udivmoddi4 NULL rem\n");
            fail = 1;
        }
        /* slow path */
        q = __udivmoddi4(0xFFFFFFFFFFFFFFFFULL, 0x100000000ULL, &r);
        if (q != 0xFFFFFFFFFFFFFFFFULL / 0x100000000ULL ||
            r != 0xFFFFFFFFFFFFFFFFULL % 0x100000000ULL) {
            printf("FAIL __udivmoddi4 slow: q=%llx r=%llx\n",
                   (unsigned long long)q, (unsigned long long)r);
            fail = 1;
        }
    }

    /* ---- signed wrappers (use unsigned fast path under the hood) ---- */
    CHK_S(__divdi3,  100LL,  7LL,  14LL);
    CHK_S(__moddi3,  100LL,  7LL,   2LL);
    CHK_S(__divdi3, -100LL,  7LL, -14LL);
    CHK_S(__moddi3, -100LL,  7LL,  -2LL);
    CHK_S(__divdi3,  100LL, -7LL, -14LL);
    CHK_S(__moddi3,  100LL, -7LL,   2LL);
    CHK_S(__divdi3, -100LL, -7LL,  14LL);
    CHK_S(__moddi3, -100LL, -7LL,  -2LL);
    /* large signed values */
    CHK_S(__divdi3, 0x123456789ABCDEFLL, 1000LL,
          0x123456789ABCDEFLL / 1000LL);
    CHK_S(__divdi3, -0x123456789ABCDEFLL, 1000LL,
          -0x123456789ABCDEFLL / 1000LL);
    CHK_S(__divdi3,  (int64_t)0x8000000000000000LL, 2LL,
          (int64_t)0x8000000000000000LL / 2LL);

    /* ---- divmoddi4 ---- */
    {
        int64_t r;
        int64_t q = __divmoddi4(-0x123456789ABCDEFLL, 1000LL, &r);
        if (q != -0x123456789ABCDEFLL / 1000LL ||
            r != -0x123456789ABCDEFLL % 1000LL) {
            printf("FAIL __divmoddi4: q=%lld r=%lld\n",
                   (long long)q, (long long)r);
            fail = 1;
        }
    }

    if (!fail) printf("PASS\n");
    return fail;
}
