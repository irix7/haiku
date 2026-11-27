/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <arch/user_debugger.h>
#include <debugger.h>
#include <thread.h>


/* Breakpoints, watchpoints and single stepping are not implemented. The CPU
   state of a stopped thread can be read. */


static void
get_cpu_state(Thread* thread, iframe* frame, debug_cpu_state* cpuState)
{
	for (int i = 0; i < 32; i++)
		cpuState->r[i] = frame->r[i];
	cpuState->r[0] = 0;
	cpuState->pc = frame->epc;
	cpuState->hi = frame->hi;
	cpuState->lo = frame->lo;
	memcpy(cpuState->f, thread->arch_info.fpuContext.f, sizeof(cpuState->f));
	cpuState->fcsr = thread->arch_info.fpuContext.fcsr;
}


void
arch_clear_team_debug_info(struct arch_team_debug_info* info)
{
}


void
arch_destroy_team_debug_info(struct arch_team_debug_info* info)
{
	arch_clear_team_debug_info(info);
}


void
arch_clear_thread_debug_info(struct arch_thread_debug_info* info)
{
}


void
arch_destroy_thread_debug_info(struct arch_thread_debug_info* info)
{
	arch_clear_thread_debug_info(info);
}


void
arch_update_thread_single_step()
{
}


void
arch_set_debug_cpu_state(const debug_cpu_state* cpuState)
{
	Thread* thread = thread_get_current_thread();
	iframe* frame = thread->arch_info.userFrame;
	if (frame == NULL)
		return;

	for (int i = 1; i < 32; i++) {
		if (i != MIPS_REG_K0 && i != MIPS_REG_K1)
			frame->r[i] = cpuState->r[i];
	}
	frame->epc = cpuState->pc;
	frame->hi = cpuState->hi;
	frame->lo = cpuState->lo;
}


void
arch_get_debug_cpu_state(debug_cpu_state* cpuState)
{
	Thread* thread = thread_get_current_thread();
	if (thread->arch_info.userFrame != NULL)
		get_cpu_state(thread, thread->arch_info.userFrame, cpuState);
}


status_t
arch_get_thread_debug_cpu_state(Thread* thread, debug_cpu_state* cpuState)
{
	iframe* frame = thread->arch_info.userFrame;
	if (frame == NULL)
		return B_BAD_VALUE;

	get_cpu_state(thread, frame, cpuState);
	return B_OK;
}


status_t
arch_set_breakpoint(void* address)
{
	return B_UNSUPPORTED;
}


status_t
arch_clear_breakpoint(void* address)
{
	return B_UNSUPPORTED;
}


status_t
arch_set_watchpoint(void* address, uint32 type, int32 length)
{
	return B_UNSUPPORTED;
}


status_t
arch_clear_watchpoint(void* address)
{
	return B_UNSUPPORTED;
}


bool
arch_has_breakpoints(struct arch_team_debug_info* info)
{
	return false;
}
