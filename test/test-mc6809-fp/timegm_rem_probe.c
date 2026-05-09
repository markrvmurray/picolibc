/* Bug #247 probe: full 1024-entry table, gmtime+timegm pair. */
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
            if (ret < 3)
                printf("[%u] t=%ld got h%d m%d s%d want h%d m%d s%d\n",
                       i, (long)t, p->tm_hour, p->tm_min, p->tm_sec,
                       tests[i].tm.tm_hour, tests[i].tm.tm_min, tests[i].tm.tm_sec);
            ret++;
        }
        struct tm tmp = tests[i].tm;
        if (timegm(&tmp) != tests[i].time) ret++;
    }
    if (!ret) printf("PASS\n");
    else printf("FAIL count=%d\n", ret);
    return !!ret;
}
