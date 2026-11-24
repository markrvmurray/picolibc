; SPDX-License-Identifier: BSD-3-Clause
;
; Copyright © 2026 Mark Murray
;
; The two Color BASIC ROM calls, out of line.
;
; These were inline asm until a CoCo 3 showed why they cannot be: this
; target does not honour inline-asm register clobbers (bug filed), so an
; asm that uses D silently corrupts whatever the allocator had there.  A
; write() loop kept its counter in D, the `clrb` needed to zero DP took
; half of it, and the loop printed for ever.  As ordinary functions the
; compiler applies its own call convention instead of trusting a clobber
; list, and what these preserve is then a promise they can keep.
;
; The ROM wants:
;
;   [$A000]  POLCAT   poll the keyboard; A = the key, or 0 if none
;   [$A002]  CHROUT   write A to the current output device
;
; reached indirectly -- those are addresses, not JMPs -- and **with the
; direct page at $00**, because Color BASIC keeps its working storage in
; page zero.  Called with our DP ($3F, where the imaginary registers live)
; CHROUT reads and writes our page and prints nothing.
;
; The ROM does not preserve U either, and the compiler uses U as a frame
; pointer, so U is saved here too.

        .text

; void __decb_putc(char c) -- the character arrives in B.
        .globl  __decb_putc
        .type   __decb_putc,@function
	.cfi_sections .debug_frame
__decb_putc:
	.cfi_startproc
        pshs    u,dp,a          ; A is ours to clobber; save it anyway, so
	.cfi_adjust_cfa_offset 4
	.cfi_offset u, -4
        tfr     b,a             ; that D is intact for the caller
        clrb
        tfr     b,dp            ; the ROM's page
        jsr     [$A002]
        puls    a,dp,u
	.cfi_adjust_cfa_offset -4
	.cfi_restore u
        rts
	.cfi_endproc
        .size   __decb_putc, .-__decb_putc

; char __decb_getc(void) -- the key, or 0, returned in B.
        .globl  __decb_getc
        .type   __decb_getc,@function
__decb_getc:
	.cfi_startproc
        pshs    u,dp,a
	.cfi_adjust_cfa_offset 4
	.cfi_offset u, -4
        clrb
        tfr     b,dp
        jsr     [$A000]
        tfr     a,b             ; the key, into the return register
        puls    a,dp,u
	.cfi_adjust_cfa_offset -4
	.cfi_restore u
        rts
	.cfi_endproc
        .size   __decb_getc, .-__decb_getc
