#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright © 2026 Mark Murray
#
# Bug #192 — end-to-end execution tests for MC6809 user-DP support
# (__attribute__((directpage)) globals → addrspace(1) → DP-mode
# load/store opcodes against linker-assigned $0021-$00FF addresses).
#
# Tests are MC6809-only by construction — the attribute is rejected
# by Sema on every other target, so we gate the whole suite at the
# host_cpu_family level.
#

if host_cpu_family != 'mc6809'
  subdir_done()
endif

tests_mc6809_dp = [
  'test-dp-byte',
  'test-dp-word',
  'test-dp-multi',
  'test-dp-init-data',
  'test-dp-init-bss',
  'test-dp-static-local',
  'test-dp-mixed',
  'test-dp-loop',
  # Bug #161 Phase 1 / 1.5 sentinel — corner cases for HD6309-MULD
  # __mulhi3 / __mulsi3. Lives here for now (no FP, integer-only, runs
  # under the same MC6809-only gating).
  'test-mc6809-mul-corners',
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

  foreach t1 : tests_mc6809_dp
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
         suite: 'test-mc6809-dp',
         env: test_env)
  endforeach

endforeach
