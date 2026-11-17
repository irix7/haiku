/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <arch_cpu_defs.h>
#include <arch_exception_block.h>

	.abicalls
	.option	pic0
	.set	noreorder
	.set	noat
	.text


/*
 * void arch_context_switch(arch_context* from, arch_context* to)
 * Saves the callee-saved registers of the n32 ABI as 64 bit values.
 */
	.globl	arch_context_switch
	.ent	arch_context_switch
arch_context_switch:
	sd	$16, CONTEXT_S(0)($4)
	sd	$17, CONTEXT_S(1)($4)
	sd	$18, CONTEXT_S(2)($4)
	sd	$19, CONTEXT_S(3)($4)
	sd	$20, CONTEXT_S(4)($4)
	sd	$21, CONTEXT_S(5)($4)
	sd	$22, CONTEXT_S(6)($4)
	sd	$23, CONTEXT_S(7)($4)
	sd	$28, CONTEXT_GP($4)
	sd	$29, CONTEXT_SP($4)
	sd	$30, CONTEXT_FP($4)
	sd	$31, CONTEXT_RA($4)

	ld	$16, CONTEXT_S(0)($5)
	ld	$17, CONTEXT_S(1)($5)
	ld	$18, CONTEXT_S(2)($5)
	ld	$19, CONTEXT_S(3)($5)
	ld	$20, CONTEXT_S(4)($5)
	ld	$21, CONTEXT_S(5)($5)
	ld	$22, CONTEXT_S(6)($5)
	ld	$23, CONTEXT_S(7)($5)
	ld	$28, CONTEXT_GP($5)
	ld	$29, CONTEXT_SP($5)
	ld	$30, CONTEXT_FP($5)
	ld	$31, CONTEXT_RA($5)
	jr	$31
	 nop
	.end	arch_context_switch


/*
 * The first code a new kernel thread runs. arch_thread_init_kthread_stack()
 * puts the function in $17 and its argument in $18.
 */
	.globl	mips_thread_entry
	.ent	mips_thread_entry
mips_thread_entry:
	move	$4, $18
	move	$25, $17
	jalr	$25
	 move	$31, $0
1:	b	1b
	 nop
	.end	mips_thread_entry


/*
 * void mips_enter_frame(iframe* frame, addr_t kernelStackTop)
 * Switches to the top of the kernel stack and returns to the frame. Used to
 * enter user mode for the first time. Does not return.
 */
	.globl	mips_enter_frame
	.ent	mips_enter_frame
mips_enter_frame:
	lui	$25, %hi(mips_return_to_frame)
	addiu	$25, $25, %lo(mips_return_to_frame)
	jr	$25
	 addiu	$29, $5, -32
	.end	mips_enter_frame


/*
 * void mips_cache_flush_all_kseg1(uint32 dcacheSize, uint32 dcacheLine,
 *		uint32 icacheSize, uint32 icacheLine)
 * Writes back and invalidates the whole data cache and invalidates the whole
 * instruction cache, with index type operations. The caller calls it at its
 * kseg1 address with interrupts off: on the R4600 an index type instruction
 * cache operation must not be fetched through the cache.
 */
	.globl	mips_cache_flush_all_kseg1
	.ent	mips_cache_flush_all_kseg1
mips_cache_flush_all_kseg1:
	lui	$8, 0x8000
	addu	$9, $8, $4
1:	cache	CACHE_INDEX_WRITEBACK_INVALIDATE_D, 0($8)
	addu	$8, $8, $5
	bne	$8, $9, 1b
	 nop

	lui	$8, 0x8000
	addu	$9, $8, $6
2:	cache	CACHE_INDEX_INVALIDATE_I, 0($8)
	addu	$8, $8, $7
	bne	$8, $9, 2b
	 nop
	sync
	jr	$31
	 nop
	.end	mips_cache_flush_all_kseg1
