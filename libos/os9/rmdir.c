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
#include <unistd.h>

/* OS-9 will not delete a file that has the directory attribute set: I$Delete
 * answers E$FNA.  Its own `deldir` does not try -- it runs `attr -d` first,
 * which clears the bit by writing the file descriptor sector back (attr.asm).
 * So removing a directory takes:
 *
 *     I$Open   the directory, DIR + UPDATE
 *     I$GetStt SS.FD           read the descriptor sector
 *     clear the directory bit in FD.ATT, its first byte
 *     I$SetStt SS.FD, 1 byte   write just the attribute back
 *     I$Close
 *     I$Delete
 *
 * Emptiness is checked first, deliberately: the attribute has to go before
 * the delete is allowed, and a directory that lost its attribute and then
 * failed to be deleted would be left behind as a plain file.  If the delete
 * fails anyway, the attribute goes back. */
#define FD_ATT  0                               /* rbf.d: FD.ATT */
#define FD_SEG  16                              /* rbf.d: FD.SEG, the header */
#define ATT_DIR 0x80                            /* os9.d: DIR. */

/* Anything of its own in it?  The first two entries are "." and "..", and an
 * entry that has been deleted has a zero first byte. */
static int
not_empty(int fd)
{
    unsigned char entry[OS9_DIRENT_SIZE];
    int n, seen = 0;

    for (;;) {
        n = OS9_DIRENT_SIZE;
        if (_os_read(fd, entry, &n) != 0 || n != OS9_DIRENT_SIZE)
            return 0;                           /* the end of the directory */
        if (entry[0] == 0)
            continue;                           /* a deleted entry */
        if (++seen > 2)
            return 1;
    }
}

static int
set_dir_bit(int fd, unsigned char *fdsec, int on)
{
    if (on)
        fdsec[FD_ATT] |= ATT_DIR;
    else
        fdsec[FD_ATT] &= (unsigned char)~ATT_DIR;
    return _os_setstt_buf(fd, OS9_SS_FD, fdsec, 1);
}

int
rmdir(const char *path)
{
    unsigned char fdsec[FD_SEG];
    int fd, err, keep;

    err = _os_open(path, OS9_DIR | OS9_UPDATE, &fd);
    if (err != 0)
        return OS9_RESULT(err);

    if (not_empty(fd)) {
        _os_close(fd);
        errno = ENOTEMPTY;
        return -1;
    }

    err = _os_getstt_buf(fd, OS9_SS_FD, fdsec, sizeof fdsec);
    if (err == 0 && (fdsec[FD_ATT] & ATT_DIR) != 0)
        err = set_dir_bit(fd, fdsec, 0);
    _os_close(fd);
    if (err != 0)
        return OS9_RESULT(err);

    err = _os_delete(path);
    if (err == 0)
        return 0;

    /* Better a directory that would not go than a directory that is no
     * longer one. */
    keep = errno;
    if (_os_open(path, OS9_UPDATE, &fd) == 0) {
        if (_os_getstt_buf(fd, OS9_SS_FD, fdsec, sizeof fdsec) == 0)
            (void)set_dir_bit(fd, fdsec, 1);
        _os_close(fd);
    }
    errno = keep;
    return -1;
}
