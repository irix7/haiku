/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/debug.h>

#include <setjmp.h>
#include <stdio.h>
#include <string.h>

#include <arch/cpu.h>
#include <arch_cpu_defs.h>
#include <arch_exception_block.h>
#include <arch_vm_translation_map.h>
#include <cpu.h>
#include <debug.h>
#include <elf.h>
#include <kernel.h>
#include <team.h>
#include <thread.h>
#include <vm/vm.h>
#include <vm/vm_priv.h>
#include <vm/VMAddressSpace.h>
#include <vm/VMTranslationMap.h>


/*
 * Stack traces
 *
 * MIPS code has no frame pointer chain: each function has its own frame
 * layout. The tracer scans the stack for values that are return addresses,
 * that is, addresses of kernel code that follow a call instruction. A stale
 * return address in an unused stack slot shows up as an extra entry.
 * Stack slots are 64 bits wide (n32).
 */




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


/*!	Where a fault in the kernel debugger continues: handle_page_fault() and
	its like pass the jump buffer of the mips_debug_fault_link.
*/
static void
fault_landing(void* jumpBuffer)
{
	longjmp(*(jmp_buf*)jumpBuffer, 1);
}


void
arch_debug_call_with_fault_handler(cpu_ent* cpu, jmp_buf jumpBuffer,
	void (*function)(void*), void* parameter)
{
	// The link lives in this stack frame for as long as the function runs.
	// debug_call_with_fault_handler() restores the pointer to the link of
	// an outer call, so each fault finds the jump buffer of the innermost
	// call that is still running.
	mips_debug_fault_link link;
	link.jump_buffer = jumpBuffer;

	cpu->fault_handler = (addr_t)&fault_landing;
	cpu->fault_handler_stack_pointer = (addr_t)&link;

	function(parameter);
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


//	#pragma mark - GDB stub


/*
 * The registers of GDB's MIPS target, in the order of its "g" packet. All
 * are 64 bits wide. The 32 FPU registers, fcsr and fir follow at 38: the
 * kernel does not use the FPU, and the stub reports them as unavailable.
 */
enum {
	GDB_REGISTER_STATUS = 32,
	GDB_REGISTER_LO,
	GDB_REGISTER_HI,
	GDB_REGISTER_BADVADDR,
	GDB_REGISTER_CAUSE,
	GDB_REGISTER_PC,
	GDB_REGISTER_COUNT,
	GDB_REGISTER_LIMIT = 90
};

static const size_t kRegisterDigits = 16;

// "break 5", the instruction GDB uses for its own breakpoints
static const uint32 kBreakpointInstruction = 0x0005000d;

// The callee-saved registers as arch_context and jmp_buf hold them:
// $16 to $23, $28 (gp), $29 (sp), $30 (fp), $31 (ra).
static const int32 kContextRegisterCount = 12;
static const uint8 kContextRegisters[kContextRegisterCount]
	= { 16, 17, 18, 19, 20, 21, 22, 23, 28, 29, 30, 31 };

static_assert(sizeof(arch_context) == kContextRegisterCount * sizeof(uint64),
	"arch_context layout");
static_assert(sizeof(jmp_buf) >= kContextRegisterCount * sizeof(uint64),
	"jmp_buf layout");

static iframe* sDebugFrame;
static uint64 sEntryContext[kContextRegisterCount];


/*!	Sets the frame of the exception that is about to enter the kernel
	debugger, and returns the previous one. GDB reads and writes the registers
	of this frame. The caller sets the previous frame again when the debugger
	has returned.
*/
iframe*
mips_debug_set_frame(iframe* frame)
{
	iframe* previous = sDebugFrame;
	sDebugFrame = frame;
	return previous;
}


/*!	Enters the kernel debugger if a break was requested through the debug
	console. Called at the end of every interrupt.
*/
void
mips_debug_check_break(iframe* frame)
{
	bool requested = mips_debug_net_take_break_request();
	if (!requested)
		requested = mips_debug_serial_break_requested();
	if (!requested || debug_debugger_running())
		return;

	iframe* previous = mips_debug_set_frame(frame);
	if (!debug_gdb_trap(GDB_SIGNAL_INT))
		debug_emergency_key_pressed('d');
	mips_debug_set_frame(previous);
}


/*!	Returns the exception frame of the thread GDB has selected: NULL if the
	debugger was not entered through an exception or GDB has selected a
	thread that is not running.
*/
static iframe*
gdb_frame()
{
	if (debug_get_debugged_thread() != thread_get_current_thread())
		return NULL;
	return sDebugFrame;
}


static void
get_gdb_registers(uint64* registers)
{
	memset(registers, 0, GDB_REGISTER_COUNT * sizeof(uint64));

	iframe* frame = gdb_frame();
	if (frame != NULL) {
		for (int32 i = 1; i < 32; i++)
			registers[i] = frame->r[i];
		registers[GDB_REGISTER_STATUS] = mips_sign_extend(frame->status);
		registers[GDB_REGISTER_LO] = frame->lo;
		registers[GDB_REGISTER_HI] = frame->hi;
		registers[GDB_REGISTER_BADVADDR] = frame->badvaddr;
		registers[GDB_REGISTER_CAUSE] = mips_sign_extend(frame->cause);
		registers[GDB_REGISTER_PC] = frame->epc;
		return;
	}

	// Without a frame only the callee-saved registers are known: those the
	// thread was switched away with, or those at the start of the session.
	const uint64* context = sEntryContext;
	Thread* thread = debug_get_debugged_thread();
	if (thread != NULL && thread != thread_get_current_thread())
		context = (const uint64*)&thread->arch_info.context;

	for (int32 i = 0; i < kContextRegisterCount; i++)
		registers[kContextRegisters[i]] = context[i];
	registers[GDB_REGISTER_PC] = registers[MIPS_REG_RA];
}


static void
set_gdb_register(iframe* frame, int32 index, uint64 value)
{
	// $0 is constant, and the exception return code uses $26 and $27.
	if (index >= 1 && index < 32) {
		if (index != MIPS_REG_K0 && index != MIPS_REG_K1)
			frame->r[index] = value;
	} else if (index == GDB_REGISTER_LO)
		frame->lo = value;
	else if (index == GDB_REGISTER_HI)
		frame->hi = value;
	else if (index == GDB_REGISTER_PC)
		frame->epc = value;
}


//!	Returns false for a register that GDB sent as unavailable ("xxxx").
static bool
parse_gdb_register(const char* digits, uint64& _value)
{
	uint64 value = 0;
	for (size_t i = 0; i < kRegisterDigits; i++) {
		char c = digits[i];
		uint32 nibble;
		if (c >= '0' && c <= '9')
			nibble = c - '0';
		else if (c >= 'a' && c <= 'f')
			nibble = c - 'a' + 10;
		else if (c >= 'A' && c <= 'F')
			nibble = c - 'A' + 10;
		else
			return false;
		value = (value << 4) | nibble;
	}

	_value = value;
	return true;
}


ssize_t
arch_debug_gdb_get_registers(char* buffer, size_t bufferSize)
{
	if (bufferSize < GDB_REGISTER_COUNT * kRegisterDigits + 1)
		return B_BUFFER_OVERFLOW;

	uint64 registers[GDB_REGISTER_COUNT];
	get_gdb_registers(registers);

	for (int32 i = 0; i < GDB_REGISTER_COUNT; i++) {
		snprintf(buffer + i * kRegisterDigits, kRegisterDigits + 1,
			"%016" B_PRIx64, registers[i]);
	}
	return GDB_REGISTER_COUNT * kRegisterDigits;
}


status_t
arch_debug_gdb_set_registers(const char* buffer, size_t length)
{
	iframe* frame = gdb_frame();
	if (frame == NULL)
		return B_NOT_ALLOWED;
	if (length < GDB_REGISTER_COUNT * kRegisterDigits)
		return B_BAD_VALUE;

	for (int32 i = 0; i < GDB_REGISTER_COUNT; i++) {
		uint64 value;
		if (parse_gdb_register(buffer + i * kRegisterDigits, value))
			set_gdb_register(frame, i, value);
	}
	return B_OK;
}


ssize_t
arch_debug_gdb_get_register(int32 index, char* buffer, size_t bufferSize)
{
	if (index < 0 || index >= GDB_REGISTER_LIMIT
		|| bufferSize < kRegisterDigits + 1) {
		return B_BAD_VALUE;
	}

	if (index >= GDB_REGISTER_COUNT) {
		memset(buffer, 'x', kRegisterDigits);
		return kRegisterDigits;
	}

	uint64 registers[GDB_REGISTER_COUNT];
	get_gdb_registers(registers);
	snprintf(buffer, kRegisterDigits + 1, "%016" B_PRIx64, registers[index]);
	return kRegisterDigits;
}


status_t
arch_debug_gdb_set_register(int32 index, const char* buffer, size_t length)
{
	iframe* frame = gdb_frame();
	if (frame == NULL)
		return B_NOT_ALLOWED;

	uint64 value;
	if (index < 0 || index >= GDB_REGISTER_COUNT || length < kRegisterDigits
		|| !parse_gdb_register(buffer, value)) {
		return B_BAD_VALUE;
	}

	set_gdb_register(frame, index, value);
	return B_OK;
}


void
arch_debug_gdb_set_entry_context(jmp_buf context)
{
	memcpy(sEntryContext, context, sizeof(sEntryContext));
}


ssize_t
arch_debug_gdb_get_breakpoint_instruction(addr_t address, int32 kind,
	void* buffer, size_t bufferSize)
{
	if ((address & 3) != 0 || bufferSize < sizeof(kBreakpointInstruction))
		return B_BAD_VALUE;

	memcpy(buffer, &kBreakpointInstruction, sizeof(kBreakpointInstruction));
	return sizeof(kBreakpointInstruction);
}


/*!	Makes the caches consistent with code the stub has written. The cache
	operations take kseg0 addresses, so a mapped page is flushed through its
	physical address.
*/
void
arch_debug_gdb_flush_code(addr_t address, size_t size)
{
	while (size > 0) {
		size_t offset = address % B_PAGE_SIZE;
		size_t length = MIN(size, B_PAGE_SIZE - offset);
		addr_t alias = 0;

		if (mips_is_kseg0(address)) {
			alias = address;
		} else if (!mips_is_kseg1(address)) {
			// Memory in kseg1 is not cached.
			VMAddressSpace* addressSpace = NULL;
			Thread* thread = debug_get_debugged_thread();
			if (IS_KERNEL_ADDRESS(address))
				addressSpace = VMAddressSpace::Kernel();
			else if (thread != NULL && thread->team != NULL)
				addressSpace = thread->team->address_space;

			phys_addr_t physical;
			uint32 flags;
			if (addressSpace != NULL
				&& addressSpace->TranslationMap()->QueryInterrupt(address,
					&physical, &flags) == B_OK
				&& (flags & PAGE_PRESENT) != 0) {
				alias = (addr_t)mips_physical_to_kseg0(
					ROUNDDOWN(physical, B_PAGE_SIZE) + offset);
			}
		}

		if (alias != 0) {
			mips_dcache_writeback_invalidate(alias, length);
			mips_icache_invalidate(alias, length);
		}

		address += length;
		size -= length;
	}
}


static inline bool
is_break_instruction(uint32 instruction)
{
	return (instruction & 0xfc00003f) == 0x0000000d;
}


/*!	Prepares the exception frame for the return to the kernel. A break
	instruction at the pc is skipped: the breakpoints of the stub are not in
	memory at this point, so the instruction is part of the code and would
	trap again.
*/
status_t
arch_debug_gdb_resume(bool hasAddress, addr_t address)
{
	iframe* frame = sDebugFrame;
	if (frame == NULL)
		return hasAddress ? B_NOT_SUPPORTED : B_OK;

	if (hasAddress)
		frame->epc = mips_sign_extend(address);

	uint32 instruction;
	if (debug_memcpy(B_CURRENT_TEAM, &instruction, (void*)(addr_t)frame->epc,
			sizeof(instruction)) == B_OK
		&& is_break_instruction(instruction)) {
		frame->epc += 4;
	}

	return B_OK;
}


/*!	Returns the addresses the CPU can be at after the instruction at the pc
	of the exception frame. The CPU has no single step mode: the stub puts a
	breakpoint at each address. A branch and its delay slot count as one
	instruction, because a breakpoint in a delay slot cannot be resumed.
*/
ssize_t
arch_debug_gdb_get_step_targets(addr_t* targets, int32 maxCount)
{
	iframe* frame = sDebugFrame;
	if (frame == NULL || maxCount < 2)
		return B_NOT_SUPPORTED;

	addr_t pc = (addr_t)frame->epc;
	uint32 instruction;
	if (debug_memcpy(B_CURRENT_TEAM, &instruction, (void*)pc,
			sizeof(instruction)) != B_OK) {
		return B_BAD_ADDRESS;
	}

	uint32 opcode = instruction >> 26;
	uint32 rs = (instruction >> 21) & 0x1f;
	uint32 rt = (instruction >> 16) & 0x1f;
	uint32 function = instruction & 0x3f;
	addr_t branchTarget = pc + 4 + ((int32)(int16)(instruction & 0xffff)) * 4;

	bool isBranch = false;
	switch (opcode) {
		case 0x00:	// special: jr, jalr
			if (function == 0x08 || function == 0x09) {
				targets[0] = rs != 0 ? (addr_t)frame->r[rs] : 0;
				return 1;
			}
			break;

		case 0x01:	// regimm: bltz, bgez and their linking and likely forms
			isBranch = (rt & 0x0c) == 0;
			break;

		case 0x02:	// j
		case 0x03:	// jal
			targets[0] = ((pc + 4) & 0xf0000000)
				| ((instruction & 0x03ffffff) << 2);
			return 1;

		case 0x04:	// beq, bne, blez, bgtz
		case 0x05:
		case 0x06:
		case 0x07:
		case 0x14:	// their likely forms
		case 0x15:
		case 0x16:
		case 0x17:
			isBranch = true;
			break;

		case 0x11:	// cop1: bc1f, bc1t
			isBranch = rs == 0x08;
			break;
	}

	if (!isBranch) {
		targets[0] = pc + 4;
		return 1;
	}

	targets[0] = pc + 8;
	if (branchTarget == pc + 8)
		return 1;
	targets[1] = branchTarget;
	return 2;
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
