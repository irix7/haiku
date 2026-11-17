/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <arch/vm.h>
#include <arch_kernel.h>
#include <arch_vm_translation_map.h>
#include <boot/kernel_args.h>
#include <kernel.h>
#include <vm/vm.h>
#include <vm/VMAddressSpace.h>


status_t
arch_vm_init(kernel_args* args)
{
	return B_OK;
}


/*!	kseg0 and kseg1 are part of the kernel address space, but the CPU maps
	them itself. Block every part of them that is not the kernel image, so
	the VM places nothing there.
*/
status_t
arch_vm_init_post_area(kernel_args* args)
{
	preloaded_elf32_image* image = static_cast<preloaded_elf32_image*>(
		args->kernel_image.Pointer());
	addr_t imageStart = MIN(image->text_region.start,
		image->data_region.start);
	addr_t imageEnd = MAX(image->text_region.start + image->text_region.size,
		image->data_region.start + image->data_region.size);
	imageStart = ROUNDDOWN(imageStart, B_PAGE_SIZE);
	imageEnd = ROUNDUP(imageEnd, B_PAGE_SIZE);

	// vm_block_address_range() returns the ID of the area it creates.
	status_t status = B_OK;
	if (imageStart > KERNEL_BASE) {
		status = vm_block_address_range("kseg0 low", (void*)KERNEL_BASE,
			imageStart - KERNEL_BASE);
	}
	if (status >= B_OK && imageEnd < KERNEL_VM_BASE) {
		status = vm_block_address_range("kseg0 and kseg1", (void*)imageEnd,
			KERNEL_VM_BASE - imageEnd);
	}
	if (status < B_OK)
		panic("could not block kseg0 and kseg1: %s", strerror(status));

	return B_OK;
}


status_t
arch_vm_init_end(kernel_args* args)
{
	// Free the memory the loader mapped into kseg2 that no area covers.
	vm_free_unused_boot_loader_range(KERNEL_VM_BASE,
		args->arch_args.virtual_end - KERNEL_VM_BASE);
	return B_OK;
}


status_t
arch_vm_init_post_modules(kernel_args* args)
{
	return B_OK;
}


void
arch_vm_aspace_swap(struct VMAddressSpace* from, struct VMAddressSpace* to)
{
	// done in arch_thread_context_switch()
}


bool
arch_vm_supports_protection(team_id team, uint32 protection)
{
	// The TLB cannot make a page writable without making it readable, and
	// it cannot keep user code away from a page below KERNEL_BASE. Both
	// combinations are accepted and get more access than they ask for.
	return true;
}


void
arch_vm_unset_memory_type(VMArea* area)
{
}


status_t
arch_vm_set_memory_type(VMArea* area, phys_addr_t physicalBase, uint32 type,
	uint32* effectiveType)
{
	// Device memory is mapped uncached and RAM cached, decided by the
	// physical address (MIPSVMTranslationMap::Map()).
	return B_OK;
}
