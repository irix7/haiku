/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_MIPS_DEBUGGER_H
#define _ARCH_MIPS_DEBUGGER_H


/* The ABI is n32: pointers are 32 bits wide, registers are 64 bits wide. */
struct mips_debug_cpu_state {
	uint64	r[32];	/* general registers, r[0] is always 0 */
	uint64	pc;
	uint64	hi;
	uint64	lo;
	double	f[32];
	uint32	fcsr;
	uint32	_reserved;
} __attribute__((aligned(8)));


#endif	// _ARCH_MIPS_DEBUGGER_H
