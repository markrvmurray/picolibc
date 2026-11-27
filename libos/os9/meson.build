#
# SPDX-License-Identifier: BSD-3-Clause
#
# Copyright © 2026 Mark Murray
#
# Redistribution and use in source and binary forms, with or without
# modification, are permitted provided that the following conditions
# are met:
#
# 1. Redistributions of source code must retain the above copyright
#    notice, this list of conditions and the following disclaimer.
#
# 2. Redistributions in binary form must reproduce the above
#    copyright notice, this list of conditions and the following
#    disclaimer in the documentation and/or other materials provided
#    with the distribution.
#
# 3. Neither the name of the copyright holder nor the names of its
#    contributors may be used to endorse or promote products derived
#    from this software without specific prior written permission.
#
# THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
# "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
# LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS
# FOR A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE
# COPYRIGHT HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT,
# INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES
# (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS OR
# SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
# HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT,
# STRICT LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE)
# ARISING IN ANY WAY OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED
# OF THE POSSIBILITY OF SUCH DAMAGE.
#

# OS-9 / NitrOS-9 (mc6809-unknown-os9): the POSIX system calls over the OS-9
# kernel.  The start-up code (crt0 + the F$Mem heap claim + argc/argv), the
# canonical nitros9-c-libs OS-call headers and the module linker script come from the
# LLVM-MC6809 OS-9 runtime (libclang_rt.os9, added to every link by the clang
# driver for this triple); the cross file names the linker script in the
# `os9_lds` property so the tests can link with it.

has_os_os9 = true
has_os_library = true
has_os_fileio = true

os9_lds = meson.get_external_property('os9_lds', '')
if os9_lds == ''
  error('os-os9 needs the `os9_lds` cross-file property: the path of mc6809-os9.lds in the LLVM resource directory')
endif
picolibc_os9_ld = files(os9_lds)[0]

srcs_os9 = [
  'access.c',
  'chdir.c',
  'close.c',
  'dirent.c',
  'errno.c',
  'fstat.c',
  'ftruncate.c',
  'getcwd.c',
  'getentropy.c',
  'getpid.c',
  'gettimeofday.c',
  'isatty.c',
  'kill.c',
  'lseek.c',
  'mkdir.c',
  'os9-dir.c',
  'open.c',
  'read.c',
  'sbrk.c',
  'rename.c',
  'rmdir.c',
  'stat.c',
  'statvfs.c',
  'unlink.c',
  'write.c',
]

# The calls this layer does not (yet) make: picolibc's fakes, which the tests
# recognise (__fake_<name>) and skip themselves.
srcs_os9_fake = [
  '../semihost/fake/fake_getrlimit.c',
  '../semihost/fake/fake_setrlimit.c',
]

# From the fallback library, which is not folded into libc when an OS library
# is present: the pieces the kernel has no say in.
srcs_os9_fallback = [
  '../fallback/fpathconf.c',
  '../fallback/pathconf.c',
  '../fallback/raise.c',
  '../fallback/signal.c',
  '../fallback/sigprocmask.c',
  '../fallback/sysconf.c',
]

src_os9 = files(srcs_os9 + srcs_os9_fake + srcs_os9_fallback)

foreach params : targets
  target = params['name']
  target_dir = params['dir']
  target_c_args = params['c_args']

  instdir = join_paths(lib_dir, target_dir)
  libos9_name = 'os9'
  if meson.version().version_compare('>=1.10')
    _lib = static_library(libos9_name,
                          src_os9,
                          build_subdir : target_dir,
                          install : really_install,
                          install_dir : instdir,
                          include_directories : inc,
                          c_args : target_c_args + c_args)
  else
    target_lib_prefix = params['lib_prefix']
    _lib = static_library(join_paths(target_dir, target_lib_prefix + libos9_name),
                          src_os9,
                          install : really_install,
                          install_dir : instdir,
                          include_directories : inc,
                          c_args : target_c_args + c_args)
  endif
  set_variable('lib_os9' + target, _lib)
endforeach
