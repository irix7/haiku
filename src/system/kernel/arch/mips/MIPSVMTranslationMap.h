/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MIPS_VM_TRANSLATION_MAP_H_
#define _MIPS_VM_TRANSLATION_MAP_H_


#include <arch_vm_translation_map.h>
#include <vm/VMTranslationMap.h>


/*!	Two level page tables, see arch_vm_translation_map.h for the layout.
	All tables are accessed through kseg0. The port supports one CPU, so TLB
	entries are invalidated at once and Flush() has nothing to do.
*/
struct MIPSVMTranslationMap : public VMTranslationMap {
								MIPSVMTranslationMap(bool kernel,
									uint32* pageDirectory);
	virtual						~MIPSVMTranslationMap();

	virtual	bool				Lock();
	virtual	void				Unlock();

	virtual	addr_t				MappedSize() const;
	virtual	size_t				MaxPagesNeededToMap(addr_t start,
									addr_t end) const;

	virtual	status_t			Map(addr_t virtualAddress,
									phys_addr_t physicalAddress,
									uint32 attributes, uint32 memoryType,
									vm_page_reservation* reservation);
	virtual	status_t			Unmap(addr_t start, addr_t end);

	virtual	status_t			UnmapPage(VMArea* area, addr_t address,
									bool updatePageQueue,
									bool deletingAddressSpace = false,
									uint32* _flags = NULL);
	virtual	void				UnmapPages(VMArea* area, addr_t base,
									size_t size, bool updatePageQueue,
									bool deletingAddressSpace = false);

	virtual	status_t			Query(addr_t virtualAddress,
									phys_addr_t* _physicalAddress,
									uint32* _flags);
	virtual	status_t			QueryInterrupt(addr_t virtualAddress,
									phys_addr_t* _physicalAddress,
									uint32* _flags);
	virtual	status_t			Protect(addr_t base, addr_t top,
									uint32 attributes, uint32 memoryType);

	virtual	status_t			ClearFlags(addr_t virtualAddress,
									uint32 flags);

	virtual	bool				ClearAccessedAndModified(
									VMArea* area, addr_t address,
									bool unmapIfUnaccessed,
									bool& _modified);

	virtual	void				Flush();

			// Called by the TLB exception handler. Sets the accessed and
			// modified state of a mapped page and reloads the TLB entry.
			// Returns false if the access is not permitted.
			bool				HandleFault(addr_t address, bool isWrite);

			// The directory the TLB refill handler uses for this map.
			uint32*				PageDirectory() const;

	static	void				Init(uint32* kernelDirectory,
									uint32* invalidTable,
									phys_addr_t ramEnd, addr_t kernelImageEnd);

private:
			uint32*				_LookupEntry(addr_t address, bool allocate,
									vm_page_reservation* reservation);
			void				_Invalidate(addr_t address);
	static	uint32*				_AllocateTablePage(
									vm_page_reservation* reservation);
	static	void				_FreeTablePage(uint32* table);

private:
			bool				fIsKernel;
			uint32*				fPageDirectory;

	static	uint32*				sKernelDirectory;
	static	uint32*				sInvalidTable;
	static	phys_addr_t			sRamEnd;
	static	addr_t				sKernelImageEnd;
};


struct MIPSVMPhysicalPageMapper : public VMPhysicalPageMapper {
	virtual	status_t			GetPage(phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPage(addr_t virtualAddress,
									void* handle);
	virtual	status_t			GetPageCurrentCPU(
									phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPageCurrentCPU(addr_t virtualAddress,
									void* _handle);
	virtual	status_t			GetPageDebug(phys_addr_t physicalAddress,
									addr_t* _virtualAddress,
									void** _handle);
	virtual	status_t			PutPageDebug(addr_t virtualAddress,
									void* handle);
	virtual	status_t			MemsetPhysical(phys_addr_t address, int value,
									phys_size_t length);
	virtual	status_t			MemcpyFromPhysical(void* to, phys_addr_t from,
									size_t length, bool user);
	virtual	status_t			MemcpyToPhysical(phys_addr_t to,
									const void* from, size_t length,
									bool user);
	virtual	void				MemcpyPhysicalPage(phys_addr_t to,
									phys_addr_t from);
};


#endif	// _MIPS_VM_TRANSLATION_MAP_H_
