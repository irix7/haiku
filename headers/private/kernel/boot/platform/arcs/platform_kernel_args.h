/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef KERNEL_BOOT_PLATFORM_ARCS_KERNEL_ARGS_H
#define KERNEL_BOOT_PLATFORM_ARCS_KERNEL_ARGS_H

#ifndef KERNEL_BOOT_KERNEL_ARGS_H
#	error This file is included from <boot/kernel_args.h> only
#endif


// must match SMP_MAX_CPUS in arch_smp.h
#define SMP_MAX_CPUS 1

#define MAX_PHYSICAL_MEMORY_RANGE 32
#define MAX_PHYSICAL_ALLOCATED_RANGE 32
#define MAX_VIRTUAL_ALLOCATED_RANGE 32


typedef struct {
	// The ARCS arguments the PROM passed to the loader. The kernel does not
	// call the firmware; these are kept for diagnostics.
	uint32	arcs_argc;
	uint32	arcs_argv;
	uint32	arcs_envp;
	// Not 0 when the PROM reports a working display ("gfx=alive"). The
	// kernel then draws its debug output on the Newport.
	uint32	prom_graphics;
} platform_kernel_args;


#endif	/* KERNEL_BOOT_PLATFORM_ARCS_KERNEL_ARGS_H */
