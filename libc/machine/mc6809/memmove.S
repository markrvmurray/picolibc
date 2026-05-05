//===-- memmove.S - MC6809 / HD6309 memmove --------------*- ASM -*-===//
//
// Part of LLVM-MC6809 / picolibc, BSD-3-Clause.
//
// Bug #161 Phase 2: overlap-safe block copy.
//
// ABI (same as memcpy):
//   X    = dst (also the return value)
//   2,S  = src
//   4,S  = n
// Callee-saved: Y, U, S. Clobbers D (and W on HD6309).
//
// Direction rule: if src < dst, copy backwards from end-1; else copy
// forwards from start. This is the standard overlap-safe rule —
// when src < dst the forward copy would overwrite source bytes
// before reading them.
//
// Register usage on 6809: same as memcpy.S — X=dst, Y=count (LEAY
// sets Z; LEAU doesn't), U=src.
//
//===----------------------------------------------------------------------===//

	.section .text,"ax",@progbits

	.globl	memmove
	.type	memmove,@function
memmove:
#ifdef __6309__
	pshs	x,y		// save dst & Y
	// Stack: 0=X, 2=Y, 4=ret, 6=src, 8=n
	ldd	6,s		// D = src
	cmpd	,s		// CC ← src - dst (unsigned)
	bhs	.Lmove_fwd	// src >= dst → forward copy
	// --- backward: src < dst, walk from end-1 ---
	ldw	8,s		// W = n (TFM count)
	beq	.Lmove_done
	ldd	8,s		// D = n
	leax	d,x		// X = dst + n
	leax	-1,x		// X = dst + n - 1
	ldy	6,s
	leay	d,y		// Y = src + n
	leay	-1,y		// Y = src + n - 1
	tfm	y-,x-
	puls	x,y,pc
.Lmove_fwd:
	ldy	6,s
	ldw	8,s
	beq	.Lmove_done
	tfm	y+,x+
.Lmove_done:
	puls	x,y,pc
#else
	pshs	x,y,u		// save dst, Y, U
	// Stack: 0=X, 2=Y, 4=U, 6=ret, 8=src, 10=n
	ldd	8,s		// D = src
	cmpd	,s		// CC ← src - dst (unsigned, vs saved-X = dst)
	bhs	.Lmove_fwd	// src >= dst → forward copy is safe

	// --- backward: src < dst, walk from end-1 ---
	ldy	10,s		// Y = n  (counter; LEAY -1,Y sets Z)
	beq	.Lmove_done
	tfr	y,d		// D = n (for the LEA-by-D adds below)
	leax	d,x		// X = dst + n
	leax	-1,x		// X = dst + n - 1
	ldu	8,s		// U = src
	leau	d,u		// U = src + n
	leau	-1,u		// U = src + n - 1
.Lmove_loop_b:
	lda	,u
	sta	,x
	leau	-1,u
	leax	-1,x
	leay	-1,y		// counter; sets Z
	bne	.Lmove_loop_b
	puls	x,y,u,pc

	// --- forward: src >= dst, identical to memcpy ---
.Lmove_fwd:
	ldu	8,s		// U = src
	ldy	10,s		// Y = n
	beq	.Lmove_done
.Lmove_loop_f:
	lda	,u+
	sta	,x+
	leay	-1,y
	bne	.Lmove_loop_f
.Lmove_done:
	puls	x,y,u,pc
#endif
