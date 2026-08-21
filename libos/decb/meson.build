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

# DECB (mc6809-unknown-decb): a program EXECed from Disk Extended Color
# BASIC.  There is no operating system underneath -- the ROM offers a
# character in and a character out, and that is the whole of it -- so this
# layer is a console and the few calls a C library needs to exist at all.
# The start-up code and the linker script come from the LLVM-MC6809 DECB
# runtime, which the clang driver adds for this triple.

has_os_decb = true
has_os_library = true

srcs_decb = [
  'rom.S',
  'close.c',
  'exit.c',
  'getpid.c',
  'isatty.c',
  'lseek.c',
  'read.c',
  'sbrk.c',
  'write.c',
]

# What this machine has no answer for: picolibc's fakes, which the tests
# recognise (__fake_<name>) and skip themselves over.
srcs_decb_fake = [
  '../semihost/fake/fake_getrlimit.c',
  '../semihost/fake/fake_setrlimit.c',
]

# From the fallback library, which is not folded into libc when an OS library
# is present: the pieces a ROM has no say in.
srcs_decb_fallback = [
  '../fallback/fpathconf.c',
  '../fallback/pathconf.c',
  '../fallback/raise.c',
  '../fallback/signal.c',
  '../fallback/sigprocmask.c',
  '../fallback/sysconf.c',
]

src_decb = files(srcs_decb + srcs_decb_fake + srcs_decb_fallback)

foreach params : targets
  target = params['name']
  target_dir = params['dir']
  target_c_args = params['c_args']

  instdir = join_paths(lib_dir, target_dir)
  libdecb_name = 'decb'
  if meson.version().version_compare('>=1.10')
    _lib = static_library(libdecb_name,
                          src_decb,
                          build_subdir : target_dir,
                          install : really_install,
                          install_dir : instdir,
                          include_directories : inc,
                          c_args : target_c_args + c_args)
  else
    target_lib_prefix = params['lib_prefix']
    _lib = static_library(join_paths(target_dir, target_lib_prefix + libdecb_name),
                          src_decb,
                          install : really_install,
                          install_dir : instdir,
                          include_directories : inc,
                          c_args : target_c_args + c_args)
  endif
  set_variable('lib_decb' + target, _lib)
endforeach
