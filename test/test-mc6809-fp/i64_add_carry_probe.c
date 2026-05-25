/*
 * Bug #311 follow-up: focused probe for the i64-add-narrowed-to-2x-i32
 * carry chain on HD6309.
 *
 * Tests the exact pattern picolibc's random() uses:
 *
 *   _rand_next = _rand_next * 6364136223846793005ULL + 1;
 *
 * With Phase 2 active, the i64 add narrows to two i32 sub-chains:
 *   - LOW i32: ADDW #1 + ADCD #0
 *   - HIGH i32: EXG D,W + ADCB #0 + ADCA #0 + EXG D,W + ADCD #0
 *
 * Between them, the post-RA expansion materialises the HIGH i32 via
 * LDQ.  Per HD6309 datasheet LDQ leaves CC.C unaffected, so the carry
 * from the LOW half's ADCD should survive across LDQ and feed the
 * HIGH half's ADCB.  If MAME's emulator (or our codegen) doesn't
 * preserve that carry, the HIGH i32 ends up off-by-1 whenever the
 * LOW i32 overflows.
 *
 * Test value: 1 * 6364136223846793005 + 1 = 6364136223846793006
 *                                         = 0x5851F42D 4C957F2E.
 *
 * Exit code:
 *   0 — exact match (codegen + emulator both correct)
 *   1 — full result wrong (suggests mul or add broken end-to-end)
 *   2 — HIGH i32 wrong only (the suspect carry-chain breakage)
 *   3 — LOW i32 wrong only (suggests mul is broken, not the add)
 */

#include <stdint.h>

__attribute__((noinline))
static uint64_t do_mul(uint64_t x, uint64_t y) {
    return x * y;
}

__attribute__((noinline))
static uint64_t do_add(uint64_t a, uint64_t b) {
    return a + b;
}

__attribute__((noinline))
static uint64_t mul_add_combined(uint64_t x, uint64_t y, uint64_t z) {
    return x * y + z;
}

/* volatile keeps the optimiser from constant-folding the inputs.
 * Forces runtime __muldi3 and runtime i64 add. */
volatile uint64_t v_x        = 1ULL;
volatile uint64_t v_y        = 6364136223846793005ULL;
volatile uint64_t v_z        = 1ULL;
volatile uint64_t v_mul_exp  = 0x5851F42D4C957F2DULL;     /* 1 * 6364136223846793005 */
volatile uint64_t v_expected = 0x5851F42D4C957F2EULL;     /* mul + 1 */

int main(void) {
    /* Isolate the multiply first. */
    uint64_t mul_result = do_mul(v_x, v_y);
    if (mul_result != v_mul_exp) {
        uint32_t got_hi = (uint32_t)(mul_result >> 32);
        uint32_t got_lo = (uint32_t)(mul_result & 0xFFFFFFFFUL);
        uint32_t want_hi = (uint32_t)(v_mul_exp >> 32);
        uint32_t want_lo = (uint32_t)(v_mul_exp & 0xFFFFFFFFUL);
        if (got_hi != want_hi && got_lo != want_lo) return 10;
        if (got_hi != want_hi) return 11;       /* mul HIGH i32 wrong */
        return 12;                              /* mul LOW i32 wrong */
    }

    /* Multiply OK — now test the i64 add. */
    uint64_t add_result = do_add(mul_result, v_z);
    if (add_result != v_expected) {
        uint32_t got_hi  = (uint32_t)(add_result   >> 32);
        uint32_t got_lo  = (uint32_t)(add_result   & 0xFFFFFFFFUL);
        uint32_t want_hi = (uint32_t)(v_expected   >> 32);
        uint32_t want_lo = (uint32_t)(v_expected   & 0xFFFFFFFFUL);
        if (got_hi != want_hi && got_lo != want_lo) return 20;
        if (got_hi != want_hi) return 21;       /* add HIGH i32 wrong */
        return 22;                              /* add LOW i32 wrong */
    }

    /* Cross-function pair OK — now exercise INTRA-function mul-then-add
     * (this is what `_rand_next = next * mul + 1` lowers to). */
    uint64_t result = mul_add_combined(v_x, v_y, v_z);
    uint64_t expected = v_expected;

    if (result == expected)
        return 0;
    uint32_t got_hi  = (uint32_t)(result   >> 32);
    uint32_t got_lo  = (uint32_t)(result   & 0xFFFFFFFFUL);
    uint32_t want_hi = (uint32_t)(expected >> 32);
    uint32_t want_lo = (uint32_t)(expected & 0xFFFFFFFFUL);
    if (got_hi != want_hi && got_lo != want_lo)
        return 1;
    if (got_hi != want_hi)
        return 2;
    return 3;
}
