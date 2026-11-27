/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_ARCH_MIPS_KERNEL_ARGS_H
#define KERNEL_ARCH_MIPS_KERNEL_ARGS_H

#ifndef KERNEL_BOOT_KERNEL_ARGS_H
#	error This file is included from <boot/kernel_args.h> only
#endif


#define _PACKED __attribute__((packed))


// kernel args
typedef struct {
	// kseg0 address of the page directory the loader built: 64 entries,
	// each the kseg0 address of a page table.
	uint32	page_directory;
	// kseg0 address of the page table that every empty directory slot
	// points at.
	uint32	invalid_page_table;
	// End of the kseg2 range the loader mapped. The range starts at
	// 0xc0000000.
	uint32	virtual_end;
	// CP0 PRId and Config as the loader read them.
	uint32	cpu_prid;
	uint32	cpu_config;
} _PACKED arch_kernel_args;


#endif	/* KERNEL_ARCH_MIPS_KERNEL_ARGS_H */
