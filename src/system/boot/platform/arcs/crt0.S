/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * Entry point of the boot loader. The ARCS PROM jumps here in kernel mode
 * with interrupts off. $4 to $6 hold the ARCS argc, argv and envp.
 */

#include <arch_cpu_defs.h>

#include "mmu.h"

	.abicalls
	.option	pic0
	.set	noreorder

	.section .text.head, "ax"
	.globl	_start
	.ent	_start
_start:
	// kernel mode, interrupts off, 32 bit addressing. The exception vectors
	// stay in the PROM until the loader installs its own.
	li	$8, CP0_STATUS_CU0 | CP0_STATUS_BEV
	mtc0	$8, C0_STATUS
	nop
	nop
	nop

	// The stack is at the top of the loader's scratch memory.
	li	$29, LOADER_STACK_TOP - 32
	move	$30, $0

	lui	$25, %hi(arcs_start)
	addiu	$25, $25, %lo(arcs_start)
	jalr	$25
	 nop

1:	b	1b
	 nop
	.end	_start
