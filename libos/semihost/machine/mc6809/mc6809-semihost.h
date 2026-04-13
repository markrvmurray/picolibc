/*
 * SPDX-License-Identifier: BSD-3-Clause
 *
 * Copyright © 2025 Mark Murray
 *
 * MC6809 "semihost" via memory-mapped I/O devices in USim:
 *   0xC000  ACIA status (bit 1 = TDRE, bit 0 = RDRF)
 *   0xC001  ACIA data (read/write)
 *   0xC002  Halt device (written value = exit code)
 */

#ifndef _MC6809_SEMIHOST_H
#define _MC6809_SEMIHOST_H

#include <stdint.h>

#define MC6809_ACIA_STATUS  (*(volatile uint8_t *)0xC000)
#define MC6809_ACIA_DATA    (*(volatile uint8_t *)0xC001)
#define MC6809_HALT         (*(volatile uint8_t *)0xC002)

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
