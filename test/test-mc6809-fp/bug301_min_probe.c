/* Bug #301: pure i64 div with non-zero value. */
#define _DEFAULT_SOURCE 1
#include <stdio.h>
#include <stdint.h>

volatile int64_t tin = 86400LL;

int main(void) {
    int64_t a = tin;
    int64_t q = a / 86400LL;
    int64_t r = a % 86400LL;
    union { int64_t v; uint8_t b[8]; } u;
    u.v = q;
    printf("q=%02x%02x%02x%02x%02x%02x%02x%02x (want 0000000000000001)\n",
        u.b[0],u.b[1],u.b[2],u.b[3],u.b[4],u.b[5],u.b[6],u.b[7]);
    u.v = r;
    printf("r=%02x%02x%02x%02x%02x%02x%02x%02x (want 0000000000000000)\n",
        u.b[0],u.b[1],u.b[2],u.b[3],u.b[4],u.b[5],u.b[6],u.b[7]);
    return 0;
}
