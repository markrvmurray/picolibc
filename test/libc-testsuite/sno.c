#include <stdio.h>
#include <string.h>
int main(void) {
    char b[16];
    for (int k = 0; k < 16; k++) b[k] = 0x5a;
    int len = snprintf(b, sizeof b, "%#o", 15);
    printf("len=%d strlen=%u bytes:", len, (unsigned)strlen(b));
    for (int i = 0; i < 8; i++) printf(" %02x", (unsigned char)b[i]);
    printf("\n");
    return 0;
}
