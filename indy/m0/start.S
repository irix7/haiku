/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Entry of the M0 test image. The PROM jumps here in kernel mode with
 * interrupts off. The image sets its own stack and does not return.
 */

	.set	noreorder
	.section .text.start,"ax"
	.globl	_start
	.ent	_start
_start:
	lui	$sp, %hi(_stack_top)
	addiu	$sp, $sp, %lo(_stack_top)
	jal	hello_main
	 addiu	$sp, $sp, -16
1:	b	1b
	 nop
	.end	_start
