/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * Prologues of _init and _fini. The bodies come from crtbegin.o and
 * crtend.o, the epilogues from crtn.S. n32: the caller's $28 is saved, and
 * $28 is set from $25, which holds the function's own address.
 */

	.abicalls
	.set	noreorder

	.section .init, "ax", @progbits
	.globl	_init
	.type	_init, @function
_init:
	addiu	$29, $29, -32
	sd	$31, 24($29)
	sd	$28, 16($29)
	lui	$28, %hi(%neg(%gp_rel(_init)))
	addu	$28, $28, $25
	addiu	$28, $28, %lo(%neg(%gp_rel(_init)))
	// crtbegin.o stuff comes here

	.section .fini, "ax", @progbits
	.globl	_fini
	.type	_fini, @function
_fini:
	addiu	$29, $29, -32
	sd	$31, 24($29)
	sd	$28, 16($29)
	lui	$28, %hi(%neg(%gp_rel(_fini)))
	addu	$28, $28, $25
	addiu	$28, $28, %lo(%neg(%gp_rel(_fini)))
	// crtend.o stuff comes here
