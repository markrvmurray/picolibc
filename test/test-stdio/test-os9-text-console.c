/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Boisy G. Pitre
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

/* Checked against the raw emulator log by run-mc6809-os9.
 * Explicit CR models the native OS-9 compiler newline even with RC2.
 * Octal 012 remains a physical LF when the compiler changes \n to CR. */
#define _DEFAULT_SOURCE
#include <stdio.h>
#include <unistd.h>

int
main(void)
{
    char small[7];
    FILE *binary;
    if (!isatty(1) || !isatty(2))
        return 77;
    if (setvbuf(stdout, small, _IOFBF, sizeof(small)) ||
        fputs("LINEONE\rLINETWO\r", stdout) < 0 ||
        fwrite("LINETHREE\r", 1, 10, stdout) != 10 || fflush(stdout))
        return 1;
    if (fputs("CONSOLE STDERR\r", stderr) < 0 || fflush(stderr))
        return 2;
    if (write(1, "CONSOLE RAW\012", 12) != 12)
        return 3;
    binary = fdopen(1, "wb");
    if (!binary || fputs("CONSOLE BINARY\012", binary) < 0 || fclose(binary))
        return 4;
    return 0;
}
