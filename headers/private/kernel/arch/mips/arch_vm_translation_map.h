/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_VM_TRANSLATION_MAP_H
#define _KERNEL_ARCH_MIPS_VM_TRANSLATION_MAP_H


#include <arch_cpu_defs.h>


/*
 * Page tables
 *
 * Two levels. The page directory has 64 entries, each the kseg0 address of a
 * page table. A page table is one 16 kB page of 4096 entries and maps 64 MB.
 * Empty directory slots point at one shared table of invalid entries, so the
 * TLB refill handler needs no check.
 *
 * A page table entry is 32 bits wide:
 *   bits 31 to 2   the EntryLo value: frame number, cache mode, D, V and G
 *   bit 1          MIPS_PTE_WRITABLE: writes are permitted
 *   bit 0          MIPS_PTE_PRESENT: the entry maps a page
 * The refill handler shifts an entry right by two bits and writes the result
 * to EntryLo.
 *
 * V and D double as the accessed and modified flags. V is set when the page
 * is mapped and cleared to detect the next access. D is set by the first
 * write to a writable page.
 *
 * This header is included by C++ and assembly sources.
 */

#define MIPS_PAGE_DIRECTORY_ENTRIES		64
#define MIPS_PAGE_DIRECTORY_SHIFT		26
#define MIPS_PAGE_TABLE_ENTRIES			4096
#define MIPS_PAGE_TABLE_RANGE			0x04000000

#define MIPS_PTE_PRESENT				0x00000001
#define MIPS_PTE_WRITABLE				0x00000002
#define MIPS_PTE_SOFTWARE_BITS			2
#define MIPS_PTE_GLOBAL			(CP0_ENTRYLO_G << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_VALID			(CP0_ENTRYLO_V << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_DIRTY			(CP0_ENTRYLO_D << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_CACHE_MASK		(CP0_ENTRYLO_C_MASK << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_UNCACHED \
	(CP0_ENTRYLO_C_UNCACHED << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_CACHED \
	(CP0_ENTRYLO_C_CACHED << MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_FRAME_SHIFT \
	(CP0_ENTRYLO_PFN_SHIFT + MIPS_PTE_SOFTWARE_BITS)
#define MIPS_PTE_FRAME_MASK				0xffffff00

// An unmapped entry of a kernel page table. Both entries of a TLB pair must
// have G set for the pair to match in every address space.
#define MIPS_PTE_KERNEL_EMPTY			MIPS_PTE_GLOBAL


#ifndef _ASSEMBLER

#include <arch/vm_translation_map.h>


static inline uint32
mips_pte_from_physical(phys_addr_t address)
{
	// The frame number field counts 4 kB units.
	return (uint32)(address >> 12) << MIPS_PTE_FRAME_SHIFT;
}


static inline phys_addr_t
mips_pte_to_physical(uint32 entry)
{
	return (phys_addr_t)(entry >> MIPS_PTE_FRAME_SHIFT) << 12;
}


static inline uint32
mips_pte_to_entrylo(uint32 entry)
{
	return entry >> MIPS_PTE_SOFTWARE_BITS;
}


static inline uint32
mips_page_directory_index(addr_t address)
{
	return address >> MIPS_PAGE_DIRECTORY_SHIFT;
}


static inline uint32
mips_page_table_index(addr_t address)
{
	return (address >> 14) & (MIPS_PAGE_TABLE_ENTRIES - 1);
}


// Physical memory through kseg0. All RAM of the machine lies below 512 MB.

static inline void*
mips_physical_to_kseg0(phys_addr_t address)
{
	return (void*)(addr_t)(address + KSEG0_BASE);
}


static inline phys_addr_t
mips_kseg0_to_physical(const void* address)
{
	return (addr_t)address - KSEG0_BASE;
}


static inline bool
mips_is_kseg0(addr_t address)
{
	return address >= KSEG0_BASE && address < KSEG1_BASE;
}


static inline bool
mips_is_kseg1(addr_t address)
{
	return address >= KSEG1_BASE && address < KSEG2_BASE;
}

#endif	/* !_ASSEMBLER */


#endif /* _KERNEL_ARCH_MIPS_VM_TRANSLATION_MAP_H */
