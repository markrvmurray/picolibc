/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2024, Synopsys Inc.
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

#define __STDC_WANT_LIB_EXT1__ 1
#include "local-stdio.h"
#include "../stdlib/local_s.h"

#define VFPRINTF_S
#ifdef _VFPRINTF_S_INTEGER
/* Bug #342: this libc was configured without a double printf engine
 * (integer-only stdio — the double __dtoa_engine is not built). Left to
 * its default, vfprintf.c builds vfprintf_s as the __IO_VARIANT_DOUBLE
 * engine and references __dtoa_engine, making every *printf_s
 * (sprintf_s / snprintf_s / vfprintf_s / ...) unlinkable. Build it with
 * the platform's configured default variant instead — no dtoa, and it
 * matches plain printf's capability. Defining PRINTF_NAME also skips
 * vfprintf.c's "#ifndef PRINTF_NAME" block that would otherwise force
 * PRINTF_VARIANT back to DOUBLE; the name itself is unused because the
 * VFPRINTF_S path names its own vfprintf_s function and (since
 * PRINTF_VARIANT == __IO_DEFAULT here) no PRINTF_NAME alias is emitted.
 * At -fp this macro is unset, so the double engine is kept and %f via
 * *printf_s still works. */
#define PRINTF_VARIANT __IO_DEFAULT
#define PRINTF_NAME    __vfprintf_s_variant
#endif
#include "vfprintf.c"
