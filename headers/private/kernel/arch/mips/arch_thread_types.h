/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_ARCH_MIPS_THREAD_TYPES_H
#define KERNEL_ARCH_MIPS_THREAD_TYPES_H


#include <kernel.h>

#include <arch_exception_block.h>


namespace BKernel {
	struct Thread;
}


/*
 * The ABI is n32: pointers are 32 bits wide, registers are 64 bits wide.
 * A 32 bit value stored into a register slot must be sign-extended. Use
 * mips_sign_extend() for every such store: user code breaks on a register
 * that holds a zero-extended address above 0x7fffffff.
 *
 * The layout is used by assembly code through the IFRAME_* macros.
 */
struct iframe {
	uint64	r[32];		// general registers by number, r[0] is unused
	uint64	hi;
	uint64	lo;
	uint64	epc;
	uint64	badvaddr;
	uint32	status;
	uint32	cause;
	uint64	_reserved;
};

// indices into iframe::r
enum {
	MIPS_REG_AT = 1,
	MIPS_REG_V0 = 2,
	MIPS_REG_V1 = 3,
	MIPS_REG_A0 = 4,
	MIPS_REG_A1 = 5,
	MIPS_REG_A2 = 6,
	MIPS_REG_A3 = 7,
	MIPS_REG_A4 = 8,
	MIPS_REG_A5 = 9,
	MIPS_REG_A6 = 10,
	MIPS_REG_A7 = 11,
	MIPS_REG_T0 = 12,
	MIPS_REG_T9 = 25,
	MIPS_REG_K0 = 26,
	MIPS_REG_K1 = 27,
	MIPS_REG_GP = 28,
	MIPS_REG_SP = 29,
	MIPS_REG_FP = 30,
	MIPS_REG_RA = 31,
};


static inline uint64
mips_sign_extend(uint32 value)
{
	return (uint64)(int64)(int32)value;
}


// The callee-saved registers. Layout used by arch_context_switch().
struct arch_context;
struct fpu_context;
struct arch_context {
	uint64	s[8];		// $16 to $23
	uint64	gp;			// $28
	uint64	sp;			// $29
	uint64	fp;			// $30
	uint64	ra;			// $31
};

// Layout used by mips_fpu_save() and mips_fpu_restore().
struct fpu_context {
	uint64	f[32];
	uint32	fcsr;
	uint32	_reserved;
};

struct arch_thread {
	arch_context	context;
	fpu_context		fpuContext;
		// Valid while the thread does not own the FPU.
	iframe*			userFrame;
		// The frame of the thread's entry from user mode.
	uint64			oldV0;
		// The syscall number, for syscall restart.
};

struct arch_team {
	// gcc treats empty structures as zero-length in C, but as if they contain
	// a char in C++. So we have to put a dummy in to be able to use the struct
	// from both in a consistent way.
	char	dummy;
};

struct arch_fork_arg {
	iframe		frame;
	fpu_context	fpuContext;
};


#ifdef __cplusplus
extern "C" {
#endif

void arch_context_switch(arch_context* from, arch_context* to);
void mips_fpu_save(fpu_context* context);
void mips_fpu_restore(const fpu_context* context);
void mips_thread_entry();
	// Returns to the frame through the exception block. Does not return.
void mips_return_to_frame(iframe* frame) __attribute__((noreturn));

#ifdef __cplusplus
}
#endif


#endif	/* KERNEL_ARCH_MIPS_THREAD_TYPES_H */
