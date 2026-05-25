/* Bug #289: verify C99 <fenv.h> works on mc6809 via the MC6839 FPCBs.
 *
 * 8 sub-tests covering exception query/clear, rounding-mode control,
 * env save/restore, and the non-standard __mc6839_set_rom_base().
 *
 * Each sub-test prints a single OK/FAIL line.  main() returns non-zero
 * if any sub-test fails.
 */
#include <fenv.h>
#include <math.h>
#include <stdio.h>
#include <stdint.h>

extern void __mc6839_set_rom_base(void *base);
extern char __mc6839_rom_start[];

static int failures = 0;

#define EXPECT(cond, msg) do {                                  \
    if (cond) {                                                 \
        printf("OK: " msg "\n");                                \
    } else {                                                    \
        printf("FAIL: " msg "\n");                              \
        failures++;                                             \
    }                                                           \
} while (0)

/* Force the compiler not to constant-fold an FP value. */
static volatile float vol_f;
static volatile double vol_d;

int main(void) {
#if defined(__FAST_MATH__)
    /* Bug #335: -Ofast implies -ffast-math, under which the compiler
     * assumes no floating-point exceptions and no fenv access. It folds
     * away (or reorders around) the operations this probe relies on to
     * set the sticky exception flags, so fenv behaviour is undefined and
     * the test cannot meaningfully pass. Skip rather than fail. */
    printf("skipping: fenv is not meaningful under -ffast-math\n");
    return 77;
#endif
    /* Test 1: sqrt(-1) raises FE_INVALID. */
    feclearexcept(FE_ALL_EXCEPT);
    vol_f = -1.0f;
    vol_f = sqrtf(vol_f);
    (void)vol_f;
    EXPECT(fetestexcept(FE_INVALID) != 0, "sqrt(-1) raises FE_INVALID");

    /* Test 2: feclearexcept actually clears. */
    feclearexcept(FE_INVALID);
    EXPECT(fetestexcept(FE_INVALID) == 0, "feclearexcept(FE_INVALID)");

    /* Test 3: fesetround / fegetround round-trip. */
    EXPECT(fesetround(FE_DOWNWARD) == 0, "fesetround(FE_DOWNWARD)");
    EXPECT(fegetround() == FE_DOWNWARD, "fegetround == FE_DOWNWARD");

    /* Test 4: rounding mode actually changes results.
     * 1.0 / 3.0 in DOWNWARD vs UPWARD should differ in the LSB. */
    feclearexcept(FE_ALL_EXCEPT);
    fesetround(FE_DOWNWARD);
    vol_f = 1.0f;
    float down = vol_f / 3.0f;
    fesetround(FE_UPWARD);
    vol_f = 1.0f;
    float up = vol_f / 3.0f;
    EXPECT(down != up, "1/3 differs between FE_DOWNWARD and FE_UPWARD");
    /* Reset to default. */
    fesetround(FE_TONEAREST);

    /* Test 5: fegetenv / fesetenv round-trip preserves rounding. */
    feclearexcept(FE_ALL_EXCEPT);
    fesetround(FE_UPWARD);
    fenv_t saved;
    fegetenv(&saved);
    fesetround(FE_TONEAREST);          /* perturb */
    fesetenv(&saved);                  /* restore */
    EXPECT(fegetround() == FE_UPWARD, "fesetenv restored FE_UPWARD");
    fesetround(FE_TONEAREST);

    /* Test 6: divide by zero raises FE_DIVBYZERO.
     * Use volatile to defeat constant folding. */
    feclearexcept(FE_ALL_EXCEPT);
    static volatile float vol_zero;     /* file-scope volatile zero */
    vol_zero = 0.0f;
    vol_f = 1.0f;
    vol_f = vol_f / vol_zero;
    (void)vol_f;
    EXPECT(fetestexcept(FE_DIVBYZERO) != 0, "1.0/0.0 raises FE_DIVBYZERO");

    /* Test 7: overflow raises FE_OVERFLOW.
     * Use double to be sure the result overflows the f64 range. */
    feclearexcept(FE_ALL_EXCEPT);
    vol_d = 1e300;
    vol_d = vol_d * vol_d;
    (void)vol_d;
    EXPECT(fetestexcept(FE_OVERFLOW) != 0, "1e300 * 1e300 raises FE_OVERFLOW");

    /* Test 8: calling __mc6839_set_rom_base with the default address
     * doesn't break subsequent FP math (sanity-check the trampoline
     * indirection works for the no-op case).  The OS-9 case with a
     * relocated ROM is tested separately when an OS-9 crt0 lands. */
    feclearexcept(FE_ALL_EXCEPT);
    __mc6839_set_rom_base(__mc6839_rom_start);
    vol_f = 2.0f;
    vol_f = vol_f + 3.0f;
    EXPECT(vol_f == 5.0f, "__mc6839_set_rom_base(default) preserves FP math");

    printf("%d failures\n", failures);
    return failures != 0;
}
