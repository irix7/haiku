/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include "ISA.h"
#include "isa_arch.h"


status_t
arch_start_isa_dma(long channel, void* buf, long transfer_count, uchar mode,
	uchar e_mode)
{
	// The machine has no ISA DMA controller.
	return B_NOT_ALLOWED;
}
