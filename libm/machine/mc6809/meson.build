#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright © 2026 Mark Murray
#
# Bug #254: MC6809 libm overrides — sqrtf/sqrt delegate to MC6839 ROM
# via compiler-rt's __sqrtsf2 / __sqrtdf2 dispatchers. Replaces the
# slow bit-by-bit fdlibm sf_sqrt.c / s_sqrt.c with a single ROM call,
# fixing a -O2 HD6309-mame-fp ULP=9999 regression on subnormal inputs.
#
srcs_libm_machine = [
  's_sqrt.c',
  'sf_sqrt.c',
]

src_libm_machine = files(srcs_libm_machine)
