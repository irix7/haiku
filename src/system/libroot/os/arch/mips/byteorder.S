/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * float __swap_float(float value)
 * double __swap_double(double value)
 *
 * Built for userland (arguments and results in FPU registers) and for the
 * kernel and the boot loader (soft float: in integer registers).
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

	.globl	__swap_float
	.type	__swap_float, @function
	.ent	__swap_float
__swap_float:
#ifdef USE_FPU
	mfc1	$12, $f12
#else
	move	$12, $4
#endif
	srl	$2, $12, 24
	srl	$13, $12, 8
	andi	$13, $13, 0xff00
	or	$2, $2, $13
	andi	$13, $12, 0xff00
	sll	$13, $13, 8
	or	$2, $2, $13
	sll	$13, $12, 24
	or	$2, $2, $13
#ifdef USE_FPU
	mtc1	$2, $f0
#endif
	jr	$31
	 nop
	.end	__swap_float
	.size	__swap_float, . - __swap_float

	.globl	__swap_double
	.type	__swap_double, @function
	.ent	__swap_double
__swap_double:
#ifdef USE_FPU
	dmfc1	$12, $f12
#else
	move	$12, $4
#endif
	li	$13, 8
	move	$2, $0
1:	dsll	$2, $2, 8
	andi	$14, $12, 0xff
	or	$2, $2, $14
	addiu	$13, $13, -1
	bnez	$13, 1b
	 dsrl	$12, $12, 8
#ifdef USE_FPU
	dmtc1	$2, $f0
#endif
	jr	$31
	 nop
	.end	__swap_double
	.size	__swap_double, . - __swap_double
