/* Bug #247 narrowed: probe MULD #-60 path from gmtime_r's tm_sec calc. */
#include <stdio.h>

__attribute__((noinline))
static int sec_calc(int x) {
    short v = (short)x;
    short q = v / 60;
    short neg3 = q * (short)-60;
    return (int)(short)(neg3 + v);
}

__attribute__((noinline))
static int neg60(int q) {
    return (int)(short)((short)q * (short)-60);
}

int main(void) {
    int fail = 0;
    /* Direct MULD: 29 * -60 = -1740, low 16 = 0xF934 */
    int n = neg60(29);
    printf("29*-60 (low16) = %d, want -1740\n", n);
    if (n != -1740) fail++;

    /* sec_calc: x mod 60 via mul-back */
    int v;
    v = sec_calc(1783); printf("sec_calc(1783)=%d want 43\n", v); if (v != 43) fail++;
    v = sec_calc(60);   printf("sec_calc(60)=%d want 0\n", v);   if (v != 0)  fail++;
    v = sec_calc(120);  printf("sec_calc(120)=%d want 0\n", v);  if (v != 0)  fail++;
    v = sec_calc(123);  printf("sec_calc(123)=%d want 3\n", v);  if (v != 3)  fail++;

    if (!fail) printf("PASS\n");
    return fail;
}
