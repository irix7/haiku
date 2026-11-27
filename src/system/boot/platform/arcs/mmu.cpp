/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "mmu.h"

#include <string.h>

#include <OS.h>

#include <arch/cpu.h>
#include <arch_exception_block.h>
#include <arch_indy.h>
#include <arch_kernel.h>
#include <arch_vm_translation_map.h>
#include <boot/kernel_args.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>
#include <kernel.h>


//#define TRACE_MMU
#ifdef TRACE_MMU
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


extern uint8 _end;
extern "C" void arcs_tlb_init(void);


struct MemoryRegion {
	addr_t	loaderAddress;		// kseg0
	addr_t	kernelAddress;		// kseg2, or kseg0 for the kernel image
	size_t	size;
};

static const uint32 kMaxRegions = 128;
static MemoryRegion sRegions[kMaxRegions];
static uint32 sRegionCount;

static phys_addr_t sRamEnd;
static phys_addr_t sPhysicalNext;
	// Regions and page tables are allocated downwards from here.
static phys_addr_t sPhysicalNextLow = LOADER_KERNEL_BASE - KSEG0_BASE;
	// When nothing is left above the kernel image: downwards from here.
static addr_t sVirtualNext = KERNEL_VM_BASE;
static addr_t sHeapNext = LOADER_SCRATCH_BASE;
static addr_t sKernelImageEnd;

static uint32* sPageDirectory;
static uint32* sInvalidPageTable;


static void*
allocate_physical(size_t size)
{
	size = ROUNDUP(size, B_PAGE_SIZE);
	if (sPhysicalNext - size
			>= LOADER_KERNEL_BASE + LOADER_KERNEL_SIZE - KSEG0_BASE) {
		sPhysicalNext -= size;
		return mips_physical_to_kseg0(sPhysicalNext);
	}

	if (sPhysicalNextLow - size < LOADER_SCRATCH_TOP - KSEG0_BASE) {
		panic("out of memory: %" B_PRIuSIZE " bytes requested", size);
		return NULL;
	}

	sPhysicalNextLow -= size;
	return mips_physical_to_kseg0(sPhysicalNextLow);
}


static uint32*
allocate_page_table(uint32 emptyEntry)
{
	uint32* table = (uint32*)allocate_physical(B_PAGE_SIZE);
	for (uint32 i = 0; i < MIPS_PAGE_TABLE_ENTRIES; i++)
		table[i] = emptyEntry;
	return table;
}


static void
map_page(addr_t virtualAddress, phys_addr_t physicalAddress)
{
	uint32* table = (uint32*)(addr_t)
		sPageDirectory[mips_page_directory_index(virtualAddress)];
	if (table == sInvalidPageTable)
		panic("no page table for %#" B_PRIxADDR, virtualAddress);

	table[mips_page_table_index(virtualAddress)]
		= mips_pte_from_physical(physicalAddress) | MIPS_PTE_GLOBAL
			| MIPS_PTE_VALID | MIPS_PTE_DIRTY | MIPS_PTE_CACHED
			| MIPS_PTE_PRESENT | MIPS_PTE_WRITABLE;
}


static MemoryRegion*
find_region_by_loader_address(addr_t address)
{
	for (uint32 i = 0; i < sRegionCount; i++) {
		if (address >= sRegions[i].loaderAddress
			&& address - sRegions[i].loaderAddress < sRegions[i].size) {
			return &sRegions[i];
		}
	}
	return NULL;
}


static MemoryRegion*
find_region_by_kernel_address(addr_t address)
{
	for (uint32 i = 0; i < sRegionCount; i++) {
		if (address >= sRegions[i].kernelAddress
			&& address - sRegions[i].kernelAddress < sRegions[i].size) {
			return &sRegions[i];
		}
	}
	return NULL;
}


/*!	Reads the size of the contiguous RAM that starts at 0x08000000 from the
	memory controller.
*/
static phys_addr_t
ram_end()
{
	uint32 shift
		= (indy_read32(INDY_MC_SYSID) & INDY_MC_SYSID_REVISION_MASK) >= 5
			? 24 : 22;
	uint32 config[2] = {
		indy_read32(INDY_MC_MEMCFG0), indy_read32(INDY_MC_MEMCFG1)
	};

	phys_addr_t end = INDY_RAM_BASE;
	// A bank extends the range if it starts where the range ends. The banks
	// are in no particular order.
	for (int pass = 0; pass < 4; pass++) {
		for (int i = 0; i < 4; i++) {
			uint32 field = (config[i / 2] >> ((i % 2) == 0 ? 16 : 0)) & 0xffff;
			if ((field & INDY_MC_MEMCFG_VALID) == 0)
				continue;
			phys_addr_t base = (phys_addr_t)(field & 0xff) << shift;
			phys_addr_t size = (phys_addr_t)(((field >> 8) & 0x1f) + 1) << shift;
			if (base == end)
				end += size;
		}
	}

	// kseg0 reaches the first 512 MB of the physical address space.
	if (end > KSEG_PHYSICAL_SIZE)
		end = KSEG_PHYSICAL_SIZE;
	return end;
}


//	#pragma mark -


extern "C" status_t
platform_allocate_region(void** _address, size_t size, uint8 protection)
{
	size = ROUNDUP(size, B_PAGE_SIZE);
	addr_t requested = (addr_t)*_address;

	if (sRegionCount == kMaxRegions)
		return B_NO_MEMORY;
	MemoryRegion& region = sRegions[sRegionCount];

	if (requested >= LOADER_KERNEL_BASE
		&& requested < LOADER_KERNEL_BASE + LOADER_KERNEL_SIZE) {
		// The kernel image. It runs at its link address in kseg0, which is
		// also where the loader reaches it.
		if (sKernelImageEnd != 0
			|| requested + size > LOADER_KERNEL_BASE + LOADER_KERNEL_SIZE) {
			return B_NO_MEMORY;
		}
		sKernelImageEnd = requested + size;
		region.loaderAddress = requested;
		region.kernelAddress = requested;
	} else {
		if (sVirtualNext + size < sVirtualNext)
			return B_NO_MEMORY;
		void* memory = allocate_physical(size);
		if (memory == NULL)
			return B_NO_MEMORY;

		region.loaderAddress = (addr_t)memory;
		region.kernelAddress = sVirtualNext;
		sVirtualNext += size;

		for (size_t offset = 0; offset < size; offset += B_PAGE_SIZE) {
			map_page(region.kernelAddress + offset,
				mips_kseg0_to_physical(memory) + offset);
		}
	}

	region.size = size;
	sRegionCount++;

	TRACE("platform_allocate_region: %#" B_PRIxADDR " -> %#" B_PRIxADDR
		", %" B_PRIuSIZE " bytes\n", region.loaderAddress,
		region.kernelAddress, size);

	*_address = (void*)region.loaderAddress;
	return B_OK;
}


extern "C" status_t
platform_free_region(void* address, size_t size)
{
	// The region stays mapped. The kernel frees the pages of every loader
	// mapping that no area covers.
	return B_OK;
}


ssize_t
platform_allocate_heap_region(size_t size, void** _base)
{
	size = ROUNDUP(size, B_PAGE_SIZE);
	if (sHeapNext + size > LOADER_STACK_TOP - LOADER_STACK_SIZE)
		return B_NO_MEMORY;

	*_base = (void*)sHeapNext;
	sHeapNext += size;
	return size;
}


void
platform_free_heap_region(void* _base, size_t size)
{
	if ((addr_t)_base + size == sHeapNext)
		sHeapNext = (addr_t)_base;
}


status_t
platform_bootloader_address_to_kernel_address(void* address, addr_t* _result)
{
	MemoryRegion* region = find_region_by_loader_address((addr_t)address);
	if (region == NULL)
		return B_ERROR;

	*_result = (addr_t)address - region->loaderAddress + region->kernelAddress;
	return B_OK;
}


status_t
platform_kernel_address_to_bootloader_address(addr_t address, void** _result)
{
	MemoryRegion* region = find_region_by_kernel_address(address);
	if (region == NULL)
		return B_ERROR;

	*_result = (void*)(address - region->kernelAddress + region->loaderAddress);
	return B_OK;
}


//	#pragma mark -


void
mmu_init()
{
	sRamEnd = ram_end();
	sPhysicalNext = sRamEnd;

	dprintf("RAM: %" B_PRIu64 " MB at %#x, loader image ends at %p\n",
		(uint64)(sRamEnd - INDY_RAM_BASE) / (1024 * 1024), INDY_RAM_BASE, &_end);

	dprintf("memory controller: MEMCFG0 %#" B_PRIx32 ", MEMCFG1 %#" B_PRIx32
		", SYSID %#" B_PRIx32 "\n", indy_read32(INDY_MC_MEMCFG0),
		indy_read32(INDY_MC_MEMCFG1), indy_read32(INDY_MC_SYSID));

	if (sRamEnd - INDY_RAM_BASE < LOADER_MINIMUM_RAM) {
		panic("%" B_PRIu64 " MB of contiguous RAM found at %#x, at least "
			"32 MB are required",
			(uint64)(sRamEnd - INDY_RAM_BASE) / (1024 * 1024), INDY_RAM_BASE);
	}

	gKernelArgs.num_physical_memory_ranges = 0;
	insert_physical_memory_range(INDY_RAM_BASE, sRamEnd - INDY_RAM_BASE);

	// The page directory and the tables for all of kseg2. The kernel shares
	// these tables between all address spaces, so they exist from the start.
	sInvalidPageTable = allocate_page_table(0);
	sPageDirectory = (uint32*)allocate_physical(B_PAGE_SIZE);
	for (uint32 i = 0; i < MIPS_PAGE_DIRECTORY_ENTRIES; i++)
		sPageDirectory[i] = (uint32)(addr_t)sInvalidPageTable;
	for (uint32 i = mips_page_directory_index(KERNEL_VM_BASE);
			i < MIPS_PAGE_DIRECTORY_ENTRIES; i++) {
		sPageDirectory[i]
			= (uint32)(addr_t)allocate_page_table(MIPS_PTE_KERNEL_EMPTY);
	}

	gKernelArgs.arch_args.page_directory = (uint32)(addr_t)sPageDirectory;
	gKernelArgs.arch_args.invalid_page_table
		= (uint32)(addr_t)sInvalidPageTable;
}


void
mmu_init_for_kernel()
{
	// the kernel's boot stack
	void* stack = NULL;
	size_t stackSize = KERNEL_STACK_SIZE
		+ KERNEL_STACK_GUARD_PAGES * B_PAGE_SIZE;
	if (platform_allocate_region(&stack, stackSize, B_READ_AREA | B_WRITE_AREA)
			!= B_OK) {
		panic("could not allocate the kernel stack");
	}
	gKernelArgs.cpu_kstack[0].start = fix_address((addr_t)stack);
	gKernelArgs.cpu_kstack[0].size = stackSize;

	gKernelArgs.arch_args.virtual_end = sVirtualNext;

	// One virtual range from the kernel image to the end of the kseg2
	// mappings. Early kernel allocations follow the last range, so they land
	// in kseg2.
	gKernelArgs.num_virtual_allocated_ranges = 0;
	insert_virtual_allocated_range(LOADER_KERNEL_BASE,
		sVirtualNext - LOADER_KERNEL_BASE);

	gKernelArgs.num_physical_allocated_ranges = 0;
	insert_physical_allocated_range(INDY_RAM_BASE, INDY_RESERVED_LOW_MEMORY);
	if (sKernelImageEnd != 0) {
		insert_physical_allocated_range(LOADER_KERNEL_BASE - KSEG0_BASE,
			sKernelImageEnd - LOADER_KERNEL_BASE);
	}
	insert_physical_allocated_range(sPhysicalNext, sRamEnd - sPhysicalNext);
	if (sPhysicalNextLow != LOADER_KERNEL_BASE - KSEG0_BASE) {
		insert_physical_allocated_range(sPhysicalNextLow,
			LOADER_KERNEL_BASE - KSEG0_BASE - sPhysicalNextLow);
	}

	dprintf("kernel stack at %#" B_PRIx64 ", kseg2 mapped up to %#"
		B_PRIxADDR ", page directory at %p\n",
		gKernelArgs.cpu_kstack[0].start, sVirtualNext, sPageDirectory);
	dprintf("loader allocations: %" B_PRIu64 " kB at the end of RAM, kernel "
		"image %" B_PRIuADDR " kB\n", (uint64)(sRamEnd - sPhysicalNext
			+ LOADER_KERNEL_BASE - KSEG0_BASE - sPhysicalNextLow) / 1024,
		sKernelImageEnd != 0 ? (sKernelImageEnd - LOADER_KERNEL_BASE) / 1024 : 0);
}


/*!	Checks that a word written through kseg0 can be read through the kseg2
	mapping of the same page. The TLB refill vector must be installed.
*/
void
mmu_test_kernel_mapping()
{
	void* memory = NULL;
	if (platform_allocate_region(&memory, B_PAGE_SIZE,
			B_READ_AREA | B_WRITE_AREA) != B_OK) {
		panic("could not allocate the test page");
	}

	volatile uint32* direct = (volatile uint32*)memory;
	volatile uint32* mapped = (volatile uint32*)fix_address((addr_t)memory);

	direct[1] = 0x4861696b;
	if (mapped[1] != 0x4861696b)
		panic("kseg2 mapping does not work");
	mapped[2] = 0x75496e64;
	if (direct[2] != 0x75496e64)
		panic("kseg2 mapping does not work for writes");

	dprintf("haiku_loader: kseg2 ok\n");
}
