/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/* Epilogues of _init and _fini, see crti.S. */

	.abicalls
	.set	noreorder

	.section .init, "ax", @progbits
	ld	$31, 24($29)
	ld	$28, 16($29)
	jr	$31
	 addiu	$29, $29, 32

	.section .fini, "ax", @progbits
	ld	$31, 24($29)
	ld	$28, 16($29)
	jr	$31
	 addiu	$29, $29, 32
