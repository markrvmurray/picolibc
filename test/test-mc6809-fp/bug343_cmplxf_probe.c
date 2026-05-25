/* Bug #343: _Complex float multiply and divide via the compiler-rt helpers
 * __mulsc3 / __divsc3.
 *
 * These were excluded from the mc6809 compiler-rt build, so any _Complex
 * float `*` or `/` failed to link. They are generic C wrappers over the
 * soft-float libcalls mc6809 already provides plus the inline scaling
 * helpers in compiler-rt's fp_lib.h; simply re-enabling them suffices.
 *
 * The helpers are large on this 16-bit soft-float target (the inlined
 * logb/scalbn/fmax + NaN/Inf recovery expand a lot — __divsc3 is ~14 KB),
 * so float and double are split into separate probes: exercising all four
 * helpers in one binary overflows the 64 KB window. This is the float half.
 *
 * Operands are chosen so the true result and every intermediate are exactly
 * representable in binary, so a correctly-rounded soft-float must produce a
 * bit-exact answer — the checks are `==` (0 ULP), not a tolerance. The
 * failure modes guarded against (unresolved symbol, _Complex ABI scramble,
 * wrong algorithm) surface as link errors, NaN, or gross many-ULP errors,
 * never sub-ULP drift.
 *
 *   (1+2i) * (3+4i)  == -5 + 10i   links __mulsc3 (clang inlines the product
 *                                  for finite operands; the symbol must
 *                                  still resolve)
 *   (-5+10i) / (3+4i) == 1 + 2i    runs  __divsc3 at runtime (ABI + scaling)
 *
 * The operands are volatile so the arithmetic cannot be folded at compile
 * time. Complex values are built with __builtin_complex (picolibc's `I` is
 * _Complex double, which would promote these to double). Reporting avoids
 * %f/%g so the probe does not drag in the double-formatting __dtoa_engine.
 *
 * Returns non-zero if any sub-test fails.
 */
#include <complex.h>
#include <stdio.h>

static volatile float fa_r = 1,  fa_i = 2,  fb_r = 3, fb_i = 4;
static volatile float fn_r = -5, fn_i = 10;

int main(void) {
    int e = 0;

    float _Complex fb = __builtin_complex(fb_r, fb_i);
    float _Complex fp = __builtin_complex(fa_r, fa_i) * fb;   /* -5 + 10i */
    float _Complex fq = __builtin_complex(fn_r, fn_i) / fb;   /*  1 +  2i */

    if (__real__ fp != -5.0f || __imag__ fp != 10.0f) {
        printf("FAIL: cfloat mul (want -5+10i)\n"); e++;
    }
    if (__real__ fq != 1.0f || __imag__ fq != 2.0f) {
        printf("FAIL: cfloat div (want 1+2i)\n");   e++;
    }

    printf("%d failures\n", e);
    return e != 0;
}
