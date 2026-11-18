/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/system_info.h>
#include <boot/kernel_args.h>


void
arch_fill_topology_node(cpu_topology_node_info* node, int32 cpu)
{
	switch (node->type) {
		case B_TOPOLOGY_ROOT:
			node->data.root.platform = B_CPU_MIPS;
			break;

		case B_TOPOLOGY_PACKAGE:
			node->data.package.vendor = B_CPU_VENDOR_UNKNOWN;
			node->data.package.cache_line_size = CACHE_LINE_SIZE;
			break;

		case B_TOPOLOGY_CORE:
			node->data.core.model = gMipsCpuInfo.prid;
			// Count runs at half the core clock on the R4600.
			node->data.core.default_frequency
				= (uint64)gMipsCpuInfo.count_frequency * 2;
			break;

		default:
			break;
	}
}


status_t
arch_system_info_init(struct kernel_args* args)
{
	return B_OK;
}


status_t
arch_get_frequency(uint64* frequency, int32 cpu)
{
	*frequency = (uint64)gMipsCpuInfo.count_frequency * 2;
	return B_OK;
}
