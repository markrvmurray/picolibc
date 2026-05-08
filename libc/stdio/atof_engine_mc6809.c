/* Bug #230: MC6839 DECBIN-backed __atof_engine.
 *
 * Replaces the multi-precision software path (libc/stdio/dtoa.c +
 * the iterative atof_engine.c) with a direct call to the MC6839 ROM's
 * DECBIN operation ($22). The ROM does the heavy floating-point math
 * in microcode; we just translate (mantissa, exp) into the 26-byte
 * BCD format DECBIN consumes.
 *
 * BCD layout (see /Users/markmurray/Documents/MC6839_ROM/abi/templates/README.md):
 *
 *   byte 0:    se   sign of exponent (00 = E >= 0, 0F = E < 0; 00 also
 *                   used for the value zero with E=0)
 *   bytes 1-4: 4-digit BCD |E|, MSB at byte 1
 *   byte 5:    sf   sign of fraction (00 = +, 0F = -)
 *   bytes 6-24: 19-digit BCD fraction (1 digit/byte, MSB at byte 6).
 *               BINDEC right-aligns k significant digits into the last k
 *               positions; we follow the same convention so DECBIN
 *               interprets our digits as a 19-digit integer that scales
 *               by 10^E.
 *   byte 25:   p    decimal-point position (0 = pure I*10^E form)
 *
 * Caller contract: m10 is the parsed positive mantissa (up to 9
 * decimal digits for f32), e10 is the signed base-10 exponent. Caller
 * (conv_flt.c) already pre-screened for inf/nan/zero/underflow/overflow
 * and applied the sign post-call, so we only handle finite positive
 * inputs.
 */

#define _NEED_IO_FLOAT32
#include <stdint.h>
#include "dtoa.h"

extern float __mc6839_decbin_f32(const uint8_t *bcd);

float
__atof_engine(uint32_t m10, int e10)
{
    uint8_t bcd[26] = { 0 };

    /* Build a 9-digit BCD representation of m10, right-aligned in
     * fraction bytes 16..24 (last 9 of the 19-digit fraction field).
     * Bytes 6..15 stay zero (set by initialiser). */
    for (int i = 24; i >= 16; i--) {
        bcd[i] = (uint8_t)(m10 % 10);
        m10 /= 10;
    }

    /* Encode signed E: |E| into bytes 1-4 as 4-digit BCD; se in byte 0. */
    int absE = e10 < 0 ? -e10 : e10;
    bcd[0] = (e10 < 0) ? 0x0F : 0x00;
    bcd[1] = (uint8_t)((absE / 1000) % 10);
    bcd[2] = (uint8_t)((absE /  100) % 10);
    bcd[3] = (uint8_t)((absE /   10) % 10);
    bcd[4] = (uint8_t)( absE         % 10);

    /* sf and p stay zero (positive fraction; pure I*10^E form). */

    return __mc6839_decbin_f32(bcd);
}
