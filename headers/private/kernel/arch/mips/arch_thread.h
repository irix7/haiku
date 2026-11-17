/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_THREAD_H
#define _KERNEL_ARCH_MIPS_THREAD_H


#include <arch/cpu.h>

#include <arch_exception_block.h>


#ifdef __cplusplus
extern "C" {
#endif


/* The current thread is a field of the exception block, at a fixed kseg0
   address. The port supports one CPU. */

static inline Thread*
arch_thread_get_current_thread(void)
{
	return (Thread*)(addr_t)gMipsExceptionBlock->current_thread;
}


static inline void
arch_thread_set_current_thread(Thread* thread)
{
	gMipsExceptionBlock->current_thread = (uint32)(addr_t)thread;
}


#ifdef __cplusplus
}
#endif


#endif /* _KERNEL_ARCH_MIPS_THREAD_H */
