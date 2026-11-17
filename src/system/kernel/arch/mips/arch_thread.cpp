/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <string.h>

#include <arch/thread.h>
#include <arch_cpu.h>
#include <arch_cpu_defs.h>
#include <arch_exception_block.h>
#include <boot/stage2.h>
#include <commpage.h>
#include <kernel.h>
#include <ksignal.h>
#include <team.h>
#include <thread.h>
#include <util/AutoLock.h>
#include <vm/vm_types.h>
#include <vm/VMAddressSpace.h>

#include "MIPSVMTranslationMap.h"


static_assert(offsetof(arch_context, gp) == CONTEXT_GP, "context layout");
static_assert(offsetof(arch_context, sp) == CONTEXT_SP, "context layout");
static_assert(offsetof(arch_context, fp) == CONTEXT_FP, "context layout");
static_assert(offsetof(arch_context, ra) == CONTEXT_RA, "context layout");
static_assert(offsetof(fpu_context, fcsr) == FPU_CONTEXT_FCSR, "fpu layout");


extern "C" void mips_enter_frame(iframe* frame, addr_t kernelStackTop)
	__attribute__((noreturn));


// Status of a thread in user mode: user mode, 64 bit addressing (the n32 ABI
// uses 64 bit registers), interrupts on. EXL is set because the frame is
// entered with eret, which clears it.
static const uint32 kUserStatus = CP0_STATUS_CU0 | CP0_STATUS_DE
	| CP0_STATUS_IM2 | CP0_STATUS_IM3 | CP0_STATUS_IM7 | CP0_STATUS_UX
	| CP0_STATUS_KSU_USER | CP0_STATUS_EXL | CP0_STATUS_IE;

// The thread whose state is in the FPU registers. A thread's state is saved
// when the thread is switched out and loaded again when it next uses the
// FPU, so the owner is either the running thread or NULL.
static Thread* sFpuOwner;


//	#pragma mark - FPU


bool
mips_fpu_is_owner(Thread* thread)
{
	return sFpuOwner == thread;
}


/*!	Handles the Coprocessor Unusable exception of a user thread: loads the
	thread's FPU state. The return to user mode enables the FPU.
*/
bool
mips_fpu_handle_unusable()
{
	InterruptsLocker locker;
	Thread* thread = thread_get_current_thread();
	if (sFpuOwner == thread)
		return true;

	mips_fpu_restore(&thread->arch_info.fpuContext);
	sFpuOwner = thread;
	return true;
}


/*!	Makes thread->arch_info.fpuContext current. The thread must be the
	running thread.
*/
static void
fpu_save_current(Thread* thread)
{
	InterruptsLocker locker;
	if (sFpuOwner == thread)
		mips_fpu_save(&thread->arch_info.fpuContext);
}


void
mips_fpu_get_fcsr(uint32* _fcsr)
{
	Thread* thread = thread_get_current_thread();
	fpu_save_current(thread);
	*_fcsr = thread->arch_info.fpuContext.fcsr;

	// Clear the cause bits, or the exception recurs when the state is
	// loaded the next time.
	InterruptsLocker locker;
	thread->arch_info.fpuContext.fcsr &= ~FCSR_CAUSE_MASK;
	if (sFpuOwner == thread)
		mips_fpu_restore(&thread->arch_info.fpuContext);
}


//	#pragma mark -


status_t
arch_thread_init(struct kernel_args* args)
{
	return B_OK;
}


status_t
arch_team_init_team_struct(Team* team, bool kernel)
{
	return B_OK;
}


status_t
arch_thread_init_thread_struct(Thread* thread)
{
	memset(&thread->arch_info, 0, sizeof(thread->arch_info));
	thread->arch_info.fpuContext.fcsr = FCSR_DEFAULT;
	return B_OK;
}


void
arch_thread_init_kthread_stack(Thread* thread, void* _stack, void* _stackTop,
	void (*function)(void*), const void* data)
{
	memset(&thread->arch_info.context, 0, sizeof(arch_context));
	// n32 requires a stack aligned to 16 bytes.
	thread->arch_info.context.sp
		= mips_sign_extend(ROUNDDOWN((addr_t)_stackTop, 16) - 32);
	thread->arch_info.context.s[1] = mips_sign_extend((addr_t)function);
	thread->arch_info.context.s[2] = mips_sign_extend((addr_t)data);
	thread->arch_info.context.ra
		= mips_sign_extend((addr_t)&mips_thread_entry);

	memset(&thread->arch_info.fpuContext, 0, sizeof(fpu_context));
	thread->arch_info.fpuContext.fcsr = FCSR_DEFAULT;
}


status_t
arch_thread_init_tls(Thread* thread)
{
	thread->user_local_storage
		= thread->user_stack_base + thread->user_stack_size;
	return B_OK;
}


void
arch_thread_context_switch(Thread* from, Thread* to)
{
	// Called with interrupts disabled.
	if (sFpuOwner == from && from != NULL) {
		mips_fpu_save(&from->arch_info.fpuContext);
		sFpuOwner = NULL;
	}

	uint32 directory = (uint32)(addr_t)((MIPSVMTranslationMap*)
		to->team->address_space->TranslationMap())->PageDirectory();
	if (directory != gMipsExceptionBlock->page_directory) {
		// All address spaces use ASID 0 for now, so the user entries of the
		// old address space must go.
		gMipsExceptionBlock->page_directory = directory;
		mips_tlb_flush_all();
	}

	gMipsExceptionBlock->kernel_stack_top = to->kernel_stack_top;
	gMipsExceptionBlock->user_tls = to->user_local_storage;

	arch_context_switch(&from->arch_info.context, &to->arch_info.context);
}


void
arch_thread_dump_info(void* info)
{
	struct arch_thread* thread = (struct arch_thread*)info;

	kprintf("\tsp: %#" B_PRIx64 "\n", thread->context.sp);
	kprintf("\tra: %#" B_PRIx64 "\n", thread->context.ra);
	kprintf("\tuser frame: %p\n", thread->userFrame);
}


status_t
arch_thread_enter_userspace(Thread* thread, addr_t entry, void* arg1,
	void* arg2)
{
	addr_t commpage = (addr_t)thread->team->commpage_address;
	addr_t threadExit;
	if (user_memcpy(&threadExit,
			&((addr_t*)commpage)[COMMPAGE_ENTRY_MIPS_THREAD_EXIT],
			sizeof(threadExit)) < B_OK) {
		return B_BAD_ADDRESS;
	}
	threadExit += commpage;

	disable_interrupts();

	iframe frame;
	memset(&frame, 0, sizeof(frame));

	frame.status = kUserStatus;
	frame.epc = mips_sign_extend(entry);
	// Position independent code expects its own address in $25.
	frame.r[MIPS_REG_T9] = frame.epc;
	frame.r[MIPS_REG_A0] = mips_sign_extend((addr_t)arg1);
	frame.r[MIPS_REG_A1] = mips_sign_extend((addr_t)arg2);
	frame.r[MIPS_REG_RA] = mips_sign_extend(threadExit);
	frame.r[MIPS_REG_SP] = mips_sign_extend(ROUNDDOWN(
		thread->user_stack_base + thread->user_stack_size, 16) - 32);

	gMipsExceptionBlock->kernel_stack_top = thread->kernel_stack_top;
	gMipsExceptionBlock->user_tls = thread->user_local_storage;

	mips_enter_frame(&frame, thread->kernel_stack_top);

	return B_ERROR;
}


bool
arch_on_signal_stack(Thread* thread)
{
	struct iframe* frame = thread->arch_info.userFrame;
	if (frame == NULL) {
		panic("arch_on_signal_stack(): No user iframe!");
		return false;
	}

	addr_t sp = (addr_t)frame->r[MIPS_REG_SP];
	return sp >= thread->signal_stack_base
		&& sp < thread->signal_stack_base + thread->signal_stack_size;
}


static uint8*
get_signal_stack(Thread* thread, struct iframe* frame,
	struct sigaction* action, size_t spaceNeeded)
{
	addr_t sp = (addr_t)frame->r[MIPS_REG_SP];

	// use the alternate signal stack if we should and can
	if (thread->signal_stack_enabled
		&& (action->sa_flags & SA_ONSTACK) != 0
		&& (sp < thread->signal_stack_base
			|| sp >= thread->signal_stack_base + thread->signal_stack_size)) {
		addr_t stackTop = thread->signal_stack_base
			+ thread->signal_stack_size;
		return (uint8*)ROUNDDOWN(stackTop - spaceNeeded, 16);
	}

	return (uint8*)ROUNDDOWN(sp - spaceNeeded, 16);
}


status_t
arch_setup_signal_frame(Thread* thread, struct sigaction* action,
	struct signal_frame_data* signalFrameData)
{
	iframe* frame = thread->arch_info.userFrame;
	if (frame == NULL) {
		panic("arch_setup_signal_frame(): No user iframe!");
		return B_BAD_VALUE;
	}

	// store the registers
	struct vregs& registers = signalFrameData->context.uc_mcontext;
	for (int i = 0; i < 32; i++)
		registers.r[i] = frame->r[i];
	registers.r[0] = 0;
	registers.pc = frame->epc;
	registers.hi = frame->hi;
	registers.lo = frame->lo;

	fpu_save_current(thread);
	memcpy(registers.f, thread->arch_info.fpuContext.f, sizeof(registers.f));
	registers.fcsr = thread->arch_info.fpuContext.fcsr;

	signal_get_user_stack((addr_t)frame->r[MIPS_REG_SP],
		&signalFrameData->context.uc_stack);

	signalFrameData->syscall_restart_return_value
		= thread->arch_info.oldV0;

	// copy the frame data to the user stack
	uint8* userStack = get_signal_stack(thread, frame, action,
		sizeof(*signalFrameData) + 32);
	status_t status = user_memcpy(userStack, signalFrameData,
		sizeof(*signalFrameData));
	if (status < B_OK)
		return status;

	addr_t commpage = (addr_t)thread->team->commpage_address;
	addr_t signalHandler;
	if (user_memcpy(&signalHandler,
			&((addr_t*)commpage)[COMMPAGE_ENTRY_MIPS_SIGNAL_HANDLER],
			sizeof(signalHandler)) < B_OK) {
		return B_BAD_ADDRESS;
	}
	signalHandler += commpage;

	// Enter the commpage's handler stub with the frame data as argument.
	frame->r[MIPS_REG_SP] = mips_sign_extend((addr_t)userStack);
	frame->r[MIPS_REG_A0] = frame->r[MIPS_REG_SP];
	frame->epc = mips_sign_extend(signalHandler);
	frame->r[MIPS_REG_T9] = frame->epc;

	return B_OK;
}


int64
arch_restore_signal_frame(struct signal_frame_data* signalFrameData)
{
	Thread* thread = thread_get_current_thread();
	iframe* frame = thread->arch_info.userFrame;

	thread->arch_info.oldV0 = signalFrameData->syscall_restart_return_value;

	const struct vregs& registers = signalFrameData->context.uc_mcontext;
	for (int i = 1; i < 32; i++) {
		// $26 and $27 belong to the kernel.
		if (i != MIPS_REG_K0 && i != MIPS_REG_K1)
			frame->r[i] = registers.r[i];
	}
	frame->epc = registers.pc;
	frame->hi = registers.hi;
	frame->lo = registers.lo;

	{
		InterruptsLocker locker;
		memcpy(thread->arch_info.fpuContext.f, registers.f,
			sizeof(registers.f));
		thread->arch_info.fpuContext.fcsr = registers.fcsr & ~FCSR_CAUSE_MASK;
		if (sFpuOwner == thread)
			mips_fpu_restore(&thread->arch_info.fpuContext);
	}

	// The syscall return path stores this value in $2.
	return (int64)frame->r[MIPS_REG_V0];
}


/**	Saves everything needed to restore the frame in the child fork in the
 *	arch_fork_arg structure to be passed to arch_restore_fork_frame().
 *	Also makes sure to return the right value.
 */
void
arch_store_fork_frame(struct arch_fork_arg* arg)
{
	Thread* thread = thread_get_current_thread();
	memcpy(&arg->frame, thread->arch_info.userFrame, sizeof(iframe));

	// fork() returns 0 in the child. The parent's frame still holds the
	// address of the syscall instruction: the return value has not been
	// stored and the pc has been advanced already.
	arg->frame.r[MIPS_REG_V0] = 0;

	// The child continues with the parent's FPU state.
	fpu_save_current(thread);
	memcpy(&arg->fpuContext, &thread->arch_info.fpuContext,
		sizeof(fpu_context));
}


/** Restores the frame from a forked team as specified by the provided
 *	arch_fork_arg structure.
 *	Needs to be called from within the child team, ie. instead of
 *	arch_thread_enter_uspace() as thread "starter".
 *	This function does not return to the caller, but will enter userland
 *	in the child team at the same position where the parent team left of.
 */
void
arch_restore_fork_frame(struct arch_fork_arg* arg)
{
	disable_interrupts();

	Thread* thread = thread_get_current_thread();
	arg->frame.status = kUserStatus;
	memcpy(&thread->arch_info.fpuContext, &arg->fpuContext,
		sizeof(fpu_context));
	thread->arch_info.fpuContext.fcsr &= ~FCSR_CAUSE_MASK;

	gMipsExceptionBlock->kernel_stack_top = thread->kernel_stack_top;
	gMipsExceptionBlock->user_tls = thread->user_local_storage;

	mips_enter_frame(&arg->frame, thread->kernel_stack_top);
}
