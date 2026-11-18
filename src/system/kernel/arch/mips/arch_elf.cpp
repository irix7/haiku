/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#ifdef _BOOT_MODE
#	include <boot/arch.h>
#endif

#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/elf.h>
#include <elf_priv.h>

#include <string.h>


//#define TRACE_ARCH_ELF
#ifdef TRACE_ARCH_ELF
#	define TRACE(x) dprintf x
#else
#	define TRACE(x) ;
#endif


#ifdef _BOOT_MODE


/*	The loader relocates the kernel only. The kernel is linked at its load
	address, so its relocation entries need no work.
*/


status_t
boot_arch_elf_relocate_rel(preloaded_elf32_image* image, Elf32_Rel* rel,
	int relLength)
{
	for (int i = 0; i < relLength / (int)sizeof(Elf32_Rel); i++) {
		int type = ELF32_R_TYPE(rel[i].r_info);
		if (type == R_MIPS_NONE)
			continue;
		if (type == R_MIPS_REL32 && image->text_region.delta == 0)
			continue;

		dprintf("boot_arch_elf_relocate_rel: unhandled relocation type %d\n",
			type);
		return B_BAD_DATA;
	}

	return B_OK;
}


status_t
boot_arch_elf_relocate_rela(preloaded_elf32_image* image, Elf32_Rela* rel,
	int relLength)
{
	dprintf("boot_arch_elf_relocate_rela: not supported on mips\n");
	return B_ERROR;
}


#else	// !_BOOT_MODE


struct mips_got_info {
	uint32*	got;
	uint32	localCount;
	uint32	firstGotSymbol;
	uint32	symbolCount;
	bool	valid;
};


static bool
is_in_image(struct elf_image_info* image, addr_t address)
{
	return (address >= image->text_region.start
			&& address < image->text_region.start
				+ image->text_region.size)
		|| (address >= image->data_region.start
			&& address < image->data_region.start
				+ image->data_region.size);
}


static void
get_got_info(struct elf_image_info* image, mips_got_info& info)
{
	info.got = NULL;
	info.localCount = 0;
	info.firstGotSymbol = 0;
	info.symbolCount = 0;

	bool hasLocalCount = false;
	bool hasGotSymbol = false;

	elf_dyn* dynamic = (elf_dyn*)image->dynamic_section;
	for (int32 i = 0; dynamic != NULL && dynamic[i].d_tag != DT_NULL; i++) {
		switch (dynamic[i].d_tag) {
			case DT_PLTGOT:
				info.got = (uint32*)(dynamic[i].d_un.d_ptr
					+ image->text_region.delta);
				break;
			case DT_MIPS_LOCAL_GOTNO:
				info.localCount = dynamic[i].d_un.d_val;
				hasLocalCount = true;
				break;
			case DT_MIPS_GOTSYM:
				info.firstGotSymbol = dynamic[i].d_un.d_val;
				hasGotSymbol = true;
				break;
			case DT_MIPS_SYMTABNO:
				info.symbolCount = dynamic[i].d_un.d_val;
				break;
		}
	}

	info.valid = info.got != NULL && hasLocalCount && hasGotSymbol
		&& info.symbolCount >= info.firstGotSymbol;
}


static status_t
resolve_symbol(struct elf_image_info* image,
	struct elf_image_info* resolveImage, uint32 index, elf_addr* _address)
{
	elf_sym* symbol = &image->syms[index];

	if (symbol->st_shndx != SHN_UNDEF) {
		// Defined in the image itself. Add-ons are linked with -Bsymbolic.
		*_address = symbol->st_value + image->text_region.delta;
		return B_OK;
	}

	status_t status = elf_resolve_symbol(image, symbol, resolveImage,
		_address);
	if (status < B_OK && symbol->Bind() == STB_WEAK) {
		*_address = 0;
		return B_OK;
	}
	return status;
}


int
arch_elf_relocate_got(struct elf_image_info* image,
	struct elf_image_info* resolveImage)
{
	mips_got_info info;
	get_got_info(image, info);

	if (info.valid) {
		addr_t delta = image->text_region.delta;

		// Entries 0 and 1 are reserved for a lazy binding resolver. All
		// symbols are bound here, so they stay unused.
		for (uint32 i = 2; i < info.localCount; i++)
			info.got[i] += delta;

		for (uint32 i = info.firstGotSymbol; i < info.symbolCount; i++) {
			uint32* entry
				= &info.got[info.localCount + i - info.firstGotSymbol];
			if (!is_in_image(image, (addr_t)entry)) {
				dprintf("arch_elf_relocate_got: GOT entry %p outside of "
					"image \"%s\"\n", entry, image->name);
				return B_BAD_ADDRESS;
			}

			elf_addr address;
			status_t status = resolve_symbol(image, resolveImage, i,
				&address);
			if (status < B_OK)
				return status;
			*entry = address;
		}
	}

	// The image was written through the data cache.
	arch_cpu_sync_icache((void*)image->text_region.start,
		image->text_region.size);
	arch_cpu_sync_icache((void*)image->data_region.start,
		image->data_region.size);

	return B_OK;
}


int
arch_elf_relocate_rel(struct elf_image_info* image,
	struct elf_image_info* resolveImage, Elf32_Rel* rel, int relLength)
{
	mips_got_info info;
	get_got_info(image, info);

	for (int i = 0; i < relLength / (int)sizeof(Elf32_Rel); i++) {
		int type = ELF32_R_TYPE(rel[i].r_info);
		uint32 symbolIndex = ELF32_R_SYM(rel[i].r_info);

		if (type == R_MIPS_NONE)
			continue;
		if (type != R_MIPS_REL32) {
			dprintf("arch_elf_relocate_rel: unhandled relocation type %d\n",
				type);
			return B_BAD_DATA;
		}

		addr_t address = rel[i].r_offset + image->text_region.delta;
		if (!is_in_image(image, address)) {
			dprintf("arch_elf_relocate_rel: invalid offset %#" B_PRIx32 "\n",
				(uint32)rel[i].r_offset);
			return B_BAD_ADDRESS;
		}

		// The field may be unaligned.
		uint32 value;
		memcpy(&value, (void*)address, sizeof(value));

		if (!info.valid || symbolIndex < info.firstGotSymbol) {
			// relative to the image, or to a local symbol
			value += image->text_region.delta;
		} else {
			elf_addr symbolAddress;
			status_t status = resolve_symbol(image, resolveImage,
				symbolIndex, &symbolAddress);
			if (status < B_OK)
				return status;
			value += symbolAddress;
		}

		memcpy((void*)address, &value, sizeof(value));
	}

	return B_OK;
}


int
arch_elf_relocate_rela(struct elf_image_info* image,
	struct elf_image_info* resolveImage, Elf32_Rela* rel, int relLength)
{
	dprintf("arch_elf_relocate_rela: not supported on mips\n");
	return B_ERROR;
}


#endif	// !_BOOT_MODE
