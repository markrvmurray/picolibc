/* memset, called by name and as the compiler's __memset, fills exactly
 * the bytes asked for: every length from 0 to 70 at every alignment
 * within four, with guard bytes either side, plus the return value. */
#include <stdio.h>
#include <string.h>

#define GUARD 8
#define MAXLEN 70

static unsigned char buf[GUARD + 4 + MAXLEN + GUARD];

/* Through a pointer the call goes to memset itself; a direct call is
 * lowered to compiler-rt's __memset. */
static void *(*volatile by_name)(void *, int, size_t) = memset;

static int check(const char *how, size_t align, size_t len, int c, void *ret) {
    unsigned char *dst = buf + GUARD + align;
    if (ret != dst) {
        printf("%s(align %u, len %u): returned %p, want %p\n", how, (unsigned)align,
               (unsigned)len, ret, (void *)dst);
        return 1;
    }
    for (size_t i = 0; i < sizeof(buf); i++) {
        unsigned char want = (buf + i >= dst && buf + i < dst + len) ? (unsigned char)c : 0x5a;
        if (buf[i] != want) {
            printf("%s(align %u, len %u, 0x%02x): byte %d is 0x%02x, want 0x%02x\n", how,
                   (unsigned)align, (unsigned)len, c & 0xff, (int)i - GUARD - (int)align,
                   buf[i], want);
            return 1;
        }
    }
    return 0;
}

int main(void) {
    static const int fills[] = { 0x00, 0xff, 0x80, 0x01, 0x1a5 };
    int errors = 0;
    for (unsigned f = 0; f < sizeof(fills) / sizeof(fills[0]); f++)
        for (size_t align = 0; align < 4; align++)
            for (size_t len = 0; len <= MAXLEN; len++) {
                volatile size_t n = len;
                unsigned char *dst = buf + GUARD + align;

                for (size_t i = 0; i < sizeof(buf); i++)
                    buf[i] = 0x5a;
                errors += check("memset", align, len, fills[f], memset(dst, fills[f], n));

                for (size_t i = 0; i < sizeof(buf); i++)
                    buf[i] = 0x5a;
                errors += check("by name", align, len, fills[f], by_name(dst, fills[f], n));
            }
    printf("memset_probe: %d errors\n", errors);
    return errors != 0;
}
