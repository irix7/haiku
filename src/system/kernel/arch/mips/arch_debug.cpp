/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/debug.h>

#include <setjmp.h>
#include <string.h>

#include <arch/cpu.h>
#include <arch_exception_block.h>
#include <cpu.h>
#include <debug.h>
#include <elf.h>
#include <kernel.h>
#include <thread.h>
#include <vm/vm.h>


/*
 * Stack traces
 *
 * MIPS code has no frame pointer chain: each function has its own frame
 * layout. The tracer scans the stack for values that are return addresses,
 * that is, addresses of kernel code that follow a call instruction. A stale
 * return address in an unused stack slot shows up as an extra entry.
 * Stack slots are 64 bits wide (n32).
 */


static jmp_buf* sFaultJumpBuffer;


static bool
is_return_address(uint64 slot, addr_t* _address)
{
	if ((uint64)(int64)(int32)slot != slot)
		return false;

	addr_t address = (addr_t)slot;
	if (!IS_KERNEL_ADDRESS(address) || (address & 3) != 0
		|| address < KERNEL_LOAD_BASE) {
		return false;
	}

	// A return address follows a call and its delay slot.
	uint32 instruction;
	if (debug_memcpy(B_CURRENT_TEAM, &instruction, (void*)(address - 8),
			sizeof(instruction)) != B_OK) {
		return false;
	}
	bool isJal = (instruction >> 26) == 3;
	bool isJalr = (instruction >> 26) == 0 && (instruction & 0x3f) == 9;
	bool isBal = (instruction >> 26) == 1
		&& (((instruction >> 16) & 0x1f) == 0x11
			|| ((instruction >> 16) & 0x1f) == 0x13);
	if (!isJal && !isJalr && !isBal)
		return false;

	*_address = address;
	return true;
}


static void
get_stack_range(Thread* thread, addr_t* _sp, addr_t* _top)
{
	if (thread == NULL || thread == thread_get_current_thread()) {
		*_sp = (addr_t)__builtin_frame_address(0);
	} else
		*_sp = (addr_t)thread->arch_info.context.sp;

	*_sp = ROUNDUP(*_sp, 8);

	if (thread != NULL && *_sp >= thread->kernel_stack_base
		&& *_sp < thread->kernel_stack_top) {
		*_top = thread->kernel_stack_top;
	} else {
		// an unknown stack, the boot stack for example
		*_top = *_sp + 8192;
	}
}


static void
print_address(addr_t address)
{
	const char* symbol;
	const char* image;
	addr_t baseAddress;
	bool exactMatch;

	if (elf_debug_lookup_symbol_address(address, &baseAddress, &symbol,
			&image, &exactMatch) == B_OK) {
		kprintf("%#010" B_PRIxADDR "  <%s> %s + %#" B_PRIxADDR "%s\n",
			address, image, symbol != NULL ? symbol : "?",
			address - baseAddress, exactMatch ? "" : " (nearest)");
	} else
		kprintf("%#010" B_PRIxADDR "\n", address);
}


static int
stack_trace(int argc, char** argv)
{
	Thread* thread = thread_get_current_thread();
	if (argc > 1) {
		thread_id id = strtoul(argv[1], NULL, 0);
		thread = Thread::GetDebug(id);
		if (thread == NULL) {
			kprintf("could not find thread %" B_PRId32 "\n", id);
			return 0;
		}
	}

	addr_t sp;
	addr_t top;
	get_stack_range(thread, &sp, &top);

	kprintf("stack trace for thread %" B_PRId32 " \"%s\", sp %#" B_PRIxADDR
		" (return addresses found on the stack)\n",
		thread != NULL ? thread->id : -1,
		thread != NULL ? thread->name : "", sp);

	for (; sp + 8 <= top; sp += 8) {
		uint64 slot;
		if (debug_memcpy(B_CURRENT_TEAM, &slot, (void*)sp, sizeof(slot))
				!= B_OK) {
			break;
		}
		addr_t address;
		if (is_return_address(slot, &address))
			print_address(address);
	}

	return 0;
}


//	#pragma mark -


void
arch_debug_save_registers(struct arch_debug_registers* registers)
{
	registers->fp = (addr_t)__builtin_frame_address(0);
	registers->sp = registers->fp;
	registers->pc = (addr_t)__builtin_return_address(0);
}


void
arch_debug_stack_trace(void)
{
	stack_trace(0, NULL);
}


bool
arch_debug_walk_stack(Thread* thread, bool (*callback)(void*, addr_t),
	void* context)
{
	addr_t sp;
	addr_t top;
	get_stack_range(thread, &sp, &top);

	for (; sp + 8 <= top; sp += 8) {
		uint64 slot;
		if (debug_memcpy(B_CURRENT_TEAM, &slot, (void*)sp, sizeof(slot))
				!= B_OK) {
			break;
		}
		addr_t address;
		if (is_return_address(slot, &address) && callback(context, address))
			return true;
	}

	return false;
}


int32
arch_get_stack_trace(addr_t* returnAddresses, int32 maxCount,
	int32 skipIframes, int32 skipFrames, uint32 flags)
{
	if ((flags & STACK_TRACE_KERNEL) == 0)
		return 0;

	Thread* thread = thread_get_current_thread();
	addr_t sp;
	addr_t top;
	get_stack_range(thread, &sp, &top);

	int32 count = 0;
	for (; sp + 8 <= top && count < maxCount; sp += 8) {
		uint64 slot = *(uint64*)sp;
		if ((uint64)(int64)(int32)slot != slot)
			continue;
		addr_t address = (addr_t)slot;
		if (address < KERNEL_LOAD_BASE || (address & 3) != 0)
			continue;
		if (skipFrames > 0) {
			skipFrames--;
			continue;
		}
		returnAddresses[count++] = address;
	}

	return count;
}


void*
arch_debug_get_interrupt_pc(bool* _isSyscall)
{
	Thread* thread = thread_get_current_thread();
	if (thread == NULL || thread->arch_info.userFrame == NULL)
		return NULL;

	if (_isSyscall != NULL)
		*_isSyscall = false;
	return (void*)(addr_t)thread->arch_info.userFrame->epc;
}


void
arch_debug_unset_current_thread(void)
{
	arch_thread_set_current_thread(NULL);
}


static void
fault_landing()
{
	longjmp(*sFaultJumpBuffer, 1);
}


void
arch_debug_call_with_fault_handler(cpu_ent* cpu, jmp_buf jumpBuffer,
	void (*function)(void*), void* parameter)
{
	// The exception handler continues at the handler with the given stack
	// pointer. The handler needs only enough stack to call longjmp().
	sFaultJumpBuffer = (jmp_buf*)jumpBuffer;
	cpu->fault_handler = (addr_t)&fault_landing;
	cpu->fault_handler_stack_pointer
		= ROUNDDOWN((addr_t)__builtin_frame_address(0), 16) - 256;

	function(parameter);

	longjmp(jumpBuffer, 1);
}


bool
arch_is_debug_variable_defined(const char* variableName)
{
	return false;
}


status_t
arch_set_debug_variable(const char* variableName, uint64 value)
{
	return B_ENTRY_NOT_FOUND;
}


status_t
arch_get_debug_variable(const char* variableName, uint64* value)
{
	return B_ENTRY_NOT_FOUND;
}


ssize_t
arch_debug_gdb_get_registers(char* buffer, size_t bufferSize)
{
	return B_NOT_SUPPORTED;
}


void
arch_debug_snooze(bigtime_t duration)
{
	// Count runs at 33 to 67 MHz. Assume the fastest rate.
	uint32 ticks = (uint32)MIN(duration, (bigtime_t)30000000) * 67;
	uint32 start = mips_read_count();
	while (mips_read_count() - start < ticks)
		;
}


status_t
arch_debug_init(kernel_args* args)
{
	add_debugger_command("where", &stack_trace, "Same as \"sc\"");
	add_debugger_command("bt", &stack_trace, "Same as \"sc\"");
	add_debugger_command("sc", &stack_trace,
		"Stack crawl for the current or the given thread: lists the kernel "
		"return addresses found on its stack");

	return B_OK;
}
