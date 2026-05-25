#include <stdio.h>
#include <stdint.h>

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

    printf("\nDONE\n");
    return 0;
}
