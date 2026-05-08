/* Bug #230: MC6839 BINDEC-backed __dtoa_engine. (See ftoa_engine_mc6809.c
 * for narrative; this is the f64 sibling.) */

#define _NEED_IO_FLOAT64
#include <stdint.h>
#include <stdbool.h>
#include "dtoa.h"

extern void __mc6839_bindec_f64(uint8_t *bcd, int k, FLOAT64 val);

int
__dtoa_engine(uint64_t val, struct dtoa *dtoa, int maxDigits, bool fmode, int maxDecimals)
{
    (void)fmode;
    (void)maxDecimals;

    uint8_t flags = 0;

    if (maxDigits > DTOA_MAX_DIG)
        maxDigits = DTOA_MAX_DIG;
    if (maxDigits < 1)
        maxDigits = 1;

    if (val & ((uint64_t)1 << 63))
        flags = DTOA_MINUS;

    uint16_t exp = (uint16_t)((val >> 52) & 0x7FFU);
    uint64_t frac = val & 0x000FFFFFFFFFFFFFULL;

    dtoa->exp = 0;

    if (exp == 0 && frac == 0) {
        flags |= DTOA_ZERO;
        dtoa->digits[0] = '0';
        dtoa->flags = flags;
        return 1;
    }
    if (exp == 0x7FF) {
        flags |= (frac == 0) ? DTOA_INF : DTOA_NAN;
        dtoa->flags = flags;
        return 0;
    }

    union { uint64_t u; FLOAT64 f; } u;
    u.u = val;
    FLOAT64 fv = u.f;
    if (flags & DTOA_MINUS)
        fv = -fv;

    uint8_t bcd[26];
    __mc6839_bindec_f64(bcd, maxDigits, fv);

    int eMag = bcd[1] * 1000 + bcd[2] * 100 + bcd[3] * 10 + bcd[4];
    int E = (bcd[0] == 0x0F) ? -eMag : eMag;

    int start = 6 + (19 - maxDigits);
    for (int i = 0; i < maxDigits; i++)
        dtoa->digits[i] = (char)(bcd[start + i] + '0');

    dtoa->exp = E + maxDigits - 1;
    dtoa->flags = flags;
    return maxDigits;
}
