/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>
#include <stdio.h>
#include <stdlib.h>

#include "runtime_loader_private.h"

#include <runtime_loader.h>

//#define TRACE_RLD
#ifdef TRACE_RLD
#	define TRACE(x...) dprintf(x)
#else
#	define TRACE(x...) ;
#endif


/*
 * The global offset table of a MIPS object has no relocation entries. The
 * dynamic section describes it: the first DT_MIPS_LOCAL_GOTNO entries are
 * local and get the load offset added (the first two are reserved). The
 * rest belong, in order, to the dynamic symbols from index DT_MIPS_GOTSYM
 * to DT_MIPS_SYMTABNO - 1. All symbols are bound at load time.
 */
struct got_info {
	uint32*	got;
	uint32	localCount;
	uint32	firstGotSymbol;
	uint32	symbolCount;
	bool	valid;
};


static void
get_got_info(image_t* image, got_info& info)
{
	info.got = NULL;
	info.localCount = 0;
	info.firstGotSymbol = 0;
	info.symbolCount = 0;

	bool hasLocalCount = false;
	bool hasGotSymbol = false;

	elf_dyn* dynamic = (elf_dyn*)image->dynamic_ptr;
	for (int32 i = 0; dynamic != NULL && dynamic[i].d_tag != DT_NULL; i++) {
		switch (dynamic[i].d_tag) {
			case DT_PLTGOT:
				info.got = (uint32*)(dynamic[i].d_un.d_ptr
					+ image->regions[0].delta);
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
resolve(image_t* rootImage, image_t* image, uint32 symbolIndex,
	SymbolLookupCache* cache, addr_t* _address, image_t** _symbolImage)
{
	elf_sym* symbol = SYMBOL(image, symbolIndex);
	image_t* symbolImage = NULL;
	status_t status = resolve_symbol(rootImage, image, symbol, cache,
		_address, &symbolImage);
	if (status < B_OK) {
		if (symbol->Bind() != STB_WEAK) {
			TRACE("resolve symbol \"%s\" returned: %" B_PRId32 "\n",
				SYMNAME(image, symbol), status);
			return status;
		}
		*_address = 0;
	}
	if (_symbolImage != NULL)
		*_symbolImage = symbolImage;
	return B_OK;
}


static status_t
relocate_got(image_t* rootImage, image_t* image, const got_info& info,
	SymbolLookupCache* cache)
{
	addr_t delta = image->regions[0].delta;

	for (uint32 i = 2; i < info.localCount; i++)
		info.got[i] += delta;

	for (uint32 i = info.firstGotSymbol; i < info.symbolCount; i++) {
		addr_t address;
		status_t status = resolve(rootImage, image, i, cache, &address, NULL);
		if (status < B_OK)
			return status;
		info.got[info.localCount + i - info.firstGotSymbol] = address;
	}

	return B_OK;
}


static status_t
relocate_rel(image_t* rootImage, image_t* image, const got_info& info,
	Elf32_Rel* rel, int relLength, SymbolLookupCache* cache)
{
	addr_t delta = image->regions[0].delta;

	for (int i = 0; i * (int)sizeof(Elf32_Rel) < relLength; i++) {
		unsigned type = ELF32_R_TYPE(rel[i].r_info);
		uint32 symbolIndex = ELF32_R_SYM(rel[i].r_info);
		void* where = (void*)(delta + rel[i].r_offset);

		// The field may be unaligned.
		uint32 value;
		memcpy(&value, where, sizeof(value));

		switch (type) {
			case R_MIPS_NONE:
				continue;

			case R_MIPS_REL32:
				if (!info.valid || symbolIndex < info.firstGotSymbol) {
					// relative to the image, or to a local symbol
					value += delta;
				} else {
					addr_t address;
					status_t status = resolve(rootImage, image, symbolIndex,
						cache, &address, NULL);
					if (status < B_OK)
						return status;
					value += address;
				}
				break;

			case R_MIPS_TLS_DTPMOD32:
			{
				image_t* symbolImage = NULL;
				if (symbolIndex != 0) {
					addr_t address;
					status_t status = resolve(rootImage, image, symbolIndex,
						cache, &address, &symbolImage);
					if (status < B_OK)
						return status;
				}
				value = symbolImage == NULL
					? image->dso_tls_id : symbolImage->dso_tls_id;
				break;
			}

			case R_MIPS_TLS_DTPREL32:
			{
				// The offset in the TLS block of the module that defines the
				// symbol, biased by TLS_DTV_OFFSET, which __tls_get_addr()
				// adds again. resolve_symbol() gives the plain st_value of a
				// TLS symbol.
				addr_t offset = 0;
				if (symbolIndex != 0) {
					status_t status = resolve(rootImage, image, symbolIndex,
						cache, &offset, NULL);
					if (status < B_OK)
						return status;
				}
				value += offset - TLS_DTV_OFFSET;
				break;
			}

			default:
				TRACE("unhandled relocation type %u\n", type);
				return B_NOT_ALLOWED;
		}

		memcpy(where, &value, sizeof(value));
	}

	return B_OK;
}


status_t
arch_relocate_image(image_t* rootImage, image_t* image,
	SymbolLookupCache* cache)
{
	got_info info;
	get_got_info(image, info);

	status_t status;
	if (info.valid) {
		status = relocate_got(rootImage, image, info, cache);
		if (status < B_OK)
			return status;
	}

	if (image->rel) {
		status = relocate_rel(rootImage, image, info, image->rel,
			image->rel_len, cache);
		if (status < B_OK)
			return status;
	}

	if (image->pltrel) {
		status = relocate_rel(rootImage, image, info, image->pltrel,
			image->pltrel_len, cache);
		if (status < B_OK)
			return status;
	}

	if (image->rela) {
		TRACE("RELA relocations not supported\n");
		return EOPNOTSUPP;
	}

	return B_OK;
}
