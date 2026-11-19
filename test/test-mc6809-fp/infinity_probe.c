/* Signed infinities through the MC6839 builtins.
 *
 * The MC6839 has two closure modes: affine (+inf and -inf, as IEEE 754
 * requires) and projective (one unsigned infinity, from a draft of the
 * standard).  The control blocks in compiler-rt must select affine.  In
 * projective mode sqrt(+inf) is invalid and gives a NaN, -inf compares
 * equal to +inf, and a sum of two negative infinities loses its sign.
 *
 * The square roots go through the compiler's own libcalls rather than
 * libm, so the probe links at levels built without libm.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <math.h>
#include <stdio.h>

extern float  __sqrtsf2(float);
extern double __sqrtdf2(double);

static volatile float  fpi, fni, f1, fbig;
static volatile double dpi, dni, d1, dbig;

int main(void) {
    int e = 0;
#if defined(__FAST_MATH__) || __FINITE_MATH_ONLY__
    /* Under -ffinite-math-only the compiler may assume there are no
     * infinities and fold every check below.  Skip rather than fail. */
    printf("skipping: infinities not meaningful under -ffinite-math-only\n");
    return 77;
#endif
    f1 = 1.0f; fbig = 3.40282347e38f;
    fpi = f1 / 0.0f; fni = -f1 / 0.0f;
    d1 = 1.0; dbig = 1.7976931348623157e308;
    dpi = d1 / 0.0;  dni = -d1 / 0.0;

    if (!(isinf(fpi) && fpi > 0)) { printf("FAIL: f32 1/0 is not +inf\n"); e++; }
    if (!(isinf(fni) && fni < 0)) { printf("FAIL: f32 -1/0 is not -inf\n"); e++; }
    if (fni == fpi)               { printf("FAIL: f32 -inf == +inf\n");     e++; }
    if (!(fni < fpi))             { printf("FAIL: f32 -inf < +inf false\n"); e++; }
    if (!(fpi > fbig))            { printf("FAIL: f32 +inf > FLT_MAX false\n"); e++; }
    if (!(fni + fni < 0))         { printf("FAIL: f32 -inf + -inf not negative\n"); e++; }
    if (__sqrtsf2(fpi) != fpi)    { printf("FAIL: f32 sqrt(+inf) != +inf\n"); e++; }

    if (!(isinf(dpi) && dpi > 0)) { printf("FAIL: f64 1/0 is not +inf\n"); e++; }
    if (!(isinf(dni) && dni < 0)) { printf("FAIL: f64 -1/0 is not -inf\n"); e++; }
    if (dni == dpi)               { printf("FAIL: f64 -inf == +inf\n");     e++; }
    if (!(dni < dpi))             { printf("FAIL: f64 -inf < +inf false\n"); e++; }
    if (!(dpi > dbig))            { printf("FAIL: f64 +inf > DBL_MAX false\n"); e++; }
    if (!(dni + dni < 0))         { printf("FAIL: f64 -inf + -inf not negative\n"); e++; }
    if (__sqrtdf2(dpi) != dpi)    { printf("FAIL: f64 sqrt(+inf) != +inf\n"); e++; }

    printf("%d failures\n", e);
    return e != 0;
}
