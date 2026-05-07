#include <stdio.h>
#include <stdint.h>
#include <string.h>

/* Bug #222: independent verification that __sqrtsf2 / __sqrtdf2
 * actually dispatch through SQRTBL/FSQRT in the MC6839 ROM, rather
 * than some other handler (cf. the FMOV opcode mis-dispatch found
 * in 245863901a9e).
 *
 * Calls the libcalls directly — picolibc's sqrtf/sqrt in libm are
 * pure-C implementations and DON'T pull in our wrappers. */

extern float  __sqrtsf2(float);
extern double __sqrtdf2(double);

static int fail;

static void chk_f32(const char *name, float got, float want) {
    uint32_t g, w;
    memcpy(&g, &got, 4);
    memcpy(&w, &want, 4);
    if (g != w) {
        printf("FAIL %s: got 0x%08lx want 0x%08lx\n",
               name, (unsigned long)g, (unsigned long)w);
        fail = 1;
    }
}

static void chk_f64(const char *name, double got, double want) {
    uint8_t gb[8], wb[8];
    memcpy(gb, &got, 8); memcpy(wb, &want, 8);
    if (memcmp(gb, wb, 8) != 0) {
        printf("FAIL %s\n", name);
        fail = 1;
    }
}

int main(void) {
    /* volatile blocks constant-folding */
    volatile float fa = 4.0f, fb = 9.0f, fc = 16.0f, fd = 0.25f, fe = 0.0f;
    volatile double da = 4.0, db = 9.0, dc = 16.0, dd = 0.25, de = 0.0;

    chk_f32("__sqrtsf2(4)",    __sqrtsf2(fa), 2.0f);
    chk_f32("__sqrtsf2(9)",    __sqrtsf2(fb), 3.0f);
    chk_f32("__sqrtsf2(16)",   __sqrtsf2(fc), 4.0f);
    chk_f32("__sqrtsf2(0.25)", __sqrtsf2(fd), 0.5f);
    chk_f32("__sqrtsf2(0)",    __sqrtsf2(fe), 0.0f);

    chk_f64("__sqrtdf2(4)",    __sqrtdf2(da), 2.0);
    chk_f64("__sqrtdf2(9)",    __sqrtdf2(db), 3.0);
    chk_f64("__sqrtdf2(16)",   __sqrtdf2(dc), 4.0);
    chk_f64("__sqrtdf2(0.25)", __sqrtdf2(dd), 0.5);
    chk_f64("__sqrtdf2(0)",    __sqrtdf2(de), 0.0);

    if (!fail) printf("PASSED\n");
    return fail;
}
