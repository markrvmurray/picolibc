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
 * Bug #289 (2026-05-24): MC6809 <fenv.h> backed by the MC6839 FP
 * coprocessor's Floating-Point Control Blocks (FPCBs).  Function
 * bodies live in compiler-rt/lib/builtins/mc6809/fenv.c — they read
 * and write the FPCB[FP_STAT] and FPCB[FP_CTRL] bytes directly to
 * implement exception query/clear and rounding-mode control.
 *
 * Constants are defined to MATCH the FPCB hardware bit layout so the
 * function bodies' mask operations are trivial (no translation).
 *
 *   FE_*           exception bits in FPCB[FP_STAT] (per nfp09-abi.s)
 *   FE_TONEAREST.. rounding bits 2:1 of FPCB[FP_CTRL]
 */

#ifndef _MACHINE_FENV_H_
#define _MACHINE_FENV_H_

#include <sys/cdefs.h>

_BEGIN_STD_C

typedef unsigned char fexcept_t;

typedef struct {
    unsigned char round_mode;    /* FPCB[FP_CTRL] bits 2:1 */
    unsigned char except_flags;  /* FE_* mask */
} fenv_t;

/* Exception bits — match FPCB[FP_STAT] hardware layout */
#define FE_INVALID    0x01  /* IOP — invalid operation */
#define FE_OVERFLOW   0x02  /* OVF */
#define FE_UNDERFLOW  0x04  /* UNF */
#define FE_DIVBYZERO  0x08  /* DZ  */
#define FE_INEXACT    0x40  /* INX */

#define FE_ALL_EXCEPT (FE_INVALID | FE_OVERFLOW | FE_UNDERFLOW | \
                       FE_DIVBYZERO | FE_INEXACT)

/* Rounding modes — match FPCB[FP_CTRL] bits 2:1 hardware encoding */
#define FE_TONEAREST  0x00
#define FE_TOWARDZERO 0x02
#define FE_UPWARD     0x04
#define FE_DOWNWARD   0x06

/* Default FP environment (zero-initialised round-to-nearest, no
 * pending exceptions).  Provide a real fenv_t so user code can pass
 * &FE_DFL_ENV to fesetenv. */
extern const fenv_t __fe_dfl_env;
#define FE_DFL_ENV (&__fe_dfl_env)

/* C99 fenv functions — bodies in compiler-rt fenv.c. */
int feclearexcept(int);
int fegetexceptflag(fexcept_t *, int);
int feraiseexcept(int);
int fesetexceptflag(const fexcept_t *, int);
int fetestexcept(int);

int fegetround(void);
int fesetround(int);

int fegetenv(fenv_t *);
int feholdexcept(fenv_t *);
int fesetenv(const fenv_t *);
int feupdateenv(const fenv_t *);

/* Non-standard MC6839 runtime entry-point setup.  Bare-metal binaries
 * never need to call this — the FP libcall entry-points are initialised
 * at .data load time to the embedded Float09 ROM image.  OS-9 crt0
 * calls F$Link("Float09") and passes the dynamic load address here. */
void __mc6839_set_rom_base(void *base);

_END_STD_C

#endif /* _MACHINE_FENV_H_ */
