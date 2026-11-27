/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include "setjmp_internal.h"

/*
 * void siglongjmp(jmp_buf buffer, int value)
 * Restores the registers sigsetjmp() saved, then continues in
 * __longjmp_return(buffer, value). That function returns the value to the
 * caller of sigsetjmp(), because $31 and $29 are those of that call.
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

	.globl	siglongjmp
	.globl	longjmp
	.globl	_longjmp
	.type	siglongjmp, @function
	.type	longjmp, @function
	.type	_longjmp, @function
	.ent	siglongjmp
siglongjmp:
longjmp:
_longjmp:
#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
	lui	$25, %hi(__longjmp_return)
	addiu	$25, $25, %lo(__longjmp_return)
#else
	// $28 of this function, before the caller's is restored
	lui	$12, %hi(%neg(%gp_rel(siglongjmp)))
	addu	$12, $12, $25
	addiu	$12, $12, %lo(%neg(%gp_rel(siglongjmp)))
	lw	$25, %call16(__longjmp_return)($12)
#endif

	ld	$16, JMP_S(0)($4)
	ld	$17, JMP_S(1)($4)
	ld	$18, JMP_S(2)($4)
	ld	$19, JMP_S(3)($4)
	ld	$20, JMP_S(4)($4)
	ld	$21, JMP_S(5)($4)
	ld	$22, JMP_S(6)($4)
	ld	$23, JMP_S(7)($4)
	ld	$28, JMP_GP($4)
	ld	$29, JMP_SP($4)
	ld	$30, JMP_FP($4)
	ld	$31, JMP_RA($4)
#ifdef USE_FPU
	ldc1	$f20, JMP_F(0)($4)
	ldc1	$f22, JMP_F(1)($4)
	ldc1	$f24, JMP_F(2)($4)
	ldc1	$f26, JMP_F(3)($4)
	ldc1	$f28, JMP_F(4)($4)
	ldc1	$f30, JMP_F(5)($4)
	ld	$13, JMP_FCSR($4)
	ctc1	$13, $31
#endif
	jr	$25
	 nop
	.end	siglongjmp
	.size	siglongjmp, . - siglongjmp
