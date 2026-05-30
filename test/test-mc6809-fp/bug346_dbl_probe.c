/* Bug #346: HD6309 f64 libm kernels miscompile on extreme-exponent
 * inputs (the test-math FAIL/TIMEOUT cluster at the HD6309-mame-fp
 * tiers; f32 is fine, and the identical binary on plain-6809-fp passes
 * with max ulp 0, so the MC6839 ROM is not at fault — only HD6309
 * codegen of the double kernels).
 *
 * The diagnosis walked these layers and found the FIRST few all CLEAN
 * on HD6309 (cases 1-9), so the bug is NOT general double movement:
 *   1-4. load/store, indexed table load, call ABI, __adddf3 libcall,
 *   5-6. i64 bit-manipulation of the double rep (exponent/mantissa),
 *   7.   log(2.0) computed bit-exact,
 *   8.   isnan / double== / nextafter,
 *   9.   test-math's ulp64() primitive on a 3-ulp-apart pair.
 *
 * The failure (case 10) only appears for log() of inputs with a large
 * |exponent| (e.g. 0x1.46p-140, as the real vectors use): HD6309
 * returns a NaN whose payload VARIES with surrounding code layout
 * (7ff49ba8.. vs 7ff4b030.. across builds) — the signature of a
 * register-allocation / spill scramble inside the kernel, the same
 * HD6309 wide-register family as Bug #344/#345/#311.  (double)k and
 * k*Ln2 in isolation are fine (case 11), so the scramble is deeper in
 * the polynomial/argument-reduction path.
 *
 * Prints raw bytes via %02x only (8-bit, integer printf) so the harness
 * itself uses NO 64-bit ops.  mc6809 is big-endian: 2.0 == 4000..00.
 * Passes on plain 6809; FAILs on HD6309 (the logvec[] NaN).
 */
#include <stdio.h>
#include <string.h>
#include <math.h>

static volatile double g = 2.0;
static volatile int    idx = 1;
static const double    tbl[2] = { 2.0, 3.0 };

static int dump(const char *tag, double d, const unsigned char *want) {
    unsigned char b[8];
    memcpy(b, &d, 8);
    printf("%-14s got=", tag);
    for (int i = 0; i < 8; i++)
        printf("%02x", b[i]);
    int bad = memcmp(b, want, 8) != 0;
    printf(" want=");
    for (int i = 0; i < 8; i++)
        printf("%02x", want[i]);
    printf(" %s\n", bad ? "MISMATCH" : "OK");
    return bad;
}

__attribute__((noinline)) static double identity(double v) {
    return v;
}

/* picolibc's modern log/exp kernels reinterpret the double as a uint64_t
 * and do exponent extraction / argument reduction with 64-bit shifts,
 * masks and subtracts.  Mimic that here; this is the suspected HD6309
 * i64-codegen miscompile (cf. Bug #344/#345). */
typedef unsigned long long u64;

static u64 asuint64(double d) {
    u64 u;
    memcpy(&u, &d, 8);
    return u;
}

__attribute__((noinline)) static int exp_field(double d) {
    /* IEEE-754 binary64 biased exponent = bits 62..52 */
    return (int)((asuint64(d) >> 52) & 0x7ff);
}

__attribute__((noinline)) static int mant_top(double d) {
    /* top byte of the 52-bit mantissa, via a 64-bit mask + shift */
    u64 m = asuint64(d) & 0x000fffffffffffffULL;
    return (int)(m >> 44);
}

/* Verbatim copy of test-math.h's ulp64() — the harness primitive that
 * every double libm test uses to score results.  Reproducing it here
 * lets us run it in isolation on HD6309. */
__attribute__((noinline)) static int my_ulp64(double ab, double bb) {
    volatile double a = ab;
    volatile double b = bb;
    if (a == b)
        return 0;
    if (isnan(a) && isnan(b))
        return 0;
    if (isinf(a) && isinf(b) && (a > 0) == (b > 0))
        return 0;
    if (isnan(a) || isnan(b))
        return 10000;            /* INV_ULP */
    int ulp = 0;
    while (a != b) {
        a = nextafter(a, b);
        ulp++;
        if (ulp == 9999)         /* MAX_ULP */
            break;
    }
    return ulp;
}

int main(void) {
    int e = 0;
    static const unsigned char two[8]   = {0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x00};
    static const unsigned char three[8] = {0x40,0x08,0x00,0x00,0x00,0x00,0x00,0x00};

    /* 1. plain load/store of a double through memory */
    double x = g;
    e += dump("load_store", x, two);

    /* 2. indexed table load (idx==1 -> 3.0) */
    double t = tbl[idx];
    e += dump("table[idx]", t, three);

    /* 3. call ABI: pass + return a double */
    double r = identity(g);
    e += dump("call_ident", r, two);

    /* 4. MC6839 libcall: 2.0 + 1.0 == 3.0 */
    double s = g + 1.0;
    e += dump("adddf3", s, three);

    /* 5. i64 bit-manipulation of the double representation (the libm
     *    exponent-extraction path).  g==2.0 -> bits 0x4000000000000000:
     *    biased exponent = 0x400, mantissa = 0. */
    int ef = exp_field(g);            /* expect 0x400 */
    int ok5 = (ef == 0x400);
    printf("exp_field     got=%03x want=400 %s\n", ef, ok5 ? "OK" : "MISMATCH");
    e += !ok5;

    int mt = mant_top(g);             /* expect 0 */
    int ok6 = (mt == 0x000);
    printf("mant_top      got=%03x want=000 %s\n", mt, ok6 ? "OK" : "MISMATCH");
    e += !ok6;

    /* 6. a non-trivial mantissa: 3.0 -> 0x4008000000000000,
     *    exponent 0x400, mantissa top nibble 0x8 -> mant_top 0x80. */
    double th = tbl[idx];             /* 3.0 */
    int ef2 = exp_field(th);
    int ok7 = (ef2 == 0x400);
    printf("exp_field(3)  got=%03x want=400 %s\n", ef2, ok7 ? "OK" : "MISMATCH");
    e += !ok7;
    int mt2 = mant_top(th);
    int ok8 = (mt2 == 0x080);
    printf("mant_top(3)   got=%03x want=080 %s\n", mt2, ok8 ? "OK" : "MISMATCH");
    e += !ok8;

    /* 7. The decisive test: compute log(2.0) directly and dump its
     *    bytes, bypassing test-math's ulp/print machinery entirely.
     *    log(2) = 0.6931471805599453 = 0x3FE62E42FEFA39EF. */
    static const unsigned char ln2[8] =
        {0x3f,0xe6,0x2e,0x42,0xfe,0xfa,0x39,0xef};
    double L = log(g);
    e += dump("log(2.0)", L, ln2);

    /* 8. test-math's ulp64() returns INV_ULP (the "10000" sentinel)
     *    when exactly one of {computed, expected} isnan().  Since the
     *    kernels compute correctly, the suspect is isnan(double) / the
     *    double compare / nextafter mis-firing that branch on HD6309. */
    static volatile double pos = 2.0;
    double nan_d = pos - pos;        /* 0.0 */
    nan_d = 0.0 / nan_d;             /* NaN via 0/0 */

    int in1 = isnan(pos) ? 1 : 0;    /* expect 0 */
    printf("isnan(2.0)    got=%d want=0 %s\n", in1, in1 == 0 ? "OK" : "MISMATCH");
    e += (in1 != 0);

    int in2 = isnan(nan_d) ? 1 : 0;  /* expect 1 */
    printf("isnan(nan)    got=%d want=1 %s\n", in2, in2 == 1 ? "OK" : "MISMATCH");
    e += (in2 != 1);

    int in3 = isnan(L) ? 1 : 0;      /* L = log(2.0); expect 0 */
    printf("isnan(log2)   got=%d want=0 %s\n", in3, in3 == 0 ? "OK" : "MISMATCH");
    e += (in3 != 0);

    /* double equality + nextafter, the other two ops ulp64 relies on */
    int eq = (pos == 2.0) ? 1 : 0;   /* expect 1 */
    printf("2.0==2.0      got=%d want=1 %s\n", eq, eq == 1 ? "OK" : "MISMATCH");
    e += (eq != 1);

    /* nextafter(2.0, 3.0) == 2.0 + 1 ulp == 0x4000000000000001 */
    static const unsigned char na_want[8] =
        {0x40,0x00,0x00,0x00,0x00,0x00,0x00,0x01};
    double na = nextafter(pos, 3.0);
    e += dump("nextafter", na, na_want);

    /* and the float control: nextafterf(2.0f,3.0f) == 0x40000001 */
    static volatile float fpos = 2.0f;
    float naf = nextafterf(fpos, 3.0f);
    unsigned char fb[4];
    memcpy(fb, &naf, 4);
    int fok = (fb[0]==0x40 && fb[1]==0x00 && fb[2]==0x00 && fb[3]==0x01);
    printf("nextafterf    got=%02x%02x%02x%02x want=40000001 %s\n",
           fb[0], fb[1], fb[2], fb[3], fok ? "OK" : "MISMATCH");
    e += !fok;

    /* 9. The actual harness primitive: ulp64() between two doubles 3
     *    ulps apart must return 3.  This is what test-math runs for
     *    every vector; if it returns 9999/10000 here, that IS the #346
     *    test failure reproduced minimally. */
    double b3 = nextafter(nextafter(nextafter(pos, 3.0), 3.0), 3.0);
    int u = my_ulp64(pos, b3);
    printf("ulp64(3apart) got=%d want=3 %s\n", u, u == 3 ? "OK" : "MISMATCH");
    e += (u != 3);

    /* 10. Replicate test-log.h vector #1 exactly: the real vectors use
     *     TINY inputs (2^-140), unlike my log(2.0).  If log() of a tiny
     *     subnormal-adjacent value is wrong/NaN on HD6309, that is the
     *     real #346 failure. */
    static const struct { double x, y; int ulp; } logvec[] = {
        { 0x1.46p-140,   -0x1.83320effbc131p6, 1 },
        { 0x1.2238p-136, -0x1.7891fb0002714p6, 1 },
    };
    for (int k = 0; k < 2; k++) {
        volatile double yy = log(logvec[k].x);
        unsigned char b[8]; memcpy(b, (void *)&yy, 8);
        int u = my_ulp64(yy, logvec[k].y);
        int nanflag = isnan((double)yy);
        printf("logvec[%d] got=", k);
        for (int i = 0; i < 8; i++) printf("%02x", b[i]);
        printf(" isnan=%d ulp64=%d %s\n", nanflag, u,
               (u <= 1 && !nanflag) ? "OK" : "MISMATCH");
        if (!(u <= 1 && !nanflag)) e++;
    }

    /* 11. Narrow the log-kernel miscompile.  The modern picolibc log
     *     reconstructs result ~= k*Ln2 + poly, where k is the (large,
     *     negative) unbiased exponent.  Probe the int->double of a
     *     large-magnitude exponent and the k*Ln2 product. */
    static volatile int km = -140;
    double kd = (double)km;          /* __floatsidf; want -140.0 = c061800000000000 */
    static const unsigned char m140[8] = {0xc0,0x61,0x80,0x00,0x00,0x00,0x00,0x00};
    e += dump("(double)-140", kd, m140);

    static volatile double ln2v = 0x1.62e42fefa39efp-1;  /* 0.6931471805599453 */
    double prod = kd * ln2v;         /* -140*ln2 = -97.04... = c0584d3d... */
    int pnan = isnan(prod);
    printf("k*ln2        got=");
    { unsigned char b[8]; memcpy(b, &prod, 8); for (int i=0;i<8;i++) printf("%02x",b[i]); }
    printf(" isnan=%d %s\n", pnan, !pnan ? "OK" : "MISMATCH");
    e += pnan;

    printf("%d mismatches\n", e);
    return e != 0;
}
