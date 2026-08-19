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

/* The heap the start-up code claimed (libclang_rt.os9's crt1: the pages the
 * kernel granted above the stack, or the linker script's static pool), a
 * bump allocator over [__os9_heap_cur, __os9_heap_end).  __os9_grow() is
 * the runtime's hook for asking the kernel for more (F$Mem); today it only
 * says no. */
extern int __os9_grow(unsigned bytes);

void *
sbrk(ptrdiff_t incr)
{
    char *cur = __os9_heap_cur;
    if (incr < 0) {
        errno = ENOMEM;
        return (void *)-1;
    }
    if ((size_t)(__os9_heap_end - cur) < (size_t)incr) {
        if (__os9_grow((unsigned)incr) != 0 ||
            (size_t)(__os9_heap_end - cur) < (size_t)incr) {
            errno = ENOMEM;
            return (void *)-1;
        }
    }
    __os9_heap_cur = cur + incr;
    return cur;
}
