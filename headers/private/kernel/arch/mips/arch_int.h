/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_INT_H
#define _KERNEL_ARCH_MIPS_INT_H


#include <SupportDefs.h>

#include <arch_cpu_defs.h>


/* Vectors 32 to 55 are the INT3 inputs (arch_indy.h). Vectors 1 and 12 are
   aliases of the keyboard and mouse input. */
#define NUM_IO_VECTORS	64


static inline int
arch_int_disable_interrupts_inline(void)
{
	uint32 status;
	__asm__ __volatile__(".set push\n"
		".set noreorder\n"
		".set noat\n"
		"mfc0 %0, $12\n"
		"ori $1, %0, 1\n"
		"xori $1, $1, 1\n"
		"mtc0 $1, $12\n"
		"nop\n"
		"nop\n"
		"nop\n"
		".set pop" : "=&r" (status) : : "$1");
	return status & CP0_STATUS_IE;
}


static inline void
arch_int_enable_interrupts_inline(void)
{
	__asm__ __volatile__(".set push\n"
		".set noreorder\n"
		".set noat\n"
		"mfc0 $1, $12\n"
		"ori $1, $1, 1\n"
		"mtc0 $1, $12\n"
		"nop\n"
		"nop\n"
		"nop\n"
		".set pop" : : : "$1");
}


static inline void
arch_int_restore_interrupts_inline(int oldState)
{
	if (oldState != 0)
		arch_int_enable_interrupts_inline();
}


static inline bool
arch_int_are_interrupts_enabled_inline(void)
{
	uint32 status;
	__asm__ __volatile__("mfc0 %0, $12" : "=r" (status));
	return (status & CP0_STATUS_IE) != 0;
}


// map the functions to the inline versions
#define arch_int_enable_interrupts()	arch_int_enable_interrupts_inline()
#define arch_int_disable_interrupts()	arch_int_disable_interrupts_inline()
#define arch_int_restore_interrupts(status)	\
	arch_int_restore_interrupts_inline(status)
#define arch_int_are_interrupts_enabled()	\
	arch_int_are_interrupts_enabled_inline()


#endif /* _KERNEL_ARCH_MIPS_INT_H */
