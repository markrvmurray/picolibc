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
 *
 * fmode handling: when fmode is true, the caller (vfprintf for `%f`,
 * fcvt_r/ecvt_r for similar) wants exactly `maxDecimals` digits to
 * the right of the decimal point. The total digit count is then
 * `decpt + maxDecimals` where decpt = exp10 + 1 (digits-before-decimal).
 * We compute exp10 with a first BINDEC call at full precision, adjust
 * maxDigits, then call BINDEC again for the digits, rounding the last
 * one off here (see the final pass for why).
 */

#define _NEED_IO_FLOAT32
#include <stdint.h>
#include <stdbool.h>
#include "dtoa.h"

extern void __mc6839_bindec_f32(uint8_t *bcd, int k, float val);
extern void __mc6839_bindec_f32_rz(uint8_t *bcd, int k, float val);

/* The most digits BINDEC produces. */
#define BINDEC_MAX_K 17

static inline int decode_E(const uint8_t *bcd) {
    int eMag = bcd[1] * 1000 + bcd[2] * 100 + bcd[3] * 10 + bcd[4];
    return (bcd[0] == 0x0F) ? -eMag : eMag;
}

/* The value's first k digits, truncated.  Returns the index in bcd of the
 * first significant digit and sets *exp10 to its decade.
 *
 * BINDEC (romsrc/outs.s, BDCNVT) starts from V := IFIX(log10(x)) + 1 - k,
 * and IFIX truncates toward zero, so for a value below 1 V comes out one
 * too large and W = x * 10^-V a digit short.  CHEKW retries when
 * W <= 10^(k-1) - 1, but it tests W before W is rounded to an integer,
 * so a W in the last unit below 10^(k-1) slips through.  Rounded, that W
 * becomes 10^(k-1), a power of ten indistinguishable from a genuine
 * carry: 0.99999988 to 7 digits came back 1.000000, not 0.9999999.
 * Truncated, it becomes a zero followed by nines -- the digits are right,
 * there is just one fewer of them, and the zero says so. */
static int bindec_rz(uint8_t *bcd, int k, float v, int *exp10)
{
    __mc6839_bindec_f32_rz(bcd, k, v);
    int start = 6 + (19 - k);
    int e = decode_E(bcd) + k - 1;
    if (bcd[start] == 0) {
        start++;
        e--;
    }
    *exp10 = e;
    return start;
}

int
__ftoa_engine(uint32_t val, struct dtoa *ftoa, int maxDigits, bool fmode, int maxDecimals)
{
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

    union { uint32_t u; float f; } u;
    u.u = val;
    float fv = u.f;
    if (flags & DTOA_MINUS)
        fv = -fv;

    uint8_t bcd[26];

    int exp10_first = 0;  /* used for the rounds-to-zero exit below */
    if (fmode) {
        /* First pass: the decade of the leading digit. */
        int exp10;
        int lead = bindec_rz(bcd, FTOA_MAX_DIG, fv, &exp10);
        exp10_first = exp10;

        /* Match the generic engine's logic (ftoa_engine.c):
         *   maxDigits = min(maxDigits,
         *                   max(maxDecimals < 0, maxDecimals + exp10 + 1));
         *
         * When maxDecimals < 0, the boolean evaluates to 1, ensuring at
         * least 1 digit (used by fcvt with negative ndigit to mean
         * "show the integer part"). When maxDecimals >= 0, the floor is 0
         * -- the engine may legitimately produce zero digits when the
         * value rounds entirely below the requested precision; the caller
         * formats that as "0.000...". */
        int adj = maxDecimals + exp10 + 1;
        /* The leading digit is the first one past the last decimal wanted:
         * the value rounds to zero there, or up to a 1 one place higher
         * (0.9999999 with "%.0f" is 1). */
        if (adj == 0 && maxDecimals >= 0 && bcd[lead] >= 5) {
            ftoa->digits[0] = '1';
            ftoa->exp = exp10 + 1;
            ftoa->flags = flags;
            return 1;
        }
        int floor = (maxDecimals < 0) ? 1 : 0;
        if (adj < floor) adj = floor;
        if (adj > maxDigits) adj = maxDigits;
        maxDigits = adj;
    }
    if (maxDigits == 0) {
        /* Engine produces no digits -- value rounds to all zeros at the
         * requested precision. Return ftoa->exp = the actual leftmost-
         * digit position so the caller (fcvt_r) computes a negative
         * `ntrailing` and hits its rounds-to-zero branch (which sets
         * *decpt = -dtoa_decimal). */
        ftoa->exp = exp10_first;
        ftoa->flags = flags;
        return 0;
    }

    /* Final pass: at least one digit more than wanted, truncated, and the
     * last one rounded off here, half up like the generic engine.  If
     * BINDEC's decade estimate cost a digit (see bindec_rz), ask for one
     * more.  BINDEC gives at most BINDEC_MAX_K digits, so a double wanting
     * 16 that loses one, or wanting all 17, has no digit to spare: the ROM
     * rounds those itself, which can only go wrong for a double within
     * 1e-16 of a power of ten below it. */
    int start, exp10;
    int have = 0;  /* significant digits in hand */
    if (maxDigits + 1 <= BINDEC_MAX_K) {
        start = bindec_rz(bcd, maxDigits + 1, fv, &exp10);
        have = 25 - start;
        if (have <= maxDigits && maxDigits + 2 <= BINDEC_MAX_K) {
            start = bindec_rz(bcd, maxDigits + 2, fv, &exp10);
            have = 25 - start;
        }
    }
    if (have <= maxDigits) {
        __mc6839_bindec_f32(bcd, maxDigits, fv);
        start = 6 + (19 - maxDigits);
        exp10 = decode_E(bcd) + maxDigits - 1;
        have = maxDigits;
    }

    for (int i = 0; i < maxDigits; i++)
        ftoa->digits[i] = (char)(bcd[start + i] + '0');

    /* Round up if the first dropped digit is 5 or more. */
    if (have > maxDigits) {
        if ((bcd[start + maxDigits] & 0x0F) >= 5) {
            int j = maxDigits - 1;
            while (j >= 0) {
                if (++ftoa->digits[j] <= '9') break;
                ftoa->digits[j] = '0';
                j--;
            }
            if (j < 0) {
                /* Carried past the leftmost digit: shift to "1" with
                 * exp+1 (e.g. "99" rounded to 1 digit -> "1" with
                 * exp10 += 1). */
                ftoa->digits[0] = '1';
                exp10++;
            }
        }
    }

    ftoa->exp = exp10;
    ftoa->flags = flags;
    return maxDigits;
}
