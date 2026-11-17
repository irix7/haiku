/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYSTEM_ARCH_MIPS_DEFS_H
#define _SYSTEM_ARCH_MIPS_DEFS_H


/* This header is included by C, C++ and assembly sources. */

/* CP0 register numbers */
#define CP0_INDEX			0
#define CP0_RANDOM			1
#define CP0_ENTRYLO0		2
#define CP0_ENTRYLO1		3
#define CP0_CONTEXT			4
#define CP0_PAGEMASK		5
#define CP0_WIRED			6
#define CP0_BADVADDR		8
#define CP0_COUNT			9
#define CP0_ENTRYHI			10
#define CP0_COMPARE			11
#define CP0_STATUS			12
#define CP0_CAUSE			13
#define CP0_EPC				14
#define CP0_PRID			15
#define CP0_CONFIG			16
#define CP0_LLADDR			17
#define CP0_WATCHLO			18
#define CP0_WATCHHI			19
#define CP0_XCONTEXT		20
#define CP0_ECC				26
#define CP0_CACHEERR		27
#define CP0_TAGLO			28
#define CP0_TAGHI			29
#define CP0_ERROREPC		30

/* Status register */
#define CP0_STATUS_IE		0x00000001	/* interrupt enable */
#define CP0_STATUS_EXL		0x00000002	/* exception level */
#define CP0_STATUS_ERL		0x00000004	/* error level */
#define CP0_STATUS_KSU_MASK	0x00000018	/* mode: 0 kernel, 0x10 user */
#define CP0_STATUS_KSU_USER	0x00000010
#define CP0_STATUS_UX		0x00000020	/* 64 bit addressing in user mode */
#define CP0_STATUS_SX		0x00000040
#define CP0_STATUS_KX		0x00000080
#define CP0_STATUS_IM_MASK	0x0000ff00	/* interrupt mask */
#define CP0_STATUS_IM_SHIFT	8
#define CP0_STATUS_IM2		0x00000400	/* INT3 local 0 */
#define CP0_STATUS_IM3		0x00000800	/* INT3 local 1 */
#define CP0_STATUS_IM6		0x00004000	/* bus error */
#define CP0_STATUS_IM7		0x00008000	/* Count/Compare */
#define CP0_STATUS_DE		0x00010000	/* no cache parity exceptions */
#define CP0_STATUS_SR		0x00100000
#define CP0_STATUS_TS		0x00200000
#define CP0_STATUS_BEV		0x00400000	/* exception vectors in the PROM */
#define CP0_STATUS_FR		0x04000000	/* 32 FPU registers of 64 bits */
#define CP0_STATUS_CU0		0x10000000
#define CP0_STATUS_CU1		0x20000000	/* FPU usable */

/* Cause register */
#define CP0_CAUSE_EXCCODE_MASK	0x0000007c
#define CP0_CAUSE_EXCCODE_SHIFT	2
#define CP0_CAUSE_IP_MASK		0x0000ff00
#define CP0_CAUSE_IP_SHIFT		8
#define CP0_CAUSE_CE_MASK		0x30000000
#define CP0_CAUSE_CE_SHIFT		28
#define CP0_CAUSE_BD			0x80000000	/* exception in a delay slot */

/* Exception codes */
#define MIPS_EXCEPTION_INTERRUPT			0
#define MIPS_EXCEPTION_TLB_MODIFIED			1
#define MIPS_EXCEPTION_TLB_LOAD				2
#define MIPS_EXCEPTION_TLB_STORE			3
#define MIPS_EXCEPTION_ADDRESS_LOAD			4
#define MIPS_EXCEPTION_ADDRESS_STORE		5
#define MIPS_EXCEPTION_BUS_INSTRUCTION		6
#define MIPS_EXCEPTION_BUS_DATA				7
#define MIPS_EXCEPTION_SYSCALL				8
#define MIPS_EXCEPTION_BREAKPOINT			9
#define MIPS_EXCEPTION_RESERVED_INSTRUCTION	10
#define MIPS_EXCEPTION_COPROCESSOR_UNUSABLE	11
#define MIPS_EXCEPTION_OVERFLOW				12
#define MIPS_EXCEPTION_TRAP					13
#define MIPS_EXCEPTION_VCE_INSTRUCTION		14
#define MIPS_EXCEPTION_FPU					15
#define MIPS_EXCEPTION_WATCH				23
#define MIPS_EXCEPTION_VCE_DATA				31

/* EntryLo0 and EntryLo1 */
#define CP0_ENTRYLO_G			0x00000001	/* global: ignore the ASID */
#define CP0_ENTRYLO_V			0x00000002	/* valid */
#define CP0_ENTRYLO_D			0x00000004	/* dirty: writes allowed */
#define CP0_ENTRYLO_C_MASK		0x00000038
#define CP0_ENTRYLO_C_UNCACHED	0x00000010
#define CP0_ENTRYLO_C_CACHED	0x00000018
#define CP0_ENTRYLO_PFN_SHIFT	6
	/* The frame number counts 4 kB units for every page size. */

/* EntryHi */
#define CP0_ENTRYHI_ASID_MASK	0x000000ff

/* PageMask value for a pair of 16 kB pages */
#define CP0_PAGEMASK_16K		0x00006000

/* Number of TLB entries. Each maps a pair of pages. */
#define MIPS_TLB_ENTRIES		48

/* PRId */
#define CP0_PRID_IMP_MASK		0x0000ff00
#define CP0_PRID_REV_MASK		0x000000ff
#define CP0_PRID_IMP_R4X00		0x00000400	/* the R4000 and the R4400 */
#define CP0_PRID_IMP_R4600		0x00002000
#define CP0_PRID_IMP_R4700		0x00002100
#define CP0_PRID_IMP_R5000		0x00002300
#define CP0_PRID_R4600_V1		0x00002010	/* with the low 4 bits masked */
#define CP0_PRID_R4600_V2		0x00002020

/* Config */
#define CP0_CONFIG_IC_SHIFT		9	/* I-cache size: 4 kB << field */
#define CP0_CONFIG_DC_SHIFT		6	/* D-cache size: 4 kB << field */
#define CP0_CONFIG_CACHE_MASK	0x7
#define CP0_CONFIG_IB			0x00000020	/* I-cache line: 32 bytes if set */
#define CP0_CONFIG_DB			0x00000010	/* D-cache line: 32 bytes if set */
#define CP0_CONFIG_K0_MASK		0x00000007	/* kseg0 cache mode */

/* cache instruction operations */
#define CACHE_INDEX_INVALIDATE_I			0x00
#define CACHE_INDEX_WRITEBACK_INVALIDATE_D	0x01
#define CACHE_HIT_INVALIDATE_I				0x10
#define CACHE_HIT_INVALIDATE_D				0x11
#define CACHE_HIT_WRITEBACK_INVALIDATE_D	0x15

/* FPU control and status register (FCSR, control register 31) */
#define FCSR_ROUNDING_MASK		0x00000003
#define FCSR_FLAGS_MASK			0x0000007c
#define FCSR_ENABLES_MASK		0x00000f80
#define FCSR_CAUSE_MASK			0x0003f000
#define FCSR_CAUSE_UNIMPLEMENTED 0x00020000
#define FCSR_CAUSE_INVALID		0x00010000
#define FCSR_CAUSE_DIVBYZERO	0x00008000
#define FCSR_CAUSE_OVERFLOW		0x00004000
#define FCSR_CAUSE_UNDERFLOW	0x00002000
#define FCSR_CAUSE_INEXACT		0x00001000
#define FCSR_FLUSH_TO_ZERO		0x01000000
/* The FCSR of a new thread: round to nearest, no traps, flush to zero. */
#define FCSR_DEFAULT			FCSR_FLUSH_TO_ZERO

/* Fixed virtual address segments */
#define KSEG0_BASE				0x80000000
#define KSEG1_BASE				0xa0000000
#define KSEG2_BASE				0xc0000000
#define KSEG_PHYSICAL_SIZE		0x20000000


#ifdef _ASSEMBLER

/* CP0 registers as assembler operands */
#define C0_INDEX		$0
#define C0_RANDOM		$1
#define C0_ENTRYLO0		$2
#define C0_ENTRYLO1		$3
#define C0_CONTEXT		$4
#define C0_PAGEMASK		$5
#define C0_WIRED		$6
#define C0_BADVADDR		$8
#define C0_COUNT		$9
#define C0_ENTRYHI		$10
#define C0_COMPARE		$11
#define C0_STATUS		$12
#define C0_CAUSE		$13
#define C0_EPC			$14
#define C0_PRID			$15
#define C0_CONFIG		$16
#define C0_LLADDR		$17
#define C0_WATCHLO		$18
#define C0_WATCHHI		$19
#define C0_XCONTEXT		$20
#define C0_ECC			$26
#define C0_CACHEERR		$27
#define C0_TAGLO		$28
#define C0_TAGHI		$29
#define C0_ERROREPC		$30

#endif	/* _ASSEMBLER */


#ifndef _ASSEMBLER

#define SPINLOCK_PAUSE()	do {} while (false)

#endif	/* !_ASSEMBLER */


#endif	/* _SYSTEM_ARCH_MIPS_DEFS_H */
