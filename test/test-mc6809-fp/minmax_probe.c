/* C23 fminimum, fmaximum, fminimum_num and fmaximum_num.
 *
 * clang turns fminimum_num and fmaximum_num into llvm.minimumnum and
 * llvm.maximumnum, which once crashed the compiler; llvm.minimum and
 * llvm.maximum (reached here through __builtin_elementwise_minimum and
 * __builtin_elementwise_maximum) become calls of fminimum and fmaximum.
 * Check each in float and double where they differ from fmin and fmax:
 * -0 is less than +0, the _num forms return the number when the other
 * operand is a NaN, and the others return the NaN.
 *
 * Returns non-zero if any sub-test fails.
 */
#define _ISOC23_SOURCE
#include <math.h>
#include <stdio.h>

static volatile double dz, dnz, d1, d2, dn;
static volatile float fz, fnz, f1, f2, fn;
static int e;

static void want(const char *what, double got, double expected) {
    if (isnan(expected) ? !isnan(got)
                        : (got != expected || signbit(got) != signbit(expected))) {
        printf("FAIL: %s\n", what);
        e++;
    }
}

static void wantf(const char *what, float got, double expected) {
    want(what, (double)got, expected);
}

int main(void) {
#if defined(__FAST_MATH__) || __FINITE_MATH_ONLY__
    printf("skipping: NaN and signed zero not meaningful under -ffast-math\n");
    return 77;
#endif
    dz = 0.0; dnz = -0.0; d1 = 1.0; d2 = 2.0; dn = dz / dz;
    fz = 0.0f; fnz = -0.0f; f1 = 1.0f; f2 = 2.0f; fn = fz / fz;
    double nan_d = dn;

    want("fminimum_num(1,2)", fminimum_num(d1, d2), 1.0);
    want("fmaximum_num(1,2)", fmaximum_num(d1, d2), 2.0);
    want("fminimum_num(-0,+0)", fminimum_num(dz, dnz), -0.0);
    want("fmaximum_num(-0,+0)", fmaximum_num(dnz, dz), 0.0);
    want("fminimum_num(nan,1)", fminimum_num(dn, d1), 1.0);
    want("fmaximum_num(1,nan)", fmaximum_num(d1, dn), 1.0);
    wantf("fminimum_numf(1,2)", fminimum_numf(f1, f2), 1.0);
    wantf("fmaximum_numf(1,2)", fmaximum_numf(f1, f2), 2.0);
    wantf("fminimum_numf(-0,+0)", fminimum_numf(fz, fnz), -0.0);
    wantf("fmaximum_numf(-0,+0)", fmaximum_numf(fnz, fz), 0.0);
    wantf("fminimum_numf(nan,1)", fminimum_numf(fn, f1), 1.0);
    wantf("fmaximum_numf(1,nan)", fmaximum_numf(f1, fn), 1.0);

    want("minimum(1,2)", __builtin_elementwise_minimum(d1, d2), 1.0);
    want("maximum(1,2)", __builtin_elementwise_maximum(d1, d2), 2.0);
    want("minimum(-0,+0)", __builtin_elementwise_minimum(dz, dnz), -0.0);
    want("maximum(-0,+0)", __builtin_elementwise_maximum(dnz, dz), 0.0);
    want("minimum(nan,1)", __builtin_elementwise_minimum(dn, d1), nan_d);
    want("maximum(1,nan)", __builtin_elementwise_maximum(d1, dn), nan_d);
    wantf("minimumf(1,2)", __builtin_elementwise_minimum(f1, f2), 1.0);
    wantf("maximumf(1,2)", __builtin_elementwise_maximum(f1, f2), 2.0);
    wantf("minimumf(-0,+0)", __builtin_elementwise_minimum(fz, fnz), -0.0);
    wantf("maximumf(-0,+0)", __builtin_elementwise_maximum(fnz, fz), 0.0);
    wantf("minimumf(nan,1)", __builtin_elementwise_minimum(fn, f1), nan_d);
    wantf("maximumf(1,nan)", __builtin_elementwise_maximum(f1, fn), nan_d);

    printf("%d failures\n", e);
    return e != 0;
}
