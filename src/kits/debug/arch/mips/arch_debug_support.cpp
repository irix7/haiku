/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <debug_support.h>

#include "arch_debug_support.h"


status_t
arch_debug_get_instruction_pointer(debug_context* context, thread_id thread,
	void** ip, void** stackFrameAddress)
{
	// get the CPU state
	debug_cpu_state cpuState;
	status_t error = debug_get_cpu_state(context, thread, NULL, &cpuState);
	if (error != B_OK)
		return error;

	*ip = (void*)(addr_t)cpuState.pc;
	// $30, the frame pointer
	*stackFrameAddress = (void*)(addr_t)cpuState.r[30];

	return B_OK;
}


status_t
arch_debug_get_stack_frame(debug_context* context, void* stackFrameAddress,
	debug_stack_frame_info* stackFrameInfo)
{
	// MIPS code has no frame pointer chain: every function has its own
	// frame layout. Walking the stack needs the unwind tables.
	return B_UNSUPPORTED;
}
