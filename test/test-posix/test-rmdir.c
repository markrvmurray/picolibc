/*
Copyright (c) Qualcomm Technologies, Inc. and/or its subsidiaries.
SPDX-License-Identifier: BSD-3-Clause-Clear

Redistribution and use in source and binary forms, with or without
modification, are permitted (subject to the limitations in the
disclaimer below) provided that the following conditions are met:

  * Redistributions of source code must retain the above copyright
    notice, this list of conditions and the following disclaimer.

  * Redistributions in binary form must reproduce the above
    copyright notice, this list of conditions and the following
    disclaimer in the documentation and/or other materials provided
    with the distribution.

  * Neither the name of Qualcomm Technologies, Inc. nor the names of its
    contributors may be used to endorse or promote products derived
    from this software without specific prior written permission.

NO EXPRESS OR IMPLIED LICENSES TO ANY PARTY'S PATENT RIGHTS ARE
GRANTED BY THIS LICENSE. THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT
HOLDERS AND CONTRIBUTORS "AS IS" AND ANY EXPRESS OR IMPLIED
WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE IMPLIED WARRANTIES OF
MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE ARE DISCLAIMED.
IN NO EVENT SHALL THE COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR
ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE
GOODS OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS
INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER
IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR
OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN
IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
*/
#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <unistd.h>

#define check(condition, message)                    \
    do {                                             \
        if (!(condition)) {                          \
            printf("%s: %s\n", message, #condition); \
            exit(1);                                 \
        }                                            \
    } while (0)

#ifdef __PICOLIBC__
__typeof(mkdir) __fake_mkdir __weak;
__typeof(rmdir) __fake_rmdir __weak;
#define not_fake(sym) ((sym) != __fake_##sym)
#else
#define not_fake(sym) 1
#endif

#ifndef TEST_DIR_NAME
#define TEST_DIR_NAME "rmdir_d"
#endif
static const char dir_name[] = TEST_DIR_NAME;
static const char file_name[] = TEST_DIR_NAME "/f.txt";

int
main(void)
{
    FILE *f;
    int   ret;

#ifndef TESTS_ENABLE_POSIX_IO
    printf("POSIX I/O not enabled, skipping\n");
    return 77;
#endif

    if (!not_fake(mkdir) || !not_fake(rmdir)) {
        printf("fake mkdir/rmdir, skipping\n");
        return 77;
    }

    /* Nothing of an earlier run left behind. */
    (void)remove(file_name);
    (void)rmdir(dir_name);

    ret = mkdir(dir_name, 0777);
    check(ret == 0, "mkdir");

    /* A directory with something in it stays. */
    f = fopen(file_name, "w");
    check(f != NULL, "fopen in the new directory");
    fputs("x", f);
    fclose(f);
    errno = 0;
    ret = rmdir(dir_name);
    check(ret == -1, "rmdir of a directory that is not empty");
    check(errno == ENOTEMPTY, "rmdir sets ENOTEMPTY");

    /* ... and is still a directory afterwards, not something else. */
    f = fopen(file_name, "r");
    check(f != NULL, "the directory still holds its file");
    fclose(f);

    /* Emptied, it goes. */
    ret = remove(file_name);
    check(ret == 0, "remove the file");
    ret = rmdir(dir_name);
    check(ret == 0, "rmdir of an empty directory");

    /* And is gone: nothing can be made in it any more. */
    f = fopen(file_name, "w");
    check(f == NULL, "the directory is gone");

    printf("rmdir test passed\n");
    exit(0);
}
