/* Bug #311 minimum %lx probe — single vfprintf("%lx\n", 0xCAFEF00D) call. */
#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

__attribute__((noinline))
static void h_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

int main(void) {
    h_outer("%lx\n", 0xCAFEF00DUL);
    return 0;
}
