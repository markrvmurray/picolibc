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

/* What is above the program is the heap -- if BASIC has been told to stay
 * out of it.  A CoCo program loaded at, say, $3F00 is expected to be preceded
 * by
 *
 *     CLEAR 200,&H3EFF
 *
 * which moves BASIC's string space and stack *below* that address and leaves
 * everything above for the program.  Without it BASIC's variables, strings
 * and stack sit exactly where this heap would grow, and a program that
 * allocates walks over them: it runs, it prints, and then it wrecks BASIC on
 * the way out.  That failure was found on a real CoCo 3 under MAME -- printf
 * worked and the machine died on the return -- and it is why this asks BASIC
 * where its memory ends rather than guessing from the stack pointer.
 *
 * Two of BASIC's own words say what is where:
 *
 *   [$0074]  the top of RAM              ($7FFF on the machines tested)
 *   [$0027]  the top of BASIC's memory, which is what CLEAR moves
 *
 * so the heap runs from the end of the program to the top of RAM, and there
 * is no heap at all if BASIC still claims that space.
 */
extern char __heap_start[];

#define DECB_TOP_OF_RAM   (*(char **)0x0074)
#define DECB_TOP_OF_BASIC (*(char **)0x0027)

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
        heap_cur = __heap_start;
        if (DECB_TOP_OF_BASIC >= heap_cur) {
            /* BASIC still owns this memory: no CLEAR, or not enough of one.
             * Refusing is the kind failure -- allocating here would corrupt
             * the interpreter the program has to return to. */
            heap_end = heap_cur;
        } else {
            heap_end = DECB_TOP_OF_RAM - DECB_STACK_RESERVE;
            if (heap_end < heap_cur)
                heap_end = heap_cur;
        }
    }
    if (incr < 0 || (size_t)(heap_end - heap_cur) < (size_t)incr) {
        errno = ENOMEM;
        return (void *)-1;
    }
    prev = heap_cur;
    heap_cur += incr;
    return prev;
}
