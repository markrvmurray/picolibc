/*
 * Bug #192: DP and non-DP globals in the same TU. Verifies the
 * sections live in distinct address ranges (DP at $00xx, non-DP
 * elsewhere) and that mutating one does not corrupt the other.
 */
#include <stdio.h>
#include <stdint.h>

static __directpage uint8_t dp_a = 0x11;
static __directpage uint8_t dp_b = 0x22;
static uint8_t                 ram_a = 0x33;     /* normal .bss/.data */
static uint8_t                 ram_b = 0x44;
static __directpage uint16_t dp_w = 0x5566;
static uint16_t                ram_w = 0x7788;

/*
 * Note: this test does NOT take addresses of the __directpage
 * globals. v1 (per the AttrDocs entry) does not support pointer
 * arithmetic on __directpage pointers — so `&dp_a`, `(uintptr_t)&dp_a`,
 * etc. would generate G_PTRTOINT on a p1 pointer, which the backend
 * emits as a SWI3 placeholder trap. Address-range verification is
 * done at link time by inspecting `llvm-readelf` output instead;
 * this test verifies the runtime value-independence invariant.
 */
int main(void)
{
    /* Initial state intact? */
    if (dp_a != 0x11 || dp_b != 0x22 || ram_a != 0x33 || ram_b != 0x44 ||
        dp_w != 0x5566 || ram_w != 0x7788) {
        printf("FAIL initial\n");
        return 1;
    }

    /* Mutate DP, check non-DP intact; mutate non-DP, check DP intact. */
    dp_a = 0xDD;
    if (ram_a != 0x33 || ram_b != 0x44 || ram_w != 0x7788) {
        printf("FAIL non-DP corrupted by DP write\n");
        return 1;
    }
    ram_a = 0xCC;
    if (dp_a != 0xDD || dp_b != 0x22 || dp_w != 0x5566) {
        printf("FAIL DP corrupted by non-DP write\n");
        return 1;
    }

    /* Reverse: mutate ram_w (16-bit non-DP) and dp_w (16-bit DP);
     * verify they don't alias. */
    dp_w = 0x1111;
    if (ram_w != 0x7788) {
        printf("FAIL ram_w (16-bit non-DP) corrupted by dp_w write\n");
        return 1;
    }
    ram_w = 0x2222;
    if (dp_w != 0x1111) {
        printf("FAIL dp_w corrupted by ram_w write\n");
        return 1;
    }
    printf("PASS\n");
    return 0;
}
