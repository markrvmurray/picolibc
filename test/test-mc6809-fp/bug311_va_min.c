/* Minimum probe for Bug #311 — single vfprintf("%d\n", 7) call. */
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
    h_outer("%d\n", 7);
    return 0;
}
