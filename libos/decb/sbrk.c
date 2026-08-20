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
#include "decb-private.h"
#include <unistd.h>

/* What is above the program is the heap.  The linker script marks where it
 * starts; where it ends is BASIC's stack coming down to meet it, and there
 * is no supported way to ask BASIC how far down that is -- so the ceiling is
 * a fixed reserve below the current stack, taken once.  A program that wants
 * more should say so with a bigger reserve at link time. */
extern char __heap_start[];

#ifndef DECB_STACK_RESERVE
#define DECB_STACK_RESERVE 512
#endif

static char *heap_cur;
static char *heap_end;

void *
sbrk(ptrdiff_t incr)
{
    char *prev;

    if (heap_cur == NULL) {
        char *sp;
        __asm__ __volatile__("sts %0" : "=m"(sp));
        heap_cur = __heap_start;
        heap_end = sp - DECB_STACK_RESERVE;
        if (heap_end < heap_cur)
            heap_end = heap_cur;
    }
    if (incr < 0 || (size_t)(heap_end - heap_cur) < (size_t)incr) {
        errno = ENOMEM;
        return (void *)-1;
    }
    prev = heap_cur;
    heap_cur += incr;
    return prev;
}
