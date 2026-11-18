/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef MMU_H
#define MMU_H


/*
 * Physical memory use of the loader (RAM starts at 0x08000000):
 *
 * 0x08000000 - 0x0803ffff  exception vectors, exception block, PROM data
 * 0x08040000 - ...         the loader image and the appended archive
 * 0x09000000 - 0x097fffff  the kernel image, at its link address
 * 0x09800000 - 0x09ffffff  loader scratch: heap (upwards), stack (downwards)
 * ...        - top of RAM  regions for the kernel, allocated downwards.
 *                          Each one is also mapped in kseg2.
 *
 * The scratch memory and the loader image are not reported as allocated, so
 * the kernel reuses them.
 */
#define LOADER_KERNEL_BASE		0x89000000
#define LOADER_KERNEL_SIZE		0x00800000
#define LOADER_SCRATCH_BASE		0x89800000
#define LOADER_SCRATCH_TOP		0x8a000000
#define LOADER_STACK_SIZE		0x00100000
#define LOADER_STACK_TOP		LOADER_SCRATCH_TOP
#define LOADER_MINIMUM_RAM		0x04000000


#ifndef _ASSEMBLER

#include <SupportDefs.h>

#include <boot/platform.h>
#include <util/FixedWidthPointer.h>


void mmu_init();
void mmu_init_for_kernel();
void mmu_test_kernel_mapping();


inline addr_t
fix_address(addr_t address)
{
	addr_t result;
	if (platform_bootloader_address_to_kernel_address((void*)address, &result)
			!= B_OK) {
		return address;
	}
	return result;
}


template<typename Type>
inline void
fix_address(FixedWidthPointer<Type>& p)
{
	if (p != NULL)
		p.SetTo(fix_address(p.Get()));
}

#endif	/* !_ASSEMBLER */


#endif	/* MMU_H */
