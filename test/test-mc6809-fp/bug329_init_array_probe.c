/* Bug #329: verify that .init_array runs constructor-attribute functions
 * on mc6809.  C++ global object ctors land in the same .init_array table,
 * so a passing C test here means C++ global ctors will also run.
 *
 * Pre-fix: g_magic stays at BSS-zero (0).
 * Post-fix: g_magic is 0xCAFE after init_ctor() runs.
 */
#include <stdio.h>
#include <stdint.h>

static uint16_t g_magic;

__attribute__((constructor))
static void init_ctor(void) {
    g_magic = 0xCAFE;
}

int main(void) {
    printf("magic=%04x (want cafe)\n", (unsigned)g_magic);
    if (g_magic == 0xCAFE) {
        printf("OK\n");
        return 0;
    } else {
        printf("FAIL\n");
        return 1;
    }
}
