/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * tan() just below pi/2, where the result is about -1.6e6 and depends on
 * the low bits of the argument reduction. fdlibm's __rem_pio2 keeps the
 * reduction's rounding error in a second word, `(z - y[0]) - pio2_1t`; a
 * library built with -ffast-math let the compiler fold that word to 0 and
 * this came out 9999 ulp off. The library is now always built with IEEE
 * arithmetic, and this checks the exact result at every level, including
 * the fast-math ones where test-math's test-tan no longer fits.
 */

#include <math.h>
#include <stdint.h>
#include <stdio.h>
#include <string.h>

int
main(void)
{
    static const uint64_t arg = 0x3ff921fc00000000ULL;
    static const uint64_t want = 0xc137d9d370b14023ULL;
    volatile double x;
    double r;
    uint64_t got;

    memcpy((void *)&x, &arg, sizeof(x));
    r = tan(x);
    memcpy(&got, &r, sizeof(got));
    if (got != want) {
        printf("tan(%08lx%08lx) = %08lx%08lx, not %08lx%08lx\n",
               (unsigned long)(arg >> 32), (unsigned long)arg,
               (unsigned long)(got >> 32), (unsigned long)got,
               (unsigned long)(want >> 32), (unsigned long)want);
        return 1;
    }
    printf("tan near pi/2 ok\n");
    return 0;
}
