/* SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2026 Mark Murray
 *
 * The Color BASIC ROM entry points this layer uses, and the little state it
 * keeps.
 *
 * The ROM vectors are the documented Color BASIC ones: a table of *addresses*
 * at $A000, called indirectly so they work on any machine in the family.
 * Read off a running CoCo 3 to be sure -- [$A000] is $A1CB and [$A002] is
 * $A282 there, addresses rather than JMP instructions, so `jsr [$A002]` is
 * the right shape:
 *
 *   [$A000]  POLCAT   poll the keyboard; A = the key, or 0 if none
 *   [$A002]  CHROUT   write A to the current output device
 *
 * **The ROM needs the direct page at $00.**  Color BASIC keeps its working
 * storage in page zero and reaches it with direct addressing, so a routine
 * called with our DP -- $3F, where the compiler's imaginary registers live --
 * reads and writes our page instead of BASIC's.  CHROUT called that way
 * prints nothing at all and quietly corrupts a few of our bytes on the way
 * past.  So each call saves DP, zeroes it, and puts it back.  That costs
 * seven cycles a character and is not optional.
 *
 * U is saved too, and for a harder-won reason: the ROM does not preserve it,
 * and the compiler uses U as a frame pointer.  A call that lost U left the
 * program running wild through its own image with the stack marching down --
 * which on a CoCo 3 looked like the program printing its line three times and
 * then wandering off.  A clobber list cannot express "the frame pointer is
 * gone", so it is pushed and pulled around the call.
 */

#ifndef _DECB_PRIVATE_H_
#define _DECB_PRIVATE_H_

#include <errno.h>
#include <stddef.h>
#include <sys/types.h>

#define DECB_POLCAT 0xA000
#define DECB_CHROUT 0xA002

/* The two ROM calls live in rom.S, out of line and deliberately so: this
 * target does not honour inline-asm register clobbers, so an asm that uses D
 * -- as zeroing the direct page must -- corrupts whatever the allocator had
 * there.  A write() loop kept its counter in D and printed for ever.  As
 * ordinary functions the call convention does the work instead. */
void __decb_putc(char __c);
char __decb_getc(void);

/* The stack BASIC handed the program, saved by the start-up code so that
 * _exit() can go back to it from wherever it is called. */
extern void *__decb_entry_sp;

#endif /* _DECB_PRIVATE_H_ */
