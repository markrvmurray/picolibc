// SPDX-License-Identifier: BSD-3-Clause
// Copyright © 2026 Mark Murray
//
// Bug #161 Phase 1 / 1.5 sentinel: corner-case validation of the
// HD6309-MULD-based __mulsi3 libcall in compiler-rt's MC6809 builtins.
//
// The sentinel exercises only the i32 (__mulsi3) path because the
// HD6309 i16 multiply is INLINED by the backend, not routed through
// __mulhi3. The inline i16 path has a separate pre-existing bug
// (filed as a sibling ticket): the inline emission produces
// `PSHS (empty postbyte 0x00) ; MULD ,S++` where it should be
// `PSHS D ; MULD ,S++`. Sentinel coverage of __mulhi3 itself awaits
// a fix to that backend bug; in the meantime we verify __mulsi3's
// corner cases independently because that's what Phase 1 actually
// touched.
//
// __mulsi3 critical path on HD6309 (per Phase 1 commit message): one
// signed MULD for the full a_lo*b_lo product, plus an unsigned
// correction step (TST + conditional ADDD) to recover unsigned i32
// semantics, plus two cross-term low-16-of-MULD additions. The
// correction step is the bug-prone bit; the cases below are chosen
// to stress operands with bit-15 of either or both halves set.

#include <stdint.h>
#include <stdio.h>

volatile uint32_t in_a32, in_b32;

#define ARR(x) (sizeof(x) / sizeof((x)[0]))

struct case32 {
    uint32_t a, b, expect;
};

static const struct case32 c32[] = {
    /* baseline */
    {0u, 0u, 0u},
    {1u, 1u, 1u},
    {0xFFFFFFFFu, 1u, 0xFFFFFFFFu},
    {0x10000u, 0x10000u, 0u},                  /* 2^32 trunc → 0 */
    /* bit-15 in low halves — exercises the unsigned correction */
    {0x0000FFFFu, 0x0000FFFFu, 0xFFFE0001u},
    {0xFFFFFFFFu, 0xFFFFFFFFu, 1u},            /* (-1)*(-1) trunc → 1 */
    {0xFFFFFFFFu, 2u, 0xFFFFFFFEu},            /* -2 */
    {0x00008000u, 0x00008000u, 0x40000000u},
    {0x00008000u, 0x00010000u, 0x80000000u},
    /* the strtol regression cases that drove the correction */
    {0xAE14u, 10u, 0x0006CCC8u},
    {0x0CCCCCCCu, 10u, 0x7FFFFFF8u},
    /* realistic mixed-bit values */
    {0xCAFEBABEu, 0xDEADBEEFu, (uint32_t)(0xCAFEBABEu * 0xDEADBEEFu)},
    {0x12345678u, 0x9ABCDEF0u, (uint32_t)(0x12345678u * 0x9ABCDEF0u)},
    {0xCCCC0000u, 2u, 0x99980000u},
    {0x80000000u, 2u, 0u},                     /* sign overflow → 0 */
    {0x80008000u, 0x00010001u,
     (uint32_t)(0x80008000u * 0x00010001u)},
    {0x12340000u, 0x00005678u,
     (uint32_t)(0x12340000u * 0x00005678u)},
    {0xABCD1234u, 0x00010001u,
     (uint32_t)(0xABCD1234u * 0x00010001u)},
};

int main(void)
{
    int errs = 0;

    for (unsigned i = 0; i < ARR(c32); i++) {
        in_a32 = c32[i].a;
        in_b32 = c32[i].b;
        uint32_t got = in_a32 * in_b32;
        if (got != c32[i].expect) {
            printf("MUL32 FAIL [%u]: 0x%08lx * 0x%08lx = 0x%08lx (expect 0x%08lx)\n",
                   i,
                   (unsigned long)c32[i].a,
                   (unsigned long)c32[i].b,
                   (unsigned long)got,
                   (unsigned long)c32[i].expect);
            errs++;
        }
    }

    if (errs == 0)
        printf("OK: __mulsi3 corner cases pass (%u i32)\n",
               (unsigned)ARR(c32));
    return errs;
}
