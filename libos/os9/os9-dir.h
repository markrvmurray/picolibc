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

#ifndef _OS9_DIR_H_
#define _OS9_DIR_H_

/* An OS-9 directory is an ordinary file of 32-byte entries: 29 bytes of
 * name, whose last character carries the high bit, then the three-byte
 * sector number of the file's descriptor.  An entry whose name starts with
 * a NUL is a deleted one. */
#define OS9_DIRENT_SIZE 32
#define OS9_DIRENT_NAME 29
#define OS9_DIRENT_LSN  29

/* Copy an entry's name out, NUL-terminated; the buffer takes
 * OS9_DIRENT_NAME + 1 bytes.  Returns its length. */
int __os9_entry_name(const unsigned char *entry, char *name);

/* OS-9 file names do not distinguish case. */
int __os9_name_eq(const char *a, const char *b);

/* The directory part of a path into `dir` (at most `dirsz`, "." when the
 * path names no directory), and the base name as the return value; null
 * when the path does not fit or names no file. */
const char *__os9_split_path(const char *path, char *dir, unsigned dirsz);

#endif /* _OS9_DIR_H_ */
