#include <stdio.h>
int main(void) {
    printf("sizeof(int)=%d sizeof(long)=%d sizeof(long long)=%d\n",
           (int)sizeof(int), (int)sizeof(long), (int)sizeof(long long));
    return 0;
}
