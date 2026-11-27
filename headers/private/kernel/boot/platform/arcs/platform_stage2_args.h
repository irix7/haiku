/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_BOOT_PLATFORM_ARCS_STAGE2_H
#define KERNEL_BOOT_PLATFORM_ARCS_STAGE2_H

#ifndef KERNEL_BOOT_STAGE2_ARGS_H
#	error This file is included from <boot/stage2_args.h> only
#endif


struct platform_stage2_args {
	// The netboot archive appended to the loader image (a gzip tar file).
	void*	archive_data;
	uint32	archive_size;
};


#endif	/* KERNEL_BOOT_PLATFORM_ARCS_STAGE2_H */
