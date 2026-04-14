;;; memset — 6809 hand-written byte fill
;;;
;;; void *memset(void *s, int c, size_t n)
;;; X = s, 2,s = c (int, low byte used), 4,s = n. Returns X = s.
;;; Clobbers: D. Preserves: Y, U.

	.section .text,"ax",@progbits

	.globl	memset
	.type	memset,@function
memset:
	pshs	x		; save s
	ldb	5,s		; c low byte (+2 saved X, +2 retaddr, +1 c_hi)
	ldy	6,s		; n
	beq	.Lmemset_done
.Lmemset_loop:
	stb	,x+
	leay	-1,y
	bne	.Lmemset_loop
.Lmemset_done:
	puls	x,pc