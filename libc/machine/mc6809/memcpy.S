//===-- memcpy.S - MC6809 / HD6309 memcpy ----------------*- ASM -*-===//
//
// Part of LLVM-MC6809 / picolibc, BSD-3-Clause.
//
// Bug #161 Phase 2: HD6309 TFM and tight 6809 byte-loop overrides for
// the picolibc generic libc/string/memcpy.c.
//
// ABI (matches what the backend emits at the call site):
//   X    = dst (also the return value)
//   2,S  = src (i16)
//   4,S  = n   (i16, byte count)
// Callee-saved: Y, U, S (per MC6809_CSR). Clobbers D (and W on HD6309).
// Returns X = original dst.
//
// Overlap is undefined for memcpy — we copy strictly forwards.
//
// 6809 register usage in the byte loop:
//   X = dst (post-incremented), Y = remaining count, U = src ptr.
//   D (= A:B) is the byte-transfer accumulator. Y as the counter is
//   chosen because LEAY -1,Y is the only decrementing LEA on 6809
//   that sets the Z flag (LEAU / LEAS do NOT affect CC) — letting us
//   loop with a single LEAY+BNE instead of LEAU+CMPU+BNE.
//
//===----------------------------------------------------------------------===//

	.section .text,"ax",@progbits

	.globl	memcpy
	.type	memcpy,@function
memcpy:
#ifdef __6309__
	pshs	x,y		// save dst (for return) & Y
	// Stack: 0=X, 2=Y, 4=ret, 6=src, 8=n
	ldy	6,s		// Y = src
	ldw	8,s		// W = n
	beq	.Lmemcpy_done
	tfm	y+,x+
.Lmemcpy_done:
	puls	x,y,pc
#else
	pshs	x,y,u		// save dst (return), Y (callee), U (we use it)
	// Stack: 0=X, 2=Y, 4=U, 6=ret, 8=src, 10=n
	ldu	8,s		// U = src
	ldy	10,s		// Y = n  (LEAY -1,Y sets Z, ideal counter)
	beq	.Lmemcpy_done
.Lmemcpy_loop:
	lda	,u+
	sta	,x+
	leay	-1,y
	bne	.Lmemcpy_loop
.Lmemcpy_done:
	puls	x,y,u,pc
#endif
