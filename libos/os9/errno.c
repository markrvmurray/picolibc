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

/* OS-9 error codes (os9.d, E$*) to errno values. */
static const unsigned char os9_errno_table[] = {
    [OS9_E_PTHFUL - 0xC8] = EMFILE,   /* path table full */
    [OS9_E_BPNUM  - 0xC8] = EBADF,    /* bad path number */
    [0xCA         - 0xC8] = EBUSY,    /* polling table full */
    [OS9_E_BMODE  - 0xC8] = EACCES,   /* bad mode */
    [0xCC         - 0xC8] = ENFILE,   /* device table overflow */
    [0xCD         - 0xC8] = ENOEXEC,  /* bad module id */
    [0xCE         - 0xC8] = ENOSPC,   /* module directory full */
    [OS9_E_MEMFUL - 0xC8] = ENOMEM,   /* process memory full */
    [OS9_E_UNKSVC - 0xC8] = ENOSYS,   /* unknown service code */
    [0xD1         - 0xC8] = EBUSY,    /* module busy */
    [0xD2         - 0xC8] = EFAULT,   /* bad page address */
    [OS9_E_EOF    - 0xC8] = 0,        /* end of file is not an error */
    [OS9_E_NES    - 0xC8] = EINVAL,   /* non-existing segment */
    [OS9_E_FNA    - 0xC8] = EACCES,   /* file not accessible */
    [OS9_E_BPNAM  - 0xC8] = ENOENT,   /* bad path name */
    [OS9_E_PNNF   - 0xC8] = ENOENT,   /* path name not found */
    [OS9_E_SLF    - 0xC8] = ENOSPC,   /* segment list full */
    [OS9_E_CEF    - 0xC8] = EEXIST,   /* creating existing file */
    [OS9_E_IBA    - 0xC8] = EIO,      /* illegal block address */
    [OS9_E_HANGUP - 0xC8] = EPIPE,    /* carrier detect lost */
    [OS9_E_MNF    - 0xC8] = ENOENT,   /* module not found */
    [OS9_E_DELSP  - 0xC8] = EINVAL,   /* deleting stack pointer memory */
    [OS9_E_IPRCID - 0xC8] = ESRCH,    /* illegal process id */
    [OS9_E_NOCHLD - 0xC8] = ECHILD,   /* no children */
    [0xE3         - 0xC8] = EINVAL,   /* illegal SWI code */
    [0xE4         - 0xC8] = EINTR,    /* process aborted */
    [0xE5         - 0xC8] = EAGAIN,   /* process table full */
    [0xE6         - 0xC8] = EINVAL,   /* illegal fork parameter */
    [OS9_E_KWNMOD - 0xC8] = EEXIST,   /* known module */
    [0xE8         - 0xC8] = ENOEXEC,  /* bad module CRC */
    [0xE9         - 0xC8] = EINTR,    /* unprocessed signal pending */
    [OS9_E_NEMOD  - 0xC8] = ENOENT,   /* non-existing module */
    [OS9_E_BNAM   - 0xC8] = EINVAL,   /* bad name */
    [0xEC         - 0xC8] = ENOEXEC,  /* bad module header parity */
    [OS9_E_NORAM  - 0xC8] = ENOMEM,   /* no system RAM */
    [OS9_E_DNE    - 0xC8] = ENOTEMPTY,/* directory not empty */
    [OS9_E_NOTASK - 0xC8] = EAGAIN,   /* no task number */
    [OS9_E_UNIT   - 0xC8] = ENXIO,    /* illegal unit */
    [OS9_E_SECT   - 0xC8] = EIO,      /* bad sector number */
    [OS9_E_WP     - 0xC8] = EROFS,    /* write protect */
    [OS9_E_CRC    - 0xC8] = EIO,      /* bad check sum */
    [OS9_E_READ   - 0xC8] = EIO,      /* read error */
    [OS9_E_WRITE  - 0xC8] = EIO,      /* write error */
    [OS9_E_NOTRDY - 0xC8] = EAGAIN,   /* device not ready */
    [OS9_E_SEEK   - 0xC8] = ESPIPE,   /* seek error */
    [OS9_E_FULL   - 0xC8] = ENOSPC,   /* media full */
    [OS9_E_BTYP   - 0xC8] = EINVAL,   /* bad type (incompatible media) */
    [OS9_E_DEVBSY - 0xC8] = EBUSY,    /* device busy */
    [OS9_E_DIDC   - 0xC8] = EIO,      /* disk id change */
    [OS9_E_LOCK   - 0xC8] = EAGAIN,   /* record is locked */
    [OS9_E_SHARE  - 0xC8] = EBUSY,    /* non-sharable file busy */
    [OS9_E_DEADLK - 0xC8] = EDEADLK,  /* I/O deadlock */
};

int
__os9_errno(int os9_code)
{
    unsigned idx = (unsigned)os9_code - 0xC8;
    int e = EIO;
    if (idx < sizeof(os9_errno_table) && os9_errno_table[idx] != 0)
        e = os9_errno_table[idx];
    errno = e;
    return -1;
}
