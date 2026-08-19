/* Bug #311 minimum %lx probe — single vfprintf("%lx\n", 0xCAFEF00D) call. */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>
#include <string.h>

__attribute__((noinline))
static void h_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}


/* The vfprintf call above is the reproducer and has to stay as it is; this
 * runs the same format and argument through the same formatting engine by
 * way of vsnprintf, so the result can be looked at rather than only
 * printed. */
__attribute__((noinline))
static int check(const char *want, const char *fmt, ...) {
    char buf[40];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    if (strcmp(buf, want) != 0) {
        printf("FAIL: got \"%s\" want \"%s\"\n", buf, want);
        return 1;
    }
    return 0;
}

int main(void) {
    h_outer("%lx\n", 0xCAFEF00DUL);
    return check("cafef00d\n", "%lx\n", 0xCAFEF00DUL);
}
