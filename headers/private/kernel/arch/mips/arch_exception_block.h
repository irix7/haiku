/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_EXCEPTION_BLOCK_H
#define _KERNEL_ARCH_MIPS_EXCEPTION_BLOCK_H


/*
 * The exception block is a fixed region in kseg0, after the exception
 * vectors. Kseg0 needs no TLB entry, so exception handlers can use the block
 * while Status.EXL is set. Handlers address it as an offset from 0x80000000
 * (lui $27, 0x8000), which needs no relocation.
 *
 * There is one block, because the port supports one CPU.
 *
 * This header is included by C, C++ and assembly sources.
 */

#define MIPS_EXCEPTION_VECTOR_TLB_REFILL	0x80000000
#define MIPS_EXCEPTION_VECTOR_XTLB_REFILL	0x80000080
#define MIPS_EXCEPTION_VECTOR_CACHE_ERROR	0xa0000100
#define MIPS_EXCEPTION_VECTOR_GENERAL		0x80000180
#define MIPS_EXCEPTION_VECTOR_SIZE			0x80

#define MIPS_EXCEPTION_BLOCK_BASE			0x80000400

/* Offsets from 0x80000000 */
#define EXCEPTION_BLOCK_KERNEL_STACK		0x400
	/* top of the current thread's kernel stack */
#define EXCEPTION_BLOCK_PAGE_DIRECTORY		0x404
	/* kseg0 address of the current page directory */
#define EXCEPTION_BLOCK_USER_TLS			0x408
	/* TLS base of the current thread. Loaded into $27 on every return to
	   user mode. */
#define EXCEPTION_BLOCK_CURRENT_THREAD		0x40c
	/* the current Thread */
#define EXCEPTION_BLOCK_IN_ENTRY			0x410
	/* not 0 while the frame below holds an exception that has not been
	   copied to a kernel stack yet */
#define EXCEPTION_BLOCK_FAULT_COUNT			0x414
	/* number of exceptions taken while IN_ENTRY was set */
#define EXCEPTION_BLOCK_FRAME				0x418
	/* an iframe: the register save area */
#define EXCEPTION_BLOCK_END					(EXCEPTION_BLOCK_FRAME + IFRAME_SIZE)

/* Layout of struct iframe (arch_thread_types.h) */
#define IFRAME_R(n)							((n) * 8)
#define IFRAME_HI							256
#define IFRAME_LO							264
#define IFRAME_EPC							272
#define IFRAME_BADVADDR						280
#define IFRAME_STATUS						288
#define IFRAME_CAUSE						292
#define IFRAME_SIZE							304

/* Layout of struct arch_context (arch_thread_types.h) */
#define CONTEXT_S(n)						((n) * 8)
#define CONTEXT_GP							64
#define CONTEXT_SP							72
#define CONTEXT_FP							80
#define CONTEXT_RA							88
#define CONTEXT_SIZE						96

/* Layout of struct fpu_context (arch_thread_types.h) */
#define FPU_CONTEXT_F(n)					((n) * 8)
#define FPU_CONTEXT_FCSR					256
#define FPU_CONTEXT_SIZE					264


#ifndef _ASSEMBLER

#include <SupportDefs.h>

struct mips_exception_block {
	uint32	kernel_stack_top;
	uint32	page_directory;
	uint32	user_tls;
	uint32	current_thread;
	uint32	in_entry;
	uint32	fault_count;
	uint64	frame[IFRAME_SIZE / 8];
};

#define gMipsExceptionBlock \
	((struct mips_exception_block*)MIPS_EXCEPTION_BLOCK_BASE)

#endif	/* !_ASSEMBLER */


#endif	/* _KERNEL_ARCH_MIPS_EXCEPTION_BLOCK_H */
