/*
 * strtod() correctness value probe.
 *
 * The libc-testsuite strtod round-trip test is skipped on MC6809: it formats
 * with snprintf("%.300f"), which needs the double vfprintf variant that
 * overflows the 64K image. strtod() itself is fine, so guard it directly here
 * with fixed decimal / hex-float / long-mantissa inputs compared against
 * double literals. All checks use ==, and output is integer-only, so this
 * needs no float printf and fits at every level.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int fails = 0;
static void chk(int id, double got, double want) {
    if (got != want) {
        fails++;
        printf("FAIL case %d\n", id);
    }
}

int main(void) {
    char buf[400];

    chk(1,  strtod("16.0", 0),            16.0);
    chk(2,  strtod("0x1p4", 0),           16.0);
    chk(3,  strtod("0x1.1p4", 0),         17.0);
    chk(4,  strtod("1.5", 0),             1.5);
    chk(5,  strtod("0.5", 0),             0.5);
    chk(6,  strtod("0.25", 0),            0.25);
    chk(7,  strtod("100.0", 0),           100.0);
    chk(8,  strtod("0.001", 0),           0.001);
    chk(9,  strtod("123456.789", 0),      123456.789);
    chk(10, strtod("3.14159265358979", 0),3.14159265358979);
    chk(11, strtod("-2.5", 0),            -2.5);
    chk(12, strtod("1e10", 0),            1e10);
    chk(13, strtod("1.5e-3", 0),          1.5e-3);

    /* long-mantissa path: "0.5" followed by 299 zeros must still be exactly 0.5 */
    buf[0] = '0'; buf[1] = '.'; buf[2] = '5';
    memset(buf + 3, '0', 299); buf[302] = 0;
    chk(14, strtod(buf, 0), 0.5);

    /* "1." + 300 zeros == 1.0 */
    buf[0] = '1'; buf[1] = '.';
    memset(buf + 2, '0', 300); buf[302] = 0;
    chk(15, strtod(buf, 0), 1.0);

    if (fails) {
        printf("FAILS=%d\n", fails);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
