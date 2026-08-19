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
#include <stdio.h>
#include <string.h>
#include <unistd.h>

/* OS-9 has no rename: a file's name lives in its directory entry, and
 * changing it means writing that entry back -- which is what the shell's
 * own `rename` does.  The entry holds only the name and the sector number
 * of the file's descriptor, so a rename within one directory is exactly
 * this write; moving between directories would mean rewriting two
 * directories and is what EXDEV is for.
 *
 * The buffers are the path components rather than whole paths: an OS-9
 * name is at most 29 characters, and a program's stack is small.
 *
 * find_and_write returns 0, an OS-9 error code, or -1 with errno already
 * set for what OS-9 has no code for. */
#define OS9_DIR_MAX 64

static int
find_and_write(int fd, const char *from, const char *to)
{
    unsigned char entry[OS9_DIRENT_SIZE];
    char name[OS9_DIRENT_NAME + 1];
    struct __os9_xu xu = { (unsigned char)fd, 0, 0 };
    unsigned long pos = 0;
    int n, len, i;

    for (;;) {
        n = OS9_DIRENT_SIZE;
        if (_os_read(fd, entry, &n) != 0)
            return errno;
        if (n != OS9_DIRENT_SIZE)
            return OS9_E_PNNF;          /* the name is not in there */
        if (entry[0] != 0) {
            __os9_entry_name(entry, name);
            if (__os9_name_eq(name, from))
                break;
        }
        pos += OS9_DIRENT_SIZE;
    }

    /* Rewrite the name in place, keeping the sector number: the new name
     * left-justified, its last character carrying the high bit, and the
     * rest of the field cleared. */
    len = (int)strlen(to);
    if (len == 0 || len > OS9_DIRENT_NAME) {
        errno = len == 0 ? EINVAL : ENAMETOOLONG;
        return -1;                      /* already a POSIX errno */
    }
    for (i = 0; i < OS9_DIRENT_NAME; i++)
        entry[i] = (unsigned char)(i < len ? to[i] : 0);
    entry[len - 1] |= 0x80;

    xu.pos = pos;
    if (__os9_seek(&xu) != 0)
        return errno;
    n = OS9_DIRENT_SIZE;
    if (_os_write(fd, entry, &n) != 0)
        return errno;
    return 0;
}

int
rename(const char *old, const char *new)
{
    char old_dir[OS9_DIR_MAX], new_dir[OS9_DIR_MAX];
    const char *old_name, *new_name;
    int fd, err;

    old_name = __os9_split_path(old, old_dir, sizeof old_dir);
    new_name = __os9_split_path(new, new_dir, sizeof new_dir);
    if (old_name == 0 || new_name == 0) {
        errno = ENAMETOOLONG;
        return -1;
    }
    if (!__os9_name_eq(old_dir, new_dir)) {
        errno = EXDEV;
        return -1;
    }
    if (__os9_name_eq(old_name, new_name))
        return 0;

    /* POSIX replaces the target; OS-9 cannot do that in one step, so the
     * target goes first and the entry is rewritten after. */
    (void)_os_delete(new);

    err = _os_open(old_dir, OS9_DIR | OS9_UPDATE, &fd);
    if (err != 0)
        return __os9_errno(err);
    err = find_and_write(fd, old_name, new_name);
    _os_close(fd);
    if (err < 0)
        return -1;                      /* errno is already set */
    if (err != 0)
        return __os9_errno(err);
    return 0;
}
