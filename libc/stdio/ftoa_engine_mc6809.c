/* Bug #230: MC6839 BINDEC-backed __ftoa_engine.
 *
 * Replaces picolibc's iterative software multi-precision path with a
 * single ROM call. BINDEC ($1C) takes (k, value, FPCB) and writes a
 * 26-byte BCD string holding `k` significant digits + signed decimal
 * exponent. We translate the BCD into picolibc's `struct dtoa`.
 *
 * Picolibc's `dtoa->exp` is the decimal exponent of the LEFTMOST
 * digit. The BCD's `E` (in se+exp-field) is such that
 * value = (digits as 19-digit integer) * 10^E. Right-aligning k
 * digits into the last k positions of the fraction gives a 19-digit
 * integer = (k-digit integer) * 10^0, so value = digits * 10^E and
 * the leftmost-digit exponent is E + (k-1).
 */

#define _NEED_IO_FLOAT32
#include <stdint.h>
#include <stdbool.h>
#include "dtoa.h"

extern void __mc6839_bindec_f32(uint8_t *bcd, int k, float val);

int
__ftoa_engine(uint32_t val, struct dtoa *ftoa, int maxDigits, bool fmode, int maxDecimals)
{
    (void)fmode;
    (void)maxDecimals;  /* vfprintf trims trailing decimals after engine returns */

    uint8_t flags = 0;

    if (maxDigits > FTOA_MAX_DIG)
        maxDigits = FTOA_MAX_DIG;
    if (maxDigits < 1)
        maxDigits = 1;

    /* Sign + special-value detection from IEEE bits */
    if (val & ((uint32_t)1 << 31))
        flags = DTOA_MINUS;

    uint8_t exp = (uint8_t)(val >> 23);
    uint32_t frac = val & 0x007FFFFFU;

    ftoa->exp = 0;

    if (exp == 0 && frac == 0) {
        flags |= DTOA_ZERO;
        ftoa->digits[0] = '0';
        ftoa->flags = flags;
        return 1;
    }
    if (exp == 0xFF) {
        flags |= (frac == 0) ? DTOA_INF : DTOA_NAN;
        ftoa->flags = flags;
        return 0;
    }

    /* Reconstruct the float and call BINDEC with the absolute value.
     * Sign already captured in flags. */
    union { uint32_t u; float f; } u;
    u.u = val;
    float fv = u.f;
    if (flags & DTOA_MINUS)
        fv = -fv;

    uint8_t bcd[26];
    __mc6839_bindec_f32(bcd, maxDigits, fv);

    /* Decode BCD: |E| from bytes 1-4 (4-digit BCD MSB-first), sign from byte 0 */
    int eMag = bcd[1] * 1000 + bcd[2] * 100 + bcd[3] * 10 + bcd[4];
    int E = (bcd[0] == 0x0F) ? -eMag : eMag;

    /* Copy k digits from the right-aligned position in the fraction field */
    int start = 6 + (19 - maxDigits);
    for (int i = 0; i < maxDigits; i++)
        ftoa->digits[i] = (char)(bcd[start + i] + '0');

    ftoa->exp = E + maxDigits - 1;
    ftoa->flags = flags;
    return maxDigits;
}
