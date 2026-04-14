;;; memcpy — 6809 hand-written byte copy
;;;
;;; void *memcpy(void *dst, const void *src, size_t n)
;;; X = dst, 2,s = src, 4,s = n. Returns X = dst.
;;; Clobbers: D. Preserves: Y, U.
;;;
;;; Works around an LLVM-MC6809 codegen bug where `while (n--)` at -Oz
;;; loses the back-branch to the loop body, making picolibc's C memcpy
;;; effectively a no-op.

	.section .text,"ax",@progbits

	.globl	memcpy
	.type	memcpy,@function
memcpy:
	pshs	x		; save dst for return
	ldd	6,s		; n
	beq	.Lmemcpy_done
	pshs	d		; counter on stack
	ldy	6,s		; src (+2 counter, +2 saved X, +2 retaddr)
.Lmemcpy_loop:
	lda	,y+
	sta	,x+
	ldd	,s
	subd	#1
	std	,s
	bne	.Lmemcpy_loop
	leas	2,s		; pop counter
.Lmemcpy_done:
	puls	x,pc