/* Bug #337: __unordsf2 / __unorddf2 soft-float unordered-compare builtins.
 *
 * isunordered(a,b) lowers to `fcmp uno` -> a call to __unordXf2(a,b),
 * which must return nonzero iff either operand is NaN. These builtins
 * were missing from the mc6809 compiler-rt (fp_cmp.S provided only the
 * ordered compares __cmpXf2/__ltXf2/...), so any isunordered / x!=x /
 * UNO comparison failed to link. Verify both precisions.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <math.h>
#include <stdio.h>

static volatile float  fn, f1;
static volatile double dn, d1;

int main(void) {
    int e = 0;
#if defined(__FAST_MATH__)
    /* Bug #335-style: -Ofast implies -ffast-math, which lets the
     * compiler assume operands are never NaN — it folds away the 0/0
     * NaN generation and the isunordered() checks, so the probe can't
     * meaningfully run. Skip rather than fail (the builtins themselves
     * are still verified at every IEEE-respecting opt level). */
    printf("skipping: NaN tests not meaningful under -ffast-math\n");
    return 77;
#endif
    fn = 0.0f; fn = fn / fn;      /* NaN (0/0) */
    f1 = 1.0f;
    dn = 0.0;  dn = dn / dn;      /* NaN */
    d1 = 1.0;

    if (!isunordered(fn, f1)) { printf("FAIL: f32 isunordered(nan,1)\n"); e++; }
    if (!isunordered(f1, fn)) { printf("FAIL: f32 isunordered(1,nan)\n"); e++; }
    if ( isunordered(f1, f1)) { printf("FAIL: f32 isunordered(1,1)\n");   e++; }
    if (!isunordered(dn, d1)) { printf("FAIL: f64 isunordered(nan,1)\n"); e++; }
    if (!isunordered(d1, dn)) { printf("FAIL: f64 isunordered(1,nan)\n"); e++; }
    if ( isunordered(d1, d1)) { printf("FAIL: f64 isunordered(1,1)\n");   e++; }

    printf("%d failures\n", e);
    return e != 0;
}
