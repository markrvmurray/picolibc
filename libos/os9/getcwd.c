/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
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

#include "os9-private.h"
#include "os9-dir.h"
#include <string.h>
#include <unistd.h>

/* OS-9 has no getcwd either: a process knows its working directory only as
 * an open path, not as a name.  The way up is the way `pwd` goes -- every
 * directory holds "." with its own sector number and ".." with its
 * parent's, so the current directory's name is the entry in its parent
 * that points at the same sector, and the walk ends where the two are
 * equal, which is the device's root.  The path is built from the right, in
 * the caller's buffer, since that is the order the names arrive in. */
#define OS9_CWD_MAX_DEPTH 16
#define OS9_PROBE_MAX (OS9_CWD_MAX_DEPTH * 3 + 2)

/* The sector numbers of a directory's "." and ".." entries. */
static int
dir_sectors(const char *path, unsigned long *self, unsigned long *parent)
{
    unsigned char entry[OS9_DIRENT_SIZE];
    char name[OS9_DIRENT_NAME + 1];
    int fd, n, seen = 0, err;

    err = _os_open(path, OS9_DIR | OS9_READ, &fd);
    if (err != 0)
        return err;
    while (seen != 3) {
        n = OS9_DIRENT_SIZE;
        if (_os_read(fd, entry, &n) != 0 || n != OS9_DIRENT_SIZE) {
            err = OS9_E_PNNF;
            break;
        }
        if (entry[0] == 0)
            continue;
        __os9_entry_name(entry, name);
        unsigned long lsn = ((unsigned long)entry[OS9_DIRENT_LSN] << 16) |
                            ((unsigned long)entry[OS9_DIRENT_LSN + 1] << 8) |
                            entry[OS9_DIRENT_LSN + 2];
        if (name[0] == '.' && name[1] == '\0') {
            *self = lsn;
            seen |= 1;
        } else if (name[0] == '.' && name[1] == '.' && name[2] == '\0') {
            *parent = lsn;
            seen |= 2;
        }
    }
    _os_close(fd);
    return seen == 3 ? 0 : err;
}

/* The name, in `path`'s directory, of the entry pointing at `lsn`. */
static int
name_of_sector(const char *path, unsigned long lsn, char *name)
{
    unsigned char entry[OS9_DIRENT_SIZE];
    int fd, n, err;

    err = _os_open(path, OS9_DIR | OS9_READ, &fd);
    if (err != 0)
        return err;
    for (;;) {
        n = OS9_DIRENT_SIZE;
        if (_os_read(fd, entry, &n) != 0 || n != OS9_DIRENT_SIZE) {
            err = OS9_E_PNNF;
            break;
        }
        if (entry[0] == 0 || entry[0] == ('.' | 0x80) ||
            (entry[0] == '.' && entry[1] == ('.' | 0x80)))
            continue;
        if ((((unsigned long)entry[OS9_DIRENT_LSN] << 16) |
             ((unsigned long)entry[OS9_DIRENT_LSN + 1] << 8) |
             entry[OS9_DIRENT_LSN + 2]) == lsn) {
            __os9_entry_name(entry, name);
            err = 0;
            break;
        }
    }
    _os_close(fd);
    return err;
}

/* Put `text` in front of what `*end` points at, inside `buf`. */
static int
prepend(char *buf, char **end, const char *text)
{
    unsigned n = (unsigned)strlen(text);

    if ((unsigned)(*end - buf) < n)
        return 0;
    *end -= n;
    memcpy(*end, text, n);
    return 1;
}

char *
getcwd(char *buf, size_t size)
{
    char probe[OS9_PROBE_MAX];
    char name[OS9_DIRENT_NAME + 1];
    unsigned long self, parent;
    char *end;
    int depth, err;

    if (buf == 0 || size < 2) {
        errno = EINVAL;
        return 0;
    }
    end = buf + size - 1;
    *end = '\0';

    strcpy(probe, ".");
    for (depth = 0; depth < OS9_CWD_MAX_DEPTH; depth++) {
        err = dir_sectors(probe, &self, &parent);
        if (err != 0) {
            __os9_errno(err);
            return 0;
        }
        if (self == parent) {
            /* The device's root: its name is the path's own device. */
            unsigned char dev[32];
            int fd;
            err = _os_open(probe, OS9_DIR | OS9_READ, &fd);
            if (err != 0) {
                __os9_errno(err);
                return 0;
            }
            err = _os_getstt_buf(fd, OS9_SS_DEVNM, dev, sizeof dev);
            _os_close(fd);
            if (err != 0) {
                __os9_errno(err);
                return 0;
            }
            __os9_entry_name(dev, name);
            if (!prepend(buf, &end, name) || !prepend(buf, &end, "/")) {
                errno = ERANGE;
                return 0;
            }
            memmove(buf, end, strlen(end) + 1);
            return buf;
        }
        /* One level up, and our own name in it. */
        if (strlen(probe) + 3 >= sizeof probe) {
            errno = ENAMETOOLONG;
            return 0;
        }
        if (depth == 0)
            strcpy(probe, "..");
        else
            strcat(probe, "/..");
        err = name_of_sector(probe, self, name);
        if (err != 0) {
            __os9_errno(err);
            return 0;
        }
        if (!prepend(buf, &end, name) || !prepend(buf, &end, "/")) {
            errno = ERANGE;
            return 0;
        }
    }
    errno = ENAMETOOLONG;
    return 0;
}
