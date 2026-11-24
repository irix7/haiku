/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch_exception_block.h>
#include <arch_indy.h>
#include <boot/heap.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <kernel.h>

#include "console.h"
#include "mmu.h"


// The archive that build/scripts/elf2ecoff.py appended to the image: a
// 4096 byte header at the first 4096 byte boundary after _end, then the data.
#define ARCHIVE_MAGIC		"HAIKUARC"
#define ARCHIVE_ALIGNMENT	4096

// defined by the linker script
extern void (*__ctor_list)(void);
extern void (*__ctor_end)(void);
extern uint8 __bss_start;
extern uint8 _end;

extern "C" int main(stage2_args* args);
extern "C" void arcs_start(uint32 argc, uint32 argv, uint32 envp);
extern "C" void cpu_init();
extern "C" void arcs_tlb_init(void);
extern "C" void arcs_flush_caches(uint32 dcacheSize, uint32 dcacheLine,
	uint32 icacheSize, uint32 icacheLine);
extern "C" void arcs_enter_kernel(kernel_args* args, addr_t entry,
	addr_t stackTop) __attribute__((noreturn));
extern "C" uint32 arcs_tlb_refill[];
extern "C" uint32 arcs_tlb_refill_end[];
void debug_cleanup(void);


static uint32 sBootOptions;
static bool sBootOptionsValid = false;


static void
call_ctors(void)
{
	void (**f)(void);

	for (f = &__ctor_list; f < &__ctor_end; f++)
		(**f)();
}


static void
flush_caches()
{
	uint32 config = mips_read_config();
	uint32 dcacheSize = 0x1000
		<< ((config >> CP0_CONFIG_DC_SHIFT) & CP0_CONFIG_CACHE_MASK);
	uint32 icacheSize = 0x1000
		<< ((config >> CP0_CONFIG_IC_SHIFT) & CP0_CONFIG_CACHE_MASK);
	uint32 dcacheLine = (config & CP0_CONFIG_DB) != 0 ? 32 : 16;
	uint32 icacheLine = (config & CP0_CONFIG_IB) != 0 ? 32 : 16;

	// Run the routine from kseg1 (see arcs_flush_caches).
	typedef void (*FlushFunction)(uint32, uint32, uint32, uint32);
	FlushFunction flush = (FlushFunction)((addr_t)&arcs_flush_caches
		- KSEG0_BASE + KSEG1_BASE);
	flush(dcacheSize, dcacheLine, icacheSize, icacheLine);
}


/*!	Installs the TLB refill handler and takes the exception vectors away from
	the PROM. A general exception has no handler in the loader: it lands in
	an endless loop at the general vector.
*/
static void
install_vectors()
{
	size_t size = (addr_t)arcs_tlb_refill_end - (addr_t)arcs_tlb_refill;
	if (size > MIPS_EXCEPTION_VECTOR_SIZE)
		panic("TLB refill handler too large");

	memset((void*)MIPS_EXCEPTION_BLOCK_BASE, 0,
		sizeof(mips_exception_block));
	gMipsExceptionBlock->page_directory
		= gKernelArgs.arch_args.page_directory;

	memcpy((void*)MIPS_EXCEPTION_VECTOR_TLB_REFILL, arcs_tlb_refill, size);
	memcpy((void*)MIPS_EXCEPTION_VECTOR_XTLB_REFILL, arcs_tlb_refill, size);

	// "b ." and a nop at the general vector and at the cache error vector
	uint32* vector = (uint32*)MIPS_EXCEPTION_VECTOR_GENERAL;
	vector[0] = 0x1000ffff;
	vector[1] = 0;
	vector = (uint32*)(MIPS_EXCEPTION_VECTOR_CACHE_ERROR - KSEG1_BASE
		+ KSEG0_BASE);
	vector[0] = 0x1000ffff;
	vector[1] = 0;

	flush_caches();
	arcs_tlb_init();

	mips_write_status(mips_read_status() & ~CP0_STATUS_BEV);
}


static uint32
check_for_boot_keys()
{
	// A space typed on the serial console during the first half second
	// opens the boot menu.
	uint32 options = 0;
	bigtime_t start = system_time();
	while (system_time() - start < 500000) {
		if (serial_try_getc() == ' ')
			options |= BOOT_OPTION_MENU;
	}
	return options;
}


extern "C" uint32
platform_boot_options(void)
{
	if (!sBootOptionsValid) {
		sBootOptions = check_for_boot_keys();
		sBootOptionsValid = true;
	}

	return sBootOptions;
}


static void
convert_preloaded_image(preloaded_elf32_image* image)
{
	fix_address(image->next);
	fix_address(image->name);
	fix_address(image->debug_string_table);
	fix_address(image->syms);
	fix_address(image->rel);
	fix_address(image->rela);
	fix_address(image->pltrel);
	fix_address(image->debug_symbols);
}


/*!	Converts the pointers in the kernel args from the loader's kseg0
	addresses to the kseg2 addresses the kernel maps the same memory at.
*/
static void
convert_kernel_args()
{
	fix_address(gKernelArgs.boot_volume);
	fix_address(gKernelArgs.vesa_modes);
	fix_address(gKernelArgs.edid_info);
	fix_address(gKernelArgs.debug_output);
	fix_address(gKernelArgs.previous_debug_output);
	fix_address(gKernelArgs.boot_splash);
	fix_address(gKernelArgs.ucode_data);

	convert_preloaded_image(static_cast<preloaded_elf32_image*>(
		gKernelArgs.kernel_image.Pointer()));
	fix_address(gKernelArgs.kernel_image);

	// Save the next pointer before converting it.
	preloaded_image* image = gKernelArgs.preloaded_images;
	fix_address(gKernelArgs.preloaded_images);
	while (image != NULL) {
		preloaded_image* next = image->next;
		convert_preloaded_image(static_cast<preloaded_elf32_image*>(image));
		image = next;
	}

	driver_settings_file* file = gKernelArgs.driver_settings;
	fix_address(gKernelArgs.driver_settings);
	while (file != NULL) {
		driver_settings_file* next = file->next;
		fix_address(file->next);
		fix_address(file->buffer);
		file = next;
	}
}


extern "C" void
platform_start_kernel(void)
{
	preloaded_elf32_image* image = static_cast<preloaded_elf32_image*>(
		gKernelArgs.kernel_image.Pointer());
	addr_t entry = image->elf_header.e_entry;

	dprintf("Muse: starting the kernel. Its log goes to serial port 1 "
		"(9600 8N1); the screen stays as it is until the desktop draws.\n");
	console_leave_prom();

	install_vectors();
	mmu_test_kernel_mapping();

	debug_cleanup();
	mmu_init_for_kernel();

	convert_kernel_args();

	addr_t stackTop
		= gKernelArgs.cpu_kstack[0].start + gKernelArgs.cpu_kstack[0].size;

	dprintf("kernel entry %#" B_PRIxADDR ", stack top %#" B_PRIxADDR "\n",
		entry, stackTop);

	// The kernel image and the modules were written through the data cache.
	flush_caches();

	arcs_enter_kernel(&gKernelArgs, entry, stackTop);
}


extern "C" void
platform_exit(void)
{
	// Jump to the PROM's reset vector.
	mips_write_status(CP0_STATUS_CU0 | CP0_STATUS_BEV);
	((void (*)(void))MIPS_KSEG1(INDY_PROM_BASE))();
	for (;;)
		;
}


static void
find_archive(stage2_args& args)
{
	uint8* header = (uint8*)ROUNDUP((addr_t)&_end, ARCHIVE_ALIGNMENT);
	args.platform.archive_data = NULL;
	args.platform.archive_size = 0;

	if (memcmp(header, ARCHIVE_MAGIC, 8) != 0) {
		dprintf("no archive at %p\n", header);
		return;
	}

	uint32 size = ((uint32)header[8] << 24) | ((uint32)header[9] << 16)
		| ((uint32)header[10] << 8) | header[11];
	args.platform.archive_data = header + ARCHIVE_ALIGNMENT;
	args.platform.archive_size = size;

	dprintf("archive at %p, %" B_PRIu32 " bytes\n",
		args.platform.archive_data, size);
}


extern "C" void
arcs_start(uint32 argc, uint32 argv, uint32 envp)
{
	// The archive follows the bss in the image file, so the PROM loaded
	// file data over the bss. Clear it. The archive starts at the next
	// 4096 byte boundary.
	memset(&__bss_start, 0, &_end - &__bss_start);
	call_ctors();

	stage2_args args;
	args.heap_size = 0;
	args.arguments = NULL;
	args.arguments_count = 0;

	console_init();
	dprintf("haiku_loader: start\n");

	gKernelArgs.platform_args.arcs_argc = argc;
	gKernelArgs.platform_args.arcs_argv = argv;
	gKernelArgs.platform_args.arcs_envp = envp;

	cpu_init();
	mmu_init();
	find_archive(args);

	main(&args);

	panic("main() returned");
}
