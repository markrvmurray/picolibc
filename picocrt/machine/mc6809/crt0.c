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

#include "../../crt0.h"
#include <string.h>

extern char __stack[];

/*
 * Bug #192: direct-page (.dp.bss / .dp.data / .dp.rodata) section
 * boundaries provided by the linker script (cross-file's
 * directpage_block in scripts/cross-clang-mc6809-unknown-elf.txt).
 */
extern char __dp_bss_start[];
extern char __dp_bss_size[];   /* opaque size: cast to uintptr_t */
extern char __dp_data_start[];
extern char __dp_data_source[];
extern char __dp_data_size[];

/*
 * The MC6809 backend strongly-references __do_zero_dp_bss /
 * __do_copy_dp_data only when a TU contains .dp.bss / .dp.data
 * (see MC6809TargetStreamer.cpp:35-45 in llvm-mc6809). When no
 * user __directpage globals exist anywhere in the link, these
 * symbols are unreferenced and the linker drops them. When they
 * ARE referenced, __dp_bss_size / __dp_data_size are non-zero
 * and the loops do useful work.
 */
void
__do_zero_dp_bss(void)
{
    memset(__dp_bss_start, 0, (uintptr_t)__dp_bss_size);
}

void
__do_copy_dp_data(void)
{
    memcpy(__dp_data_start, __dp_data_source, (uintptr_t)__dp_data_size);
}

/*
 * MC6809 compiler imaginary register allocation in direct page (DP=0).
 *
 * The LLVM-MC6809 backend uses memory-backed "imaginary registers" for
 * additional byte (RC0-RC15) and word (RS0-RS7) storage beyond the real
 * hardware registers. These live in direct-page memory so the compiler
 * can use fast <addr (direct page) addressing.
 *
 * __scratch is a single byte used by emit6809RegByteFromMem's byte-ALU
 * path-C optimization (replaces PSHS/op/LEAS triple).
 *
 * Layout (0x00..0x30, 49 bytes):
 *   __rc0..__rc7    at 0x00..0x07 (8 bytes, 8-bit regs)
 *   __rs0..__rs3    at 0x08..0x0F (8 bytes, 16-bit regs)
 *   __rc8..__rc15   at 0x10..0x17 (8 bytes, 8-bit regs)
 *   __rs4..__rs7    at 0x18..0x1F (8 bytes, 16-bit regs)
 *   __scratch       at 0x20       (1 byte, byte-ALU scratch)
 *
 * These are defined via absolute-address .set directives so they bind
 * without needing a linker script section. The direct-page region is
 * otherwise unused since picolibc's RAM starts at 0x0100.
 */
__asm__(
    ".set __rc0, 0x00\n"
    ".set __rc1, 0x01\n"
    ".set __rc2, 0x02\n"
    ".set __rc3, 0x03\n"
    ".set __rc4, 0x04\n"
    ".set __rc5, 0x05\n"
    ".set __rc6, 0x06\n"
    ".set __rc7, 0x07\n"
    ".set __rs0, 0x08\n"
    ".set __rs1, 0x0A\n"
    ".set __rs2, 0x0C\n"
    ".set __rs3, 0x0E\n"
    ".set __rc8,  0x10\n"
    ".set __rc9,  0x11\n"
    ".set __rc10, 0x12\n"
    ".set __rc11, 0x13\n"
    ".set __rc12, 0x14\n"
    ".set __rc13, 0x15\n"
    ".set __rc14, 0x16\n"
    ".set __rc15, 0x17\n"
    ".set __rs4, 0x18\n"
    ".set __rs5, 0x1A\n"
    ".set __rs6, 0x1C\n"
    ".set __rs7, 0x1E\n"
    ".set __scratch, 0x20\n"
    ".globl __rc0,__rc1,__rc2,__rc3,__rc4,__rc5,__rc6,__rc7\n"
    ".globl __rs0,__rs1,__rs2,__rs3\n"
    ".globl __rc8,__rc9,__rc10,__rc11,__rc12,__rc13,__rc14,__rc15\n"
    ".globl __rs4,__rs5,__rs6,__rs7\n"
    ".globl __scratch\n"
);

static void __used
_cstart(void)
{
    /* Bug #192: explicit DP register init. Hardware reset defaults
     * DP=0, but a soft-reset path could leave it non-zero — and any
     * non-zero DP would silently break every direct-page access
     * (LDAd / STAd / LDDd / STDd plus all imaginary-reg accesses).
     * MC6809's CLR instruction is memory-only; the canonical idiom
     * to zero DP is `clra; tfr a,dp`. */
    __asm__ volatile (
        "clra\n\t"
        "tfr a,dp"
        : : : "a"
    );

    /* Initialise the MC6850 ACIA semihost device.
     *
     * USim's mc6850 emulation works without explicit initialisation
     * (the constructor's reset() sets sr=TDRE), but real hardware and
     * other emulators may leave the ACIA in an indeterminate state.
     * Do a master reset (cr=0x03) followed by operating mode:
     *   cr = 0x15 = 0b00010101 = /16 clock, 8N1, no IRQ, RTS=low
     */
    volatile unsigned char *acia_status_ctrl = (volatile unsigned char *)0xFFD0;
    *acia_status_ctrl = 0x03;   /* master reset */
    *acia_status_ctrl = 0x15;   /* /16 clock, 8N1, no IRQ */

    /* Bug #192: zero .dp.bss and copy .dp.data initialisers. The
     * backend strongly-references these symbols only when the link
     * actually contains .dp.* sections, so they cost nothing on
     * builds without __directpage globals. */
    __do_zero_dp_bss();
    __do_copy_dp_data();

    __start();
}

void __section(".text.init.enter") __used
_start(void)
{
    /* Initialise stack pointer to top-of-stack from the linker
     * script, then long-branch to the C bring-up. The 6809 has
     * no separate "interrupt vector" table loaded by hardware —
     * the reset vector at 0xFFFE is injected into the hex file
     * by the run-mc6809 wrapper before USim loads it. */
    __asm__ volatile("lds #__stack");
    __asm__ volatile("lbra _cstart");
}
