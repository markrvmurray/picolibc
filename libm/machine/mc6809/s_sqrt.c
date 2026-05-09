/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
 *
 * Bug #254 (companion): MC6809 sqrt(double) override that delegates
 * to MC6839 ROM (FPOP_FSQRT, double precision dispatch). picolibc's
 * portable s_sqrt.c is the same bit-by-bit fdlibm code as the float
 * version; using __sqrtdf2 (MC6839) avoids the same class of issues
 * and is much faster.
 */

#include "fdlibm.h"

extern double __sqrtdf2(double);

double
sqrt(double x)
{
    return __sqrtdf2(x);
}

_MATH_ALIAS_d_d(sqrt)
