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
 * MC6809 compiler imaginary register allocation in direct page.
 *
 * The LLVM-MC6809 backend uses memory-backed "imaginary registers" for
 * additional byte (RC0-RC15) and word (RS0-RS7) storage beyond the real
 * hardware registers. These live in direct-page memory so the compiler
 * can use fast <addr (direct page) addressing.
 *
 * __scratch is a single byte used by emit6809RegByteFromMem's byte-ALU
 * path-C optimization (replaces PSHS/op/LEAS triple).
 *
 * Layout (relative to __dp_base_addr; 33 bytes total):
 *   __rc0..__rc7    at +0x00..+0x07 (8 bytes, 8-bit regs)
 *   __rs0..__rs3    at +0x08..+0x0F (8 bytes, 16-bit regs)
 *   __rc8..__rc15   at +0x10..+0x17 (8 bytes, 8-bit regs)
 *   __rs4..__rs7    at +0x18..+0x1F (8 bytes, 16-bit regs)
 *   __scratch       at +0x20        (1 byte, byte-ALU scratch)
 *
 * Bug #193: these symbols are PROVIDE'd by the linker script
 * (cross-file's directpage_block) as `__dp_base_addr + N` so a
 * non-zero DP base (e.g. $1F00) places the imaginary regs correctly.
 * The R_MC6809_DIRECT8 relocation on `LDA <__rcN` extracts the low 8
 * bits of the resolved value, which equals N regardless of
 * __dp_base_addr — so direct-mode access from code is byte-identical
 * at every DP base.
 */

static void __used
_cstart(void)
{
    /* Bug #192/#193: explicit DP register init. Hardware reset
     * defaults DP=0, but a soft-reset path could leave it stale —
     * and a wrong DP would silently break every direct-page access
     * (LDAd / STAd / LDDd / STDd plus all imaginary-reg accesses).
     *
     * `__dp_base` is PROVIDE'd by the linker script (default $00,
     * overridable via the `directpage_base` cross-file property or
     * `-Wl,--defsym=__dp_base=0xNN`). MC6809's CLR instruction is
     * memory-only; the canonical idiom for an arbitrary DP value is
     * `lda #<imm>; tfr a,dp`. */
    __asm__ volatile (
        "lda #__dp_base\n\t"
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
     *
     * Bug #330 (2026-05-23): ACIA moved from $FFD0-$FFD1 to
     * $FFC4-$FFC5 in USim commit 78a1fa4 (pico-thing alignment).
     */
    volatile unsigned char *acia_status_ctrl = (volatile unsigned char *)0xFFC4;
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

/*
 * Bug #273: diagnostic SWI3 handler.
 *
 * The LLVM-MC6809 backend's Bug #186 v5 COPY_CC_PLACEHOLDER pseudo
 * lowers to a `SWI3` instruction when AsmPrinter encounters a
 * register-pair COPY shape it has no rule for. Without a defined
 * SWI3 vector at $FFF2-$FFF3, the CPU loads PC from whatever the
 * hex/cart happens to leave there (usually zero), jumps to garbage,
 * and produces an indistinguishable "infinite loop / impossibly
 * low cycle count" symptom in the bench tally. Bug #266 took
 * multiple sessions to diagnose for exactly this reason — the
 * SWI3 trap was firing in test-ffs but looked like a generic
 * runtime crash.
 *
 * This handler writes a sentinel exit code (99) to the halt port
 * ($FFCC — moved from $FFD2 in USim commit 78a1fa4, Bug #330),
 * making the bench tally show FAIL rc=99 — instantly
 * recognisable as a Bug #186 v5 placeholder hit, not a generic
 * crash. The `run-mc6809` (USim) and `run-mc6809-mame` (MAME)
 * wrappers inject the address of `__swi3_trap` at $FFF2-$FFF3
 * (mirroring the existing reset-vector injection at $FFFE-$FFFF)
 * when they find the symbol in the ELF via `llvm-nm`.
 *
 * The function is marked `__used` so crt0.o's whole-object link
 * carry keeps it present in every executable, even when no caller
 * references it directly.
 */
void __used
__swi3_trap(void)
{
    __asm__ volatile (
        "lda  #99\n"            /* sentinel: SWI3 placeholder hit */
        "sta  0xFFCC\n"         /* halt port: bench reports FAIL rc=99 */
        "1: bra 1b"             /* belt-and-braces if halt-port write missed */
        : : : "a"
    );
}
