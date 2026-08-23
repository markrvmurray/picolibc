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
#include <sys/statvfs.h>

/* OS-9 has no call that describes a filesystem, so this is what `free`
 * does (level1/cmds/free.asm): open the whole device as a file -- the
 * device name with `@` after it -- and read its identification sector,
 * which counts the sectors and sizes the allocation bitmap; the bitmap
 * itself, from sector 1 on, has a bit per cluster, clear when the cluster
 * is free.  The device behind an open path comes from SS.DevNm, so a path
 * and a descriptor are answered the same way. */
#define OS9_SECTOR 256                          /* an RBF logical sector */

/* The identification sector (rbf.d). */
#define DD_TOT 0                                /* total sectors, 3 bytes */
#define DD_MAP 4                                /* bitmap size in bytes */
#define DD_BIT 6                                /* sectors per cluster */
#define DD_DSK 14                               /* disk ID */
#define DD_ID_LEN 16

#define DT_RBF 1                                /* PD.DTP: an RBF device */

/* Free clusters are clear bits. */
static const unsigned char zero_bits[16] = { 4, 3, 3, 2, 3, 2, 2, 1,
                                             3, 2, 2, 1, 2, 1, 1, 0 };

__attribute__((noinline)) static unsigned long
count_free(int dev, unsigned long map_bytes)
{
    unsigned char buf[64];
    unsigned long free_clusters = 0;
    int i, n;

    while (map_bytes != 0) {
        n = map_bytes < sizeof buf ? (int)map_bytes : (int)sizeof buf;
        if (_os_read(dev, buf, &n) != 0 || n <= 0)
            break;
        for (i = 0; i < n; i++)
            free_clusters += zero_bits[buf[i] & 0x0F] + zero_bits[buf[i] >> 4];
        map_bytes -= (unsigned long)n;
    }
    return free_clusters;
}

/* "/<device>@", the whole disk as a file, for the device `fd` is open on. */
__attribute__((noinline)) static int
device_path(int fd, char *path)
{
    unsigned char dev[32];
    int err, len;

    err = _os_getstt_buf(fd, OS9_SS_DEVNM, dev, sizeof dev);
    if (err != 0)
        return err;
    path[0] = '/';
    len = __os9_entry_name(dev, path + 1);
    path[1 + len] = '@';
    path[2 + len] = '\0';
    return 0;
}

__attribute__((noinline)) static int
read_device(int dev, struct statvfs *buf)
{
    unsigned char id[DD_ID_LEN];
    struct __os9_xu xu;
    unsigned long total, map_bytes, per_cluster, avail;
    int n = sizeof id;

    if (_os_read(dev, id, &n) != 0 || n != sizeof id)
        return __os9_errno(errno);
    total = ((unsigned long)id[DD_TOT] << 16) |
            ((unsigned long)id[DD_TOT + 1] << 8) | id[DD_TOT + 2];
    map_bytes = ((unsigned long)id[DD_MAP] << 8) | id[DD_MAP + 1];
    per_cluster = ((unsigned long)id[DD_BIT] << 8) | id[DD_BIT + 1];
    if (per_cluster == 0)
        per_cluster = 1;

    xu.path = (unsigned char)dev;
    xu.fn = 0;
    xu.pos = OS9_SECTOR;                        /* the bitmap starts at LSN 1 */
    if (__os9_seek(&xu) != 0)
        return __os9_errno(errno);
    avail = count_free(dev, map_bytes) * per_cluster;
    /* The bitmap is a whole number of bytes, so its last bits can describe
     * clusters the disk does not have; format marks them allocated, but do
     * not trust that over the sector count. */
    if (avail > total)
        avail = total;

    /* Everything is counted in sectors (f_frsize); f_bsize is what a caller
     * should read and write in, and a hard disc's physical sector is two of
     * ours.  OS-9 has no table of file serial numbers -- a file costs at
     * least the sector its descriptor lives in, so the sector count is also
     * the ceiling on the number of files. */
    buf->f_bsize = 2 * OS9_SECTOR;
    buf->f_frsize = OS9_SECTOR;
    buf->f_blocks = total;
    buf->f_bfree = avail;
    buf->f_bavail = avail;
    buf->f_files = total;
    buf->f_ffree = avail;
    buf->f_favail = avail;
    buf->f_fsid = ((unsigned long)id[DD_DSK] << 8) | id[DD_DSK + 1];
    buf->f_flag = ST_NOSUID;                    /* OS-9 has no set-user-id */
    buf->f_namemax = OS9_DIRENT_NAME;
    return 0;
}

/* The device an open path lives on, described. */
static int
statvfs_of_path(int fd, struct statvfs *buf)
{
    unsigned char opts[32];
    char path[OS9_DIRENT_NAME + 3];
    int dev, err;

    if (_os_getstt_buf(fd, OS9_SS_OPT, opts, sizeof opts) == 0 &&
        opts[0] != DT_RBF) {
        errno = ENOTSUP;                        /* not a disk */
        return -1;
    }
    err = device_path(fd, path);
    if (err == 0)
        err = _os_open(path, OS9_READ, &dev);
    if (err != 0)
        return __os9_errno(err);
    err = read_device(dev, buf);
    _os_close(dev);
    return err;
}

int
fstatvfs(int fd, struct statvfs *buf)
{
    return statvfs_of_path(fd, buf);
}

int
statvfs(const char *path, struct statvfs *buf)
{
    int fd, err;

    /* Any path on the device will do, and a directory needs telling. */
    err = _os_open(path, OS9_READ, &fd);
    if (err != 0)
        err = _os_open(path, OS9_DIR | OS9_READ, &fd);
    if (err != 0)
        return __os9_errno(err);
    err = statvfs_of_path(fd, buf);
    _os_close(fd);
    return err;
}
