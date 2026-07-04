/*
 * Signed 16-bit add-overflow under byte spill pressure.
 *
 * __builtin_add_overflow on int16 lowers to a byte carry chain whose high
 * byte sets CC.V. A branch on that overflow must capture V into a byte at the
 * producer: every 6809 store clears V, and the register allocator inserts a
 * spill store between the adding instruction and the branch (always at -O0,
 * where the sum is spilled the instant it is defined). A branch that reads V
 * live then reads a V the spill has already cleared and silently misses the
 * overflow -- visibly wrong at the INT16_MIN edge.
 *
 * saddo16() below is the exact fragile shape (x plus a runtime-negated y).
 * ref() is an int32 oracle that never enters the i16 overflow path. Both are
 * driven from a runtime value array so nothing constant-folds away.
 */
#include <stdio.h>
#include <stdint.h>

static int16_t __attribute__((noinline)) saddo16(int16_t x, int16_t y)
{
    int16_t ny = (int16_t)(0u - (uint16_t)y);
    int16_t s;
    if (__builtin_add_overflow(x, ny, &s))
        return -999;
    return s;
}

/* Wide oracle: 32-bit negate + 32-bit add, range-checked. */
static int16_t ref(int16_t x, int16_t y)
{
    int16_t ny = (int16_t)(-(int32_t)y);
    int32_t s = (int32_t)x + (int32_t)ny;
    if (s > 32767 || s < -32768)
        return -999;
    return (int16_t)s;
}

int main(void)
{
    static const int16_t v[] = {
        -32768, -32767, -256, -128, -2, -1, 0, 1, 2, 127, 255, 32766, 32767
    };
    const unsigned n = sizeof(v) / sizeof(v[0]);
    int fails = 0;

    for (unsigned i = 0; i < n; i++) {
        for (unsigned j = 0; j < n; j++) {
            int16_t got = saddo16(v[i], v[j]);
            int16_t want = ref(v[i], v[j]);
            if (got != want) {
                if (fails < 6)
                    printf("FAIL saddo16(%d,%d)=%d want %d\n",
                           (int)v[i], (int)v[j], (int)got, (int)want);
                fails++;
            }
        }
    }

    if (fails) {
        printf("FAIL %d/%u\n", fails, n * n);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
