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
#include <sys/stat.h>
#include <time.h>

/* The file descriptor sector (rbf.d): attributes, owner, the date last
 * modified as year-1900, month, day, hour, minute, the link count, the
 * size, and the creation date as year-1900, month, day. */
#define FD_ATT   0
#define FD_DAT   3
#define FD_LNK   8
#define FD_SIZ   9
#define FD_CREAT 13
#define FD_LEN   16

static time_t
fd_time(const unsigned char *d, int have_time)
{
    struct tm tm;

    if (d[0] == 0 && d[1] == 0)
        return 0;
    tm.tm_year = d[0];
    tm.tm_mon = d[1] - 1;
    tm.tm_mday = d[2];
    tm.tm_hour = have_time ? d[3] : 0;
    tm.tm_min = have_time ? d[4] : 0;
    tm.tm_sec = 0;
    tm.tm_isdst = 0;
    return mktime(&tm);
}

/* What a path can say about itself: its length (SS.Size) and the kind of
 * device behind it (the path options' first byte, PD.DTP: 0 = a character
 * device such as the terminal, 1 = a file on an RBF disk).  Ownership,
 * links and times are not modelled. */
int
__os9_fstat(int fd, struct stat *st)
{
    unsigned char opts[32];
    unsigned char desc[FD_LEN];
    struct __os9_xu xu = { (unsigned char)fd, OS9_SS_SIZE, 0 };
    int is_file = 1;

    if (_os_getstt_buf(fd, OS9_SS_OPT, opts, sizeof opts) == 0)
        is_file = opts[0] != 0;

    st->st_size = 0;
    st->st_nlink = 1;
    st->st_atime = st->st_mtime = st->st_ctime = 0;
    if (is_file) {
        if (__os9_getstt_xu(&xu) != 0)
            return __os9_errno(errno);
        st->st_size = (off_t)xu.pos;
        st->st_mode = S_IFREG | 0644;
        /* The file descriptor sector carries the attributes, the link count
         * and the dates.  A device that is not a disk file has none, so a
         * failure here leaves the defaults. */
        if (_os_getstt_buf(fd, OS9_SS_FD, desc, sizeof desc) == 0) {
            st->st_mode = (desc[FD_ATT] & OS9_DIR) ? (S_IFDIR | 0755)
                                                   : (S_IFREG | 0644);
            st->st_nlink = desc[FD_LNK];
            st->st_mtime = fd_time(desc + FD_DAT, 1);
            st->st_ctime = fd_time(desc + FD_CREAT, 0);
            /* OS-9 does not record an access time; the last write is the
             * closest thing to one. */
            st->st_atime = st->st_mtime;
        }
    } else {
        st->st_mode = S_IFCHR | 0666;
    }
    st->st_uid = 0;
    st->st_gid = 0;
    st->st_dev = 0;
    st->st_rdev = 0;
    st->st_ino = 0;
    st->st_blksize = 256;                       /* an OS-9 disk sector */
    st->st_blocks = (st->st_size + 255) / 256;
    return 0;
}

int
fstat(int fd, struct stat *st)
{
    return __os9_fstat(fd, st);
}
