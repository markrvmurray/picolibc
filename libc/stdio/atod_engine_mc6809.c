/* Bug #230: MC6839 DECBIN-backed __atod_engine. (See atof_engine_mc6809.c
 * for narrative; this is the f64 sibling.) */

#define _NEED_IO_FLOAT64
#include <stdint.h>
#include "dtoa.h"

extern FLOAT64 __mc6839_decbin_f64(const uint8_t *bcd);

FLOAT64
__atod_engine(uint64_t m10, int e10)
{
    uint8_t bcd[26] = { 0 };

    /* m10 may be up to 19 decimal digits; place right-aligned in
     * fraction bytes 6..24. */
    for (int i = 24; i >= 6; i--) {
        bcd[i] = (uint8_t)(m10 % 10);
        m10 /= 10;
    }

    int absE = e10 < 0 ? -e10 : e10;
    bcd[0] = (e10 < 0) ? 0x0F : 0x00;
    bcd[1] = (uint8_t)((absE / 1000) % 10);
    bcd[2] = (uint8_t)((absE /  100) % 10);
    bcd[3] = (uint8_t)((absE /   10) % 10);
    bcd[4] = (uint8_t)( absE         % 10);

    return __mc6839_decbin_f64(bcd);
}
