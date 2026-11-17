/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/user_memory.h>
#include <arch_exception_block.h>
#include <arch_indy.h>
#include <arch_vm_translation_map.h>
#include <boot/kernel_args.h>
#include <cpu.h>
#include <interrupts.h>
#include <thread.h>
#include <util/AutoLock.h>
#include <vm/VMAddressSpace.h>
#include <vm/VMTranslationMap.h>


arch_cpu_info gMipsCpuInfo;


extern "C" void mips_cache_flush_all_kseg1(uint32 dcacheSize,
	uint32 dcacheLine, uint32 icacheSize, uint32 icacheLine);


/*
 * Cache loops
 *
 * R4600 revision 1.x gives wrong results for a hit type data cache operation
 * unless the four instructions before it are free of loads and stores.
 * Revision 2.0 needs an empty data cache refill buffer, which one load from
 * kseg1 produces. So every loop runs with interrupts off, starts with a load
 * from kseg1 and four nops, and has no load or store inside. The same code
 * runs on every CPU. The addresses are in kseg0, so no TLB refill handler
 * runs inside the loop.
 */
#define DEFINE_CACHE_LOOP(name, operation) \
	static void \
	name(addr_t start, addr_t end, uint32 lineSize) \
	{ \
		InterruptsLocker locker; \
		__asm__ __volatile__( \
			".set push\n" \
			".set noreorder\n" \
			".set noat\n" \
			"lui	$1, 0xa000\n" \
			"lw	$1, 0($1)\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"nop\n" \
			"1:	cache " #operation ", 0(%0)\n" \
			"addu	%0, %0, %2\n" \
			"sltu	$1, %0, %1\n" \
			"bnez	$1, 1b\n" \
			" nop\n" \
			"sync\n" \
			".set pop\n" \
			: "+r" (start) : "r" (end), "r" (lineSize) : "$1", "memory"); \
	}

DEFINE_CACHE_LOOP(cache_loop_writeback_invalidate_d, 0x15)
DEFINE_CACHE_LOOP(cache_loop_invalidate_d, 0x11)
DEFINE_CACHE_LOOP(cache_loop_invalidate_i, 0x10)


void
mips_cache_init(void)
{
	uint32 config = mips_read_config();
	gMipsCpuInfo.prid = mips_read_prid();
	gMipsCpuInfo.config = config;
	gMipsCpuInfo.icache_size = 0x1000
		<< ((config >> CP0_CONFIG_IC_SHIFT) & CP0_CONFIG_CACHE_MASK);
	gMipsCpuInfo.dcache_size = 0x1000
		<< ((config >> CP0_CONFIG_DC_SHIFT) & CP0_CONFIG_CACHE_MASK);
	gMipsCpuInfo.icache_line_size = (config & CP0_CONFIG_IB) != 0 ? 32 : 16;
	gMipsCpuInfo.dcache_line_size = (config & CP0_CONFIG_DB) != 0 ? 32 : 16;

	// The R4000 and R4400 caches are direct mapped. The R4600, R4700 and
	// R5000 caches have two ways.
	uint32 implementation = gMipsCpuInfo.prid & CP0_PRID_IMP_MASK;
	gMipsCpuInfo.cache_ways = implementation == CP0_PRID_IMP_R4X00 ? 1 : 2;
}


void
mips_dcache_writeback_invalidate(addr_t address, size_t length)
{
	if (length == 0)
		return;
	uint32 line = gMipsCpuInfo.dcache_line_size;
	cache_loop_writeback_invalidate_d(address & ~(addr_t)(line - 1),
		address + length, line);
}


void
mips_dcache_invalidate(addr_t address, size_t length)
{
	if (length == 0)
		return;
	uint32 line = gMipsCpuInfo.dcache_line_size;
	cache_loop_invalidate_d(address & ~(addr_t)(line - 1), address + length,
		line);
}


void
mips_icache_invalidate(addr_t address, size_t length)
{
	if (length == 0)
		return;
	uint32 line = gMipsCpuInfo.icache_line_size;
	cache_loop_invalidate_i(address & ~(addr_t)(line - 1), address + length,
		line);
}


void
mips_cache_flush_all(void)
{
	typedef void (*FlushFunction)(uint32, uint32, uint32, uint32);
	FlushFunction flush = (FlushFunction)((addr_t)&mips_cache_flush_all_kseg1
		- KSEG0_BASE + KSEG1_BASE);

	InterruptsLocker locker;
	flush(gMipsCpuInfo.dcache_size, gMipsCpuInfo.dcache_line_size,
		gMipsCpuInfo.icache_size, gMipsCpuInfo.icache_line_size);
}


void
arch_cpu_dma_sync_for_device(phys_addr_t address, size_t length)
{
	mips_dcache_writeback_invalidate((addr_t)mips_physical_to_kseg0(address),
		length);
}


void
arch_cpu_dma_sync_for_cpu(phys_addr_t address, size_t length)
{
	mips_dcache_invalidate((addr_t)mips_physical_to_kseg0(address), length);
}


status_t
arch_cpu_probe_read32(addr_t address, uint32* _value)
{
	uint32 value = 0;
	bool ok = user_access([&] { value = *(volatile uint32*)address; });
	if (!ok)
		return B_BAD_ADDRESS;

	*_value = value;
	return B_OK;
}


//	#pragma mark -


status_t
arch_cpu_preboot_init_percpu(kernel_args* args, int curr_cpu)
{
	mips_cache_init();
	return B_OK;
}


status_t
arch_cpu_init_percpu(kernel_args* args, int curr_cpu)
{
	return B_OK;
}


status_t
arch_cpu_init(kernel_args* args)
{
	const char* name = "unknown";
	switch (gMipsCpuInfo.prid & CP0_PRID_IMP_MASK) {
		case CP0_PRID_IMP_R4X00:
			name = "R4000/R4400";
			break;
		case CP0_PRID_IMP_R4600:
			name = "R4600";
			break;
		case CP0_PRID_IMP_R4700:
			name = "R4700";
			break;
		case CP0_PRID_IMP_R5000:
			name = "R5000";
			break;
	}

	dprintf("CPU: %s, PRId %#" B_PRIx32 ", Config %#" B_PRIx32 "\n", name,
		gMipsCpuInfo.prid, gMipsCpuInfo.config);
	dprintf("CPU: I-cache %" B_PRIu32 " kB (line %" B_PRIu32 "), D-cache %"
		B_PRIu32 " kB (line %" B_PRIu32 "), %" B_PRIu32 " way(s)\n",
		gMipsCpuInfo.icache_size / 1024, gMipsCpuInfo.icache_line_size,
		gMipsCpuInfo.dcache_size / 1024, gMipsCpuInfo.dcache_line_size,
		gMipsCpuInfo.cache_ways);

	// A page must cover a whole cache way, see mips_cache_init().
	if (gMipsCpuInfo.dcache_size / gMipsCpuInfo.cache_ways > B_PAGE_SIZE
		|| gMipsCpuInfo.icache_size / gMipsCpuInfo.cache_ways > B_PAGE_SIZE) {
		panic("cache way larger than a page: cache aliases are not handled");
	}

	return B_OK;
}


status_t
arch_cpu_init_post_vm(kernel_args* args)
{
	return B_OK;
}


status_t
arch_cpu_init_post_modules(kernel_args* args)
{
	return B_OK;
}


/*!	Makes the instruction cache see what was written to the range. Works on
	the kseg0 address of each page, which shares its cache lines with every
	other mapping of the page.
*/
void
arch_cpu_sync_icache(void* _address, size_t length)
{
	addr_t address = (addr_t)_address;
	addr_t end = address + length;

	while (address < end) {
		addr_t pageEnd = ROUNDDOWN(address, B_PAGE_SIZE) + B_PAGE_SIZE;
		size_t chunk = (pageEnd > end || pageEnd < address ? end : pageEnd)
			- address;

		addr_t direct = 0;
		if (mips_is_kseg0(address)) {
			direct = address;
		} else if (mips_is_kseg1(address)) {
			direct = address - KSEG1_BASE + KSEG0_BASE;
		} else {
			VMAddressSpace* addressSpace = IS_KERNEL_ADDRESS(address)
				? VMAddressSpace::Kernel() : VMAddressSpace::GetCurrent();
			phys_addr_t physical;
			uint32 flags;
			if (addressSpace != NULL
				&& addressSpace->TranslationMap()->QueryInterrupt(address,
					&physical, &flags) == B_OK
				&& (flags & PAGE_PRESENT) != 0) {
				direct = (addr_t)mips_physical_to_kseg0(physical)
					+ address % B_PAGE_SIZE;
			}
			if (!IS_KERNEL_ADDRESS(address) && addressSpace != NULL)
				addressSpace->Put();
		}

		if (direct != 0) {
			mips_dcache_writeback_invalidate(direct, chunk);
			mips_icache_invalidate(direct, chunk);
		}

		address += chunk;
	}
}


void
arch_cpu_invalidate_tlb_range(intptr_t context, addr_t start, addr_t end)
{
	int32 pages = (end - start) / B_PAGE_SIZE + 1;
	if (pages > MIPS_TLB_ENTRIES) {
		mips_tlb_flush_all();
		return;
	}

	start = ROUNDDOWN(start, B_PAGE_SIZE);
	for (int32 i = 0; i < pages; i++)
		mips_tlb_invalidate_page(start + (addr_t)i * B_PAGE_SIZE);
}


void
arch_cpu_invalidate_tlb_list(intptr_t context, addr_t pages[], int num_pages)
{
	for (int i = 0; i < num_pages; i++)
		mips_tlb_invalidate_page(pages[i]);
}


void
arch_cpu_global_tlb_invalidate(void)
{
	mips_tlb_flush_all();
}


void
arch_cpu_user_tlb_invalidate(intptr_t context)
{
	mips_tlb_flush_all();
}


status_t
arch_cpu_shutdown(bool reboot)
{
	// Both end in the PROM: it shows its menu, from where the machine can
	// be powered off.
	disable_interrupts();
	mips_write_status(CP0_STATUS_CU0 | CP0_STATUS_BEV);
	((void (*)(void))MIPS_KSEG1(INDY_PROM_BASE))();
	return B_ERROR;
}
