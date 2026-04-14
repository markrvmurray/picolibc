;;; memmove — 6809 hand-written byte copy with overlap handling
;;;
;;; void *memmove(void *dst, const void *src, size_t n)
;;; X = dst, 2,s = src, 4,s = n. Returns X = dst.
;;; Clobbers: D. Preserves: Y, U.

	.section .text,"ax",@progbits

	.globl	memmove
	.type	memmove,@function
memmove:
	pshs	x		; save dst
	ldd	6,s		; n
	beq	.Lmemmove_done
	ldy	4,s		; src (+2 saved X, +2 retaddr)
	;; If dst <= src, copy forwards; otherwise backwards.
	pshs	y
	cmpx	,s++
	bls	.Lmemmove_fwd
	;; dst > src: backwards
	pshs	d		; counter
	leax	d,x		; X = dst + n
	leay	d,y		; Y = src + n
.Lmemmove_bwd:
	lda	,-y
	sta	,-x
	ldd	,s
	subd	#1
	std	,s
	bne	.Lmemmove_bwd
	leas	2,s
	bra	.Lmemmove_done
.Lmemmove_fwd:
	pshs	d		; counter
.Lmemmove_fwd_loop:
	lda	,y+
	sta	,x+
	ldd	,s
	subd	#1
	std	,s
	bne	.Lmemmove_fwd_loop
	leas	2,s
.Lmemmove_done:
	puls	x,pc