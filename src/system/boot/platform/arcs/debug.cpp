/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <stdarg.h>
#include <string.h>

#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>

#include "console.h"


// The log of the loader's debug output, handed to the kernel.
static char sLogBuffer[16384];
static size_t sLogSize;


static void
debug_write(const char* string, size_t length)
{
	serial_puts(string, length);

	if (length > sizeof(sLogBuffer) - sLogSize)
		length = sizeof(sLogBuffer) - sLogSize;
	memcpy(sLogBuffer + sLogSize, string, length);
	sLogSize += length;
}


extern "C" void
panic(const char* format, ...)
{
	static const char kHint[] = "*** PANIC ***\n";
	char buffer[512];
	va_list list;

	debug_write(kHint, sizeof(kHint) - 1);

	va_start(list, format);
	int length = vsnprintf(buffer, sizeof(buffer), format, list);
	va_end(list);

	if (length >= (int)sizeof(buffer))
		length = sizeof(buffer) - 1;
	debug_write(buffer, length);
	debug_write("\nPress a key to return to the PROM.\n", 36);

	while (serial_try_getc() < 0)
		;
	platform_exit();
}


extern "C" void
dprintf(const char* format, ...)
{
	char buffer[512];
	va_list list;

	va_start(list, format);
	int length = vsnprintf(buffer, sizeof(buffer), format, list);
	va_end(list);

	if (length >= (int)sizeof(buffer))
		length = sizeof(buffer) - 1;
	debug_write(buffer, length);
}


char*
platform_debug_get_log_buffer(size_t* _size)
{
	if (_size != NULL)
		*_size = sLogSize;
	return sLogBuffer;
}


/*!	Copies the log to kernel args memory. Call it before the last
	kernel_args_malloc() user runs.
*/
void
debug_cleanup(void)
{
	if (sLogSize == 0)
		return;

	void* buffer = kernel_args_malloc(sLogSize);
	if (buffer == NULL)
		return;

	memcpy(buffer, sLogBuffer, sLogSize);
	gKernelArgs.debug_output = buffer;
	gKernelArgs.debug_size = sLogSize;
	gKernelArgs.keep_debug_output_buffer = false;
}
