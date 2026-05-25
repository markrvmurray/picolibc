//===-- setjmp.S - MC6809 setjmp / longjmp ---------------*- ASM -*-===//
//
// Part of LLVM-MC6809 / picolibc, BSD-3-Clause.
//
// Bug #338: setjmp/longjmp for mc6809 (gcc6809 has these; the LLVM
// port did not, so any user of setjmp.h failed to link).
//
// jmp_buf is 4 x 16-bit words (machine/setjmp.h: _JBLEN 4), holding:
//   [0] U   (frame pointer, callee-saved per MC6809_CSR)
//   [2] Y   (callee-saved per MC6809_CSR)
//   [4] S   (the caller's continuation SP, i.e. S after setjmp's own
//            return address has been popped)
//   [6] PC  (setjmp's return address)
//
// D and X are caller-saved, so they are not preserved; DP is a fixed
// global base and is not in MC6809_CSR, so it is not saved either.
//
// ABI (matches the backend call site):
//   setjmp(jmp_buf):        X = &jmp_buf;  returns int in X.
//   longjmp(jmp_buf, val):  X = &jmp_buf;  val (i16) on stack at 2,S
//                           (X is the only ptr/i16 arg register, so the
//                            second argument spills — cf. memcpy.S).
//
//===----------------------------------------------------------------------===//

	.section .text,"ax",@progbits

	.globl	setjmp
	.type	setjmp,@function
setjmp:
	stu	,x		; jb[0] = U  (callee-saved frame pointer)
	sty	2,x		; jb[1] = Y  (callee-saved)
	tfr	s,d		; D = S
	addd	#2		; D = S + 2 = caller's continuation SP
	std	4,x		; jb[2] = continuation SP
	ldd	,s		; D = return address (16-bit at top of stack)
	std	6,x		; jb[3] = return PC
	ldx	#0		; first return: 0
	rts

	.globl	longjmp
	.type	longjmp,@function
longjmp:
	ldd	2,s		; D = val (read before S is restored)
	bne	.Llj_val
	ldd	#1		; longjmp(jb, 0) must make setjmp return 1
.Llj_val:
	ldu	,x		; restore U  = jb[0]
	ldy	6,x		; Y = jb[3] = saved return PC (temporary)
	lds	4,x		; restore S  = jb[2] (continuation SP)
	pshs	y		; push saved PC onto the restored stack
	ldy	2,x		; restore Y  = jb[1]  (X still = &jmp_buf)
	tfr	d,x		; X = result value (setjmp's second return)
	rts			; pop PC = jb[3]; resume setjmp's caller
