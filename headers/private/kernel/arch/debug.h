/*
 * Copyright 2005-2009, Axel Dörfler, axeld@pinc-software.de.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2001-2002, Travis Geiselbrecht. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */
#ifndef _KERNEL_ARCH_DEBUG_H
#define _KERNEL_ARCH_DEBUG_H


#include <SupportDefs.h>

#include <arch_debug.h>
#include <cpu.h>


struct kernel_args;

namespace BKernel {
	struct Thread;
}

using BKernel::Thread;


// arch_debug_get_stack_trace() flags
#define STACK_TRACE_KERNEL	0x01
#define STACK_TRACE_USER	0x02


#ifdef __cplusplus
extern "C" {
#endif

status_t arch_debug_init(kernel_args *args);
void arch_debug_stack_trace();
int32 arch_get_stack_trace(addr_t* returnAddresses, int32 maxCount,
		int32 skipIframes, int32 skipFrames, uint32 flags);
void* arch_debug_get_interrupt_pc(bool* _isSyscall);
bool arch_debug_walk_stack(Thread* thread,
		bool (*callback)(void*, addr_t), void* context);
void arch_debug_save_registers(struct arch_debug_registers* registers);
void arch_debug_unset_current_thread(void);
void arch_debug_call_with_fault_handler(cpu_ent* cpu, jmp_buf jumpBuffer,
		void (*function)(void*), void* parameter);

bool arch_is_debug_variable_defined(const char* variableName);
status_t arch_set_debug_variable(const char* variableName, uint64 value);
status_t arch_get_debug_variable(const char* variableName, uint64* value);

// Hooks of the GDB stub (debug/gdb.cpp). Only the first one is mandatory:
// the stub has weak defaults for the others.
ssize_t arch_debug_gdb_get_registers(char* buffer, size_t bufferSize);
	// all registers as hex digits, in the order of GDB's "g" packet
status_t arch_debug_gdb_set_registers(const char* buffer, size_t length);
ssize_t arch_debug_gdb_get_register(int32 index, char* buffer,
		size_t bufferSize);
status_t arch_debug_gdb_set_register(int32 index, const char* buffer,
		size_t length);
void arch_debug_gdb_set_entry_context(jmp_buf context);
	// the registers to report if no exception entered the debugger
ssize_t arch_debug_gdb_get_breakpoint_instruction(addr_t address, int32 kind,
		void* buffer, size_t bufferSize);
void arch_debug_gdb_flush_code(addr_t address, size_t size);
	// called after the stub has written to memory
status_t arch_debug_gdb_resume(bool hasAddress, addr_t address);
	// called before the kernel resumes, with the new pc if GDB gave one
ssize_t arch_debug_gdb_get_step_targets(addr_t* targets, int32 maxCount);
	// the addresses the CPU can be at after the next instruction
char arch_debug_gdb_getchar(void);
void arch_debug_gdb_putchar(char c);
void arch_debug_gdb_flush(void);
	// the channel to GDB, by default the serial debug port
bool arch_debug_gdb_uses_serial_port(void);

// Called when the kernel debugger has been entered and before it is left.
void arch_debug_enter_debugger(void);
void arch_debug_exit_debugger(void);

void arch_debug_snooze(bigtime_t duration);

#ifdef __cplusplus
}
#endif

#endif	/* _KERNEL_ARCH_DEBUG_H */
