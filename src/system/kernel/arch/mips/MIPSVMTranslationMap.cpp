/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "MIPSVMTranslationMap.h"

#include <string.h>

#include <arch/cpu.h>
#include <arch_indy.h>
#include <arch_kernel.h>
#include <kernel.h>
#include <slab/Slab.h>
#include <thread.h>
#include <util/AutoLock.h>
#include <vm/vm.h>
#include <vm/vm_page.h>
#include <vm/vm_priv.h>
#include <vm/VMAddressSpace.h>
#include <vm/VMCache.h>


//#define TRACE_TRANSLATION_MAP
#ifdef TRACE_TRANSLATION_MAP
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


uint32* MIPSVMTranslationMap::sKernelDirectory;
uint32* MIPSVMTranslationMap::sInvalidTable;
phys_addr_t MIPSVMTranslationMap::sRamEnd;
addr_t MIPSVMTranslationMap::sKernelImageEnd;

static const uint32 kFirstKernelSlot = KERNEL_BASE >> MIPS_PAGE_DIRECTORY_SHIFT;


static inline uint32
flags_from_entry(uint32 entry, bool kernel)
{
	if ((entry & MIPS_PTE_PRESENT) == 0)
		return 0;

	uint32 flags = PAGE_PRESENT;
	if ((entry & MIPS_PTE_VALID) != 0)
		flags |= PAGE_ACCESSED;
	if ((entry & MIPS_PTE_DIRTY) != 0)
		flags |= PAGE_MODIFIED;

	// The TLB has no execute or user bit. Every page that can be read can
	// be executed, and every page below KERNEL_BASE is a user page.
	if (kernel) {
		flags |= B_KERNEL_READ_AREA | B_KERNEL_EXECUTE_AREA;
		if ((entry & MIPS_PTE_WRITABLE) != 0)
			flags |= B_KERNEL_WRITE_AREA;
	} else {
		flags |= B_READ_AREA | B_EXECUTE_AREA | B_KERNEL_READ_AREA
			| B_KERNEL_EXECUTE_AREA;
		if ((entry & MIPS_PTE_WRITABLE) != 0)
			flags |= B_WRITE_AREA | B_KERNEL_WRITE_AREA;
	}
	return flags;
}


static inline bool
attributes_writable(uint32 attributes)
{
	if ((attributes & B_USER_PROTECTION) != 0)
		return (attributes & B_WRITE_AREA) != 0;
	return (attributes & B_KERNEL_WRITE_AREA) != 0;
}


//	#pragma mark - MIPSVMTranslationMap


/*static*/ void
MIPSVMTranslationMap::Init(uint32* kernelDirectory, uint32* invalidTable,
	phys_addr_t ramEnd, addr_t kernelImageEnd)
{
	sKernelDirectory = kernelDirectory;
	sInvalidTable = invalidTable;
	sRamEnd = ramEnd;
	sKernelImageEnd = kernelImageEnd;
}


MIPSVMTranslationMap::MIPSVMTranslationMap(bool kernel, uint32* pageDirectory)
	:
	fIsKernel(kernel),
	fPageDirectory(pageDirectory)
{
}


MIPSVMTranslationMap::~MIPSVMTranslationMap()
{
	ASSERT(!fIsKernel);

	if (fPageDirectory == NULL)
		return;

	ASSERT((addr_t)fPageDirectory != gMipsExceptionBlock->page_directory);

	for (uint32 i = 0; i < kFirstKernelSlot; i++) {
		uint32* table = (uint32*)(addr_t)fPageDirectory[i];
		if (table != sInvalidTable)
			_FreeTablePage(table);
	}
	_FreeTablePage(fPageDirectory);
}


uint32*
MIPSVMTranslationMap::PageDirectory() const
{
	return fPageDirectory != NULL ? fPageDirectory : sKernelDirectory;
}


/*static*/ uint32*
MIPSVMTranslationMap::_AllocateTablePage(vm_page_reservation* reservation)
{
	vm_page* page = vm_page_allocate_page(reservation,
		PAGE_STATE_WIRED | VM_PAGE_ALLOC_CLEAR);
	if (page == NULL)
		return NULL;
	DEBUG_PAGE_ACCESS_END(page);

	return (uint32*)mips_physical_to_kseg0(
		(phys_addr_t)page->physical_page_number * B_PAGE_SIZE);
}


/*static*/ void
MIPSVMTranslationMap::_FreeTablePage(uint32* table)
{
	vm_page* page = vm_lookup_page(mips_kseg0_to_physical(table)
		/ B_PAGE_SIZE);
	if (page == NULL) {
		panic("page table %p has no vm_page", table);
		return;
	}
	DEBUG_PAGE_ACCESS_START(page);
	vm_page_free(NULL, page);
}


/*!	Returns the page table entry of the address, or NULL if there is no page
	table for it and \a allocate is false.
*/
uint32*
MIPSVMTranslationMap::_LookupEntry(addr_t address, bool allocate,
	vm_page_reservation* reservation)
{
	if (fPageDirectory == NULL) {
		if (!allocate)
			return NULL;

		// The directory of a user map: the user slots are empty, the kernel
		// slots are those of the kernel map. The kernel's page tables all
		// exist from boot on, so the kernel slots never change.
		uint32* directory = _AllocateTablePage(reservation);
		if (directory == NULL)
			return NULL;
		for (uint32 i = 0; i < kFirstKernelSlot; i++)
			directory[i] = (uint32)(addr_t)sInvalidTable;
		for (uint32 i = kFirstKernelSlot; i < MIPS_PAGE_DIRECTORY_ENTRIES; i++)
			directory[i] = sKernelDirectory[i];
		fPageDirectory = directory;

		// The map may belong to the running team, which so far used the
		// kernel's directory.
		Thread* thread = thread_get_current_thread();
		if (thread != NULL && thread->team != NULL
			&& thread->team->address_space != NULL
			&& thread->team->address_space->TranslationMap() == this) {
			gMipsExceptionBlock->page_directory = (uint32)(addr_t)directory;
		}
	}

	uint32 slot = mips_page_directory_index(address);
	uint32* table = (uint32*)(addr_t)fPageDirectory[slot];
	if (table == sInvalidTable) {
		if (!allocate || slot >= kFirstKernelSlot)
			return NULL;

		table = _AllocateTablePage(reservation);
		if (table == NULL)
			return NULL;
		fPageDirectory[slot] = (uint32)(addr_t)table;
	}

	return &table[mips_page_table_index(address)];
}


void
MIPSVMTranslationMap::_Invalidate(addr_t address)
{
	mips_tlb_invalidate_page(address);
}


bool
MIPSVMTranslationMap::Lock()
{
	recursive_lock_lock(&fLock);
	return true;
}


void
MIPSVMTranslationMap::Unlock()
{
	recursive_lock_unlock(&fLock);
}


addr_t
MIPSVMTranslationMap::MappedSize() const
{
	return (addr_t)fMapCount * B_PAGE_SIZE;
}


size_t
MIPSVMTranslationMap::MaxPagesNeededToMap(addr_t start, addr_t end) const
{
	// If start == 0, the actual base address is not yet known to the caller
	// and we shall assume the worst case.
	if (start == 0) {
		start = MIPS_PAGE_TABLE_RANGE - B_PAGE_SIZE;
		end += start;
	}

	// the page tables, and the directory
	return end / MIPS_PAGE_TABLE_RANGE + 1 - start / MIPS_PAGE_TABLE_RANGE
		+ 1;
}


status_t
MIPSVMTranslationMap::Map(addr_t virtualAddress, phys_addr_t physicalAddress,
	uint32 attributes, uint32 memoryType, vm_page_reservation* reservation)
{
	TRACE("Map(%#" B_PRIxADDR ", %#" B_PRIxPHYSADDR ")\n", virtualAddress,
		physicalAddress);

	if (mips_is_kseg0(virtualAddress) || mips_is_kseg1(virtualAddress))
		return B_OK;

	uint32* entry = _LookupEntry(virtualAddress, true, reservation);
	if (entry == NULL) {
		panic("no page table for %#" B_PRIxADDR, virtualAddress);
		return B_NO_MEMORY;
	}

	// Device registers are mapped uncached, RAM is mapped cached.
	bool isRam = physicalAddress >= INDY_RAM_BASE && physicalAddress < sRamEnd;

	// V is set at once: a page is mapped because it is about to be used.
	// D is set by the first write (HandleFault()).
	uint32 newEntry = mips_pte_from_physical(physicalAddress)
		| MIPS_PTE_PRESENT | MIPS_PTE_VALID
		| (isRam ? MIPS_PTE_CACHED : MIPS_PTE_UNCACHED);
	if (attributes_writable(attributes))
		newEntry |= MIPS_PTE_WRITABLE;
	if (fIsKernel)
		newEntry |= MIPS_PTE_GLOBAL;

	// The page may have been filled through kseg0 and may be executed in
	// this mapping. The instruction cache reads memory, so the data cache
	// lines of the page are written back first.
	// TODO: do this once per page, when its content changes.
	if (isRam && (attributes & (B_EXECUTE_AREA | B_KERNEL_EXECUTE_AREA)) != 0) {
		addr_t direct = (addr_t)mips_physical_to_kseg0(physicalAddress);
		mips_dcache_writeback_invalidate(direct, B_PAGE_SIZE);
		mips_icache_invalidate(direct, B_PAGE_SIZE);
	}

	InterruptsLocker locker;
	bool wasPresent = (*entry & MIPS_PTE_PRESENT) != 0;
	*entry = newEntry;
	_Invalidate(virtualAddress);
	locker.Unlock();

	if (!wasPresent)
		fMapCount++;

	return B_OK;
}


status_t
MIPSVMTranslationMap::Unmap(addr_t start, addr_t end)
{
	TRACE("Unmap(%#" B_PRIxADDR ", %#" B_PRIxADDR ")\n", start, end);

	start = ROUNDDOWN(start, B_PAGE_SIZE);

	for (addr_t page = start; page < end; page += B_PAGE_SIZE) {
		if (mips_is_kseg0(page) || mips_is_kseg1(page))
			continue;

		uint32* entry = _LookupEntry(page, false, NULL);
		if (entry == NULL)
			continue;

		InterruptsLocker locker;
		uint32 oldEntry = *entry;
		*entry = fIsKernel ? MIPS_PTE_KERNEL_EMPTY : 0;
		_Invalidate(page);
		locker.Unlock();

		if ((oldEntry & MIPS_PTE_PRESENT) != 0)
			fMapCount--;

		if (page + B_PAGE_SIZE < page)
			break;
	}

	return B_OK;
}


status_t
MIPSVMTranslationMap::UnmapPage(VMArea* area, addr_t address,
	bool updatePageQueue, bool deletingAddressSpace, uint32* _flags)
{
	ASSERT(address % B_PAGE_SIZE == 0);
	ASSERT(_flags == NULL || !updatePageQueue);

	uint32* entry = _LookupEntry(address, false, NULL);
	if (entry == NULL || (*entry & MIPS_PTE_PRESENT) == 0)
		return B_ENTRY_NOT_FOUND;

	RecursiveLocker locker(fLock);

	InterruptsLocker interruptsLocker;
	uint32 oldEntry = *entry;
	*entry = fIsKernel ? MIPS_PTE_KERNEL_EMPTY : 0;
	_Invalidate(address);
	interruptsLocker.Unlock();

	if ((oldEntry & MIPS_PTE_PRESENT) == 0)
		return B_ENTRY_NOT_FOUND;

	fMapCount--;

	bool accessed = (oldEntry & MIPS_PTE_VALID) != 0;
	bool modified = (oldEntry & MIPS_PTE_DIRTY) != 0;

	if (_flags == NULL) {
		locker.Detach();
			// PageUnmapped() will unlock for us

		PageUnmapped(area, mips_pte_to_physical(oldEntry) / B_PAGE_SIZE,
			accessed, modified, updatePageQueue);
	} else {
		uint32 flags = PAGE_PRESENT;
		if (accessed)
			flags |= PAGE_ACCESSED;
		if (modified)
			flags |= PAGE_MODIFIED;
		*_flags = flags;
	}

	return B_OK;
}


void
MIPSVMTranslationMap::UnmapPages(VMArea* area, addr_t base, size_t size,
	bool updatePageQueue, bool deletingAddressSpace)
{
	if (size == 0)
		return;

	addr_t end = base + size - 1;

	VMAreaMappings queue;
	RecursiveLocker locker(fLock);

	for (addr_t address = base; address < end; address += B_PAGE_SIZE) {
		uint32* entry = _LookupEntry(address, false, NULL);
		if (entry == NULL)
			continue;

		InterruptsLocker interruptsLocker;
		uint32 oldEntry = *entry;
		*entry = fIsKernel ? MIPS_PTE_KERNEL_EMPTY : 0;
		_Invalidate(address);
		interruptsLocker.Unlock();

		if ((oldEntry & MIPS_PTE_PRESENT) == 0)
			continue;

		fMapCount--;

		if (area->cache_type != CACHE_TYPE_DEVICE) {
			PageUnmapped(area, mips_pte_to_physical(oldEntry) / B_PAGE_SIZE,
				(oldEntry & MIPS_PTE_VALID) != 0,
				(oldEntry & MIPS_PTE_DIRTY) != 0, updatePageQueue, &queue);
		}
	}

	// TODO: As in UnmapPage() we can lose page dirty flags here. ATM it's not
	// really critical here, as in all cases this method is used, the unmapped
	// area range is unmapped for good (resized/cut) and the pages will likely
	// be freed.

	locker.Unlock();

	// free removed mappings
	bool isKernelSpace = area->address_space == VMAddressSpace::Kernel();
	uint32 freeFlags = CACHE_DONT_WAIT_FOR_MEMORY
		| (isKernelSpace ? CACHE_DONT_LOCK_KERNEL_SPACE : 0);

	while (vm_page_mapping* mapping = queue.RemoveHead()) {
		vm_free_page_mapping(mapping->page->physical_page_number, mapping,
			freeFlags);
	}
}


status_t
MIPSVMTranslationMap::Query(addr_t virtualAddress,
	phys_addr_t* _physicalAddress, uint32* _flags)
{
	*_flags = 0;
	*_physicalAddress = 0;

	if (mips_is_kseg0(virtualAddress)) {
		// Only the kernel image counts as mapped here. kseg0 reaches all
		// RAM, but the rest of it belongs to other mappings.
		if (fIsKernel && virtualAddress >= KERNEL_LOAD_BASE
			&& virtualAddress < sKernelImageEnd) {
			*_physicalAddress = virtualAddress - KSEG0_BASE;
			*_flags = PAGE_PRESENT | PAGE_ACCESSED | PAGE_MODIFIED
				| B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA
				| B_KERNEL_EXECUTE_AREA;
		}
		return B_OK;
	}
	if (mips_is_kseg1(virtualAddress))
		return B_OK;

	uint32* entry = _LookupEntry(virtualAddress, false, NULL);
	if (entry == NULL)
		return B_OK;

	uint32 value = *entry;
	if ((value & MIPS_PTE_PRESENT) == 0)
		return B_OK;

	*_physicalAddress = mips_pte_to_physical(value);
	*_flags = flags_from_entry(value, fIsKernel);
	return B_OK;
}


status_t
MIPSVMTranslationMap::QueryInterrupt(addr_t virtualAddress,
	phys_addr_t* _physicalAddress, uint32* _flags)
{
	return Query(virtualAddress, _physicalAddress, _flags);
}


status_t
MIPSVMTranslationMap::Protect(addr_t base, addr_t top, uint32 attributes,
	uint32 memoryType)
{
	base = ROUNDDOWN(base, B_PAGE_SIZE);
	bool writable = attributes_writable(attributes);

	for (addr_t page = base; page < top; page += B_PAGE_SIZE) {
		if (mips_is_kseg0(page) || mips_is_kseg1(page))
			continue;

		uint32* entry = _LookupEntry(page, false, NULL);
		if (entry != NULL) {
			InterruptsLocker locker;
			uint32 value = *entry;
			if ((value & MIPS_PTE_PRESENT) != 0) {
				if (writable) {
					value |= MIPS_PTE_WRITABLE;
				} else {
					// The refill handler copies D to the TLB, so a page
					// without write permission must not have D set. Move
					// the modified state to the vm_page.
					if ((value & MIPS_PTE_DIRTY) != 0) {
						vm_page* vmPage = vm_lookup_page(
							mips_pte_to_physical(value) / B_PAGE_SIZE);
						if (vmPage != NULL)
							vmPage->modified = true;
					}
					value &= ~(MIPS_PTE_WRITABLE | MIPS_PTE_DIRTY);
				}
				*entry = value;
				_Invalidate(page);
			}
		}

		if (page + B_PAGE_SIZE < page)
			break;
	}

	return B_OK;
}


status_t
MIPSVMTranslationMap::ClearFlags(addr_t address, uint32 flags)
{
	uint32* entry = _LookupEntry(address, false, NULL);
	if (entry == NULL)
		return B_OK;

	InterruptsLocker locker;
	uint32 value = *entry;
	if ((value & MIPS_PTE_PRESENT) == 0)
		return B_OK;

	if ((flags & PAGE_ACCESSED) != 0)
		value &= ~MIPS_PTE_VALID;
	if ((flags & PAGE_MODIFIED) != 0)
		value &= ~MIPS_PTE_DIRTY;
	*entry = value;
	_Invalidate(address);

	return B_OK;
}


bool
MIPSVMTranslationMap::ClearAccessedAndModified(VMArea* area, addr_t address,
	bool unmapIfUnaccessed, bool& _modified)
{
	RecursiveLocker locker(fLock);

	uint32* entry = _LookupEntry(address, false, NULL);
	if (entry == NULL)
		return false;

	InterruptsLocker interruptsLocker;
	uint32 oldEntry = *entry;
	if ((oldEntry & MIPS_PTE_PRESENT) == 0)
		return false;

	bool accessed = (oldEntry & MIPS_PTE_VALID) != 0;
	_modified = (oldEntry & MIPS_PTE_DIRTY) != 0;

	if (accessed || !unmapIfUnaccessed) {
		*entry = oldEntry & ~(MIPS_PTE_VALID | MIPS_PTE_DIRTY);
		_Invalidate(address);
		return accessed;
	}

	// The page was not accessed: unmap it.
	*entry = fIsKernel ? MIPS_PTE_KERNEL_EMPTY : 0;
	_Invalidate(address);
	interruptsLocker.Unlock();

	fMapCount--;

	locker.Detach();
		// UnaccessedPageUnmapped() will unlock for us

	UnaccessedPageUnmapped(area, mips_pte_to_physical(oldEntry) / B_PAGE_SIZE);
	return false;
}


void
MIPSVMTranslationMap::Flush()
{
}


bool
MIPSVMTranslationMap::HandleFault(addr_t address, bool isWrite)
{
	if (mips_is_kseg0(address) || mips_is_kseg1(address))
		return false;

	uint32* entry = _LookupEntry(address, false, NULL);
	if (entry == NULL)
		return false;

	InterruptsLocker locker;
	uint32 value = *entry;
	if ((value & MIPS_PTE_PRESENT) == 0)
		return false;
	if (isWrite && (value & MIPS_PTE_WRITABLE) == 0)
		return false;

	value |= MIPS_PTE_VALID;
	if (isWrite)
		value |= MIPS_PTE_DIRTY;
	*entry = value;

	// Reload the pair of pages the TLB entry covers.
	uint32* pair = (uint32*)((addr_t)entry & ~(addr_t)7);
	mips_tlb_update(address, mips_pte_to_entrylo(pair[0]),
		mips_pte_to_entrylo(pair[1]));
	return true;
}


//	#pragma mark - MIPSVMPhysicalPageMapper


/*	kseg0 gives cached access to all RAM without a TLB entry, and a page has
	the same cache index there as in every other mapping. Physical pages
	need no temporary mappings.
*/


status_t
MIPSVMPhysicalPageMapper::GetPage(phys_addr_t physicalAddress,
	addr_t* _virtualAddress, void** _handle)
{
	*_virtualAddress = (addr_t)mips_physical_to_kseg0(physicalAddress);
	*_handle = NULL;
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::PutPage(addr_t virtualAddress, void* handle)
{
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::GetPageCurrentCPU(phys_addr_t physicalAddress,
	addr_t* _virtualAddress, void** _handle)
{
	return GetPage(physicalAddress, _virtualAddress, _handle);
}


status_t
MIPSVMPhysicalPageMapper::PutPageCurrentCPU(addr_t virtualAddress,
	void* _handle)
{
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::GetPageDebug(phys_addr_t physicalAddress,
	addr_t* _virtualAddress, void** _handle)
{
	return GetPage(physicalAddress, _virtualAddress, _handle);
}


status_t
MIPSVMPhysicalPageMapper::PutPageDebug(addr_t virtualAddress, void* handle)
{
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::MemsetPhysical(phys_addr_t address, int value,
	phys_size_t length)
{
	memset(mips_physical_to_kseg0(address), value, length);
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::MemcpyFromPhysical(void* to, phys_addr_t from,
	size_t length, bool user)
{
	if (user)
		return user_memcpy(to, mips_physical_to_kseg0(from), length);

	memcpy(to, mips_physical_to_kseg0(from), length);
	return B_OK;
}


status_t
MIPSVMPhysicalPageMapper::MemcpyToPhysical(phys_addr_t to, const void* from,
	size_t length, bool user)
{
	if (user)
		return user_memcpy(mips_physical_to_kseg0(to), from, length);

	memcpy(mips_physical_to_kseg0(to), from, length);
	return B_OK;
}


void
MIPSVMPhysicalPageMapper::MemcpyPhysicalPage(phys_addr_t to, phys_addr_t from)
{
	memcpy(mips_physical_to_kseg0(to), mips_physical_to_kseg0(from),
		B_PAGE_SIZE);
}
