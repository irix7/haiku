/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * Exception vectors, exception entry and exception return.
 *
 * Registers are named by number: the assembler's names for $8 to $15 differ
 * between the ABIs. $26 and $27 are reserved for the kernel. In user mode
 * $27 holds the thread's TLS base, so every return to user mode loads it as
 * its last step.
 *
 * Entry: all registers are saved in the exception block while Status.EXL is
 * set. The block is in kseg0 and needs no TLB entry. Then EXL is cleared with
 * interrupts off and the frame is copied to the kernel stack. Kernel stacks
 * are mapped in kseg2: a TLB miss on one while EXL is set would enter the
 * general vector again and destroy the state of the first exception.
 * Return: the reverse.
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
 * TLB refill vector for kernel mode addresses (0x80000000).
 * Position independent, at most 32 instructions.
 */
	.globl	mips_tlb_refill_vector
	.globl	mips_tlb_refill_vector_end
	.ent	mips_tlb_refill_vector
mips_tlb_refill_vector:
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
	lui	$27, 0x8000
	lw	$27, EXCEPTION_BLOCK_USER_TLS($27)
	eret
mips_tlb_refill_vector_end:
	.end	mips_tlb_refill_vector


/*
 * TLB refill vector for user mode addresses (0x80000080). User threads run
 * with 64 bit addressing enabled, so the address can lie above the 2 GB the
 * page tables cover. Such an address is passed on to the general handler.
 */
	.globl	mips_xtlb_refill_vector
	.globl	mips_xtlb_refill_vector_end
	.ent	mips_xtlb_refill_vector
mips_xtlb_refill_vector:
	dmfc0	$26, C0_BADVADDR
	dsrl	$27, $26, 31
	bnez	$27, 1f
	 lui	$27, 0x8000
	lw	$27, EXCEPTION_BLOCK_PAGE_DIRECTORY($27)
	srl	$26, $26, MIPS_PAGE_DIRECTORY_SHIFT
	sll	$26, $26, 2
	addu	$27, $27, $26
	lw	$27, 0($27)
	mfc0	$26, C0_BADVADDR
	srl	$26, $26, 15
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
	lui	$27, 0x8000
	lw	$27, EXCEPTION_BLOCK_USER_TLS($27)
	eret
1:	lui	$26, %hi(mips_exception_entry)
	addiu	$26, $26, %lo(mips_exception_entry)
	jr	$26
	 nop
mips_xtlb_refill_vector_end:
	.end	mips_xtlb_refill_vector


/*
 * General exception vector (0x80000180), and the cache error vector.
 */
	.globl	mips_general_vector
	.globl	mips_general_vector_end
	.ent	mips_general_vector
mips_general_vector:
	lui	$26, %hi(mips_exception_entry)
	addiu	$26, $26, %lo(mips_exception_entry)
	jr	$26
	 nop
mips_general_vector_end:
	.end	mips_general_vector

	.globl	mips_cache_error_vector
	.globl	mips_cache_error_vector_end
	.ent	mips_cache_error_vector
mips_cache_error_vector:
	lui	$26, %hi(mips_cache_error)
	addiu	$26, $26, %lo(mips_cache_error)
	jr	$26
	 nop
mips_cache_error_vector_end:
	.end	mips_cache_error_vector


/*
 * Common exception entry.
 */
	.globl	mips_exception_entry
	.ent	mips_exception_entry
mips_exception_entry:
	lui	$27, 0x8000

	// An exception while the block still holds an uncopied frame is fatal:
	// it happened in the entry or return code itself.
	lw	$26, EXCEPTION_BLOCK_IN_ENTRY($27)
	bnez	$26, mips_double_fault
	 nop
	addiu	$26, $0, 1
	sw	$26, EXCEPTION_BLOCK_IN_ENTRY($27)

	sd	$1, (EXCEPTION_BLOCK_FRAME + IFRAME_R(1))($27)
	sd	$2, (EXCEPTION_BLOCK_FRAME + IFRAME_R(2))($27)
	sd	$3, (EXCEPTION_BLOCK_FRAME + IFRAME_R(3))($27)
	sd	$4, (EXCEPTION_BLOCK_FRAME + IFRAME_R(4))($27)
	sd	$5, (EXCEPTION_BLOCK_FRAME + IFRAME_R(5))($27)
	sd	$6, (EXCEPTION_BLOCK_FRAME + IFRAME_R(6))($27)
	sd	$7, (EXCEPTION_BLOCK_FRAME + IFRAME_R(7))($27)
	sd	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_R(8))($27)
	sd	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_R(9))($27)
	sd	$10, (EXCEPTION_BLOCK_FRAME + IFRAME_R(10))($27)
	sd	$11, (EXCEPTION_BLOCK_FRAME + IFRAME_R(11))($27)
	sd	$12, (EXCEPTION_BLOCK_FRAME + IFRAME_R(12))($27)
	sd	$13, (EXCEPTION_BLOCK_FRAME + IFRAME_R(13))($27)
	sd	$14, (EXCEPTION_BLOCK_FRAME + IFRAME_R(14))($27)
	sd	$15, (EXCEPTION_BLOCK_FRAME + IFRAME_R(15))($27)
	sd	$16, (EXCEPTION_BLOCK_FRAME + IFRAME_R(16))($27)
	sd	$17, (EXCEPTION_BLOCK_FRAME + IFRAME_R(17))($27)
	sd	$18, (EXCEPTION_BLOCK_FRAME + IFRAME_R(18))($27)
	sd	$19, (EXCEPTION_BLOCK_FRAME + IFRAME_R(19))($27)
	sd	$20, (EXCEPTION_BLOCK_FRAME + IFRAME_R(20))($27)
	sd	$21, (EXCEPTION_BLOCK_FRAME + IFRAME_R(21))($27)
	sd	$22, (EXCEPTION_BLOCK_FRAME + IFRAME_R(22))($27)
	sd	$23, (EXCEPTION_BLOCK_FRAME + IFRAME_R(23))($27)
	sd	$24, (EXCEPTION_BLOCK_FRAME + IFRAME_R(24))($27)
	sd	$25, (EXCEPTION_BLOCK_FRAME + IFRAME_R(25))($27)
	sd	$28, (EXCEPTION_BLOCK_FRAME + IFRAME_R(28))($27)
	sd	$29, (EXCEPTION_BLOCK_FRAME + IFRAME_R(29))($27)
	sd	$30, (EXCEPTION_BLOCK_FRAME + IFRAME_R(30))($27)
	sd	$31, (EXCEPTION_BLOCK_FRAME + IFRAME_R(31))($27)
	mfhi	$8
	mflo	$9
	sd	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_HI)($27)
	sd	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_LO)($27)
	dmfc0	$8, C0_EPC
	dmfc0	$9, C0_BADVADDR
	mfc0	$10, C0_STATUS
	mfc0	$11, C0_CAUSE
	sd	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_EPC)($27)
	sd	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_BADVADDR)($27)
	sw	$10, (EXCEPTION_BLOCK_FRAME + IFRAME_STATUS)($27)
	sw	$11, (EXCEPTION_BLOCK_FRAME + IFRAME_CAUSE)($27)

	// Choose the stack: the thread's kernel stack when coming from user
	// mode, the current stack otherwise.
	andi	$8, $10, CP0_STATUS_KSU_MASK
	beqz	$8, 1f
	 move	$16, $29
	lw	$16, EXCEPTION_BLOCK_KERNEL_STACK($27)
1:	addiu	$16, $16, -IFRAME_SIZE
	move	$17, $27			// the block: $27 does not survive below

	// Kernel mode, EXL clear, interrupts off, 32 bit addressing, FPU off.
	// From here on TLB misses are handled normally and $26 and $27 can
	// change at any instruction.
	lui	$8, ((CP0_STATUS_CU0 | CP0_STATUS_DE) >> 16)
	andi	$9, $10, CP0_STATUS_IM_MASK
	or	$8, $8, $9
	mtc0	$8, C0_STATUS
	nop
	nop
	nop

	// copy the frame to the stack
	addiu	$8, $17, EXCEPTION_BLOCK_FRAME
	move	$9, $16
	addiu	$10, $8, IFRAME_SIZE
2:	ld	$11, 0($8)
	addiu	$8, $8, 8
	sd	$11, 0($9)
	bne	$8, $10, 2b
	 addiu	$9, $9, 8

	sw	$0, EXCEPTION_BLOCK_IN_ENTRY($17)

	move	$29, $16
	move	$4, $16				// the iframe
	addiu	$29, $29, -32
	move	$30, $0
	lui	$25, %hi(mips_handle_exception)
	addiu	$25, $25, %lo(mips_handle_exception)
	jalr	$25
	 nop

	move	$4, $16
	// fall through
	.end	mips_exception_entry


/*
 * void mips_return_to_frame(iframe* frame)
 * Restores the frame and returns to it. Interrupts are turned off here.
 */
	.globl	mips_return_to_frame
	.ent	mips_return_to_frame
mips_return_to_frame:
	mfc0	$8, C0_STATUS
	ori	$8, $8, CP0_STATUS_IE
	xori	$8, $8, CP0_STATUS_IE
	mtc0	$8, C0_STATUS
	nop
	nop
	nop

	lui	$17, 0x8000
	addiu	$9, $0, 1
	sw	$9, EXCEPTION_BLOCK_IN_ENTRY($17)

	// copy the frame to the block
	addiu	$9, $17, EXCEPTION_BLOCK_FRAME
	addiu	$10, $4, IFRAME_SIZE
1:	ld	$11, 0($4)
	addiu	$4, $4, 8
	sd	$11, 0($9)
	bne	$4, $10, 1b
	 addiu	$9, $9, 8

	// Set EXL. From here on only kseg0 is accessed.
	ori	$8, $8, CP0_STATUS_EXL
	mtc0	$8, C0_STATUS
	nop
	nop
	nop

	lui	$27, 0x8000
	lw	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_STATUS)($27)
	ld	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_EPC)($27)
	ori	$8, $8, CP0_STATUS_EXL
	mtc0	$8, C0_STATUS
	dmtc0	$9, C0_EPC
	ld	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_HI)($27)
	ld	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_LO)($27)
	mthi	$8
	mtlo	$9

	ld	$1, (EXCEPTION_BLOCK_FRAME + IFRAME_R(1))($27)
	ld	$2, (EXCEPTION_BLOCK_FRAME + IFRAME_R(2))($27)
	ld	$3, (EXCEPTION_BLOCK_FRAME + IFRAME_R(3))($27)
	ld	$4, (EXCEPTION_BLOCK_FRAME + IFRAME_R(4))($27)
	ld	$5, (EXCEPTION_BLOCK_FRAME + IFRAME_R(5))($27)
	ld	$6, (EXCEPTION_BLOCK_FRAME + IFRAME_R(6))($27)
	ld	$7, (EXCEPTION_BLOCK_FRAME + IFRAME_R(7))($27)
	ld	$8, (EXCEPTION_BLOCK_FRAME + IFRAME_R(8))($27)
	ld	$9, (EXCEPTION_BLOCK_FRAME + IFRAME_R(9))($27)
	ld	$10, (EXCEPTION_BLOCK_FRAME + IFRAME_R(10))($27)
	ld	$11, (EXCEPTION_BLOCK_FRAME + IFRAME_R(11))($27)
	ld	$12, (EXCEPTION_BLOCK_FRAME + IFRAME_R(12))($27)
	ld	$13, (EXCEPTION_BLOCK_FRAME + IFRAME_R(13))($27)
	ld	$14, (EXCEPTION_BLOCK_FRAME + IFRAME_R(14))($27)
	ld	$15, (EXCEPTION_BLOCK_FRAME + IFRAME_R(15))($27)
	ld	$16, (EXCEPTION_BLOCK_FRAME + IFRAME_R(16))($27)
	ld	$17, (EXCEPTION_BLOCK_FRAME + IFRAME_R(17))($27)
	ld	$18, (EXCEPTION_BLOCK_FRAME + IFRAME_R(18))($27)
	ld	$19, (EXCEPTION_BLOCK_FRAME + IFRAME_R(19))($27)
	ld	$20, (EXCEPTION_BLOCK_FRAME + IFRAME_R(20))($27)
	ld	$21, (EXCEPTION_BLOCK_FRAME + IFRAME_R(21))($27)
	ld	$22, (EXCEPTION_BLOCK_FRAME + IFRAME_R(22))($27)
	ld	$23, (EXCEPTION_BLOCK_FRAME + IFRAME_R(23))($27)
	ld	$24, (EXCEPTION_BLOCK_FRAME + IFRAME_R(24))($27)
	ld	$25, (EXCEPTION_BLOCK_FRAME + IFRAME_R(25))($27)
	ld	$28, (EXCEPTION_BLOCK_FRAME + IFRAME_R(28))($27)
	ld	$29, (EXCEPTION_BLOCK_FRAME + IFRAME_R(29))($27)
	ld	$30, (EXCEPTION_BLOCK_FRAME + IFRAME_R(30))($27)
	ld	$31, (EXCEPTION_BLOCK_FRAME + IFRAME_R(31))($27)

	sw	$0, EXCEPTION_BLOCK_IN_ENTRY($27)
	lw	$27, EXCEPTION_BLOCK_USER_TLS($27)
	eret
	.end	mips_return_to_frame


/*
 * An exception in the entry or return path. The frame in the block is lost.
 * Continue on the emergency stack, which is in the kernel's bss (kseg0).
 */
	.ent	mips_double_fault
mips_double_fault:
	lw	$26, EXCEPTION_BLOCK_FAULT_COUNT($27)
	addiu	$26, $26, 1
	sw	$26, EXCEPTION_BLOCK_FAULT_COUNT($27)

	dmfc0	$4, C0_EPC
	dmfc0	$5, C0_BADVADDR
	mfc0	$6, C0_CAUSE
	move	$7, $29

	lui	$8, ((CP0_STATUS_CU0 | CP0_STATUS_DE) >> 16)
	mtc0	$8, C0_STATUS
	nop
	nop
	nop

	lui	$29, %hi(gMipsEmergencyStackTop)
	lw	$29, %lo(gMipsEmergencyStackTop)($29)
	addiu	$29, $29, -32
	lui	$25, %hi(mips_double_fault_handler)
	addiu	$25, $25, %lo(mips_double_fault_handler)
	jalr	$25
	 nop
1:	b	1b
	 nop
	.end	mips_double_fault


	.ent	mips_cache_error
mips_cache_error:
	lui	$8, ((CP0_STATUS_CU0 | CP0_STATUS_DE) >> 16)
	mtc0	$8, C0_STATUS
	nop
	nop
	nop
	lui	$29, %hi(gMipsEmergencyStackTop)
	lw	$29, %lo(gMipsEmergencyStackTop)($29)
	addiu	$29, $29, -32
	lui	$25, %hi(mips_cache_error_handler)
	addiu	$25, $25, %lo(mips_cache_error_handler)
	jalr	$25
	 nop
1:	b	1b
	 nop
	.end	mips_cache_error
