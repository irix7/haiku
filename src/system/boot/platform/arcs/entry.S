/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

#include <arch_cpu_defs.h>
#include <arch_exception_block.h>
#include <arch_vm_translation_map.h>

	.abicalls
	.option	pic0
	.set	noreorder
	.set	noat
	.text


/*
 * TLB refill handler. arcs_install_vectors() copies it to 0x80000000 and to
 * 0x80000080. It is position independent and at most 32 instructions long.
 * The kernel installs its own copy later (arch_exceptions.S).
 */
	.globl	arcs_tlb_refill
	.globl	arcs_tlb_refill_end
	.ent	arcs_tlb_refill
arcs_tlb_refill:
	lui	$27, 0x8000
	lw	$27, EXCEPTION_BLOCK_PAGE_DIRECTORY($27)
	mfc0	$26, C0_BADVADDR
	srl	$26, $26, MIPS_PAGE_DIRECTORY_SHIFT
	sll	$26, $26, 2
	addu	$27, $27, $26
	lw	$27, 0($27)			// the page table
	mfc0	$26, C0_BADVADDR
	srl	$26, $26, 15			// index of the pair of pages
	andi	$26, $26, 0x7ff
	sll	$26, $26, 3
	addu	$27, $27, $26
	lw	$26, 0($27)
	lw	$27, 4($27)
	srl	$26, $26, MIPS_PTE_SOFTWARE_BITS
	srl	$27, $27, MIPS_PTE_SOFTWARE_BITS
	mtc0	$26, C0_ENTRYLO0
	mtc0	$27, C0_ENTRYLO1
	nop
	tlbwr
	nop
	eret
arcs_tlb_refill_end:
	.end	arcs_tlb_refill


/*
 * void arcs_tlb_init(void)
 * Sets the page size and gives every TLB entry an address that cannot match.
 */
	.globl	arcs_tlb_init
	.ent	arcs_tlb_init
arcs_tlb_init:
	li	$8, CP0_PAGEMASK_16K
	mtc0	$8, C0_PAGEMASK
	mtc0	$0, C0_WIRED
	mtc0	$0, C0_ENTRYLO0
	mtc0	$0, C0_ENTRYLO1
	li	$8, 0
	li	$9, MIPS_TLB_ENTRIES
	lui	$10, 0x8000
	ori	$11, $0, 0x8000
1:	mtc0	$8, C0_INDEX
	mtc0	$10, C0_ENTRYHI
	nop
	tlbwi
	nop
	addiu	$8, $8, 1
	bne	$8, $9, 1b
	 addu	$10, $10, $11
	mtc0	$0, C0_ENTRYHI
	nop
	jr	$31
	 nop
	.end	arcs_tlb_init


/*
 * void arcs_flush_caches(uint32 dcacheSize, uint32 dcacheLine,
 *		uint32 icacheSize, uint32 icacheLine)
 * Writes back and invalidates the whole data cache and invalidates the whole
 * instruction cache. Call it through its kseg1 address: on the R4600 an
 * index type instruction cache operation must not be fetched through the
 * cache. Interrupts must be off.
 */
	.globl	arcs_flush_caches
	.ent	arcs_flush_caches
arcs_flush_caches:
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
	.end	arcs_flush_caches


/*
 * void arcs_enter_kernel(kernel_args* args, addr_t entry, addr_t stackTop)
 * Does not return.
 */
	.globl	arcs_enter_kernel
	.ent	arcs_enter_kernel
arcs_enter_kernel:
	move	$29, $6
	addiu	$29, $29, -32
	move	$30, $0
	move	$31, $0
	move	$25, $5
	jr	$25
	 move	$5, $0				// current CPU
	.end	arcs_enter_kernel


/*
 * int32 arcs_call(uint32 function, uint32 a0, uint32 a1, uint32 a2, uint32 a3)
 * Calls a function of the ARCS firmware vector. The PROM is o32 code: it
 * stores its arguments in the 16 bytes above the stack pointer, and it saves
 * only the low 32 bits of the callee-saved registers. The loader is n32
 * code, so the full registers are saved here.
 */
	.globl	arcs_call
	.ent	arcs_call
arcs_call:
	daddiu	$29, $29, -128
	sd	$31, 16($29)
	sd	$16, 24($29)
	sd	$17, 32($29)
	sd	$18, 40($29)
	sd	$19, 48($29)
	sd	$20, 56($29)
	sd	$21, 64($29)
	sd	$22, 72($29)
	sd	$23, 80($29)
	sd	$28, 88($29)
	sd	$30, 96($29)
	move	$25, $4
	move	$4, $5
	move	$5, $6
	move	$6, $7
	jalr	$25
	move	$7, $8
	ld	$31, 16($29)
	ld	$16, 24($29)
	ld	$17, 32($29)
	ld	$18, 40($29)
	ld	$19, 48($29)
	ld	$20, 56($29)
	ld	$21, 64($29)
	ld	$22, 72($29)
	ld	$23, 80($29)
	ld	$28, 88($29)
	ld	$30, 96($29)
	jr	$31
	daddiu	$29, $29, 128
	.end	arcs_call
