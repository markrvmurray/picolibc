#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright © 2026 Mark Murray
#
# Bug #162 — end-to-end execution tests for MC6839 soft-float wrappers.
# Exercises f32/f64 arithmetic, sqrt, comparisons, and type conversions
# via the __addsf3 / __mulsf3 / ... libcalls backed by the MC6839 ROM.
#
# MC6809-only: the ROM and calling-convention wrappers only exist for
# the mc6809 target.
#

if host_cpu_family != 'mc6809'
  subdir_done()
endif

tests_mc6809_fp = [
  'test-mc6809-fp',
  'clzsi2_test',
  'sqrt_probe',
  'bcd_probe',  # Bug #230 BINDEC/DECBIN smoke
  'divsi_signed_probe',  # Bug ba5485d HD6309 DIVQ signed i32 fast paths
  'muldi_probe',  # Bug aacd0b7 HD6309 MULD i64 multiply
  'divdi_probe',  # Bug f98d69f HD6309 DIVQ i64 divide/mod
  'umulodi4_probe',  # Bug #239 __umulodi4 i64 multiply with overflow
  'ctzsi2_test',  # Bug #245 hand-written __ctzsi2
  'timegm_rem_probe',  # Bug #247 HD6309-LTO i64 rem decomposition
  'bug301_min_probe',  # Bug #301 minimal gmtime(t=0) probe
  'bug326_min_probe',  # Bug #326 TFM-memcpy/memset correctness probe
  'bug329_init_array_probe',  # Bug #329 .init_array runs C++ global ctors (here via __attribute__((constructor)))
  'muld_neg60_probe',  # Bug #247 narrowed MULD #-60 probe
  'bug311_va_probe',  # Bug #311 vararg-printf u64 isolation probe
  'bug311_va_isolate',  # Bug #311 va_list ABI vs vfprintf isolation
  'bug311_va_min',  # Bug #311 minimal single-vfprintf probe for gdb
  'bug311_lx_min',  # Bug #311 minimal single-%lx probe for gdb
  'udivsi3_probe',  # Bug #311 direct __udivsi3 verification
  'sizes_probe',  # Bug #311 sizeof check
  'i64_add_carry_probe',  # Bug #311 Phase 2 i64-add-narrowed-to-i32-carry-chain probe
]

foreach params : targets
  target = params['name']
  target_dir = params['dir']
  target_c_args = params['c_args']

  _libs = [get_variable('lib_c' + target)]
  if is_variable(oslib_test + target)
    _libs += [get_variable(oslib_test + target)]
  endif
  if is_variable(libfallback_var + target)
    _libs += [get_variable(libfallback_var + target)]
  endif

  _lib_files=[]
  foreach _lib : _libs
    _lib_files += _lib.full_path()
  endforeach

  if is_variable(crt0_test + target)
    _objs = [get_variable(crt0_test + target)]
  else
    _objs = []
  endif

  _c_args = target_c_args + get_variable('test_c_args' + target, test_c_args)
  _link_args = target_c_args + _lib_files + get_variable('test_link_args' + target, test_link_args)
  _link_depends = get_variable('test_link_depends' + target, test_link_depends) + _libs

  foreach t1 : tests_mc6809_fp
    t1_src = t1 + '.c'
    test_file_name_arg=['-DTEST_FILE_NAME="' + t1 + target + '.txt"']

    test(t1 + target,
         executable(t1 + target, [t1_src] + test_lock_valid,
                    c_args: printf_compile_args_d + test_file_name_arg + _c_args,
                    link_args: printf_link_args_d + _link_args,
                    objects: _objs,
                    link_depends: _link_depends,
                    include_directories: inc),
         depends: bios_bin,
         timeout: 60,
         suite: 'test-mc6809-fp',
         env: test_env)
  endforeach

endforeach
