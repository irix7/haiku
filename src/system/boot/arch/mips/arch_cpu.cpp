/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <boot/arch/mips/arch_cpu.h>

#include <KernelExport.h>

#include <arch/cpu.h>
#include <boot/kernel_args.h>
#include <boot/stage2.h>


status_t
boot_arch_cpu_init(void)
{
	gKernelArgs.arch_args.cpu_prid = mips_read_prid();
	gKernelArgs.arch_args.cpu_config = mips_read_config();
	gKernelArgs.num_cpus = 1;

	dprintf("CPU: PRId %#" B_PRIx32 ", Config %#" B_PRIx32 "\n",
		gKernelArgs.arch_args.cpu_prid, gKernelArgs.arch_args.cpu_config);
	return B_OK;
}


void
arch_ucode_load(BootVolume& volume)
{
	// This architecture has no microcode.
}
