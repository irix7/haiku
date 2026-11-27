/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * FPU state save and restore. The kernel is built without FPU use, so the
 * FPU is enabled here for the duration of each function. Callers have
 * interrupts disabled.
 */

#include <arch_cpu_defs.h>
#include <arch_exception_block.h>

	.abicalls
	.option	pic0
	.set	noreorder
	.set	noat
	.set	hardfloat
	.text

// Enables the FPU with 32 registers of 64 bits. The old Status is in $12.
#define FPU_ENABLE \
	mfc0	$12, C0_STATUS; \
	lui	$13, ((CP0_STATUS_CU1 | CP0_STATUS_FR) >> 16); \
	or	$13, $13, $12; \
	mtc0	$13, C0_STATUS; \
	nop; \
	nop; \
	nop

#define FPU_DISABLE \
	mtc0	$12, C0_STATUS; \
	nop; \
	nop; \
	nop

#define FPU_ALL(instruction, base) \
	instruction	$f0, FPU_CONTEXT_F(0)(base); \
	instruction	$f1, FPU_CONTEXT_F(1)(base); \
	instruction	$f2, FPU_CONTEXT_F(2)(base); \
	instruction	$f3, FPU_CONTEXT_F(3)(base); \
	instruction	$f4, FPU_CONTEXT_F(4)(base); \
	instruction	$f5, FPU_CONTEXT_F(5)(base); \
	instruction	$f6, FPU_CONTEXT_F(6)(base); \
	instruction	$f7, FPU_CONTEXT_F(7)(base); \
	instruction	$f8, FPU_CONTEXT_F(8)(base); \
	instruction	$f9, FPU_CONTEXT_F(9)(base); \
	instruction	$f10, FPU_CONTEXT_F(10)(base); \
	instruction	$f11, FPU_CONTEXT_F(11)(base); \
	instruction	$f12, FPU_CONTEXT_F(12)(base); \
	instruction	$f13, FPU_CONTEXT_F(13)(base); \
	instruction	$f14, FPU_CONTEXT_F(14)(base); \
	instruction	$f15, FPU_CONTEXT_F(15)(base); \
	instruction	$f16, FPU_CONTEXT_F(16)(base); \
	instruction	$f17, FPU_CONTEXT_F(17)(base); \
	instruction	$f18, FPU_CONTEXT_F(18)(base); \
	instruction	$f19, FPU_CONTEXT_F(19)(base); \
	instruction	$f20, FPU_CONTEXT_F(20)(base); \
	instruction	$f21, FPU_CONTEXT_F(21)(base); \
	instruction	$f22, FPU_CONTEXT_F(22)(base); \
	instruction	$f23, FPU_CONTEXT_F(23)(base); \
	instruction	$f24, FPU_CONTEXT_F(24)(base); \
	instruction	$f25, FPU_CONTEXT_F(25)(base); \
	instruction	$f26, FPU_CONTEXT_F(26)(base); \
	instruction	$f27, FPU_CONTEXT_F(27)(base); \
	instruction	$f28, FPU_CONTEXT_F(28)(base); \
	instruction	$f29, FPU_CONTEXT_F(29)(base); \
	instruction	$f30, FPU_CONTEXT_F(30)(base); \
	instruction	$f31, FPU_CONTEXT_F(31)(base)


/*
 * void mips_fpu_save(fpu_context* context)
 */
	.globl	mips_fpu_save
	.ent	mips_fpu_save
mips_fpu_save:
	FPU_ENABLE
	FPU_ALL(sdc1, $4)
	cfc1	$13, $31
	nop
	sw	$13, FPU_CONTEXT_FCSR($4)
	FPU_DISABLE
	jr	$31
	 nop
	.end	mips_fpu_save


/*
 * void mips_fpu_restore(const fpu_context* context)
 * The cause bits of FCSR are cleared: writing a cause bit whose enable bit
 * is set raises an FPU exception at once.
 */
	.globl	mips_fpu_restore
	.ent	mips_fpu_restore
mips_fpu_restore:
	FPU_ENABLE
	FPU_ALL(ldc1, $4)
	lw	$13, FPU_CONTEXT_FCSR($4)
	lui	$14, (FCSR_CAUSE_MASK >> 16)
	ori	$14, $14, (FCSR_CAUSE_MASK & 0xffff)
	nor	$14, $14, $0
	and	$13, $13, $14
	ctc1	$13, $31
	nop
	FPU_DISABLE
	jr	$31
	 nop
	.end	mips_fpu_restore
