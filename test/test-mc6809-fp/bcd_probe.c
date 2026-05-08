/* Bug #230 smoke probe: dump BINDEC output for known values; verify
 * BCD layout against the agent's spec before building the engines.
 *
 * Layout per /Users/markmurray/Documents/MC6839_ROM/abi/templates/README.md
 * and the manual:
 *   byte 0:    se (sign-of-exponent: $00=+, $0F=-, $0A=+inf, $0B=-inf, $0C=NaN)
 *   bytes 1-4: 4-digit BCD exponent (MSB at 1)
 *   byte 5:    sf (sign-of-fraction: $00=+, $0F=-)
 *   bytes 6-24: 19-digit BCD fraction (MSB at 6)
 *   byte 25:   p (decimal-point position)
 */
#include <stdio.h>
#include <stdint.h>
#include <string.h>

extern void   __mc6839_bindec_f64(uint8_t *bcd, int k, double val);
extern void   __mc6839_bindec_f32(uint8_t *bcd, int k, float val);
extern double __mc6839_decbin_f64(const uint8_t *bcd);
extern float  __mc6839_decbin_f32(const uint8_t *bcd);

static void dump(const char *name, const uint8_t *bcd) {
    printf("%-22s = se=%02x exp=%x%x%x%x sf=%02x frac=", name,
           bcd[0], bcd[1],bcd[2],bcd[3],bcd[4], bcd[5]);
    for (int i = 6; i <= 24; i++) printf("%x", bcd[i] & 0x0F);
    printf(" p=%d\n", bcd[25]);
}

int main(void) {
    uint8_t bcd[26];
    /* test: BINDEC of 1.0, 2.0, 3.14159, 0.0 */
    volatile double d;

    memset(bcd, 0xCC, 26);
    d = 1.0;
    __mc6839_bindec_f64(bcd, 17, d);
    dump("bindec(1.0, 17)", bcd);

    memset(bcd, 0xCC, 26);
    d = 2.0;
    __mc6839_bindec_f64(bcd, 17, d);
    dump("bindec(2.0, 17)", bcd);

    memset(bcd, 0xCC, 26);
    d = 3.14159;
    __mc6839_bindec_f64(bcd, 6, d);
    dump("bindec(3.14159, 6)", bcd);

    memset(bcd, 0xCC, 26);
    d = 0.0;
    __mc6839_bindec_f64(bcd, 17, d);
    dump("bindec(0.0, 17)", bcd);

    memset(bcd, 0xCC, 26);
    d = -1.5;
    __mc6839_bindec_f64(bcd, 17, d);
    dump("bindec(-1.5, 17)", bcd);

    /* float variants */
    memset(bcd, 0xCC, 26);
    volatile float f = 1.0f;
    __mc6839_bindec_f32(bcd, 9, f);
    dump("bindec_f32(1.0, 9)", bcd);

    memset(bcd, 0xCC, 26);
    f = 0.5f;
    __mc6839_bindec_f32(bcd, 9, f);
    dump("bindec_f32(0.5, 9)", bcd);

    /* edge: very small value with k=1 */
    memset(bcd, 0xCC, 26);
    f = 1.23456e-4f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(1.23e-4,1)", bcd);
    memset(bcd, 0xCC, 26);
    __mc6839_bindec_f32(bcd, 2, f);
    dump("bindec_f32(1.23e-4,2)", bcd);
    memset(bcd, 0xCC, 26);
    __mc6839_bindec_f32(bcd, 3, f);
    dump("bindec_f32(1.23e-4,3)", bcd);
    memset(bcd, 0xCC, 26);
    f = 1.5f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(1.5,1)", bcd);
    memset(bcd, 0xCC, 26);
    f = 9.5f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(9.5,1)", bcd);
    memset(bcd, 0xCC, 26);
    f = 12345.0f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(12345,1)", bcd);
    memset(bcd, 0xCC, 26);
    f = 1.0e-4f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(1.0e-4,1)", bcd);
    memset(bcd, 0xCC, 26);
    f = 5.0e-4f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(5.0e-4,1)", bcd);
    memset(bcd, 0xCC, 26);
    f = 0.5e-4f;
    __mc6839_bindec_f32(bcd, 1, f);
    dump("bindec_f32(0.5e-4,1)", bcd);

    /* round-trip: BINDEC then DECBIN, expect bit-equivalence */
    volatile double in = 1.5;
    uint8_t b[26];
    __mc6839_bindec_f64(b, 17, in);
    double out = __mc6839_decbin_f64(b);
    uint64_t in_bits, out_bits;
    memcpy(&in_bits, (const void*)&in, 8);
    memcpy(&out_bits, &out, 8);
    printf("roundtrip 1.5: in_bits=%08lx_%08lx out_bits=%08lx_%08lx %s\n",
           (unsigned long)(in_bits>>32), (unsigned long)in_bits,
           (unsigned long)(out_bits>>32), (unsigned long)out_bits,
           in_bits == out_bits ? "OK" : "MISMATCH");
    return 0;
}
