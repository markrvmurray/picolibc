/* Bug #343: _Complex double multiply and divide via the compiler-rt helpers
 * __muldc3 / __divdc3.  Double half of the split (see bug343_cmplxf_probe.c
 * for the full rationale); split because __divdc3 alone is ~16 KB on this
 * 16-bit soft-float target and all four helpers will not co-exist in 64 KB.
 *
 * Exact-result operands -> bit-exact (0 ULP) checks:
 *   (1+2i) * (3+4i)  == -5 + 10i   links __muldc3
 *   (-5+10i) / (3+4i) == 1 + 2i    runs  __divdc3 at runtime (ABI + scaling)
 *
 * volatile operands defeat constant-folding; __builtin_complex avoids
 * picolibc's _Complex double `I`; reporting avoids %f/%g (no __dtoa_engine).
 *
 * Returns non-zero if any sub-test fails.
 */
#include <complex.h>
#include <stdio.h>

static volatile double da_r = 1,  da_i = 2,  db_r = 3, db_i = 4;
static volatile double dn_r = -5, dn_i = 10;

int main(void) {
    int e = 0;

    double _Complex db = __builtin_complex(db_r, db_i);
    double _Complex dp = __builtin_complex(da_r, da_i) * db;  /* -5 + 10i */
    double _Complex dq = __builtin_complex(dn_r, dn_i) / db;  /*  1 +  2i */

    if (__real__ dp != -5.0 || __imag__ dp != 10.0) {
        printf("FAIL: cdouble mul (want -5+10i)\n"); e++;
    }
    if (__real__ dq != 1.0 || __imag__ dq != 2.0) {
        printf("FAIL: cdouble div (want 1+2i)\n");   e++;
    }

    printf("%d failures\n", e);
    return e != 0;
}
