/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <new>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/vm_translation_map.h>
#include <arch_exception_block.h>
#include <arch_kernel.h>
#include <boot/kernel_args.h>
#include <kernel.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_priv.h>

#include "MIPSVMTranslationMap.h"


static uint32* sKernelDirectory;
static uint32* sInvalidTable;
static char sPhysicalPageMapperData[sizeof(MIPSVMPhysicalPageMapper)];


status_t
arch_vm_translation_map_init(kernel_args* args,
	VMPhysicalPageMapper** _physicalPageMapper)
{
	sKernelDirectory = (uint32*)(addr_t)args->arch_args.page_directory;
	sInvalidTable = (uint32*)(addr_t)args->arch_args.invalid_page_table;

	preloaded_elf32_image* image = static_cast<preloaded_elf32_image*>(
		args->kernel_image.Pointer());
	addr_t imageEnd = ROUNDUP(
		MAX(image->text_region.start + image->text_region.size,
			image->data_region.start + image->data_region.size), B_PAGE_SIZE);

	phys_addr_t ramEnd = 0;
	for (uint32 i = 0; i < args->num_physical_memory_ranges; i++) {
		phys_addr_t end = args->physical_memory_range[i].start
			+ args->physical_memory_range[i].size;
		if (end > ramEnd)
			ramEnd = end;
	}

	MIPSVMTranslationMap::Init(sKernelDirectory, sInvalidTable, ramEnd,
		imageEnd);

	gMipsExceptionBlock->page_directory = (uint32)(addr_t)sKernelDirectory;

	*_physicalPageMapper = new(&sPhysicalPageMapperData)
		MIPSVMPhysicalPageMapper();

	return B_OK;
}


status_t
arch_vm_translation_map_init_post_sem(kernel_args* args)
{
	return B_OK;
}


status_t
arch_vm_translation_map_init_post_area(kernel_args* args)
{
	return B_OK;
}


/*!	Maps a page before the VM is up. The loader has created the page tables
	for all of kseg2, so none needs to be allocated.
*/
status_t
arch_vm_translation_map_early_map(kernel_args* args, addr_t virtualAddress,
	phys_addr_t physicalAddress, uint8 attributes)
{
	if (virtualAddress < KERNEL_VM_BASE) {
		panic("early map of %#" B_PRIxADDR " outside of kseg2",
			virtualAddress);
		return B_BAD_VALUE;
	}

	uint32* table = (uint32*)(addr_t)
		sKernelDirectory[mips_page_directory_index(virtualAddress)];
	if (table == sInvalidTable) {
		panic("no page table for %#" B_PRIxADDR, virtualAddress);
		return B_ERROR;
	}

	uint32 entry = mips_pte_from_physical(physicalAddress) | MIPS_PTE_GLOBAL
		| MIPS_PTE_PRESENT | MIPS_PTE_VALID | MIPS_PTE_CACHED;
	if ((attributes & B_KERNEL_WRITE_AREA) != 0)
		entry |= MIPS_PTE_WRITABLE | MIPS_PTE_DIRTY;

	table[mips_page_table_index(virtualAddress)] = entry;
	mips_tlb_invalidate_page(virtualAddress);

	return B_OK;
}


status_t
arch_vm_translation_map_create_map(bool kernel, VMTranslationMap** _map)
{
	*_map = new(std::nothrow) MIPSVMTranslationMap(kernel,
		kernel ? sKernelDirectory : NULL);

	if (*_map == NULL)
		return B_NO_MEMORY;

	return B_OK;
}


bool
arch_vm_translation_map_is_kernel_page_accessible(addr_t virtualAddress,
	uint32 protection)
{
	if (mips_is_kseg0(virtualAddress) || mips_is_kseg1(virtualAddress))
		return true;
	if (virtualAddress < KERNEL_VM_BASE || sKernelDirectory == NULL)
		return false;

	uint32* table = (uint32*)(addr_t)
		sKernelDirectory[mips_page_directory_index(virtualAddress)];
	uint32 entry = table[mips_page_table_index(virtualAddress)];
	if ((entry & MIPS_PTE_PRESENT) == 0)
		return false;
	if ((protection & B_KERNEL_WRITE_AREA) != 0
		&& (entry & MIPS_PTE_WRITABLE) == 0) {
		return false;
	}
	return true;
}
