/* The data model, pinned.  These are the sizes every other test's
 * expectations are written against -- an accidental change to any of them
 * would break far more than it would obviously explain, so say so here. */
#include <stdio.h>

int main(void) {
    int fail = 0;

    printf("sizeof(int)=%d sizeof(long)=%d sizeof(long long)=%d\n",
           (int)sizeof(int), (int)sizeof(long), (int)sizeof(long long));
    if (sizeof(int) != 2)       { printf("FAIL: int is not 2 bytes\n");       fail++; }
    if (sizeof(long) != 4)      { printf("FAIL: long is not 4 bytes\n");      fail++; }
    if (sizeof(long long) != 8) { printf("FAIL: long long is not 8 bytes\n"); fail++; }
    if (sizeof(void *) != 2)    { printf("FAIL: a pointer is not 2 bytes\n"); fail++; }
    return fail;
}
