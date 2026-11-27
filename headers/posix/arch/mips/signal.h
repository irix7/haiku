/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_MIPS_SIGNAL_H_
#define _ARCH_MIPS_SIGNAL_H_


/*
 * Architecture-specific structure passed to signal handlers
 *
 * The ABI is n32: pointers are 32 bits wide, registers are 64 bits wide.
 */

#if defined(__mips__)
struct vregs {
	unsigned long long	r[32];	/* general registers, r[0] is always 0 */
	unsigned long long	pc;
	unsigned long long	hi;
	unsigned long long	lo;
	double				f[32];
	unsigned int		fcsr;
	unsigned int		_reserved;
};
#endif /* defined(__mips__) */


#endif /* _ARCH_MIPS_SIGNAL_H_ */
