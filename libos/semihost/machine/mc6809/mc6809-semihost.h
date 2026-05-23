/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2025 Mark Murray
 *
 * MC6809 "semihost" via memory-mapped I/O devices in USim.
 *
 * Bug #330 (2026-05-23): USim's IO slab moved from $FFD0-$FFD3 to
 * $FFC4-$FFCC in commit 78a1fa4 (pico-thing alignment).  Picolibc and
 * the linker script (cross-clang-mc6809-unknown-elf.txt) updated to
 * match.  MAME llvm6309 driver tracks the same change separately.
 *
 *   0xFFC4  ACIA status (bit 1 = TDRE, bit 0 = RDRF)
 *   0xFFC5  ACIA data (read/write)
 *   0xFFCB  TraceCtl (USim --brk-gated / --trace toggle)
 *   0xFFCC  Halt device (written value = exit code)
 */

#ifndef _MC6809_SEMIHOST_H
#define _MC6809_SEMIHOST_H

#include <stdint.h>

#define MC6809_ACIA_STATUS  (*(volatile uint8_t *)0xFFC4)
#define MC6809_ACIA_DATA    (*(volatile uint8_t *)0xFFC5)
#define MC6809_HALT         (*(volatile uint8_t *)0xFFCC)

#define MC6809_ACIA_TDRE    0x02  /* Transmit Data Register Empty */
#define MC6809_ACIA_RDRF    0x01  /* Receive Data Register Full */

static inline void
mc6809_putchar(char c)
{
    while (!(MC6809_ACIA_STATUS & MC6809_ACIA_TDRE))
        ;
    MC6809_ACIA_DATA = (uint8_t)c;
}

static inline int
mc6809_getchar(void)
{
    while (!(MC6809_ACIA_STATUS & MC6809_ACIA_RDRF))
        ;
    return MC6809_ACIA_DATA;
}

#endif /* _MC6809_SEMIHOST_H */
