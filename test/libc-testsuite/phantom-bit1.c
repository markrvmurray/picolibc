/*
 * Phantom-BIT1 regression probes — bug #152.
 *
 * The MC6809 backend treats the s1 carry/overflow result of
 * arithmetic primitives (G_UADDO/G_SADDO/..., the SetCarry/SetOverflow
 * pseudos) as a "scheduling phantom" whose real value lives in CC
 * (C for carry, V for signed overflow) rather than in the allocated
 * byte-LSB. Five prior bugs in this family (#115, #117, #140, #147,
 * #148) were single-consumer fixes. These probes exercise the three
 * consumer paths that were latent miscompiles as of 2026-04-23 AM:
 * G_ZEXT s1→s8, G_SELECT on phantom cond, and G_PHI of phantom
 * values. All three PASS post-commit `6f67d3b37a5e` (bug #152 phase
 * 2+3).
 *
 * Without the fix: 6 of 8 cases FAIL (empirically verified during
 * diagnosis). With the fix: all 8 PASS.
 *
 * Runs under picolibc's libc-testsuite harness. Returns 0 on full
 * pass; non-zero = count of failed probes.
 */

#include <stdio.h>
#include <stdint.h>

volatile int sink;

/* Audit #3: G_ZEXT s1→s8 on phantom-BIT1.
 * Expected: (unsigned char)__builtin_sadd_overflow(127,0,...) == 0,
 * (unsigned char)__builtin_sadd_overflow(INT16_MAX,1,...) == 1. */
__attribute__((noinline, optnone))
unsigned char probe_zext(int a, int b, int *out) {
    int ovf = __builtin_sadd_overflow(a, b, out);
    return (unsigned char)ovf;
}

/* Audit #9: G_PHI on phantom-BIT1. */
__attribute__((noinline, optnone))
int probe_phi(int a, int b, int cond) {
    int x;
    int ovf;
    if (cond) {
        ovf = __builtin_sadd_overflow(a, b, &x);
    } else {
        ovf = __builtin_sadd_overflow(a, 0, &x);
    }
    return ovf;
}

/* Audit #6: G_SELECT cond from phantom-BIT1. */
__attribute__((noinline, optnone))
int probe_select(int a, int b, int tv, int fv) {
    int x;
    int ovf = __builtin_sadd_overflow(a, b, &x);
    return ovf ? tv : fv;
}

static int check(const char *name, int got, int want) {
    if (got == want) return 0;
    printf("phantom-bit1: %s got=%d want=%d FAIL\n", name, got, want);
    return 1;
}

int main(void) {
    int out;
    int fails = 0;

    fails += check("zext(127,0)",   probe_zext(127, 0, &out),   0);
    fails += check("zext(MAX,1)",   probe_zext(32767, 1, &out), 1);

    fails += check("phi(127,0,T)",  probe_phi(127, 0, 1), 0);
    fails += check("phi(127,0,F)",  probe_phi(127, 0, 0), 0);
    fails += check("phi(MAX,1,T)",  probe_phi(32767, 1, 1), 1);
    fails += check("phi(MAX,1,F)",  probe_phi(32767, 1, 0), 0);

    fails += check("sel(127,0)",    probe_select(127, 0, 10, 20),   20);
    fails += check("sel(MAX,1)",    probe_select(32767, 1, 10, 20), 10);

    if (fails)
        printf("phantom-bit1 test failed, %d error(s)\n", fails);
    else
        printf("phantom-bit1 test passed\n");
    return fails;
}
