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
#include <fcntl.h>
#include <unistd.h>

/* A new file's attributes: readable and writable by its owner and by the
 * public, which is what a test expects of the files it creates. */
#define OS9_NEW_FILE_ATTRS (OS9_READ | OS9_WRITE | OS9_PREAD | OS9_PWRITE)

/* Kept out of open() itself: the 6809 has few enough registers that folding
 * these into it runs the allocator out. */
__attribute__((noinline)) static int
truncate_path(int path_num)
{
    struct __os9_xu xu = { (unsigned char)path_num, OS9_SS_SIZE, 0 };
    return __os9_setstt_xu(&xu);
}

__attribute__((noinline)) static void
seek_to_end(int path_num)
{
    struct __os9_xu xu = { (unsigned char)path_num, OS9_SS_SIZE, 0 };
    if (__os9_getstt_xu(&xu) != 0)
        return;
    xu.fn = 0;
    __os9_seek(&xu);
}

/* The create-or-open decision, likewise apart: it holds the path, the mode
 * and the attributes across a system call whose operands are pinned to
 * particular registers. */
__attribute__((noinline)) static int
create_or_open(const char *path, int mode, int oflag, int *pathp)
{
    int err;

    if (oflag & O_CREAT) {
        /* I$Create needs a writable mode even for a file about to be read,
         * and fails with E$CEF when the file is already there. */
        err = _os_create(path, mode | OS9_WRITE, OS9_NEW_FILE_ATTRS, pathp);
        if (err == 0)
            return 0;
        if (err != OS9_E_CEF || (oflag & O_EXCL))
            return err;
    }
    return _os_open(path, mode, pathp);
}

static int
access_mode(int oflag)
{
    int mode;

    switch (oflag & O_ACCMODE) {
    case O_RDONLY:
        mode = OS9_READ;
        break;
    case O_WRONLY:
        mode = OS9_WRITE;
        break;
    default:
        mode = OS9_UPDATE;
        break;
    }
    if (oflag & O_DIRECTORY)
        mode |= OS9_DIR;
    return mode;
}

int
open(const char *path, int oflag, ...)
{
    int path_num, err;

    err = create_or_open(path, access_mode(oflag), oflag, &path_num);
    if (err != 0)
        return __os9_errno(err);

    if (oflag & O_TRUNC) {
        err = truncate_path(path_num);
        if (err != 0) {
            _os_close(path_num);
            return __os9_errno(err);
        }
    }
    if (oflag & O_APPEND)
        seek_to_end(path_num);
    return path_num;
}
