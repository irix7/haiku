/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <stdio.h>
#include <string.h>

#include <algorithm>

#include <arch/cpu.h>
#include <arch/debug.h>
#include <arch_cpu_defs.h>
#include <arch_exception_block.h>
#include <arch_indy.h>
#include <arch_thread_types.h>
#include <boot/kernel_args.h>
#include <cpu.h>
#include <debug.h>
#include <elf.h>
#include <interrupts.h>
#include <kscheduler.h>
#include <ksignal.h>
#include <ksyscalls.h>
#include <ScopeExit.h>
#include <smp.h>
#include <syscall_numbers.h>
#include <thread.h>
#include <timer.h>
#include <user_debugger.h>
#include <util/AutoLock.h>
#include <vm/vm.h>
#include <vm/vm_priv.h>
#include <vm/VMAddressSpace.h>

#include "MIPSVMTranslationMap.h"


static_assert(offsetof(iframe, hi) == IFRAME_HI, "iframe layout");
static_assert(offsetof(iframe, epc) == IFRAME_EPC, "iframe layout");
static_assert(offsetof(iframe, badvaddr) == IFRAME_BADVADDR, "iframe layout");
static_assert(offsetof(iframe, status) == IFRAME_STATUS, "iframe layout");
static_assert(offsetof(iframe, cause) == IFRAME_CAUSE, "iframe layout");
static_assert(sizeof(iframe) == IFRAME_SIZE, "iframe layout");
static_assert(offsetof(mips_exception_block, frame) + 0x400
	== EXCEPTION_BLOCK_FRAME, "exception block layout");


extern "C" {
extern uint32 mips_tlb_refill_vector[];
extern uint32 mips_tlb_refill_vector_end[];
extern uint32 mips_xtlb_refill_vector[];
extern uint32 mips_xtlb_refill_vector_end[];
extern uint32 mips_general_vector[];
extern uint32 mips_general_vector_end[];
extern uint32 mips_cache_error_vector[];
extern uint32 mips_cache_error_vector_end[];

void mips_handle_exception(iframe* frame);
void mips_double_fault_handler(uint64 epc, uint64 badvaddr, uint32 cause,
	addr_t stackPointer);
void mips_cache_error_handler(void);
}

// arch_timer.cpp
void mips_timer_interrupt();
// arch_thread.cpp
bool mips_fpu_handle_unusable();
bool mips_fpu_is_owner(Thread* thread);
void mips_fpu_get_fcsr(uint32* _fcsr);


// The stack for exceptions in the exception entry code.
static uint8 sEmergencyStack[16384] __attribute__((aligned(16)));
addr_t gMipsEmergencyStackTop = (addr_t)sEmergencyStack
	+ sizeof(sEmergencyStack);

static uint32 sSpuriousInterrupts[3];


static const char* const kExceptionNames[32] = {
	"interrupt", "TLB modified", "TLB load", "TLB store", "address error "
	"(load)", "address error (store)", "bus error (instruction)",
	"bus error (data)", "syscall", "breakpoint", "reserved instruction",
	"coprocessor unusable", "overflow", "trap", "virtual coherency "
	"(instruction)", "FPU", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"watch", NULL, NULL, NULL, NULL, NULL, NULL, NULL,
	"virtual coherency (data)"
};


static inline bool
is_user_frame(const iframe* frame)
{
	return (frame->status & CP0_STATUS_KSU_MASK) != 0;
}


static inline uint32
exception_code(const iframe* frame)
{
	return (frame->cause & CP0_CAUSE_EXCCODE_MASK) >> CP0_CAUSE_EXCCODE_SHIFT;
}


static void
dump_frame(const iframe* frame)
{
	uint32 code = exception_code(frame);
	dprintf("exception %" B_PRIu32 " (%s) in %s mode\n", code,
		kExceptionNames[code] != NULL ? kExceptionNames[code] : "unknown",
		is_user_frame(frame) ? "user" : "kernel");
	dprintf("epc %#018" B_PRIx64 "  badvaddr %#018" B_PRIx64 "\n",
		frame->epc, frame->badvaddr);
	dprintf("status %#010" B_PRIx32 "  cause %#010" B_PRIx32 "\n",
		frame->status, frame->cause);
	for (int i = 0; i < 32; i += 2) {
		dprintf("r%-2d %#018" B_PRIx64 "  r%-2d %#018" B_PRIx64 "\n", i,
			frame->r[i], i + 1, frame->r[i + 1]);
	}
	dprintf("hi  %#018" B_PRIx64 "  lo  %#018" B_PRIx64 "\n", frame->hi,
		frame->lo);
}


/*!	Panics for an exception in kernel mode. An attached GDB is told
	\a signal and gets the registers of the frame.
*/
static void
kernel_exception_panic(iframe* frame, int32 signal, const char* message)
{
	dump_frame(frame);

	iframe* previous = mips_debug_set_frame(frame);
	debug_gdb_set_stop_signal(signal);
	panic("%s", message);
	mips_debug_set_frame(previous);
}


//!	Returns the number GDB's remote protocol has for a signal.
static int32
gdb_signal(uint32 signalNumber)
{
	switch (signalNumber) {
		case SIGILL:
			return GDB_SIGNAL_ILL;
		case SIGFPE:
			return GDB_SIGNAL_FPE;
		case SIGBUS:
			return GDB_SIGNAL_BUS;
		case SIGSEGV:
			return GDB_SIGNAL_SEGV;
		default:
			return GDB_SIGNAL_ABRT;
	}
}


/*!	Prints the function and the image of an address of the current team.
*/
static void
dump_user_symbol(const char* name, addr_t address)
{
	addr_t base = 0;
	const char* symbol = NULL;
	const char* image = NULL;
	bool exact = false;
	elf_debug_lookup_user_symbol_address(thread_get_current_thread()->team,
		address, &base, &symbol, &image, &exact);
	dprintf("%s %#" B_PRIxADDR ": %s + %#" B_PRIxADDR " in %s\n", name, address,
		symbol != NULL ? symbol : "?", address - base,
		image != NULL ? image : "?");
}


/*!	Sends a signal for an exception in user mode. Panics for an exception in
	kernel mode.
*/
static void
send_signal(iframe* frame, debug_exception_type type, uint32 signalNumber,
	int32 signalCode, addr_t signalAddress = 0, int32 signalError = B_ERROR)
{
	if (!is_user_frame(frame)) {
		kernel_exception_panic(frame, gdb_signal(signalNumber),
			"unexpected exception in kernel mode");
		return;
	}

	struct sigaction action;
	Thread* thread = thread_get_current_thread();

	dprintf("thread %" B_PRId32 " (%s): signal %" B_PRIu32 ", code %" B_PRId32
		", address %#" B_PRIxADDR "\n", thread->id, thread->name, signalNumber,
		signalCode, signalAddress);
	dump_frame(frame);

	enable_interrupts();

	dump_user_symbol("epc", (addr_t)frame->epc);
	dump_user_symbol("ra ", (addr_t)frame->r[31]);

	if ((sigaction(signalNumber, NULL, &action) == 0
			&& action.sa_handler != SIG_DFL
			&& action.sa_handler != SIG_IGN)
		|| user_debug_exception_occurred(type, signalNumber)) {
		Signal signal(signalNumber, signalCode, signalError,
			thread->team->id);
		signal.SetAddress((void*)signalAddress);
		send_signal_to_thread(thread, signal, 0);
	}
}


static void
after_interrupt()
{
	if (debug_debugger_running())
		return;

	Thread* thread = thread_get_current_thread();
	cpu_status state = disable_interrupts();
	if (thread->post_interrupt_callback != NULL) {
		void (*callback)(void*) = thread->post_interrupt_callback;
		void* data = thread->post_interrupt_data;

		thread->post_interrupt_callback = NULL;
		thread->post_interrupt_data = NULL;

		restore_interrupts(state);

		callback(data);
	} else if (thread->cpu->invoke_scheduler) {
		SpinLocker schedulerLocker(thread->scheduler_lock);
		scheduler_reschedule(B_THREAD_READY);
		schedulerLocker.Unlock();
		restore_interrupts(state);
	}
}


//	#pragma mark - interrupts


static void
dispatch_mapped(uint8 pending)
{
	for (int32 bit = 0; bit < 8; bit++) {
		if ((pending & (1 << bit)) == 0)
			continue;

		if (INDY_IRQ_MAPPED(bit) == INDY_IRQ_KEYBOARD_MOUSE) {
			// The ps2 bus manager installs the same handler on the PC's
			// keyboard and mouse vectors, 1 and 12. It serves both devices,
			// so one call is enough. A second call, or a call of the
			// mapped vector that has no handler, counts as unhandled, and
			// the kernel disables a vector after 10000 of those.
			io_interrupt_handler(1, B_LEVEL_TRIGGERED);
			continue;
		}

		io_interrupt_handler(INDY_IRQ_MAPPED(bit), B_LEVEL_TRIGGERED);
	}
}


static void
handle_local0()
{
	uint8 pending = indy_read8(INDY_INT3_L0_STATUS)
		& indy_read8(INDY_INT3_L0_MASK);
	if (pending == 0) {
		sSpuriousInterrupts[0]++;
		return;
	}

	for (int32 bit = 0; bit < 7; bit++) {
		if ((pending & (1 << bit)) != 0)
			io_interrupt_handler(INDY_IRQ_LOCAL0(bit), B_LEVEL_TRIGGERED);
	}
	if ((pending & INDY_INT3_LOCAL0_CASCADE) != 0) {
		dispatch_mapped(indy_read8(INDY_INT3_MAP_STATUS)
			& indy_read8(INDY_INT3_MAP_MASK0));
	}
}


static void
handle_local1()
{
	uint8 pending = indy_read8(INDY_INT3_L1_STATUS)
		& indy_read8(INDY_INT3_L1_MASK);
	if (pending == 0) {
		sSpuriousInterrupts[1]++;
		return;
	}

	for (int32 bit = 0; bit < 8; bit++) {
		if ((pending & (1 << bit)) == 0)
			continue;
		if ((1 << bit) == INDY_INT3_LOCAL1_CASCADE) {
			dispatch_mapped(indy_read8(INDY_INT3_MAP_STATUS)
				& indy_read8(INDY_INT3_MAP_MASK1));
		} else
			io_interrupt_handler(INDY_IRQ_LOCAL1(bit), B_LEVEL_TRIGGERED);
	}
}


static void
handle_interrupt(iframe* frame)
{
	uint32 pending = frame->cause & frame->status & CP0_CAUSE_IP_MASK;

	if ((pending & CP0_STATUS_IM7) != 0)
		mips_timer_interrupt();
	if ((pending & CP0_STATUS_IM2) != 0)
		handle_local0();
	if ((pending & CP0_STATUS_IM3) != 0)
		handle_local1();
	if ((pending & CP0_STATUS_IM6) != 0) {
		dump_frame(frame);
		panic("bus error interrupt");
	}
	if ((pending & (CP0_STATUS_IM2 | CP0_STATUS_IM3 | CP0_STATUS_IM6
			| CP0_STATUS_IM7)) == 0) {
		sSpuriousInterrupts[2]++;
	}

	mips_debug_check_break(frame);

	after_interrupt();
}


//	#pragma mark - memory faults


/*!	Sets the accessed and modified state for a fault on a mapped page.
	Returns false if the fault is one for the VM.
*/
static bool
handle_tlb_fixup(addr_t address, bool isWrite)
{
	Thread* thread = thread_get_current_thread();
	VMAddressSpace* addressSpace = NULL;
	if (IS_KERNEL_ADDRESS(address))
		addressSpace = VMAddressSpace::Kernel();
	else if (thread != NULL && thread->team != NULL)
		addressSpace = thread->team->address_space;
	if (addressSpace == NULL)
		return false;

	MIPSVMTranslationMap* map
		= (MIPSVMTranslationMap*)addressSpace->TranslationMap();
	return map->HandleFault(address, isWrite);
}


/*!	Continues at the fault handler of the kernel debugger, if one is set
	(arch_debug_call_with_fault_handler()). Returns false otherwise.
*/
static bool
call_debugger_fault_handler(iframe* frame, addr_t address, bool isWrite)
{
	if (!debug_debugger_running() || thread_get_current_thread() == NULL)
		return false;

	cpu_ent* cpu = &gCPU[smp_get_current_cpu()];
	if (cpu->fault_handler == 0)
		return false;

	mips_debug_fault_link* link
		= (mips_debug_fault_link*)cpu->fault_handler_stack_pointer;

	debug_set_page_fault_info(address, (addr_t)frame->epc,
		isWrite ? DEBUG_PAGE_FAULT_WRITE : 0);
	frame->epc = mips_sign_extend(cpu->fault_handler);
	frame->r[MIPS_REG_T9] = frame->epc;
	frame->r[MIPS_REG_A0] = mips_sign_extend((addr_t)link->jump_buffer);
	// The handler needs only enough stack to call longjmp().
	frame->r[MIPS_REG_SP]
		= mips_sign_extend(ROUNDDOWN((addr_t)link, 16) - 256);
	return true;
}


static void
handle_page_fault(iframe* frame, addr_t address, bool isWrite)
{
	bool isUser = is_user_frame(frame);
	addr_t pc = (addr_t)frame->epc;

	if (debug_debugger_running()) {
		Thread* thread = thread_get_current_thread();
		if (thread != NULL) {
			if (call_debugger_fault_handler(frame, address, isWrite))
				return;

			if (thread->fault_handler != 0) {
				kprintf("ERROR: thread::fault_handler used in kernel "
					"debugger!\n");
				debug_set_page_fault_info(address, pc,
					isWrite ? DEBUG_PAGE_FAULT_WRITE : 0);
				frame->epc = mips_sign_extend((addr_t)thread->fault_handler);
				frame->r[MIPS_REG_T9] = frame->epc;
				return;
			}
		}

		dump_frame(frame);
		panic("page fault in debugger without fault handler! Touching "
			"address %p from pc %p\n", (void*)address, (void*)pc);
		return;
	}

	if ((frame->status & CP0_STATUS_IE) == 0) {
		// Interrupts were off. Only a fault handler can recover.
		Thread* thread = thread_get_current_thread();
		if (thread != NULL && thread->fault_handler != 0) {
			addr_t handler = (addr_t)thread->fault_handler;
			if (pc != handler) {
				frame->epc = mips_sign_extend(handler);
				frame->r[MIPS_REG_T9] = frame->epc;
				return;
			}
		}
		dump_frame(frame);
		panic("page fault with interrupts disabled, address %#" B_PRIxADDR,
			address);
		return;
	}

	addr_t newPC = 0;
	enable_interrupts();

	if (isUser && address < B_PAGE_SIZE) {
		// A jump or an access through a null pointer. The caller is in ra.
		dprintf("null pointer access in user mode: address %#" B_PRIxADDR
			", pc %#" B_PRIxADDR "\n", address, pc);
		dump_user_symbol("ra", (addr_t)frame->r[MIPS_REG_RA]);
	}

	vm_page_fault(address, pc, isWrite, address == pc, isUser, &newPC);

	if (newPC != 0) {
		// The fault handler of the thread. It is a function, and MIPS
		// functions expect their own address in $25.
		frame->epc = mips_sign_extend(newPC);
		frame->r[MIPS_REG_T9] = frame->epc;
	}
}


//	#pragma mark - instruction emulation


/*!	Emulates "rdhwr rt, $29", which reads the thread pointer. The CPU has no
	such instruction; compilers emit it for thread local storage.
*/
static bool
emulate_rdhwr(iframe* frame)
{
	if ((frame->cause & CP0_CAUSE_BD) != 0)
		return false;

	uint32 instruction;
	if (user_memcpy(&instruction, (void*)(addr_t)frame->epc,
			sizeof(instruction)) != B_OK) {
		return false;
	}
	if ((instruction & 0xffe0ffff) != 0x7c00e83b)
		return false;

	uint32 target = (instruction >> 16) & 0x1f;
	if (target != 0) {
		frame->r[target] = mips_sign_extend(
			thread_get_current_thread()->user_local_storage);
	}
	frame->epc += 4;
	return true;
}


/*!	Emulates an unaligned load or store of the kernel. The boot loader hands
	the kernel packed structures, and generic kernel code reads their fields
	through plain pointers, which the compiler takes to be aligned.
	Returns false if the instruction is not a load or store this handles.
*/
/*!	Computes where a branch or jump instruction at \a pc continues, and sets
	the link register if the instruction has one. Used when an emulated
	instruction sits in the delay slot of that branch.
	Returns false for an instruction that is not a branch or jump.
*/
static bool
emulate_branch(iframe* frame, addr_t pc, uint64* _nextPC)
{
	uint32 instruction = *(uint32*)pc;
	uint32 opcode = instruction >> 26;
	uint32 rs = (instruction >> 21) & 0x1f;
	uint32 rt = (instruction >> 16) & 0x1f;
	int64 a = (int64)frame->r[rs];
	int64 b = (int64)frame->r[rt];
	if (rs == 0)
		a = 0;
	if (rt == 0)
		b = 0;

	uint64 fallThrough = mips_sign_extend(pc + 8);
	uint64 branchTarget = mips_sign_extend(
		pc + 4 + ((int32)(int16)(instruction & 0xffff)) * 4);
	bool taken;

	switch (opcode) {
		case 0x00:	// special: jr, jalr
		{
			uint32 function = instruction & 0x3f;
			if (function != 0x08 && function != 0x09)
				return false;
			*_nextPC = (uint64)a;
			if (function == 0x09) {
				uint32 rd = (instruction >> 11) & 0x1f;
				if (rd != 0)
					frame->r[rd] = fallThrough;
			}
			return true;
		}

		case 0x01:	// regimm: bltz, bgez and their linking and likely forms
			switch (rt & 0x0f) {
				case 0x00:
				case 0x02:
					taken = a < 0;
					break;
				case 0x01:
				case 0x03:
					taken = a >= 0;
					break;
				default:
					return false;
			}
			if ((rt & 0x10) != 0)
				frame->r[MIPS_REG_RA] = fallThrough;
			break;

		case 0x02:	// j
		case 0x03:	// jal
			*_nextPC = mips_sign_extend(((pc + 4) & 0xf0000000)
				| ((instruction & 0x03ffffff) << 2));
			if (opcode == 0x03)
				frame->r[MIPS_REG_RA] = fallThrough;
			return true;

		case 0x04:	// beq
		case 0x14:	// beql
			taken = a == b;
			break;
		case 0x05:	// bne
		case 0x15:	// bnel
			taken = a != b;
			break;
		case 0x06:	// blez
		case 0x16:	// blezl
			taken = a <= 0;
			break;
		case 0x07:	// bgtz
		case 0x17:	// bgtzl
			taken = a > 0;
			break;

		default:
			return false;
	}

	// The delay slot of a "likely" branch runs only if the branch is taken,
	// so a fault there means it is.
	*_nextPC = taken ? branchTarget : fallThrough;
	return true;
}


static bool
emulate_unaligned_access(iframe* frame, addr_t address)
{
	// In a delay slot the faulting instruction follows the branch at epc.
	bool inDelaySlot = (frame->cause & CP0_CAUSE_BD) != 0;
	addr_t pc = (addr_t)frame->epc + (inDelaySlot ? 4 : 0);
	bool isUser = is_user_frame(frame);

	// The instruction ran a moment ago, so its page is mapped. The data of a
	// user access can be paged out or missing: user_memcpy() takes the page
	// fault with interrupts enabled.
	uint32 instruction = *(uint32*)pc;
	uint32 opcode = instruction >> 26;
	uint32 target = (instruction >> 16) & 0x1f;
	uint8 memory[8];

	size_t size;
	bool isStore = false;
	bool isSigned = true;
	switch (opcode) {
		case 0x21:	// lh
			size = 2;
			break;
		case 0x25:	// lhu
			size = 2;
			isSigned = false;
			break;
		case 0x23:	// lw
			size = 4;
			break;
		case 0x27:	// lwu
			size = 4;
			isSigned = false;
			break;
		case 0x37:	// ld
			size = 8;
			break;
		case 0x29:	// sh
			size = 2;
			isStore = true;
			break;
		case 0x2b:	// sw
			size = 4;
			isStore = true;
			break;
		case 0x3f:	// sd
			size = 8;
			isStore = true;
			break;
		default:
			return false;
	}

	// The branch decides before its delay slot runs, so evaluate it before
	// the load changes a register.
	uint64 nextPC = frame->epc + 4;
	if (inDelaySlot && !emulate_branch(frame, (addr_t)frame->epc, &nextPC))
		return false;

	if (isUser) {
		if (!IS_USER_ADDRESS(address) || !IS_USER_ADDRESS(address + size - 1))
			return false;
		enable_interrupts();
	}

	// big-endian: the most significant byte is first
	if (isStore) {
		uint64 value = frame->r[target];
		for (size_t i = 0; i < size; i++)
			memory[i] = (uint8)(value >> ((size - 1 - i) * 8));
		if (!isUser)
			memcpy((void*)address, memory, size);
		else if (user_memcpy((void*)address, memory, size) != B_OK)
			return false;
	} else {
		if (!isUser)
			memcpy(memory, (void*)address, size);
		else if (user_memcpy(memory, (void*)address, size) != B_OK)
			return false;

		uint64 value = 0;
		for (size_t i = 0; i < size; i++)
			value = (value << 8) | memory[i];
		if (isSigned && size == 2)
			value = (uint64)(int64)(int16)value;
		else if (isSigned && size == 4)
			value = (uint64)(int64)(int32)value;
		if (target != 0)
			frame->r[target] = value;
	}

	frame->epc = nextPC;
	return true;
}


//	#pragma mark - syscalls


static void
handle_syscall(iframe* frame)
{
	frame->epc += 4;

	uint64 syscall = frame->r[MIPS_REG_V0];
	uint64 args[MAX_SYSCALL_PARAMETERS];
	bool extend = true;

	if (syscall < (uint64)kSyscallCount) {
		// Every argument is one 64 bit slot. The first eight are in $4 to
		// $11, the rest on the user stack.
		uint32 count = kExtendedSyscallInfos[syscall].parameter_count;
		memcpy(&args[0], &frame->r[MIPS_REG_A0],
			sizeof(uint64) * std::min<uint32>(count, 8));
		if (count > 8) {
			status_t status = user_memcpy(&args[8],
				(void*)(addr_t)frame->r[MIPS_REG_SP],
				sizeof(uint64) * (count - 8));
			if (status < B_OK) {
				frame->r[MIPS_REG_V0] = (uint64)(int64)status;
				return;
			}
		}
		extend = kExtendedSyscallInfos[syscall].return_type.size < 8;
	}

	enable_interrupts();

	uint64 returnValue = 0;
	syscall_dispatcher(syscall, (void*)args, &returnValue);

	// A result narrower than 64 bits is returned sign-extended.
	if (extend)
		returnValue = mips_sign_extend((uint32)returnValue);
	frame->r[MIPS_REG_V0] = returnValue;
}


//	#pragma mark - exception dispatch


void
mips_handle_exception(iframe* frame)
{
	uint32 code = exception_code(frame);
	bool isUser = is_user_frame(frame);

	// An address outside the 32 bit address space. User threads run with
	// 64 bit addressing, so they can produce one.
	bool validAddress
		= (uint64)(int64)(int32)frame->badvaddr == frame->badvaddr;
	addr_t address = (addr_t)frame->badvaddr;

	switch (code) {
		case MIPS_EXCEPTION_TLB_MODIFIED:
		case MIPS_EXCEPTION_TLB_LOAD:
		case MIPS_EXCEPTION_TLB_STORE:
			if (validAddress && handle_tlb_fixup(address,
					code != MIPS_EXCEPTION_TLB_LOAD)) {
				return;
			}
			break;
	}

	Thread* thread = thread_get_current_thread();

	if (isUser) {
		thread->arch_info.userFrame = frame;
		thread->arch_info.oldV0 = frame->r[MIPS_REG_V0];
		thread_at_kernel_entry(system_time());
	}

	const auto& kernelExit = ScopeExit([&]() {
		if (!isUser)
			return;

		disable_interrupts();
		atomic_and(&thread->flags, ~THREAD_FLAGS_SYSCALL_RESTARTED);
		if ((thread->flags & (THREAD_FLAGS_SIGNALS_PENDING
				| THREAD_FLAGS_DEBUG_THREAD
				| THREAD_FLAGS_TRAP_FOR_CORE_DUMP)) != 0) {
			enable_interrupts();
			thread_at_kernel_exit();
		} else
			thread_at_kernel_exit_no_signals();

		if ((thread->flags & THREAD_FLAGS_RESTART_SYSCALL) != 0) {
			atomic_and(&thread->flags, ~THREAD_FLAGS_RESTART_SYSCALL);
			atomic_or(&thread->flags, THREAD_FLAGS_SYSCALL_RESTARTED);

			frame->r[MIPS_REG_V0] = thread->arch_info.oldV0;
			frame->epc -= 4;
		}
		thread->arch_info.userFrame = NULL;

		// Interrupts are off from here to the return to user mode. The
		// thread may use the FPU only while the FPU holds its state.
		frame->status &= ~(CP0_STATUS_CU1 | CP0_STATUS_FR);
		if (mips_fpu_is_owner(thread))
			frame->status |= CP0_STATUS_CU1 | CP0_STATUS_FR;
	});

	switch (code) {
		case MIPS_EXCEPTION_INTERRUPT:
			handle_interrupt(frame);
			return;

		case MIPS_EXCEPTION_TLB_MODIFIED:
		case MIPS_EXCEPTION_TLB_LOAD:
		case MIPS_EXCEPTION_TLB_STORE:
			if (!validAddress) {
				send_signal(frame, B_SEGMENT_VIOLATION, SIGSEGV, SEGV_MAPERR,
					address);
				return;
			}
			handle_page_fault(frame, address,
				code != MIPS_EXCEPTION_TLB_LOAD);
			return;

		case MIPS_EXCEPTION_ADDRESS_LOAD:
		case MIPS_EXCEPTION_ADDRESS_STORE:
			// Haiku's userland reads unaligned fields, for example the item
			// sizes of a BMessage, so the kernel emulates the access.
			if (validAddress && IS_KERNEL_ADDRESS(address) == !isUser
				&& address != (addr_t)frame->epc
				&& emulate_unaligned_access(frame, address)) {
				return;
			}
			if (!isUser && call_debugger_fault_handler(frame, address,
					code == MIPS_EXCEPTION_ADDRESS_STORE)) {
				return;
			}
			if (!isUser && thread != NULL && thread->fault_handler != NULL) {
				// A bad user pointer that the kernel dereferenced.
				frame->epc = mips_sign_extend((addr_t)thread->fault_handler);
				frame->r[MIPS_REG_T9] = frame->epc;
				return;
			}
			if (validAddress && (address & 0x80000000) == 0) {
				send_signal(frame, B_ALIGNMENT_EXCEPTION, SIGBUS, BUS_ADRALN,
					address);
			} else {
				// A user access to a kernel address.
				send_signal(frame, B_SEGMENT_VIOLATION, SIGSEGV, SEGV_MAPERR,
					address);
			}
			return;

		case MIPS_EXCEPTION_BUS_INSTRUCTION:
		case MIPS_EXCEPTION_BUS_DATA:
			// The address of a bus error is not known.
			if (!isUser && call_debugger_fault_handler(frame, 0, false))
				return;
			if (!isUser && thread != NULL && thread->fault_handler != NULL) {
				// A probe of a device that is not there
				// (arch_cpu_probe_read32()).
				frame->epc = mips_sign_extend((addr_t)thread->fault_handler);
				frame->r[MIPS_REG_T9] = frame->epc;
				return;
			}
			send_signal(frame, B_SEGMENT_VIOLATION, SIGBUS, BUS_ADRERR,
				(addr_t)frame->epc);
			return;

		case MIPS_EXCEPTION_SYSCALL:
			if (!isUser)
				break;
			handle_syscall(frame);
			return;

		case MIPS_EXCEPTION_BREAKPOINT:
			if (isUser) {
				dprintf("thread %" B_PRId32 " (%s): break instruction\n",
					thread_get_current_thread()->id,
					thread_get_current_thread()->name);
				dump_frame(frame);
				enable_interrupts();
				user_debug_breakpoint_hit(true);
			} else {
				// An attached GDB gets the breakpoint. It moves the pc
				// itself when it resumes the kernel.
				uint64 pc = frame->epc;
				iframe* previous = mips_debug_set_frame(frame);
				if (!debug_gdb_trap(GDB_SIGNAL_TRAP)) {
					dump_frame(frame);
					panic("kernel breakpoint");
					if (frame->epc == pc)
						frame->epc += 4;
				}
				mips_debug_set_frame(previous);
			}
			return;

		case MIPS_EXCEPTION_RESERVED_INSTRUCTION:
			if (isUser && emulate_rdhwr(frame))
				return;
			send_signal(frame, B_INVALID_OPCODE_EXCEPTION, SIGILL, ILL_ILLOPC,
				(addr_t)frame->epc);
			return;

		case MIPS_EXCEPTION_COPROCESSOR_UNUSABLE:
		{
			uint32 coprocessor = (frame->cause & CP0_CAUSE_CE_MASK)
				>> CP0_CAUSE_CE_SHIFT;
			if (isUser && coprocessor == 1 && mips_fpu_handle_unusable())
				return;
			send_signal(frame, B_INVALID_OPCODE_EXCEPTION, SIGILL, ILL_COPROC,
				(addr_t)frame->epc);
			return;
		}

		case MIPS_EXCEPTION_OVERFLOW:
			send_signal(frame, B_OVERFLOW_EXCEPTION, SIGFPE,
				FPE_INTOVF, (addr_t)frame->epc);
			return;

		case MIPS_EXCEPTION_TRAP:
			// Compilers emit a conditional trap for a division by zero.
			send_signal(frame, B_DIVIDE_ERROR, SIGFPE, FPE_INTDIV,
				(addr_t)frame->epc);
			return;

		case MIPS_EXCEPTION_FPU:
		{
			uint32 fcsr = 0;
			int32 signalCode = FPE_FLTINV;
			if (isUser) {
				mips_fpu_get_fcsr(&fcsr);
				if ((fcsr & FCSR_CAUSE_DIVBYZERO) != 0)
					signalCode = FPE_FLTDIV;
				else if ((fcsr & FCSR_CAUSE_OVERFLOW) != 0)
					signalCode = FPE_FLTOVF;
				else if ((fcsr & FCSR_CAUSE_UNDERFLOW) != 0)
					signalCode = FPE_FLTUND;
				else if ((fcsr & FCSR_CAUSE_INEXACT) != 0)
					signalCode = FPE_FLTRES;
			}
			send_signal(frame, B_FLOATING_POINT_EXCEPTION, SIGFPE, signalCode,
				(addr_t)frame->epc);
			return;
		}
	}

	if (isUser) {
		send_signal(frame, B_INVALID_OPCODE_EXCEPTION, SIGILL, ILL_ILLOPC,
			(addr_t)frame->epc);
		return;
	}

	char message[64];
	snprintf(message, sizeof(message), "unhandled exception %" B_PRIu32
		" in kernel mode", code);
	kernel_exception_panic(frame, GDB_SIGNAL_ABRT, message);
}


void
mips_double_fault_handler(uint64 epc, uint64 badvaddr, uint32 cause,
	addr_t stackPointer)
{
	gMipsExceptionBlock->in_entry = 0;
	panic("exception in the exception entry code: epc %#" B_PRIx64
		", badvaddr %#" B_PRIx64 ", cause %#" B_PRIx32 ", sp %#" B_PRIxADDR
		" (kernel stack overflow?)", epc, badvaddr, cause, stackPointer);
}


void
mips_cache_error_handler(void)
{
	panic("cache error exception");
}


//	#pragma mark -


static void
install_vector(addr_t address, uint32* start, uint32* end)
{
	size_t size = (addr_t)end - (addr_t)start;
	if (size > MIPS_EXCEPTION_VECTOR_SIZE)
		panic("exception vector code too large");

	memcpy((void*)address, start, size);
	mips_dcache_writeback_invalidate(address, size);
	mips_icache_invalidate(address, size);
}


status_t
arch_int_init(kernel_args* args)
{
	// mask every INT3 input
	indy_write8(INDY_INT3_L0_MASK, 0);
	indy_write8(INDY_INT3_L1_MASK, 0);
	indy_write8(INDY_INT3_MAP_MASK0, 0);
	indy_write8(INDY_INT3_MAP_MASK1, 0);

	gMipsExceptionBlock->in_entry = 0;
	gMipsExceptionBlock->fault_count = 0;

	install_vector(MIPS_EXCEPTION_VECTOR_TLB_REFILL, mips_tlb_refill_vector,
		mips_tlb_refill_vector_end);
	install_vector(MIPS_EXCEPTION_VECTOR_XTLB_REFILL, mips_xtlb_refill_vector,
		mips_xtlb_refill_vector_end);
	install_vector(MIPS_EXCEPTION_VECTOR_GENERAL, mips_general_vector,
		mips_general_vector_end);
	// The cache error vector is in kseg1. The same memory is at 0x80000100.
	install_vector(MIPS_EXCEPTION_VECTOR_CACHE_ERROR - KSEG1_BASE + KSEG0_BASE,
		mips_cache_error_vector, mips_cache_error_vector_end);

	// Vectors in RAM, kernel mode, interrupts off. Unmask the INT3 lines
	// and the timer. Cache parity exceptions stay off.
	mips_write_status(CP0_STATUS_CU0 | CP0_STATUS_DE | CP0_STATUS_IM2
		| CP0_STATUS_IM3 | CP0_STATUS_IM7);

	reserve_io_interrupt_vectors(NUM_IO_VECTORS, 0, INTERRUPT_TYPE_IRQ);

	return B_OK;
}


status_t
arch_int_init_post_vm(kernel_args* args)
{
	return B_OK;
}


status_t
arch_int_init_io(kernel_args* args)
{
	return B_OK;
}


status_t
arch_int_init_post_device_manager(struct kernel_args* args)
{
	return B_OK;
}


/*!	Maps a vector to its INT3 mask register and bit. Vectors 1 and 12 are
	aliases of the keyboard and mouse input.
*/
static bool
get_mask_register(int32 irq, uint32& _register, uint8& _bit)
{
	if (irq == 1 || irq == 12)
		irq = INDY_IRQ_KEYBOARD_MOUSE;

	if (irq >= INDY_IRQ_LOCAL0(0) && irq < INDY_IRQ_LOCAL0(8)) {
		_register = INDY_INT3_L0_MASK;
		_bit = 1 << (irq - INDY_IRQ_LOCAL0(0));
		return true;
	}
	if (irq >= INDY_IRQ_LOCAL1(0) && irq < INDY_IRQ_LOCAL1(8)) {
		_register = INDY_INT3_L1_MASK;
		_bit = 1 << (irq - INDY_IRQ_LOCAL1(0));
		return true;
	}
	if (irq >= INDY_IRQ_MAPPED(0) && irq < INDY_IRQ_MAPPED(8)) {
		// The mapped inputs are routed to local 0 through mask 0.
		_register = INDY_INT3_MAP_MASK0;
		_bit = 1 << (irq - INDY_IRQ_MAPPED(0));
		return true;
	}
	return false;
}


void
arch_int_enable_io_interrupt(int32 irq)
{
	uint32 maskRegister;
	uint8 bit;
	if (!get_mask_register(irq, maskRegister, bit))
		return;

	InterruptsLocker locker;
	indy_write8(maskRegister, indy_read8(maskRegister) | bit);
	if (maskRegister == INDY_INT3_MAP_MASK0) {
		indy_write8(INDY_INT3_L0_MASK, indy_read8(INDY_INT3_L0_MASK)
			| INDY_INT3_LOCAL0_CASCADE);
	}
}


void
arch_int_disable_io_interrupt(int32 irq)
{
	uint32 maskRegister;
	uint8 bit;
	if (!get_mask_register(irq, maskRegister, bit))
		return;

	InterruptsLocker locker;
	indy_write8(maskRegister, indy_read8(maskRegister) & ~bit);
}


void
arch_int_configure_io_interrupt(int32 irq, interrupt_trigger_mode mode,
	interrupt_trigger_polarity polarity)
{
}


int32
arch_int_assign_to_cpu(int32 irq, int32 cpu)
{
	return 0;
}


#undef arch_int_enable_interrupts
#undef arch_int_disable_interrupts
#undef arch_int_restore_interrupts
#undef arch_int_are_interrupts_enabled


extern "C" void
arch_int_enable_interrupts()
{
	arch_int_enable_interrupts_inline();
}


extern "C" int
arch_int_disable_interrupts()
{
	return arch_int_disable_interrupts_inline();
}


extern "C" void
arch_int_restore_interrupts(int oldState)
{
	arch_int_restore_interrupts_inline(oldState);
}


extern "C" bool
arch_int_are_interrupts_enabled()
{
	return arch_int_are_interrupts_enabled_inline();
}
