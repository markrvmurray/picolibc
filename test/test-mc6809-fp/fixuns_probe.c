/* Floating point to unsigned long and unsigned long long.
 *
 * The casts become calls of __fixunssfsi, __fixunsdfsi, __fixunssfdi and
 * __fixunsdfdi, which compiler-rt once lacked for this target, so any
 * such program failed to link.  Check values across the range: small,
 * just below and above 2^31 and 2^63 (where the conversion changes
 * method), the top of each range, and out-of-range values, which give 0
 * or the largest value as the generic versions do (C leaves those
 * undefined; NaN is not checked).
 *
 * Returns non-zero if any sub-test fails.
 */
#include <stdint.h>
#include <stdio.h>

static const struct { double d; float f; uint32_t d32, f32; uint64_t d64, f64; } cases[] = {
    { 0.0, 0.0f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { -0.0, -0.0f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { 0.75, 0.75f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { -0.75, -0.75f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { 1.0, 1.0f, 0x1u, 0x1u, 0x1ull, 0x1ull },
    { 1.5, 1.5f, 0x1u, 0x1u, 0x1ull, 0x1ull },
    { 65535.0, 65535.0f, 0xffffu, 0xffffu, 0xffffull, 0xffffull },
    { 65536.0, 65536.0f, 0x10000u, 0x10000u, 0x10000ull, 0x10000ull },
    { 2147483647.0, 2147483648.0f, 0x7fffffffu, 0x80000000u, 0x7fffffffull, 0x80000000ull },
    { 2147483647.5, 2147483648.0f, 0x7fffffffu, 0x80000000u, 0x7fffffffull, 0x80000000ull },
    { 2147483648.0, 2147483648.0f, 0x80000000u, 0x80000000u, 0x80000000ull, 0x80000000ull },
    { 2147483649.0, 2147483648.0f, 0x80000001u, 0x80000000u, 0x80000001ull, 0x80000000ull },
    { 3000000000.0, 3000000000.0f, 0xb2d05e00u, 0xb2d05e00u, 0xb2d05e00ull, 0xb2d05e00ull },
    { 4294967295.0, 4294967296.0f, 0xffffffffu, 0xffffffffu, 0xffffffffull, 0x100000000ull },
    { 4294967295.5, 4294967296.0f, 0xffffffffu, 0xffffffffu, 0xffffffffull, 0x100000000ull },
    { 4294967296.0, 4294967296.0f, 0xffffffffu, 0xffffffffu, 0x100000000ull, 0x100000000ull },
    { 10000000000.0, 10000000000.0f, 0xffffffffu, 0xffffffffu, 0x2540be400ull, 0x2540be400ull },
    { 9007199254740992.0, 9007199254740992.0f, 0xffffffffu, 0xffffffffu, 0x20000000000000ull, 0x20000000000000ull },
    { 4.611686018427388e+18, 4.611686018427388e+18f, 0xffffffffu, 0xffffffffu, 0x4000000000000000ull, 0x4000000000000000ull },
    { 9.223372036854775e+18, 9.223372036854776e+18f, 0xffffffffu, 0xffffffffu, 0x7ffffffffffffc00ull, 0x8000000000000000ull },
    { 9.223372036854776e+18, 9.223372036854776e+18f, 0xffffffffu, 0xffffffffu, 0x8000000000000000ull, 0x8000000000000000ull },
    { 9.223372036854778e+18, 9.223372036854776e+18f, 0xffffffffu, 0xffffffffu, 0x8000000000000800ull, 0x8000000000000000ull },
    { 1.3835058055282164e+19, 1.3835058055282164e+19f, 0xffffffffu, 0xffffffffu, 0xc000000000000000ull, 0xc000000000000000ull },
    { 1.844674407370955e+19, 1.8446744073709552e+19f, 0xffffffffu, 0xffffffffu, 0xfffffffffffff800ull, 0xffffffffffffffffull },
    { 1.8446744073709552e+19, 1.8446744073709552e+19f, 0xffffffffu, 0xffffffffu, 0xffffffffffffffffull, 0xffffffffffffffffull },
    { 1e+20, 1.0000000200408773e+20f, 0xffffffffu, 0xffffffffu, 0xffffffffffffffffull, 0xffffffffffffffffull },
    { -1.0, -1.0f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { -5.0, -5.0f, 0x0u, 0x0u, 0x0ull, 0x0ull },
    { __builtin_inf(), (float)__builtin_inf(), 0xffffffffu, 0xffffffffu, 0xffffffffffffffffull, 0xffffffffffffffffull },
    { -__builtin_inf(), (float)-__builtin_inf(), 0x0u, 0x0u, 0x0ull, 0x0ull },
};

static volatile double vd;
static volatile float vf;

int main(void) {
    int e = 0;
#if defined(__FAST_MATH__) || __FINITE_MATH_ONLY__
    printf("skipping: infinities not meaningful under -ffinite-math-only\n");
    return 77;
#endif
    for (unsigned i = 0; i < sizeof cases / sizeof cases[0]; i++) {
        vd = cases[i].d;
        vf = cases[i].f;
        if ((uint32_t)(unsigned long)vd != cases[i].d32) {
            printf("FAIL %u: (unsigned long) double\n", i);
            e++;
        }
        if ((uint32_t)(unsigned long)vf != cases[i].f32) {
            printf("FAIL %u: (unsigned long) float\n", i);
            e++;
        }
        if ((uint64_t)(unsigned long long)vd != cases[i].d64) {
            printf("FAIL %u: (unsigned long long) double\n", i);
            e++;
        }
        if ((uint64_t)(unsigned long long)vf != cases[i].f64) {
            printf("FAIL %u: (unsigned long long) float\n", i);
            e++;
        }
    }
    printf("%d failures\n", e);
    return e != 0;
}
