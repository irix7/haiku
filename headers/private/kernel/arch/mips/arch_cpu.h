/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_CPU_H
#define _KERNEL_ARCH_MIPS_CPU_H


#include <arch_cpu_defs.h>


#define CPU_MAX_CACHE_LEVEL	8
#define CACHE_LINE_SIZE		32


#ifndef _ASSEMBLER

#include <SupportDefs.h>


typedef struct arch_cpu_info {
	uint32	prid;
	uint32	config;
	uint32	icache_size;
	uint32	icache_line_size;
	uint32	dcache_size;
	uint32	dcache_line_size;
	uint32	cache_ways;
	uint32	count_frequency;
		// CP0 Count ticks per second, measured at boot
} arch_cpu_info;


#ifdef __cplusplus
extern "C" {
#endif


/*
 * CP0 access. mfc0 and mtc0 move 32 bits and sign-extend, which is what the
 * 32 bit kernel address mode needs. A write is followed by three nops: the
 * CPU takes that long to act on a new Status value.
 */

#define MIPS_CP0_ACCESSORS(name, number) \
	static inline uint32 \
	mips_read_##name(void) \
	{ \
		uint32 value; \
		__asm__ __volatile__("mfc0 %0, $" #number : "=r" (value)); \
		return value; \
	} \
	\
	static inline void \
	mips_write_##name(uint32 value) \
	{ \
		__asm__ __volatile__(".set push\n" \
			".set noreorder\n" \
			"mtc0 %0, $" #number "\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			".set pop" : : "r" (value)); \
	}

MIPS_CP0_ACCESSORS(index, 0)
MIPS_CP0_ACCESSORS(random, 1)
MIPS_CP0_ACCESSORS(entrylo0, 2)
MIPS_CP0_ACCESSORS(entrylo1, 3)
MIPS_CP0_ACCESSORS(pagemask, 5)
MIPS_CP0_ACCESSORS(wired, 6)
MIPS_CP0_ACCESSORS(badvaddr, 8)
MIPS_CP0_ACCESSORS(count, 9)
MIPS_CP0_ACCESSORS(entryhi, 10)
MIPS_CP0_ACCESSORS(compare, 11)
MIPS_CP0_ACCESSORS(status, 12)
MIPS_CP0_ACCESSORS(cause, 13)
MIPS_CP0_ACCESSORS(epc, 14)
MIPS_CP0_ACCESSORS(prid, 15)
MIPS_CP0_ACCESSORS(config, 16)

#undef MIPS_CP0_ACCESSORS


/* The kernel can always read and write mapped user memory. */

static inline void
arch_cpu_enable_user_access()
{
}


static inline void
arch_cpu_disable_user_access()
{
}


static inline void
arch_cpu_pause(void)
{
}


static inline void
arch_cpu_idle(void)
{
	// The R4600 has a "wait" instruction and the R4400 does not. It is not
	// used until it has been tested on the machine (task M9).
}


/*
 * Cache maintenance (arch_cpu.cpp). All of these take kseg0 addresses.
 * A page has the same cache index in kseg0 and at every other virtual
 * address, because the page size is at least the size of a cache way.
 */

void mips_cache_init(void);
void mips_dcache_writeback_invalidate(addr_t kseg0Address, size_t length);
void mips_dcache_invalidate(addr_t kseg0Address, size_t length);
void mips_icache_invalidate(addr_t kseg0Address, size_t length);
void mips_cache_flush_all(void);

/*
 * Cache maintenance around device DMA, by physical address.
 * Call arch_cpu_dma_sync_for_device() before a device reads or writes the
 * memory, and arch_cpu_dma_sync_for_cpu() after a device has written it.
 */
void arch_cpu_dma_sync_for_device(phys_addr_t address, size_t length);
void arch_cpu_dma_sync_for_cpu(phys_addr_t address, size_t length);


/*
 * Reads a device register that may not exist. An access to an address no
 * device answers raises a bus error; this returns B_BAD_ADDRESS then.
 */
status_t arch_cpu_probe_read32(addr_t address, uint32* _value);


/*
 * TLB primitives (arch_tlb.S). All of them run with interrupts disabled and
 * preserve EntryHi.
 */

void mips_tlb_write_indexed(uint32 index, uint32 entryHi, uint32 entryLo0,
	uint32 entryLo1);
void mips_tlb_write_random(uint32 entryHi, uint32 entryLo0, uint32 entryLo1);
	// Rewrites the entry that maps the pair of pages around the address,
	// if the TLB holds one. Returns whether it did.
bool mips_tlb_update(addr_t address, uint32 entryLo0, uint32 entryLo1);
	// Removes the entry that maps the address, if the TLB holds one.
void mips_tlb_invalidate_page(addr_t address);
void mips_tlb_flush_all(void);


extern arch_cpu_info gMipsCpuInfo;


#ifdef __cplusplus
}
#endif

#endif	/* !_ASSEMBLER */


#endif	/* _KERNEL_ARCH_MIPS_CPU_H */
