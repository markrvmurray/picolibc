// SPDX-License-Identifier: BSD-3-Clause
// Copyright © 2026 Mark Murray
//
// Bug #161 Phase 3 sentinel: HD6309 DIVD-based __udivhi3 / __umodhi3
// fast path. Validates corner cases where the long-division-by-DIVD
// scheme might misbehave, especially the boundary around the
// signed-DIVD safety constraint (divisor must be in [2, 127] for the
// fast path; divisor ∈ {0, 1, 128..65535} flows through the original
// 6809 16-iteration loop or the divisor==1 short-circuit).

#include <stdint.h>
#include <stdio.h>

volatile uint16_t in_a, in_b;
volatile int16_t in_sa, in_sb;
volatile uint32_t in_a32, in_b32;

#define ARR(x) (sizeof(x) / sizeof((x)[0]))

struct caseu16 {
    uint16_t a, b, q, r;
};

static const struct caseu16 cases[] = {
    /* divisor==1: short-circuit return */
    {0, 1, 0, 0},
    {1, 1, 1, 0},
    {65535, 1, 65535, 0},
    /* divisor in fast-path range [2, 127] — the bit we actually care about */
    {0, 2, 0, 0},
    {1, 2, 0, 1},
    {2, 2, 1, 0},
    {255, 2, 127, 1},                 /* 8-bit overflow boundary */
    {256, 2, 128, 0},                 /* dividend.hi != 0 */
    {65535, 2, 32767, 1},
    {65535, 3, 21845, 0},
    {65534, 3, 21844, 2},
    {1000, 10, 100, 0},               /* common decimal extraction */
    {12345, 10, 1234, 5},
    {65535, 10, 6553, 5},
    {65535, 100, 655, 35},
    {65535, 127, 516, 3},             /* upper-fast-path boundary */
    {32768, 127, 258, 2},
    /* divisor == 128: first divisor that bypasses fast path (signed-DIVD bound) */
    {32768, 128, 256, 0},
    {65535, 128, 511, 127},
    /* divisor > 255: high byte non-zero, skip fast path */
    {65535, 256, 255, 255},
    {65535, 1000, 65, 535},
    {12345, 6789, 1, 5556},
    {65535, 65535, 1, 0},
    {65534, 65535, 0, 65534},
    /* divisor == 0 (undefined): we don't test, just don't crash */
};

struct caseu32 {
    uint32_t a, b, q, r;
};

static const struct caseu32 cases32[] = {
    /* divisor==1 */
    {0u, 1u, 0u, 0u},
    {0xFFFFFFFFu, 1u, 0xFFFFFFFFu, 0u},
    /* fast path: divisor in [2, 0x7FFF] */
    {0x12345678u, 0x0000000Au, 0x01D208A5u, 0x00000006u},
    {0xFFFFFFFFu, 0x0000000Au, 0x19999999u, 0x00000005u},
    {0xFFFFFFFFu, 0x00000064u, 0x028F5C28u, 0x0000005Fu},
    {0xFFFFFFFFu, 0x000003E8u, 0x00418937u, 0x00000127u},
    {0xFFFFFFFFu, 0x00007FFFu, 0x00020004u, 0x00000003u}, /* upper fast-path boundary */
    {0x80000000u, 0x00007FFFu, 0x00010002u, 0x00000002u},
    {0x12345678u, 0x00001234u, 0x00010004u, 0x00000DA8u},
    /* divisor == 0x8000: first that bypasses fast path */
    {0x80000000u, 0x00008000u, 0x00010000u, 0x00000000u},
    {0xFFFFFFFFu, 0x00008000u, 0x0001FFFFu, 0x00007FFFu},
    /* divisor with high half nonzero: bypass fast path */
    {0xFFFFFFFFu, 0x00010000u, 0x0000FFFFu, 0x0000FFFFu},
    {0xFFFFFFFFu, 0xFFFFFFFFu, 0x00000001u, 0x00000000u},
    {0x12345678u, 0xFEDCBA98u, 0x00000000u, 0x12345678u},
};

int main(void)
{
    int errs = 0;

    for (unsigned i = 0; i < ARR(cases); i++) {
        in_a = cases[i].a;
        in_b = cases[i].b;
        uint16_t q = in_a / in_b;
        uint16_t r = in_a % in_b;
        if (q != cases[i].q || r != cases[i].r) {
            printf("DIVHI FAIL [%u]: %u / %u = q=%u r=%u (expect q=%u r=%u)\n",
                   i, cases[i].a, cases[i].b, q, r, cases[i].q, cases[i].r);
            errs++;
        }
    }

    for (unsigned i = 0; i < ARR(cases32); i++) {
        in_a32 = cases32[i].a;
        in_b32 = cases32[i].b;
        uint32_t q = in_a32 / in_b32;
        uint32_t r = in_a32 % in_b32;
        if (q != cases32[i].q || r != cases32[i].r) {
            printf("DIVSI FAIL [%u]: 0x%08lx / 0x%08lx = q=0x%08lx r=0x%08lx (expect q=0x%08lx r=0x%08lx)\n",
                   i,
                   (unsigned long)cases32[i].a, (unsigned long)cases32[i].b,
                   (unsigned long)q, (unsigned long)r,
                   (unsigned long)cases32[i].q, (unsigned long)cases32[i].r);
            errs++;
        }
    }

    /* ---- signed i16 ---- */
    struct { int16_t a, b, q, r; } scases[] = {
        /* divisor == 1 */
        {1, 1, 1, 0},
        {-1, 1, -1, 0},
        {127, 1, 127, 0},
        {-32767, 1, -32767, 0},
        /* divisor == -1 */
        {1, -1, -1, 0},
        {-1, -1, 1, 0},
        {127, -1, -127, 0},
        /* fast path: positive divisor ∈ [2, 127] */
        {100, 10, 10, 0},
        {-100, 10, -10, 0},
        {100, -10, -10, 0},
        {-100, -10, 10, 0},
        {32767, 127, 258, 1},
        {-32767, 127, -258, -1},
        {32767, 2, 16383, 1},
        {-32768, 2, -16384, 0},
        /* fast path: negative divisor ∈ [-2, -127] */
        {100, -7, -14, 2},
        {-100, -7, 14, -2},
        /* slow path: |b| ≥ 128 */
        {10000, 200, 50, 0},
        {-10000, 200, -50, 0},
        /* slow path: |b| ≥ 256 */
        {32767, 1000, 32, 767},
        {-32767, 1000, -32, -767},
    };
    for (unsigned i = 0; i < ARR(scases); i++) {
        in_sa = scases[i].a;
        in_sb = scases[i].b;
        int16_t q = in_sa / in_sb;
        int16_t r = in_sa % in_sb;
        if (q != scases[i].q || r != scases[i].r) {
            printf("DIVHI SIGNED FAIL [%u]: %d / %d = q=%d r=%d (expect q=%d r=%d)\n",
                   i, scases[i].a, scases[i].b, q, r, scases[i].q, scases[i].r);
            errs++;
        }
    }

    if (errs == 0)
        printf("OK: __udivhi3 / __umodhi3 / __divhi3 / __modhi3 / __udivsi3 / __umodsi3 corner cases pass (%u u16, %u i16, %u u32)\n",
               (unsigned)ARR(cases), (unsigned)ARR(scases), (unsigned)ARR(cases32));
    return errs;
}
