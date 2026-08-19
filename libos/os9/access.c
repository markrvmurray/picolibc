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

/* OS-9 answers "may I?" by being asked to open the path that way. */
/* R_OK/W_OK/X_OK to OS-9's read/write/execute, which are the same three
 * bits the other way round.  Spelled as a table rather than as three bit
 * tests: written as tests, the optimiser recognises the permutation and
 * asks for a bit reversal, which is a good deal more code than this.
 * F_OK (no bits) asks only whether the path is there, for which a read is
 * the cheapest question. */
static const unsigned char os9_access_mode[8] = {
    OS9_READ,                             /* F_OK           */
    OS9_EXEC,                             /* X_OK           */
    OS9_WRITE,                            /* W_OK           */
    OS9_WRITE | OS9_EXEC,                 /* W_OK|X_OK      */
    OS9_READ,                             /* R_OK           */
    OS9_READ | OS9_EXEC,                  /* R_OK|X_OK      */
    OS9_READ | OS9_WRITE,                 /* R_OK|W_OK      */
    OS9_READ | OS9_WRITE | OS9_EXEC,      /* R_OK|W_OK|X_OK */
};

int
access(const char *path, int amode)
{
    int mode, path_num, err;

    mode = os9_access_mode[amode & (R_OK | W_OK | X_OK)];

    err = _os_open(path, mode, &path_num);
    if (err == OS9_E_FNA && !(amode & (R_OK | W_OK | X_OK)))
        return 0;                       /* it is there, just not readable */
    if (err != 0) {
        /* A directory needs the directory bit, and a file that exists but
         * denies this access is EACCES either way. */
        err = _os_open(path, mode | OS9_DIR, &path_num);
        if (err != 0)
            return __os9_errno(err);
    }
    _os_close(path_num);
    return 0;
}
