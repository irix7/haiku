/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include "setjmp_internal.h"

/*
 * int sigsetjmp(jmp_buf buffer, int saveMask)
 * Saves the callee-saved registers of the n32 ABI as 64 bit values, then
 * continues in __setjmp_save_sigs(buffer, saveMask), which returns 0.
 * The kernel and the boot loader are built without FPU use.
 */

// The kernel and the boot loader do not use the FPU. Their assembly files
// are assembled without -msoft-float, so the compiler's macro does not tell.
#if defined(__mips_hard_float) && !defined(_KERNEL_MODE) \
	&& !defined(_BOOT_MODE)
#	define USE_FPU
#endif

	.abicalls
#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
	// The kernel and the boot loader are not position independent.
	.option	pic0
#endif
	.set	noreorder
	.text

	.globl	sigsetjmp
	.globl	__sigsetjmp
	.type	sigsetjmp, @function
	.type	__sigsetjmp, @function
	.ent	sigsetjmp
sigsetjmp:
__sigsetjmp:
	sd	$16, JMP_S(0)($4)
	sd	$17, JMP_S(1)($4)
	sd	$18, JMP_S(2)($4)
	sd	$19, JMP_S(3)($4)
	sd	$20, JMP_S(4)($4)
	sd	$21, JMP_S(5)($4)
	sd	$22, JMP_S(6)($4)
	sd	$23, JMP_S(7)($4)
	sd	$28, JMP_GP($4)
	sd	$29, JMP_SP($4)
	sd	$30, JMP_FP($4)
	sd	$31, JMP_RA($4)
#ifdef USE_FPU
	sdc1	$f20, JMP_F(0)($4)
	sdc1	$f22, JMP_F(1)($4)
	sdc1	$f24, JMP_F(2)($4)
	sdc1	$f26, JMP_F(3)($4)
	sdc1	$f28, JMP_F(4)($4)
	sdc1	$f30, JMP_F(5)($4)
	cfc1	$12, $31
	sd	$12, JMP_FCSR($4)
#endif

#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
	j	__setjmp_save_sigs
	 nop
#else
	// tail call, with $28 set up for this function in a scratch register
	lui	$12, %hi(%neg(%gp_rel(sigsetjmp)))
	addu	$12, $12, $25
	addiu	$12, $12, %lo(%neg(%gp_rel(sigsetjmp)))
	lw	$25, %call16(__setjmp_save_sigs)($12)
	jr	$25
	 nop
#endif
	.end	sigsetjmp
	.size	sigsetjmp, . - sigsetjmp


/* int setjmp(jmp_buf buffer) */
	.globl	setjmp
	.globl	_setjmp
	.type	setjmp, @function
	.type	_setjmp, @function
	.ent	setjmp
setjmp:
_setjmp:
#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
	j	sigsetjmp
	 move	$5, $0
#else
	lui	$12, %hi(%neg(%gp_rel(setjmp)))
	addu	$12, $12, $25
	addiu	$12, $12, %lo(%neg(%gp_rel(setjmp)))
	lw	$25, %call16(sigsetjmp)($12)
	jr	$25
	 move	$5, $0
#endif
	.end	setjmp
	.size	setjmp, . - setjmp
