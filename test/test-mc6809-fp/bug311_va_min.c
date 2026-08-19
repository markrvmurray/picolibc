/* Minimum probe for Bug #311 — single vfprintf("%d\n", 7) call. */
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
    h_outer("%d\n", 7);
    return check("7\n", "%d\n", 7);
}
