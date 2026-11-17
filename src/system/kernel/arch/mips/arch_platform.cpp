/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <arch/platform.h>
#include <boot/kernel_args.h>


status_t
arch_platform_init(struct kernel_args* args)
{
	return B_OK;
}


status_t
arch_platform_init_post_vm(struct kernel_args* args)
{
	return B_OK;
}


status_t
arch_platform_init_post_thread(struct kernel_args* args)
{
	// Gate marker of milestone M3.
	dprintf("haiku: m3 threads ok\n");
	return B_OK;
}
