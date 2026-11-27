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
 * 0x08800000 - 0x08bfffff  loader scratch: heap (upwards), stack (downwards)
 * 0x08c00000 - 0x08ffffff  regions for the kernel, second choice
 * 0x09000000 - 0x092fffff  the kernel image, at its link address
 * 0x09300000 - top of RAM  regions for the kernel, allocated downwards.
 *                          Each one is also mapped in kseg2.
 *
 * With 32 MB the regions (about 15 MB, 11 MB of them the unpacked boot
 * archive) do not fit above the kernel, so the rest goes below it.
 *
 * The scratch memory and the loader image are not reported as allocated, so
 * the kernel reuses them.
 */
#define LOADER_KERNEL_BASE		0x89000000
#define LOADER_KERNEL_SIZE		0x00300000
#define LOADER_SCRATCH_BASE		0x88800000
#define LOADER_SCRATCH_TOP		0x88c00000
#define LOADER_STACK_SIZE		0x00100000
#define LOADER_STACK_TOP		LOADER_SCRATCH_TOP
#define LOADER_MINIMUM_RAM		0x02000000


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
