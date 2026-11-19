/* Double to float conversion of values below FLT_MIN.
 *
 * The MC6839 ROM's FMOV turns a result below FLT_MIN into a NaN, so
 * compiler-rt's __truncdfsf2 converts those itself.  Check the result's
 * encoding in all four rounding modes, both signs, the ties, the value
 * that rounds up to FLT_MIN, and the underflow and inexact flags.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <fenv.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

static const struct {
    uint64_t d;
    uint32_t f[4]; /* to nearest, toward zero, upward, downward */
} cases[] = {
    { 0x36a0000000000000ull, { 0x00000001, 0x00000001, 0x00000001, 0x00000001 } }, /* 2^-149 */
    { 0x36a8000000000000ull, { 0x00000002, 0x00000001, 0x00000002, 0x00000001 } }, /* 1.5 * 2^-149 */
    { 0x36b4000000000000ull, { 0x00000002, 0x00000002, 0x00000003, 0x00000002 } }, /* 2.5 * 2^-149 */
    { 0x3690000000000000ull, { 0x00000000, 0x00000000, 0x00000001, 0x00000000 } }, /* 2^-150 */
    { 0x3800000000000000ull, { 0x00400000, 0x00400000, 0x00400000, 0x00400000 } }, /* 2^-127 */
    { 0x380fffffe0000000ull, { 0x00800000, 0x007fffff, 0x00800000, 0x007fffff } }, /* FLT_MIN - 2^-150 */
    { 0x37b0efd800000000ull, { 0x00021dfb, 0x00021dfb, 0x00021dfb, 0x00021dfb } }, /* exact */
    { 0x0000000000000001ull, { 0x00000000, 0x00000000, 0x00000001, 0x00000000 } }, /* double subnormal */
};
static const int modes[4] = { FE_TONEAREST, FE_TOWARDZERO, FE_UPWARD, FE_DOWNWARD };
static const char *const names[4] = { "nearest", "zero", "upward", "downward" };

static volatile double vd;
static volatile float vf;

static uint32_t convert(uint64_t bits) {
    double d;
    float f;
    uint32_t u;
    memcpy(&d, &bits, sizeof d);
    vd = d;
    vf = (float)vd;
    f = vf;
    memcpy(&u, &f, sizeof u);
    return u;
}

int main(void) {
    int e = 0;
#if defined(__FAST_MATH__) || __FINITE_MATH_ONLY__
    printf("skipping: rounding modes not meaningful under -ffast-math\n");
    return 77;
#endif
    for (unsigned m = 0; m < 4; m++) {
        fesetround(modes[m]);
        for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
            for (int neg = 0; neg < 2; neg++) {
                uint64_t d = cases[i].d | (neg ? UINT64_C(0x8000000000000000) : 0);
                /* a negative value rounds upward as its magnitude rounds downward */
                unsigned mm = neg && m >= 2 ? 5 - m : m;
                uint32_t want = cases[i].f[mm] | (neg ? UINT32_C(0x80000000) : 0);
                uint32_t got = convert(d);
                if (got != want) {
                    printf("FAIL: %s %s%u: got %08lx want %08lx\n", names[m], neg ? "-" : "+",
                           i, (unsigned long)got, (unsigned long)want);
                    e++;
                }
            }
        }
    }
    fesetround(FE_TONEAREST);

    feclearexcept(FE_ALL_EXCEPT);
    (void)convert(0x36a8000000000000ull);
    if (fetestexcept(FE_UNDERFLOW | FE_INEXACT) != (FE_UNDERFLOW | FE_INEXACT)) {
        printf("FAIL: an inexact tiny result did not raise underflow and inexact\n");
        e++;
    }
    feclearexcept(FE_ALL_EXCEPT);
    (void)convert(0x3800000000000000ull);
    if (fetestexcept(FE_UNDERFLOW | FE_INEXACT | FE_INVALID)) {
        printf("FAIL: an exact tiny result raised an exception\n");
        e++;
    }

    printf("%d failures\n", e);
    return e != 0;
}
