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

/*
 * The OS-9 (NitrOS-9) system library: the POSIX calls picolibc expects,
 * built on the one-instruction system-call shims of <os9.h> (from the
 * LLVM-MC6809 OS-9 runtime, libclang_rt.os9).  Path numbers are file
 * descriptors: 0/1/2 are stdin/stdout/stderr as the shell forked them.
 */

#ifndef _OS9_PRIVATE_H_
#define _OS9_PRIVATE_H_

#define _DEFAULT_SOURCE
#include <errno.h>
#include <stdint.h>
#include <sys/types.h>

/* picolibc's errno macro and the runtime header's `extern int errno`
 * name the same plain int (no TLS on this target); hide the header's
 * declaration behind the macro. */
#include <os9.h>
#ifndef NITROS9_OS_CALLS_ABI
#error "OS-9 requires LLVM built with the canonical nitros9-c-libs OS calls"
#endif

/* Turn an OS-9 error code into errno and -1 (the shims have already left
 * the OS-9 code in errno; overwrite it with the POSIX value). */
int __os9_errno(int os9_code);

/* Descriptor-backed stdio text output; raw write remains byte-preserving. */
ssize_t __os9_text_write(int fd, const void *buf, size_t count);

/* A call that returns 0 or an OS-9 code, as a POSIX -1/errno result. */
#define OS9_RESULT(call) ((call) == 0 ? 0 : __os9_errno(errno))

#endif /* _OS9_PRIVATE_H_ */
