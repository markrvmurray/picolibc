/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2025 Mark Murray
 *
 * MC6809 stdio via ACIA memory-mapped I/O (USim).
 */

#include <stdio.h>
#include "mc6809-semihost.h"

static int
mc6809_putc(char c, FILE *file)
{
    (void)file;
    mc6809_putchar(c);
    return (unsigned char)c;
}

static int
mc6809_getc(FILE *file)
{
    (void)file;
    return mc6809_getchar();
}

static FILE __stdin  = FDEV_SETUP_STREAM(NULL, mc6809_getc, NULL, _FDEV_SETUP_READ);
static FILE __stdout = FDEV_SETUP_STREAM(mc6809_putc, NULL, NULL, _FDEV_SETUP_WRITE);

#ifdef __strong_reference
#define STDIO_ALIAS(x, y) __strong_reference(x, y);
#else
#define STDIO_ALIAS(x, y) FILE * const y = &__ ## x;
#endif

FILE * const stdin  = &__stdin;
FILE * const stdout = &__stdout;
STDIO_ALIAS(stdout, stderr);
