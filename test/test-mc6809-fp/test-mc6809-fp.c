/*
 * Bug #162: end-to-end execution test for MC6839 soft-float wrappers.
 *
 * Uses exact integer-valued floats so every expected result is
 * bit-for-bit IEEE 754 correct with no rounding.
 * sqrt() and transcendental functions omitted: sqrt() requires libm
 * (absent in standard builds), and transcendental functions diverge
 * from IEEE 754-1985 in the MC6839. Both are covered by the lit test.
 *
 * volatile prevents the compiler from constant-folding away the calls.
 */
#include <stdint.h>
#include <stdio.h>

typedef union { float f; uint32_t u; } f32;
typedef union { double d; uint64_t u; } f64;

static int fail_count;

static void chk_f32(const char *name, float got, float want)
{
    f32 g, w;
    g.f = got;
    w.f = want;
    if (g.u != w.u) {
        printf("FAIL %s: got 0x%08lx want 0x%08lx\n",
               name, (unsigned long)g.u, (unsigned long)w.u);
        fail_count++;
    }
}

static void chk_f64(const char *name, double got, double want)
{
    f64 g, w;
    g.d = got;
    w.d = want;
    if (g.u != w.u) {
        printf("FAIL %s: got 0x%016llx want 0x%016llx\n",
               name, (unsigned long long)g.u, (unsigned long long)w.u);
        fail_count++;
    }
}

static void chk_int(const char *name, int got, int want)
{
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", name, got, want);
        fail_count++;
    }
}

static void chk_bool(const char *name, int got, int want)
{
    if ((!!got) != (!!want)) {
        printf("FAIL %s: got %d want %d\n", name, got, want);
        fail_count++;
    }
}

int main(void)
{
    volatile float a, b, r;
    volatile double da, db, dr;

    /* ---- f32 arithmetic ---- */
    a = 1.0f; b = 2.0f;
    r = a + b;
    chk_f32("fadd 1+2", r, 3.0f);

    a = 4.0f; b = 1.5f;
    r = a - b;
    chk_f32("fsub 4-1.5", r, 2.5f);

    a = 2.0f; b = 3.0f;
    r = a * b;
    chk_f32("fmul 2*3", r, 6.0f);

    a = 6.0f; b = 2.0f;
    r = a / b;
    chk_f32("fdiv 6/2", r, 3.0f);

    /* ---- f32 comparisons ---- */
    a = 1.0f; b = 2.0f;
    chk_bool("1<2",  a < b,  1);
    chk_bool("1>2",  a > b,  0);
    chk_bool("1==2", a == b, 0);
    chk_bool("2==2", b == b, 1);
    chk_bool("2>=2", b >= b, 1);

    /* ---- f32 → int (truncation toward zero) ---- */
    a = 3.75f;
    chk_int("(int)3.75f", (int)a, 3);

    a = -3.75f;
    chk_int("(int)-3.75f", (int)a, -3);

    /* ---- int → f32 ---- */
    volatile int i = 7;
    r = (float)i;
    chk_f32("(float)7", r, 7.0f);

    i = -5;
    r = (float)i;
    chk_f32("(float)-5", r, -5.0f);

    /* ---- f64 arithmetic ---- */
    da = 1.0; db = 2.0;
    dr = da + db;
    chk_f64("dadd 1+2", dr, 3.0);

    da = 6.0; db = 2.0;
    dr = da / db;
    chk_f64("ddiv 6/2", dr, 3.0);

    /* ---- f32 ↔ f64 conversions ---- */
    a = 1.5f;
    dr = (double)a;
    chk_f64("extend 1.5f→d", dr, 1.5);

    da = 2.5;
    r = (float)da;
    chk_f32("trunc 2.5→f", r, 2.5f);

    if (fail_count)
        printf("FAIL (%d failures)\n", fail_count);
    else
        printf("PASS\n");

    return fail_count ? 1 : 0;
}
