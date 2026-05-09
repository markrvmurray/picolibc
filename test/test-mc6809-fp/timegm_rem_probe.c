/* Bug #247: HD6309-LTO test-timegm tm_hour/min/sec all return 0.
 *
 * Minimal reproducer: a loop that
 *   1. calls gmtime() and reads tm_sec/tm_min/tm_hour fields,
 *   2. calls timegm() and USES its return value
 * triggers a regalloc bug where gmtime's tm_sec/min/hour come back wrong.
 *
 * Variants that DO NOT trigger the bug:
 *   - no timegm() call at all
 *   - call timegm() but discard the return value
 *   - same with i64 multiplication busy-work in place of timegm
 *
 * So the bug specifically needs the inlined `timegm` body's escaping
 * return value to alter regalloc enough to corrupt the inlined gmtime
 * intermediates. Confirmed at -Wl,--lto-O2 on HD6309 only.
 *
 * Returns 0 on PASS, 1 on FAIL. */

#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <time.h>
extern time_t timegm(struct tm *);

#define NUM_TEST 1024

const struct _test {
    struct tm tm;
    time_t time;
} tests[NUM_TEST] = {
#include "../test-timegm.h"
};

int main(void) {
    int ret = 0;
    for (unsigned i = 0; i < NUM_TEST; i++) {
        time_t t = tests[i].time;
        struct tm *p = gmtime(&t);
        if (p->tm_sec != tests[i].tm.tm_sec ||
            p->tm_min != tests[i].tm.tm_min ||
            p->tm_hour != tests[i].tm.tm_hour) {
            ret++;
        }
        struct tm tmp = tests[i].tm;
        if (timegm(&tmp) != tests[i].time) ret++;
    }
    if (ret) {
        printf("FAIL count=%d\n", ret);
        return 1;
    }
    printf("PASS\n");
    return 0;
}
