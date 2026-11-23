/*
 * Copyright 2005-2007, Axel Dörfler, axeld@pinc-software.de.
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * Copyright 2002, Manuel J. Petit. All rights reserved.
 * Distributed under the terms of the NewOS License.
 */


/*!	The stub for a remote GDB.

	The stub runs inside the kernel debugger and talks through the debug
	console of the architecture (arch_debug_gdb_getchar() and friends).
	"gdb" at the KDL prompt, or a '$' or '+' as the first character of a
	line, starts a session.

	A session stays attached when GDB resumes the kernel. The next entry into
	the kernel debugger then reports a stop to GDB and does not show the
	prompt. "detach" in GDB ends the session and resumes the kernel. "kill"
	ends it and shows the KDL prompt.

	While a session is attached:
	- Breakpoints are in memory only while the kernel runs. The kernel
	  debugger and the stub run on unmodified code.
	- Kernel debug output is sent to GDB as console output while the kernel
	  runs, and is dropped while the stub talks to GDB.
*/


#include "gdb.h"

#include <setjmp.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include <algorithm>

#include <arch/debug.h>
#include <arch/debug_console.h>
#include <debug.h>
#include <elf.h>
#include <elf_priv.h>
#include <kernel.h>
#include <team.h>
#include <thread.h>
#include <thread_types.h>


// what the session does after a packet
enum {
	SESSION_CONTINUE = 0,
	SESSION_RESUME,		// leave the kernel debugger
	SESSION_PROMPT		// show the KDL prompt
};

static const size_t kBufferSize = 2048;
static const size_t kPacketSize = 1024;
	// The size GDB is told. A reply is at most twice kMaxMemorySize long.
static const size_t kMaxMemorySize = 480;
static const int32 kMaxBreakpoints = 64;
static const int32 kMaxStepTargets = 2;
static const size_t kMaxBreakpointSize = 8;
static const int32 kMaxSendTries = 5;

struct Breakpoint {
	addr_t	address;
	uint8	size;			// 0: the slot is free
	bool	temporary;		// set for a single step
	bool	inserted;
	uint8	instruction[kMaxBreakpointSize];
	uint8	saved[kMaxBreakpointSize];
};

struct ThreadList {
	int32	first;
	int32	index;
	int32	count;
	size_t	length;
};


static char sCommand[kBufferSize];
static char sReply[kBufferSize];
static uint8 sMemory[kMaxMemorySize];

static bool sAttached;
static bool sInSession;
static bool sPacketStarted;
	// GDB sent the '$' of its next packet in place of an acknowledgement
static int32 sStopSignal;
	// the reason for the next stop, 0 if nobody has set one
static int32 sLastSignal = GDB_SIGNAL_TRAP;
static int32 sThreadListIndex;
static jmp_buf sEntryContext;
static Breakpoint sBreakpoints[kMaxBreakpoints + kMaxStepTargets];

static const char kHexDigits[] = "0123456789abcdef";


//	#pragma mark - hooks an architecture can leave out


#define WEAK __attribute__((weak))


WEAK char
arch_debug_gdb_getchar(void)
{
	return arch_debug_serial_getchar();
}


WEAK void
arch_debug_gdb_putchar(char c)
{
	arch_debug_serial_putchar(c);
}


WEAK void
arch_debug_gdb_flush(void)
{
}


WEAK void
arch_debug_gdb_set_entry_context(jmp_buf context)
{
}


WEAK status_t
arch_debug_gdb_set_registers(const char* buffer, size_t length)
{
	return B_NOT_SUPPORTED;
}


WEAK ssize_t
arch_debug_gdb_get_register(int32 index, char* buffer, size_t bufferSize)
{
	return B_NOT_SUPPORTED;
}


WEAK status_t
arch_debug_gdb_set_register(int32 index, const char* buffer, size_t length)
{
	return B_NOT_SUPPORTED;
}


WEAK ssize_t
arch_debug_gdb_get_breakpoint_instruction(addr_t address, int32 kind,
	void* buffer, size_t bufferSize)
{
	return B_NOT_SUPPORTED;
}


WEAK void
arch_debug_gdb_flush_code(addr_t address, size_t size)
{
}


WEAK status_t
arch_debug_gdb_resume(bool hasAddress, addr_t address)
{
	return hasAddress ? B_NOT_SUPPORTED : B_OK;
}


WEAK ssize_t
arch_debug_gdb_get_step_targets(addr_t* targets, int32 maxCount)
{
	return B_NOT_SUPPORTED;
}


//	#pragma mark - utility functions


static int
parse_nibble(int input)
{
	if (input >= '0' && input <= '9')
		return input - '0';
	if (input >= 'A' && input <= 'F')
		return 0x0a + input - 'A';
	if (input >= 'a' && input <= 'f')
		return 0x0a + input - 'a';

	return -1;
}


/*!	Parses a hexadecimal number and moves \a string behind it.
	Returns false if there is no digit.
*/
static bool
parse_hex(const char*& string, uint64& _value)
{
	uint64 value = 0;
	int32 digits = 0;
	int nibble;
	while ((nibble = parse_nibble(*string)) >= 0) {
		value = (value << 4) | nibble;
		string++;
		digits++;
	}

	_value = value;
	return digits > 0;
}


static bool
parse_address(const char*& string, addr_t& _address)
{
	uint64 value;
	if (!parse_hex(string, value))
		return false;

	_address = (addr_t)value;
	return true;
}


//!	Parses a thread ID: a hexadecimal number or "-1".
static bool
parse_thread_id(const char*& string, thread_id& _id)
{
	if (string[0] == '-' && string[1] == '1') {
		string += 2;
		_id = -1;
		return true;
	}

	uint64 value;
	if (!parse_hex(string, value))
		return false;

	_id = (thread_id)value;
	return true;
}


static bool
skip_char(const char*& string, char c)
{
	if (*string != c)
		return false;

	string++;
	return true;
}


static bool
decode_hex(const char* string, uint8* buffer, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		int high = parse_nibble(string[2 * i]);
		if (high < 0)
			return false;
		int low = parse_nibble(string[2 * i + 1]);
		if (low < 0)
			return false;
		buffer[i] = (high << 4) | low;
	}

	return true;
}


static size_t
encode_hex(char* string, const void* _buffer, size_t size)
{
	const uint8* buffer = (const uint8*)_buffer;
	for (size_t i = 0; i < size; i++) {
		string[2 * i] = kHexDigits[buffer[i] >> 4];
		string[2 * i + 1] = kHexDigits[buffer[i] & 0x0f];
	}

	return 2 * size;
}


static uint8
read_char()
{
	return (uint8)arch_debug_gdb_getchar();
}


static void
put_hex_byte(uint8 value)
{
	arch_debug_gdb_putchar(kHexDigits[value >> 4]);
	arch_debug_gdb_putchar(kHexDigits[value & 0x0f]);
}


//	#pragma mark - packets


static void
send_frame(const char* data, size_t length)
{
	uint8 sum = 0;

	arch_debug_gdb_putchar('$');
	for (size_t i = 0; i < length; i++) {
		arch_debug_gdb_putchar(data[i]);
		sum += (uint8)data[i];
	}
	arch_debug_gdb_putchar('#');
	put_hex_byte(sum);
	arch_debug_gdb_flush();
}


//!	Sends a packet and waits for GDB to acknowledge it.
static void
send_packet(const char* data, size_t length)
{
	for (int32 tries = 0; tries < kMaxSendTries; tries++) {
		send_frame(data, length);

		while (true) {
			uint8 c = read_char();
			if (c == '+')
				return;
			if (c == '-')
				break;
			if (c == '$') {
				sPacketStarted = true;
				return;
			}
		}
	}
}


static void reply(const char* format, ...) __attribute__((format(printf, 1, 2)));

static void
reply(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	int length = vsnprintf(sReply, sizeof(sReply), format, args);
	va_end(args);

	send_packet(sReply, std::min((size_t)std::max(length, 0),
		sizeof(sReply) - 1));
}


//!	The answer to a packet the stub does not support.
static void
reply_empty()
{
	send_packet("", 0);
}


static void
reply_ok()
{
	send_packet("OK", 2);
}


static void
reply_error(int32 code)
{
	reply("E%02x", (unsigned int)code);
}


static void
reply_hex(const void* data, size_t size)
{
	size = std::min(size, (sizeof(sReply) - 1) / 2);
	send_packet(sReply, encode_hex(sReply, data, size));
}


/*!	Reads the next packet into sCommand, which it terminates with a null
	byte. Acknowledges the packet.
*/
static void
read_packet()
{
	while (true) {
		if (!sPacketStarted) {
			while (read_char() != '$')
				;
		}
		sPacketStarted = false;

		size_t length = 0;
		uint8 sum = 0;
		bool tooLong = false;

		while (true) {
			uint8 c = read_char();
			if (c == '$') {
				// the sender started again
				length = 0;
				sum = 0;
				tooLong = false;
				continue;
			}
			if (c == '#')
				break;

			sum += c;
			if (length < sizeof(sCommand) - 1)
				sCommand[length++] = c;
			else
				tooLong = true;
		}
		sCommand[length] = '\0';

		int high = parse_nibble(read_char());
		int low = parse_nibble(read_char());
		bool good = !tooLong && high >= 0 && low >= 0
			&& ((high << 4) | low) == sum;

		arch_debug_gdb_putchar(good ? '+' : '-');
		arch_debug_gdb_flush();
		if (good)
			return;
	}
}


//	#pragma mark - memory and breakpoints


static status_t
read_memory(addr_t address, void* buffer, size_t size)
{
	return debug_memcpy(B_CURRENT_TEAM, buffer, (void*)address, size);
}


static status_t
write_memory(addr_t address, const void* buffer, size_t size)
{
	status_t status = debug_memcpy(B_CURRENT_TEAM, (void*)address, buffer,
		size);
	if (status == B_OK)
		arch_debug_gdb_flush_code(address, size);

	return status;
}


static Breakpoint*
find_breakpoint(addr_t address)
{
	for (size_t i = 0; i < B_COUNT_OF(sBreakpoints); i++) {
		if (sBreakpoints[i].size != 0 && sBreakpoints[i].address == address)
			return &sBreakpoints[i];
	}

	return NULL;
}


static status_t
add_breakpoint(addr_t address, int32 kind, bool temporary)
{
	Breakpoint* breakpoint = find_breakpoint(address);
	if (breakpoint != NULL) {
		// A breakpoint of GDB replaces one of a single step.
		if (!temporary)
			breakpoint->temporary = false;
		return B_OK;
	}

	uint8 instruction[kMaxBreakpointSize];
	ssize_t size = arch_debug_gdb_get_breakpoint_instruction(address, kind,
		instruction, sizeof(instruction));
	if (size < 0)
		return size;
	if (size == 0 || (size_t)size > sizeof(instruction))
		return B_BAD_VALUE;

	uint8 current[kMaxBreakpointSize];
	if (read_memory(address, current, size) != B_OK)
		return B_BAD_ADDRESS;

	// The slots after kMaxBreakpoints are left for single steps.
	size_t count = temporary ? B_COUNT_OF(sBreakpoints) : kMaxBreakpoints;
	for (size_t i = 0; i < count; i++) {
		breakpoint = &sBreakpoints[i];
		if (breakpoint->size != 0)
			continue;

		breakpoint->address = address;
		breakpoint->size = size;
		breakpoint->temporary = temporary;
		breakpoint->inserted = false;
		memcpy(breakpoint->instruction, instruction, size);
		return B_OK;
	}

	return B_NO_MEMORY;
}


static void
remove_breakpoint(addr_t address)
{
	Breakpoint* breakpoint = find_breakpoint(address);
	if (breakpoint != NULL && !breakpoint->inserted)
		breakpoint->size = 0;
}


static void
remove_temporary_breakpoints()
{
	for (size_t i = 0; i < B_COUNT_OF(sBreakpoints); i++) {
		Breakpoint& breakpoint = sBreakpoints[i];
		if (breakpoint.temporary && !breakpoint.inserted)
			breakpoint.size = 0;
	}
}


//!	Writes the breakpoints into memory. Done when the kernel resumes.
static void
insert_breakpoints()
{
	for (size_t i = 0; i < B_COUNT_OF(sBreakpoints); i++) {
		Breakpoint& breakpoint = sBreakpoints[i];
		if (breakpoint.size == 0 || breakpoint.inserted)
			continue;

		if (read_memory(breakpoint.address, breakpoint.saved,
				breakpoint.size) != B_OK
			|| write_memory(breakpoint.address, breakpoint.instruction,
				breakpoint.size) != B_OK) {
			continue;
		}
		breakpoint.inserted = true;
	}
}


/*!	Restores the code under the breakpoints. Done when the kernel stops.
	The breakpoints of a single step are gone afterwards.
*/
static void
extract_breakpoints()
{
	for (size_t i = 0; i < B_COUNT_OF(sBreakpoints); i++) {
		Breakpoint& breakpoint = sBreakpoints[i];
		if (breakpoint.size == 0)
			continue;

		if (breakpoint.inserted) {
			write_memory(breakpoint.address, breakpoint.saved,
				breakpoint.size);
			breakpoint.inserted = false;
		}
		if (breakpoint.temporary)
			breakpoint.size = 0;
	}
}


static void
detach()
{
	memset(sBreakpoints, 0, sizeof(sBreakpoints));
	sAttached = false;
}


//	#pragma mark - threads


/*!	The ID GDB knows the running thread by. Early in the boot process there
	is no thread, and the stub reports one with the ID 1.
*/
static thread_id
current_thread_id()
{
	Thread* thread = thread_get_current_thread();
	return thread != NULL ? thread->id : 1;
}


/*!	Returns whether the thread exists. \a _thread is set to NULL for the
	running thread.
*/
static bool
find_thread(thread_id id, Thread*& _thread)
{
	_thread = NULL;
	if (id <= 0 || id == current_thread_id()
		|| thread_get_current_thread() == NULL) {
		return true;
	}

	_thread = Thread::GetDebug(id);
	return _thread != NULL;
}


static bool
add_thread_to_list(Thread* thread, void* _list)
{
	ThreadList* list = (ThreadList*)_list;
	if (list->index++ < list->first)
		return false;

	// an ID has at most eight digits
	if (list->length + 10 > kPacketSize)
		return true;

	list->length += snprintf(sReply + list->length,
		sizeof(sReply) - list->length, "%c%" B_PRIx32,
		list->count == 0 ? 'm' : ',', thread->id);
	list->count++;
	return false;
}


//!	Sends the next part of the thread list, "l" at its end.
static void
reply_thread_list()
{
	ThreadList list = { sThreadListIndex, 0, 0, 0 };

	if (thread_get_current_thread() == NULL) {
		if (sThreadListIndex == 0) {
			list.length = snprintf(sReply, sizeof(sReply), "m%" B_PRIx32,
				current_thread_id());
			list.count = 1;
		}
	} else
		thread_debug_map(&add_thread_to_list, &list);

	sThreadListIndex += list.count;

	if (list.count == 0)
		send_packet("l", 1);
	else
		send_packet(sReply, list.length);
}


static void
reply_thread_info(thread_id id)
{
	Thread* thread;
	if (!find_thread(id, thread)) {
		reply_error(1);
		return;
	}
	if (thread == NULL)
		thread = thread_get_current_thread();

	char info[B_OS_NAME_LENGTH + 32];
	if (thread != NULL) {
		snprintf(info, sizeof(info), "%s, team %" B_PRId32, thread->name,
			thread->team != NULL ? thread->team->id : -1);
	} else
		strlcpy(info, "kernel startup", sizeof(info));

	reply_hex(info, strlen(info));
}


static void
reply_stop()
{
	reply("T%02xthread:%" B_PRIx32 ";", (unsigned int)sLastSignal,
		current_thread_id());
}


//	#pragma mark - commands


static void
handle_read_registers()
{
	ssize_t length = arch_debug_gdb_get_registers(sReply, sizeof(sReply));
	if (length < 0)
		reply_error(1);
	else
		send_packet(sReply, length);
}


//!	"p n": reads one register.
static void
handle_read_register(const char* arguments)
{
	uint64 index;
	if (!parse_hex(arguments, index)) {
		reply_error(1);
		return;
	}

	ssize_t length = arch_debug_gdb_get_register(index, sReply,
		sizeof(sReply));
	if (length == B_NOT_SUPPORTED)
		reply_empty();
	else if (length < 0)
		reply_error(1);
	else
		send_packet(sReply, length);
}


//!	"P n=value": writes one register.
static void
handle_write_register(const char* arguments)
{
	uint64 index;
	if (!parse_hex(arguments, index) || !skip_char(arguments, '=')) {
		reply_error(1);
		return;
	}

	status_t status = arch_debug_gdb_set_register(index, arguments,
		strlen(arguments));
	if (status == B_NOT_SUPPORTED)
		reply_empty();
	else if (status != B_OK)
		reply_error(1);
	else
		reply_ok();
}


//!	"m address,size"
static void
handle_read_memory(const char* arguments)
{
	addr_t address;
	uint64 size;
	if (!parse_address(arguments, address) || !skip_char(arguments, ',')
		|| !parse_hex(arguments, size)) {
		reply_error(1);
		return;
	}

	// GDB may be reading through a stray pointer. debug_memcpy() survives
	// that.
	size = std::min(size, (uint64)sizeof(sMemory));
	if (read_memory(address, sMemory, size) != B_OK)
		reply_error(2);
	else
		reply_hex(sMemory, size);
}


//!	"M address,size:bytes"
static void
handle_write_memory(const char* arguments)
{
	addr_t address;
	uint64 size;
	if (!parse_address(arguments, address) || !skip_char(arguments, ',')
		|| !parse_hex(arguments, size) || !skip_char(arguments, ':')
		|| size > sizeof(sMemory) || strlen(arguments) < 2 * size
		|| !decode_hex(arguments, sMemory, size)) {
		reply_error(1);
		return;
	}

	if (write_memory(address, sMemory, size) != B_OK)
		reply_error(2);
	else
		reply_ok();
}


//!	"Z0,address,kind" and "z0,address,kind": software breakpoints.
static void
handle_breakpoint(const char* arguments, bool insert)
{
	if (!skip_char(arguments, '0')) {
		// hardware breakpoints and watchpoints
		reply_empty();
		return;
	}

	addr_t address;
	uint64 kind;
	if (!skip_char(arguments, ',') || !parse_address(arguments, address)
		|| !skip_char(arguments, ',') || !parse_hex(arguments, kind)) {
		reply_error(1);
		return;
	}

	if (!insert) {
		remove_breakpoint(address);
		reply_ok();
		return;
	}

	status_t status = add_breakpoint(address, kind, false);
	if (status == B_NOT_SUPPORTED) {
		// GDB then writes its own breakpoint instructions.
		reply_empty();
	} else if (status != B_OK)
		reply_error(1);
	else
		reply_ok();
}


/*!	"c [address]" and "s [address]", and "C signal[;address]" and
	"S signal[;address]", whose signal is ignored.
*/
static int32
handle_resume(const char* arguments, bool step, bool withSignal)
{
	uint64 value;
	if (withSignal) {
		parse_hex(arguments, value);
		skip_char(arguments, ';');
	}

	addr_t address = 0;
	bool hasAddress = parse_address(arguments, address);

	if (arch_debug_gdb_resume(hasAddress, address) != B_OK) {
		reply_error(1);
		return SESSION_CONTINUE;
	}

	if (step) {
		addr_t targets[kMaxStepTargets];
		ssize_t count = arch_debug_gdb_get_step_targets(targets,
			kMaxStepTargets);
		if (count <= 0) {
			reply_error(1);
			return SESSION_CONTINUE;
		}

		for (ssize_t i = 0; i < count; i++) {
			if (add_breakpoint(targets[i], 0, true) != B_OK) {
				remove_temporary_breakpoints();
				reply_error(2);
				return SESSION_CONTINUE;
			}
		}
	}

	// The answer is the stop reply of the next entry into the debugger.
	return SESSION_RESUME;
}


//!	"Hg id" selects the thread whose registers GDB reads. "Hc id" is accepted.
static void
handle_set_thread(const char* arguments)
{
	char operation = *arguments;
	thread_id id;
	Thread* thread;
	if (operation == '\0' || !parse_thread_id(++arguments, id)
		|| !find_thread(id, thread)) {
		reply_error(1);
		return;
	}

	if (operation == 'g')
		debug_set_debugged_thread(thread);
	reply_ok();
}


static void
handle_query(const char* query)
{
	if (strncmp(query, "Supported", 9) == 0) {
		reply("PacketSize=%" B_PRIxSIZE, kPacketSize);
	} else if (strcmp(query, "C") == 0) {
		reply("QC%" B_PRIx32, current_thread_id());
	} else if (strcmp(query, "Attached") == 0) {
		send_packet("1", 1);
	} else if (strcmp(query, "Offsets") == 0) {
		elf_image_info* kernelImage = elf_get_kernel_image();
		reply("Text=%lx;Data=%lx;Bss=%lx",
			kernelImage->text_region.delta,
			kernelImage->data_region.delta,
			kernelImage->data_region.delta);
	} else if (strcmp(query, "fThreadInfo") == 0) {
		sThreadListIndex = 0;
		reply_thread_list();
	} else if (strcmp(query, "sThreadInfo") == 0) {
		reply_thread_list();
	} else if (strncmp(query, "ThreadExtraInfo,", 16) == 0) {
		const char* arguments = query + 16;
		thread_id id;
		if (parse_thread_id(arguments, id))
			reply_thread_info(id);
		else
			reply_error(1);
	} else if (strncmp(query, "Symbol:", 7) == 0) {
		reply_ok();
	} else
		reply_empty();
}


static int32
handle_packet()
{
	const char* arguments = sCommand + 1;

	switch (sCommand[0]) {
		case '?':
			reply_stop();
			break;

		case 'g':
			handle_read_registers();
			break;

		case 'G':
			if (arch_debug_gdb_set_registers(arguments, strlen(arguments))
					!= B_OK) {
				reply_error(1);
			} else
				reply_ok();
			break;

		case 'p':
			handle_read_register(arguments);
			break;

		case 'P':
			handle_write_register(arguments);
			break;

		case 'm':
			handle_read_memory(arguments);
			break;

		case 'M':
			handle_write_memory(arguments);
			break;

		case 'Z':
			handle_breakpoint(arguments, true);
			break;

		case 'z':
			handle_breakpoint(arguments, false);
			break;

		case 'c':
			return handle_resume(arguments, false, false);

		case 'C':
			return handle_resume(arguments, false, true);

		case 's':
			return handle_resume(arguments, true, false);

		case 'S':
			return handle_resume(arguments, true, true);

		case 'H':
			handle_set_thread(arguments);
			break;

		case 'T':
		{
			thread_id id;
			Thread* thread;
			if (parse_thread_id(arguments, id) && find_thread(id, thread))
				reply_ok();
			else
				reply_error(1);
			break;
		}

		case 'q':
			handle_query(arguments);
			break;

		case 'D':
			// GDB detaches: the kernel runs on without the stub.
			reply_ok();
			detach();
			return SESSION_RESUME;

		case 'k':
			// "kill" ends the session. The kernel stays in its debugger.
			detach();
			return SESSION_PROMPT;

		default:
			reply_empty();
			break;
	}

	return SESSION_CONTINUE;
}


//	#pragma mark - interface of the kernel debugger


/*!	The "gdb" command. The kernel debugger also calls it in place of its
	prompt while a session is attached.
*/
int
cmd_gdb(int argc, char** argv)
{
	// If the debugger was not entered through an exception, GDB gets the
	// registers of this function, whose stack frame exists as long as the
	// session runs.
	setjmp(sEntryContext);
	arch_debug_gdb_set_entry_context(sEntryContext);

	bool resumed = sAttached;
	sAttached = true;
	sInSession = true;
	sPacketStarted = argc > 1 && strcmp(argv[1], "packet") == 0;
		// the KDL prompt has read the '$' of the first packet

	Thread* previousThread = debug_set_debugged_thread(NULL);

	if (sStopSignal != 0)
		sLastSignal = sStopSignal;
	else
		sLastSignal = resumed ? GDB_SIGNAL_ABRT : GDB_SIGNAL_TRAP;
	sStopSignal = 0;

	// the answer to the "c" or "s" that resumed the kernel
	if (resumed)
		reply_stop();

	int32 result;
	do {
		read_packet();
		result = handle_packet();
	} while (result == SESSION_CONTINUE);

	debug_set_debugged_thread(previousThread);
	sInSession = false;

	return result == SESSION_RESUME ? B_KDEBUG_QUIT : 0;
}


//!	Called when the kernel debugger has been entered.
void
gdb_stub_debugger_entered(void)
{
	if (sAttached)
		extract_breakpoints();
}


//!	Called before the kernel debugger is left.
void
gdb_stub_debugger_exiting(void)
{
	if (sAttached)
		insert_breakpoints();
}


/*!	Sends kernel debug output to GDB, which accepts it only while the kernel
	runs.
*/
void
gdb_stub_console_output(const char* string, size_t length)
{
	static const size_t kMaxChunk = 200;

	if (sInSession)
		return;

	while (length > 0) {
		size_t chunk = std::min(length, kMaxChunk);
		uint8 sum = 'O';

		arch_debug_gdb_putchar('$');
		arch_debug_gdb_putchar('O');
		for (size_t i = 0; i < chunk; i++) {
			uint8 c = (uint8)string[i];
			put_hex_byte(c);
			sum += kHexDigits[c >> 4] + kHexDigits[c & 0x0f];
		}
		arch_debug_gdb_putchar('#');
		put_hex_byte(sum);
		arch_debug_gdb_flush();

		string += chunk;
		length -= chunk;
	}
}


//	#pragma mark - interface of the architecture


bool
debug_gdb_attached(void)
{
	return sAttached;
}


/*!	Sets the signal the next stop is reported with. Used before a panic()
	for an exception.
*/
void
debug_gdb_set_stop_signal(int32 signal)
{
	sStopSignal = signal;
}


/*!	Enters the kernel debugger for a breakpoint or a break request and
	reports \a signal to GDB.
	Returns false if no GDB is attached or the debugger is running already.
	The caller then handles the event itself.
*/
bool
debug_gdb_trap(int32 signal)
{
	if (!sAttached || debug_debugger_running())
		return false;

	sStopSignal = signal;
	kernel_debugger(NULL);
	return true;
}
