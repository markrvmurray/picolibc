/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
 *
 * The Color BASIC ROM entry points this layer uses, and the little state it
 * keeps.
 *
 * The ROM vectors are the documented Color BASIC ones -- a table of JMPs at
 * $A000 -- reached indirectly so they work on any machine in the family:
 *
 *   [$A000]  POLCAT   poll the keyboard; A = the key, or 0 if none
 *   [$A002]  CHROUT   write A to the current output device
 *
 * NOTE: nothing here has been run.  There is no CoCo emulator in this tree,
 * so these addresses are from the published ROM maps and not from a machine
 * that answered.  They are gathered here so that correcting them is a
 * one-line job.
 */

#ifndef _DECB_PRIVATE_H_
#define _DECB_PRIVATE_H_

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#define DECB_POLCAT 0xA000
#define DECB_CHROUT 0xA002

/* Write one character through the ROM. */
static inline void __decb_putc(char __c) {
  __asm__ __volatile__("lda %0\n\tjsr [%c1]"
                       :
                       : "m"(__c), "i"(DECB_CHROUT)
                       : "a", "b", "x", "y", "memory");
}

/* Poll the keyboard: the key, or 0 if none is waiting. */
static inline char __decb_getc(void) {
  char __c;
  __asm__ __volatile__("jsr [%c1]\n\tsta %0"
                       : "=m"(__c)
                       : "i"(DECB_POLCAT)
                       : "a", "b", "x", "y", "memory");
  return __c;
}

/* The stack BASIC handed the program, saved by the start-up code so that
 * _exit() can go back to it from wherever it is called. */
extern void *__decb_entry_sp;

#endif /* _DECB_PRIVATE_H_ */
