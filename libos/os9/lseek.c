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
#include <unistd.h>

/* I$Seek takes the position in X:U; SS.Pos and SS.Size report the current
 * one and the file's length the same way (__os9_seek and friends are the
 * runtime's stubs, since U is the process data-area base).  Seeking past
 * the end is legal: a later write extends the file. */
off_t
lseek(int fd, off_t offset, int whence)
{
    struct __os9_xu xu = { (unsigned char)fd, 0, 0 };
    unsigned long base = 0;

    switch (whence) {
    case SEEK_SET:
        break;
    case SEEK_CUR:
        xu.fn = OS9_SS_POS;
        if (__os9_getstt_xu(&xu) != 0)
            return __os9_errno(errno);
        base = xu.pos;
        break;
    case SEEK_END:
        xu.fn = OS9_SS_SIZE;
        if (__os9_getstt_xu(&xu) != 0)
            return __os9_errno(errno);
        base = xu.pos;
        break;
    default:
        errno = EINVAL;
        return -1;
    }

    xu.fn = 0;
    xu.pos = base + (unsigned long)offset;
    if (__os9_seek(&xu) != 0)
        return __os9_errno(errno);
    return (off_t)xu.pos;
}
