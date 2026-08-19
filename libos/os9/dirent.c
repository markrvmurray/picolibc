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
#include <dirent.h>
#include <stdlib.h>
#include <string.h>

/* An OS-9 directory is an ordinary file of 32-byte entries: 29 bytes of
 * name, whose last character carries the high bit, then the three-byte
 * sector number of the file's descriptor.  An entry whose name starts with
 * a NUL is a deleted one and is skipped. */
#define OS9_DIRENT_SIZE 32
#define OS9_DIRENT_NAME 29

DIR *
opendir(const char *name)
{
    DIR *dir;
    int path_num;

    if (_os_open(name, OS9_DIR | OS9_READ, &path_num) != 0) {
        __os9_errno(errno);
        return 0;
    }
    dir = malloc(sizeof(DIR));
    if (dir == 0) {
        _os_close(path_num);
        errno = ENOMEM;
        return 0;
    }
    dir->fd = path_num;
    dir->offset = 0;
    dir->count = 0;
    return dir;
}

struct dirent *
readdir(DIR *dir)
{
    unsigned char ent[OS9_DIRENT_SIZE];
    int i, n;

    for (;;) {
        n = OS9_DIRENT_SIZE;
        if (_os_read(dir->fd, ent, &n) != 0) {
            __os9_errno(errno);
            return 0;
        }
        if (n != OS9_DIRENT_SIZE)
            return 0;                   /* the end of the directory */
        dir->offset += OS9_DIRENT_SIZE;
        if (ent[0] == 0)
            continue;                   /* a deleted entry */
        for (i = 0; i < OS9_DIRENT_NAME; i++) {
            dir->dirent.d_name[i] = (char)(ent[i] & 0x7F);
            if (ent[i] & 0x80) {
                i++;
                break;
            }
        }
        dir->dirent.d_name[i] = '\0';
        dir->dirent.d_ino = ((ino_t)ent[OS9_DIRENT_NAME] << 16) |
                            ((ino_t)ent[OS9_DIRENT_NAME + 1] << 8) |
                            ent[OS9_DIRENT_NAME + 2];
        dir->dirent.d_type = DT_UNKNOWN;
        return &dir->dirent;
    }
}

void
rewinddir(DIR *dir)
{
    struct __os9_xu xu = { (unsigned char)dir->fd, 0, 0 };

    if (__os9_seek(&xu) == 0)
        dir->offset = 0;
}

int
dirfd(DIR *dir)
{
    return dir->fd;
}

int
closedir(DIR *dir)
{
    int path_num = dir->fd;

    free(dir);
    return OS9_RESULT(_os_close(path_num));
}
