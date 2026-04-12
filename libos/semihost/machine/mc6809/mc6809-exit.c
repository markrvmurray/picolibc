/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2025 Mark Murray
 *
 * MC6809 _exit via halt device (USim).
 * The written byte value becomes the process exit code.
 */

#include <stdlib.h>
#include <unistd.h>
#include "mc6809-semihost.h"

__noreturn void
_exit(int code)
{
    MC6809_HALT = (uint8_t)code;
    while (1)
        ;
}
