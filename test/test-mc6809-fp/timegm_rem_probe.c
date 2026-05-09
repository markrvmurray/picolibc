/* Bug #247: HD6309-LTO test-timegm tm_hour/min/sec all return 0.
 *
 * Sentinel for the i64 rem-decomposition pattern that LTO -O2
 * produces for `lcltime % SECSPERDAY`:
 *
 *   div = lcltime / 86400         (sdiv i64 → __divdi3)
 *   neg = div * 4294880896        (mul i64 → __muldi3, where
 *                                   4294880896 = -86400 mod 2^32, zext)
 *   rem32 = trunc((neg + lcltime), i32)
 *
 * In isolation this lowers correctly; the actual test-timegm fails only
 * when full gmtime/timegm cross-TU LTO inlining inflates main and the
 * surrounding regalloc pressure perturbs codegen. This probe stays as
 * a regression sentinel for the *isolated* pattern. */

#include <stdio.h>
#include <stdint.h>

__attribute__((noinline))
static int32_t rem_decomp(int64_t lcltime) {
    int64_t div = lcltime / 86400;
    int64_t neg = div * (int64_t)4294880896LL;
    return (int32_t)(neg + lcltime);
}

__attribute__((noinline))
static int32_t rem_srem(int64_t lcltime) {
    return (int32_t)(lcltime % 86400);
}

static int fail;

static void chk(int64_t t, int32_t want) {
    int32_t r1 = rem_decomp(t);
    int32_t r2 = rem_srem(t);
    if (r1 != want || r2 != want) {
        printf("FAIL t=%lld decomp=%ld srem=%ld want=%ld\n",
               (long long)t, (long)r1, (long)r2, (long)want);
        fail = 1;
    }
}

int main(void) {
    chk(0LL, 0);
    chk(86400LL, 0);
    chk(86400LL * 1000, 0);
    chk(100000LL, 13600);
    chk(86399LL, 86399);
    chk(86401LL, 1);
    chk(1700000000LL, 80000);
    chk(86400LL * 25000 + 79123, 79123);
    chk(86400LL * 12345 + 53, 53);
    chk(0x12345678LL, 0x12345678LL % 86400);
    chk(0x7FFFFFFFLL, 0x7FFFFFFFLL % 86400);
    if (!fail) printf("PASS\n");
    return fail;
}
