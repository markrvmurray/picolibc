/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
 *
 * Bug #254: MC6809 sqrtf override that delegates to MC6839 ROM.
 *
 * picolibc's portable sf_sqrt.c is a slow bit-by-bit fdlibm
 * implementation. At -O2 HD6309 it ULP-fails by 9999 on subnormal
 * inputs (entry 1 of the test-sqrt vector table). compiler-rt's
 * __sqrtsf2 is a direct dispatch to MC6839's FPOP_FSQRT, which is
 * a single-pass IEEE-754 hardware-style square root that handles
 * the full domain (NaN, +/-inf, +/-0, negative, subnormal). Using
 * it gives correct + much faster results in one go.
 *
 * Note: __sqrtsf2 is the GCC libcall name (binary fp), not the
 * IEEE name `sqrtf`. We just rename it.
 */

#include "fdlibm.h"

extern float __sqrtsf2(float);

float
sqrtf(float x)
{
    return __sqrtsf2(x);
}

_MATH_ALIAS_f_f(sqrt)
