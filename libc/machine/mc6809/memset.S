//===-- memset.S - MC6809 / HD6309 memset ----------------*- ASM -*-===//
//
// Part of LLVM-MC6809 / picolibc, BSD-3-Clause.
//
// Overrides the generic libc/string/memset.c, which stores a byte at a
// time.  The compiler calls compiler-rt's __memset for memset in C
// code; this is the memset called by name, through a pointer, for
// instance.
//
// ABI (matches what the backend emits at the call site):
//   X    = dst (also the return value)
//   2,S  = c   (int; the fill byte is its low byte, at 3,S)
//   4,S  = n   (i16, byte count)
// Callee-saved: Y, U, S (per MC6809_CSR). Clobbers D (and W on HD6309).
// Returns X = original dst.
//
// HD6309: TFM from the fill byte in place, a fixed source, 3 cycles
// a byte.
//
// 6809: a byte to make the count even, a word to make it a multiple of
// four, then two words per pass: 24 cycles per four bytes.
//
//===----------------------------------------------------------------------===//

	.section .text.memset,"ax",@progbits
	.globl	memset
	.type	memset,@function
	.cfi_sections .debug_frame
memset:
	.cfi_startproc
	pshs	x,y		// save dst (for return) & Y
	.cfi_adjust_cfa_offset 4
	.cfi_offset y, -4
	// Stack: 0=X, 2=Y, 4=ret, 6=c, 8=n
#ifdef __6309__
	ldw	8,s		// W = n
	beq	.Lmemset_done
	leay	7,s		// Y -> the fill byte (low byte of c)
	tfm	y,x+
#else
	ldy	8,s		// Y = n  (LEAY sets Z, ideal counter)
	beq	.Lmemset_done
	lda	7,s		// A = fill byte
	ldb	9,s		// B = low byte of n
	lsrb			// C = bit 0 of n
	bcc	.Lmemset_even
	sta	,x+		// odd count: one byte first
	leay	-1,y
	beq	.Lmemset_done
.Lmemset_even:
	lsrb			// C = bit 1 of n
	tfr	a,b		// D = fill:fill (TFR leaves CC alone)
	bcc	.Lmemset_loop
	std	,x++		// count not a multiple of four: one word first
	leay	-2,y
	beq	.Lmemset_done
.Lmemset_loop:
	std	,x++
	std	,x++
	leay	-4,y
	bne	.Lmemset_loop
#endif
.Lmemset_done:
	puls	x,y,pc
	.cfi_endproc
