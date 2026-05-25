/* Bug #311 isolation probe.
 *
 * Sidestep vfprintf entirely.  Use minimal putc-based hex print
 * routines to determine whether the corruption is in:
 *   (a) the va_list ABI itself (caller pushes vs callee reads
 *       mismatch — would corrupt EVERY vararg call), or
 *   (b) vfprintf's internal va_list traversal (would corrupt
 *       only vfprintf, not custom vararg fns).
 *
 * Layout:
 *   Step A — direct (non-vararg) call with u32 arg.  Baseline.
 *   Step B — single vararg fn, reads one u32 via va_arg.
 *   Step C — single vararg fn, reads u16 + u32 mixed.
 *   Step D — single vararg fn, reads u32 + u64 mixed.
 */

#include <stdio.h>
#include <stdarg.h>
#include <stdint.h>

static void put_hex_byte(uint8_t b) {
    static const char tab[] = "0123456789abcdef";
    putchar(tab[(b >> 4) & 0xf]);
    putchar(tab[b & 0xf]);
}

static void put_hex_u16(uint16_t v) {
    put_hex_byte((uint8_t)(v >> 8));
    put_hex_byte((uint8_t)v);
}

static void put_hex_u32(uint32_t v) {
    put_hex_u16((uint16_t)(v >> 16));
    put_hex_u16((uint16_t)v);
}

static void put_hex_u64(uint64_t v) {
    put_hex_u32((uint32_t)(v >> 32));
    put_hex_u32((uint32_t)v);
}

/* Step A — direct call.  No varargs. */
static void direct_u32(uint32_t v) {
    fputs("A: ", stdout);
    put_hex_u32(v);
    putchar('\n');
}

/* Step B — single u32 via va_arg. */
static void va_u32(int dummy, ...) {
    (void)dummy;
    va_list ap;
    va_start(ap, dummy);
    uint32_t v = va_arg(ap, uint32_t);
    fputs("B: ", stdout);
    put_hex_u32(v);
    putchar('\n');
    va_end(ap);
}

/* Step C — u16 then u32 via va_arg. */
static void va_u16_u32(int dummy, ...) {
    (void)dummy;
    va_list ap;
    va_start(ap, dummy);
    /* uint16_t is promoted to int via default promotions. */
    unsigned s = va_arg(ap, unsigned);
    uint32_t i = va_arg(ap, uint32_t);
    fputs("C: ", stdout);
    put_hex_u16((uint16_t)s);
    putchar(' ');
    put_hex_u32(i);
    putchar('\n');
    va_end(ap);
}

/* Step D — u32 then u64 via va_arg. */
static void va_u32_u64(int dummy, ...) {
    (void)dummy;
    va_list ap;
    va_start(ap, dummy);
    uint32_t i = va_arg(ap, uint32_t);
    uint64_t a = va_arg(ap, uint64_t);
    fputs("D: ", stdout);
    put_hex_u32(i);
    putchar(' ');
    put_hex_u64(a);
    putchar('\n');
    va_end(ap);
}

/* Step E — printf-style two-level: outer takes ..., passes
 * va_list (by value) to inner, which reads via va_arg.  This
 * mirrors picolibc's printf -> vfprintf control flow.  Force
 * noinline so the call boundary is real (matches vfprintf
 * being in a separate TU). */
__attribute__((noinline))
static void e_inner(va_list ap) {
    unsigned s = va_arg(ap, unsigned);
    uint32_t i = va_arg(ap, uint32_t);
    uint64_t a = va_arg(ap, uint64_t);
    fputs("E: ", stdout);
    put_hex_u16((uint16_t)s);
    putchar(' ');
    put_hex_u32(i);
    putchar(' ');
    put_hex_u64(a);
    putchar('\n');
}

static void e_outer(int dummy, ...) {
    (void)dummy;
    va_list ap;
    va_start(ap, dummy);
    e_inner(ap);
    va_end(ap);
}

/* Step F — printf's exact pattern: outer takes a leading
 * non-vararg fmt-like arg AND vararg, forwards to inner with
 * (extra_ptr, fmt_ptr, ap). */
__attribute__((noinline))
static void f_inner(void *ignored, const char *fmt, va_list ap) {
    (void)ignored;
    (void)fmt;
    unsigned s = va_arg(ap, unsigned);
    uint32_t i = va_arg(ap, uint32_t);
    uint64_t a = va_arg(ap, uint64_t);
    fputs("F: ", stdout);
    put_hex_u16((uint16_t)s);
    putchar(' ');
    put_hex_u32(i);
    putchar(' ');
    put_hex_u64(a);
    putchar('\n');
}

static void f_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    f_inner(stdout, fmt, ap);
    va_end(ap);
}

/* Step G — like f_outer but calls via a volatile fn ptr to
 * defeat any compile-time visibility into the inner function.
 * This is the closest analog to picolibc's printf -> vfprintf
 * where the body of vfprintf is invisible (separate TU). */
void g_outer(const char *fmt, ...) {
    void (*volatile fp)(void *, const char *, va_list) = f_inner;
    va_list ap;
    va_start(ap, fmt);
    fputs("G: ", stdout);
    fp(stdout, fmt, ap);
    va_end(ap);
}

/* Step J — same as I, but with a HUGE local buffer to push the
 * stack-frame size to vfprintf's scale (~1108 bytes).  If J
 * corrupts but I doesn't, the bug is triggered by frames
 * that exceed the 8-bit signed offset range for U-relative
 * addressing. */
__attribute__((noinline))
static void my_vfprintf_huge(const char *fmt, va_list ap) {
    volatile char huge_buf[1100];
    huge_buf[0] = (char)(uintptr_t)fmt;
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int v = va_arg(ap, int);
                put_hex_u16((uint16_t)v);
            } else if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    fmt++;
                    if (*fmt == 'x') {
                        unsigned long long llv = va_arg(ap, unsigned long long);
                        put_hex_u64(llv);
                    }
                } else if (*fmt == 'x') {
                    unsigned long lv = va_arg(ap, unsigned long);
                    put_hex_u32(lv);
                }
            } else if (*fmt == 'x') {
                unsigned xv = va_arg(ap, unsigned);
                put_hex_u16((uint16_t)xv);
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
    huge_buf[0] = 0;  /* keep buf alive */
}

static void j_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("J: ", stdout);
    my_vfprintf_huge(fmt, ap);
    va_end(ap);
}

/* Step I — vfprintf-shape: a function that takes va_list and
 * walks a fmt string in a loop, calling va_arg for each %
 * specifier.  Mirrors vfprintf's STRUCTURE without all its
 * complexity. */
__attribute__((noinline))
static void my_vfprintf_shape(const char *fmt, va_list ap) {
    while (*fmt) {
        if (*fmt == '%') {
            fmt++;
            if (*fmt == 'd') {
                int v = va_arg(ap, int);
                put_hex_u16((uint16_t)v);
            } else if (*fmt == 'l') {
                fmt++;
                if (*fmt == 'l') {
                    fmt++;
                    if (*fmt == 'x') {
                        unsigned long long llv = va_arg(ap, unsigned long long);
                        put_hex_u64(llv);
                    }
                } else if (*fmt == 'x') {
                    unsigned long lv = va_arg(ap, unsigned long);
                    put_hex_u32(lv);
                }
            } else if (*fmt == 'x') {
                unsigned xv = va_arg(ap, unsigned);
                put_hex_u16((uint16_t)xv);
            }
        } else {
            putchar(*fmt);
        }
        fmt++;
    }
}

static void i_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("I: ", stdout);
    my_vfprintf_shape(fmt, ap);
    va_end(ap);
}

/* Step H — call picolibc's vfprintf directly, bypassing printf.
 * If H corrupts but F doesn't, vfprintf itself (in libc.a) is
 * the broken party.  If H works, then picolibc's printf wrapper
 * around vfprintf is at fault. */
static void h_outer(const char *fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    fputs("H: ", stdout);
    vfprintf(stdout, fmt, ap);
    va_end(ap);
}

int main(void) {
    uint32_t i = 0xCAFEF00DUL;
    uint64_t a = 0x1122334455667788ULL;
    uint16_t s = 0x1234;

    direct_u32(i);
    va_u32(0, i);
    va_u16_u32(0, (unsigned)s, i);
    va_u32_u64(0, i, a);
    e_outer(0, (unsigned)s, i, a);
    f_outer("dummy_fmt", (unsigned)s, i, a);
    g_outer("dummy_fmt", (unsigned)s, i, a);

    /* Step I — vfprintf-shape function in same TU. */
    i_outer("%x %lx %llx\n", (unsigned)s, i, a);

    /* Step J — same as I but with HUGE stack frame (~1100 bytes
     * local buffer) to match vfprintf's frame size. */
    j_outer("%x %lx %llx\n", (unsigned)s, i, a);

    /* Step H — direct vfprintf, bypassing printf wrapper. */
    h_outer("%x %lx %llx\n", (unsigned)s, i, a);

    /* Triangulate vfprintf's failure mode with simpler args. */
    fputs("H1 (int): ", stdout); h_outer("%d\n", 7);
    fputs("H2 (int int): ", stdout); h_outer("%d %d\n", 7, 8);
    fputs("H3 (x): ", stdout); h_outer("%x\n", (unsigned)s);
    fputs("H4 (lx): ", stdout); h_outer("%lx\n", i);
    fputs("H5 (llx): ", stdout); h_outer("%llx\n", a);
    fputs("H6 (s): ", stdout); h_outer("%s\n", "hello");

    /* And the broken case: plain printf. */
    printf("PRINTF: %x %lx %llx\n", (unsigned)s, i, a);

    fputs("DONE\n", stdout);
    return 0;
}
