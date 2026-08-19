#include <stdio.h>
#include <stdint.h>
#include <stdarg.h>
#include <string.h>

/* Check a format and its arguments through the same engine the printf
 * calls below use, so a wrong answer fails rather than merely appearing.
 * The 64-bit values are left to the eye: these levels build without
 * long-long printf support, so %llx prints the low half and pinning that
 * would pin a build option, not the variadic handling this probe is
 * about. */
static int failures;

__attribute__((noinline))
static void check(const char *want, const char *fmt, ...) {
    char buf[64];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (strcmp(buf, want) != 0) {
        printf("FAIL: got \"%s\" want \"%s\"\n", buf, want);
        failures++;
    }
}

int main(void) {
    uint16_t s = 0x1234;
    uint32_t i = 0xCAFEF00DUL;
    uint64_t a = 0x1122334455667788ULL;
    uint64_t b = 0x9988776655443322ULL;
    int      x = 7;

    printf("s=%x\n", (unsigned)s);
    printf("i=%lx\n", (unsigned long)i);
    printf("a=%llx\n", (unsigned long long)a);
    printf("mixed: s=%x i=%lx a=%llx x=%d\n",
           (unsigned)s, (unsigned long)i, (unsigned long long)a, x);
    printf("a-then-x: a=%llx x=%d\n",
           (unsigned long long)a, x);
    printf("two-u64: %llx %llx\n",
           (unsigned long long)a, (unsigned long long)b);

    check("s=1234\n", "s=%x\n", (unsigned)s);
    check("i=cafef00d\n", "i=%lx\n", (unsigned long)i);
    check("mixed: s=1234 x=7\n", "mixed: s=%x x=%d\n", (unsigned)s, x);
    check("after-u64: x=7\n", "after-u64: x=%d\n", x);

    printf("\nDONE\n");
    return failures != 0;
}
