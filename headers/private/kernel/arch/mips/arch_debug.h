/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_DEBUG_H
#define _KERNEL_ARCH_MIPS_DEBUG_H


#include <SupportDefs.h>


struct arch_debug_registers {
	addr_t	fp;
	addr_t	sp;
	addr_t	pc;
};


#endif	// _KERNEL_ARCH_MIPS_DEBUG_H
