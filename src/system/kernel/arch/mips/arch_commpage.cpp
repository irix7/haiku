/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <commpage.h>

#include <string.h>

#include <KernelExport.h>

#include <cpu.h>
#include <elf.h>
#include <ksignal.h>
#include <smp.h>

#include "syscall_numbers.h"


#define TO_STRING_LITERAL_HELPER(number)	#number
#define TO_STRING_LITERAL(number)	TO_STRING_LITERAL_HELPER(number)


/*	The two functions below are copied to the commpage and run in user mode.
	They must not refer to anything by address. Calls through a function
	pointer load the target into $25, as position independent code expects.
*/


extern "C" void __attribute__((noreturn))
mips_user_signal_handler(signal_frame_data* data)
{
	if (data->siginfo_handler) {
		auto handler = (void (*)(int, siginfo_t*, void*, void*))data->handler;
		handler(data->info.si_signo, &data->info, &data->context,
			data->user_data);
	} else {
		auto handler = (void (*)(int, void*, vregs*))data->handler;
		handler(data->info.si_signo, data->user_data,
			&data->context.uc_mcontext);
	}

	__asm__ __volatile__(
		"move	$4, %0\n"
		"li	$2, " TO_STRING_LITERAL(SYSCALL_RESTORE_SIGNAL_FRAME) "\n"
		"syscall\n"
		: : "r" (data) : "$2", "$4");

	__builtin_unreachable();
}


/*	A thread's entry function returns here, with its return value in $2. */
extern "C" void mips_user_thread_exit();
__asm__(
	".text\n"
	".globl	mips_user_thread_exit\n"
	".type	mips_user_thread_exit, @function\n"
	".ent	mips_user_thread_exit\n"
	"mips_user_thread_exit:\n"
	"	.set	push\n"
	"	.set	noreorder\n"
	"	move	$4, $2\n"
	"	li	$2, " TO_STRING_LITERAL(SYSCALL_EXIT_THREAD) "\n"
	"	syscall\n"
	"1:	b	1b\n"
	"	 nop\n"
	"	.set	pop\n"
	".end	mips_user_thread_exit\n"
	".size	mips_user_thread_exit, . - mips_user_thread_exit\n"
);


static void
register_commpage_function(const char* functionName, int32 commpageIndex,
	const char* commpageSymbolName, addr_t expectedAddress)
{
	// get address and size of function
	elf_symbol_info symbolInfo;
	if (elf_lookup_kernel_symbol(functionName, &symbolInfo) != B_OK) {
		panic("register_commpage_function(): Failed to find "
			"function \"%s\"!", functionName);
	}

	ASSERT(expectedAddress == symbolInfo.address);

	// fill in the commpage table entry
	addr_t position = fill_commpage_entry(commpageIndex,
		(void*)symbolInfo.address, symbolInfo.size);

	// add symbol to the commpage image
	image_id image = get_commpage_image();
	elf_add_memory_image_symbol(image, commpageSymbolName, position,
		symbolInfo.size, B_SYMBOL_TYPE_TEXT);
}


status_t
arch_commpage_init(void)
{
	return B_OK;
}


status_t
arch_commpage_init_post_cpus(void)
{
	register_commpage_function("mips_user_signal_handler",
		COMMPAGE_ENTRY_MIPS_SIGNAL_HANDLER, "commpage_signal_handler",
		(addr_t)&mips_user_signal_handler);

	register_commpage_function("mips_user_thread_exit",
		COMMPAGE_ENTRY_MIPS_THREAD_EXIT, "commpage_thread_exit",
		(addr_t)&mips_user_thread_exit);

	return B_OK;
}
