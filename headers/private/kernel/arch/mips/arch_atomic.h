/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_ATOMIC_H
#define _KERNEL_ARCH_MIPS_ATOMIC_H


/* The atomic functions themselves are the compiler's ll/sc and lld/scd
   sequences (generic_atomic.cpp). */

static inline void
memory_read_barrier_inline(void)
{
	__asm__ __volatile__("sync" : : : "memory");
}


static inline void
memory_write_barrier_inline(void)
{
	__asm__ __volatile__("sync" : : : "memory");
}


static inline void
memory_full_barrier_inline(void)
{
	__asm__ __volatile__("sync" : : : "memory");
}


#define memory_read_barrier memory_read_barrier_inline
#define memory_write_barrier memory_write_barrier_inline
#define memory_full_barrier memory_full_barrier_inline


#endif	// _KERNEL_ARCH_MIPS_ATOMIC_H
