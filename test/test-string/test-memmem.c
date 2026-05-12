/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2025 Keith Packard
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

#define _GNU_SOURCE
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <inttypes.h>

#ifdef __MSP430__
#define HAY_MAX    256
#define NEEDLE_MAX 16
#elif defined(__MC6809__)
/* MC6809 (USim ~30 Mcyc/s, MAME ~19 Mcyc/s emulating HD6309 at
 * 1800 sim-sec cap) wall-times past the 30 min PICOLIBC_TIMEOUT cap
 * at stock 2048/256/LOOPS=10 and still hits MAME's cap at the
 * 1024/64/LOOPS=6 first-pass trim. Measured: HAY_MAX=1024 reaches
 * only 2 of 4 outer iterations in 1800 sim sec on Og-hd6309-mame.
 *
 * Final sizing: 512 B haystack × 32 B needle is still substantially
 * larger than MSP430 (256/16) and exceeds typical short-string usage.
 * Alignment coverage (sizeof(long)==4 → 4×4 = 16 align combos) is
 * preserved unchanged. */
#define HAY_MAX    512
#define NEEDLE_MAX 32
#else
#define HAY_MAX    2048
#define NEEDLE_MAX 256
#endif

#define SEED 42

static uint8_t hay[HAY_MAX];
static uint8_t needle[NEEDLE_MAX];

/* Generate random uint8_t */
static uint8_t
rand_byte(void)
{
    return rand() & 0xff;
}

/* Generate random size_t */
static size_t
rand_size_t(void)
{
    size_t r = 0;
    size_t i;

    for (i = 0; i < SIZE_WIDTH; i += 8) {
        r |= (size_t)rand_byte() << i;
    }
    return r;
}

/* Generate random number between 0 and max inclusive */
static size_t
rand_range(size_t max)
{
    size_t mask = ~(size_t)0;
    size_t ret;

    if (max == 0)
        return 0;
    while (mask >> 1 > max)
        mask >>= 1;
    for (;;) {
        ret = rand_size_t() & mask;
        if (ret <= max)
            return ret;
    }
}

static size_t
rand_size(size_t max)
{
    for (;;) {
        size_t ret = rand_range(max);
        if (ret)
            return ret;
    }
}

static size_t
rand_pos(size_t max)
{
    return rand_range(max);
}

static size_t
rand_elt(size_t max)
{
    return rand_range(max - 1);
}

#ifdef __MSP430__
/* MSP430 emulator is rather slow */
#define LOOPS 4
#elif defined(__MC6809__)
/* MC6809 USim/MAME budget (matches MSP430 — LOOPS=4 is what fits
 * MAME's 1800 sim-sec cap with comfortable margin): 4³ = 64
 * randomised combinations per (hay_align, needle_align) pair × 16
 * align pairs = 1024 memmem pairs. */
#define LOOPS 4
#else
#define LOOPS 10
#endif

static void
generate_hay(uint8_t *dst, size_t size)
{
    while (size) {
        *dst++ = rand_byte();
        size--;
    }
}

static void
fixup_hay(uint8_t *hay, size_t hay_size, uint8_t *needle, size_t needle_size)
{
    uint8_t *fix_pos = &needle[rand_elt(needle_size)];
    uint8_t  fix = *fix_pos;

    while (hay_size) {
        if (*hay == fix)
            *hay ^= 0x55;
        hay++;
        hay_size--;
    }

    while (needle_size) {
        if (needle != fix_pos && *needle == fix)
            *needle ^= 0xaa;
        needle++;
        needle_size--;
    }
}

int
main(void)
{
    static size_t hay_start, hay_size;
    static size_t needle_start, needle_size;
    static size_t needle_pos;
    int           ret = 0;
    int           hay_size_loop, needle_size_loop;
    int           needle_pos_loop;

    srand(SEED);

    /* Check long alignments for hay and needle */
    for (hay_start = 0; hay_start < sizeof(long); hay_start++) {
        printf("loop %zd of %zd\n", hay_start, sizeof(long));

        for (needle_start = 0; needle_start < sizeof(long); needle_start++) {

            /* Generate LOOPS random hay sizes */
            for (hay_size_loop = 0; hay_size_loop < LOOPS; hay_size_loop++) {
                hay_size = rand_size(HAY_MAX - hay_start);
                size_t needle_max = NEEDLE_MAX - needle_start;
                if (needle_max > hay_size)
                    needle_max = hay_size;

                /* Generate LOOPS random needle sizes */
                for (needle_size_loop = 0; needle_size_loop < LOOPS; needle_size_loop++) {
                    needle_size = rand_size(needle_max);

                    /* Generate LOOPS random needle locations */
                    for (needle_pos_loop = 0; needle_pos_loop < LOOPS; needle_pos_loop++) {
                        needle_pos = rand_pos(hay_size - needle_size);

                        uint8_t *hay_cur = &hay[hay_start];
                        uint8_t *needle_cur = &needle[needle_start];

#if 0
                        printf("hay_start %zu hay_size %zu needle_start %zu needle_size %zu needle_pos %zu\n",
                               hay_start, hay_size, needle_start, needle_size, needle_pos);
#endif

                        /* Set up the data */
                        memset(hay, 0, HAY_MAX);
                        memset(needle, 0, NEEDLE_MAX);
                        generate_hay(hay_cur, hay_size);
                        generate_hay(needle_cur, needle_size);

                        /*
                         * Make sure the needle doesn't already exist in hay by
                         * adjusting both
                         */
                        fixup_hay(hay_cur, hay_size, needle_cur, needle_size);

                        /* Place the needle in the haystack */
                        memcpy(&hay_cur[needle_pos], needle_cur, needle_size);

                        uint8_t *result;

                        result = memmem(hay_cur, hay_size, needle_cur, needle_size);

                        if (result != hay_cur + needle_pos) {
                            printf("expected needle at %zu got %zu\n", needle_pos,
                                   result - hay_cur);
                            printf("hay_start %zu hay_size %zu needle_start %zu needle_size %zu "
                                   "needle_pos %zu\n",
                                   hay_start, hay_size, needle_start, needle_size, needle_pos);
                            ret = 1;
                        }

                        /*
                         * Introduce a single bit error in the hay
                         * so that it no longer matches anywhere
                         */
                        size_t damage_pos = rand_elt(needle_size);
                        hay_cur[needle_pos + damage_pos] ^= 1;

                        result = memmem(hay_cur, hay_size, needle_cur, needle_size);

                        if (result != NULL) {
                            printf("expected no needle, got %zu\n", result - hay_cur);
                            printf("hay_start %zu hay_size %zu needle_start %zu needle_size %zu "
                                   "needle_pos %zu\n",
                                   hay_start, hay_size, needle_start, needle_size, needle_pos);
                            ret = 1;
                        }
                    }
                }
            }
        }
    }
    return ret;
}
