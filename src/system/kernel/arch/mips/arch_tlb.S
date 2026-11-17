/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * TLB primitives. Each one turns interrupts off, sets PageMask for 16 kB
 * pages, restores EntryHi (the current ASID) and the interrupt state.
 * A TLB instruction has one nop before and one after it, and a result is
 * used three instructions after the instruction that produces it.
 */

#include <arch_cpu_defs.h>

	.abicalls
	.option	pic0
	.set	noreorder
	.set	noat
	.text

// Saves Status in $12 and EntryHi in $13, turns interrupts off.
#define TLB_ENTER \
	mfc0	$12, C0_STATUS; \
	ori	$13, $12, CP0_STATUS_IE; \
	xori	$13, $13, CP0_STATUS_IE; \
	mtc0	$13, C0_STATUS; \
	nop; \
	nop; \
	nop; \
	mfc0	$13, C0_ENTRYHI; \
	li	$14, CP0_PAGEMASK_16K; \
	mtc0	$14, C0_PAGEMASK

#define TLB_LEAVE \
	mtc0	$13, C0_ENTRYHI; \
	nop; \
	mtc0	$12, C0_STATUS; \
	nop; \
	nop; \
	nop


/*
 * void mips_tlb_write_indexed(uint32 index, uint32 entryHi, uint32 entryLo0,
 *		uint32 entryLo1)
 */
	.globl	mips_tlb_write_indexed
	.ent	mips_tlb_write_indexed
mips_tlb_write_indexed:
	TLB_ENTER
	mtc0	$4, C0_INDEX
	mtc0	$5, C0_ENTRYHI
	mtc0	$6, C0_ENTRYLO0
	mtc0	$7, C0_ENTRYLO1
	nop
	tlbwi
	nop
	TLB_LEAVE
	jr	$31
	 nop
	.end	mips_tlb_write_indexed


/*
 * void mips_tlb_write_random(uint32 entryHi, uint32 entryLo0,
 *		uint32 entryLo1)
 */
	.globl	mips_tlb_write_random
	.ent	mips_tlb_write_random
mips_tlb_write_random:
	TLB_ENTER
	mtc0	$4, C0_ENTRYHI
	mtc0	$5, C0_ENTRYLO0
	mtc0	$6, C0_ENTRYLO1
	nop
	tlbwr
	nop
	TLB_LEAVE
	jr	$31
	 nop
	.end	mips_tlb_write_random


/*
 * bool mips_tlb_update(addr_t address, uint32 entryLo0, uint32 entryLo1)
 * Rewrites the entry for the pair of pages around the address if there is
 * one, and writes a new entry otherwise. Returns whether an entry existed.
 */
	.globl	mips_tlb_update
	.ent	mips_tlb_update
mips_tlb_update:
	TLB_ENTER
	srl	$4, $4, 15
	sll	$4, $4, 15
	andi	$14, $13, CP0_ENTRYHI_ASID_MASK
	or	$4, $4, $14
	mtc0	$4, C0_ENTRYHI
	nop
	tlbp
	nop
	nop
	nop
	mfc0	$14, C0_INDEX
	mtc0	$5, C0_ENTRYLO0
	mtc0	$6, C0_ENTRYLO1
	bltz	$14, 1f
	 nop
	tlbwi
	b	2f
	 addiu	$2, $0, 1
1:	tlbwr
	move	$2, $0
2:	nop
	TLB_LEAVE
	jr	$31
	 nop
	.end	mips_tlb_update


/*
 * void mips_tlb_invalidate_page(addr_t address)
 * Replaces the entry that maps the address, if there is one, with an entry
 * that cannot match: an address in kseg0 that depends on the index.
 */
	.globl	mips_tlb_invalidate_page
	.ent	mips_tlb_invalidate_page
mips_tlb_invalidate_page:
	TLB_ENTER
	srl	$4, $4, 15
	sll	$4, $4, 15
	andi	$14, $13, CP0_ENTRYHI_ASID_MASK
	or	$4, $4, $14
	mtc0	$4, C0_ENTRYHI
	nop
	tlbp
	nop
	nop
	nop
	mfc0	$14, C0_INDEX
	bltz	$14, 1f
	 sll	$15, $14, 15
	lui	$4, 0x8000
	addu	$4, $4, $15
	mtc0	$4, C0_ENTRYHI
	mtc0	$0, C0_ENTRYLO0
	mtc0	$0, C0_ENTRYLO1
	nop
	tlbwi
	nop
1:	TLB_LEAVE
	jr	$31
	 nop
	.end	mips_tlb_invalidate_page


/*
 * void mips_tlb_flush_all(void)
 */
	.globl	mips_tlb_flush_all
	.ent	mips_tlb_flush_all
mips_tlb_flush_all:
	TLB_ENTER
	mtc0	$0, C0_ENTRYLO0
	mtc0	$0, C0_ENTRYLO1
	move	$8, $0
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
	TLB_LEAVE
	jr	$31
	 nop
	.end	mips_tlb_flush_all
