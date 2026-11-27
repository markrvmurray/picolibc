/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Boisy G. Pitre
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 *
 * 2. Redistributions in binary form must reproduce the above
 *    copyright notice, this list of conditions and the following
 *    disclaimer in the documentation and/or other materials provided
 *    with the distribution.
 *
 * 3. Neither the name of the copyright holder nor the names of its
 *    contributors may be used to endorse or promote products derived
 *    from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
 * FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
 * COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
 * INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
 * (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
 * SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
 * STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
 * ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
 * OF THE POSSIBILITY OF SUCH DAMAGE.
 */

/* Disk, binary, raw and cookie I/O retain bytes without newline translation. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <stdio-bufio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define NAME "OS9TEXT.TMP"
#define check(x)                                   \
    do {                                           \
        if (!(x)) {                                \
            printf("FAIL %d: %s\n", __LINE__, #x); \
            exit(1);                               \
        }                                          \
    } while (0)

static unsigned char data[256], got[256];

static size_t        cookie_pos;

static ssize_t
cookie_read(void *cookie, void *buf, size_t n)
{
    if (n > sizeof(data) - cookie_pos)
        n = sizeof(data) - cookie_pos;
    memcpy(buf, (unsigned char *)cookie + cookie_pos, n);
    cookie_pos += n;
    return n;
}

static ssize_t
cookie_write(void *cookie, const void *buf, size_t n)
{
    if (n > sizeof(data) - cookie_pos)
        n = sizeof(data) - cookie_pos;
    memcpy((unsigned char *)cookie + cookie_pos, buf, n);
    cookie_pos += n;
    return n;
}

static ssize_t
descriptor_write(int fd, const void *buf, size_t n)
{
    check(fd == 123 && n <= sizeof(got));
    memcpy(got, buf, n);
    return n;
}

int
main(void)
{
    FILE *f;
    int fd, i;
    char small[7], line[8];
    const char native_lines[] = "one\ntwo\n";

    for (i = 0; i < 256; i++)
        data[i] = i;

    /* Disk text and binary streams are equally byte-preserving. Newline
     * values and fgets/line-buffering delimiters belong to the compiler. */
    const char *modes[] = { "w+", "wb+", "w+b" };
    for (i = 0; i < 3; i++) {
        f = fopen(NAME, modes[i]);
        check(f != NULL);
        check(setvbuf(f, small, _IOFBF, sizeof(small)) == 0);
        check(fwrite(data, 1, sizeof(data), f) == sizeof(data));
        check(fseek(f, 0, SEEK_SET) == 0);
        check(fread(got, 1, sizeof(got), f) == sizeof(got));
        check(memcmp(data, got, sizeof(data)) == 0);
        check(freopen(NAME, "r", f) == f);
        check(fread(got, 1, sizeof(got), f) == sizeof(got));
        check(memcmp(data, got, sizeof(data)) == 0);
        check(freopen(NAME, "rb", f) == f);
        check(fseek(f, '\r', SEEK_SET) == 0 && fgetc(f) == '\r');
        check(ungetc('\r', f) == '\r' && fgetc(f) == '\r');
        check(fclose(f) == 0);
        fd = open(NAME, O_RDONLY);
        check(fd >= 0 && read(fd, got, sizeof(got)) == sizeof(got));
        check(memcmp(data, got, sizeof(data)) == 0);
        check(close(fd) == 0);
    }

    fd = open(NAME, O_WRONLY | O_CREAT | O_TRUNC, 0666);
    check(fd >= 0 && write(fd, native_lines, sizeof(native_lines) - 1) == sizeof(native_lines) - 1);
    check(close(fd) == 0);
    fd = open(NAME, O_RDONLY);
    check(fd >= 0);
    f = fdopen(fd, "r");
    check(f != NULL);
    check(setvbuf(f, small, _IOFBF, sizeof(small)) == 0);
    check(fgets(line, sizeof(line), f) && strcmp(line, "one\n") == 0);
    check(ftell(f) == 4);
    check(fgets(line, sizeof(line), f) && strcmp(line, "two\n") == 0);
    check(fgets(line, sizeof(line), f) == NULL && feof(f));
    check(fclose(f) == 0);

    f = fopen(NAME, "w");
    check(f != NULL);
    check(setvbuf(f, small, _IOLBF, sizeof(small)) == 0);
    check(fputc('\n', f) == '\n');
    fd = open(NAME, O_RDONLY);
    check(fd >= 0 && read(fd, got, 1) == 1 && got[0] == '\n');
    check(close(fd) == 0);
    check(fclose(f) == 0);

    f = fmemopen(data, sizeof(data), "r");
    check(f && fread(got, 1, sizeof(got), f) == sizeof(got));
    check(memcmp(data, got, sizeof(data)) == 0);
    check(fclose(f) == 0);
    cookie_pos = 0;
    f = funopen(data, cookie_read, NULL, NULL, NULL);
    check(f && fread(got, 1, sizeof(got), f) == sizeof(got));
    check(memcmp(data, got, sizeof(data)) == 0);
    check(fclose(f) == 0);
    cookie_pos = 0;
    f = funopen(got, NULL, cookie_write, NULL, NULL);
    check(f && fwrite(data, 1, sizeof(data), f) == sizeof(data));
    check(fclose(f) == 0);
    check(memcmp(data, got, sizeof(data)) == 0);
    {
        struct __file_bufio custom =
            FDEV_SETUP_BUFIO(123, small, sizeof(small), NULL, descriptor_write, NULL, NULL, __SWR, 0);
        FILE *stream = &custom.xfile.cfile.file;
        check(fputs("user\r", stream) >= 0 && fflush(stream) == 0);
        check(memcmp(got, "user\r", 5) == 0);
        check(fclose(stream) == 0);
    }
    check(remove(NAME) == 0);
    return 0;
}
