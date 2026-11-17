/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include "console.h"

#include <stdio.h>
#include <string.h>

#include <SupportDefs.h>

#include <arch_indy.h>
#include <boot/stage2.h>
#include <boot/vfs.h>
#include <util/kernel_cpp.h>


/*!	The loader's console is serial port 1 (Z85C30 channel B), which the PROM
	has set to 9600 8N1. The terminal is expected to understand VT100
	sequences.
*/
class SerialConsole : public ConsoleNode {
public:
							SerialConsole();

	virtual ssize_t			ReadAt(void* cookie, off_t pos, void* buffer,
								size_t bufferSize);
	virtual ssize_t			WriteAt(void* cookie, off_t pos,
								const void* buffer, size_t bufferSize);

	virtual void			ClearScreen();
	virtual int32			Width();
	virtual int32			Height();
	virtual void			SetCursor(int32 x, int32 y);
	virtual void			SetCursorVisible(bool visible);
	virtual void			SetColors(int32 foreground, int32 background);
};


extern ConsoleNode* gConsoleNode;
static SerialConsole sConsole;

FILE* stdin;
FILE* stdout;
FILE* stderr;


void
serial_putc(char c)
{
	while ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_TX_EMPTY) == 0)
		;
	indy_write8(INDY_SCC_B_DATA, c);
}


void
serial_puts(const char* string, size_t size)
{
	for (size_t i = 0; i < size; i++) {
		if (string[i] == '\n')
			serial_putc('\r');
		serial_putc(string[i]);
	}
}


int
serial_try_getc(void)
{
	if ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_RX_AVAILABLE) == 0)
		return -1;
	return indy_read8(INDY_SCC_B_DATA);
}


//	#pragma mark -


SerialConsole::SerialConsole()
	:
	ConsoleNode()
{
}


ssize_t
SerialConsole::ReadAt(void* cookie, off_t pos, void* buffer,
	size_t bufferSize)
{
	return B_ERROR;
}


ssize_t
SerialConsole::WriteAt(void* cookie, off_t pos, const void* buffer,
	size_t bufferSize)
{
	serial_puts((const char*)buffer, bufferSize);
	return bufferSize;
}


void
SerialConsole::ClearScreen()
{
	WriteAt(NULL, 0, "\033[2J", 4);
}


int32
SerialConsole::Width()
{
	return 80;
}


int32
SerialConsole::Height()
{
	return 25;
}


void
SerialConsole::SetCursor(int32 x, int32 y)
{
	char buffer[16];
	x = MIN(80, MAX(1, x + 1));
	y = MIN(25, MAX(1, y + 1));
	int length = snprintf(buffer, sizeof(buffer),
		"\033[%" B_PRId32 ";%" B_PRId32 "H", y, x);
	WriteAt(NULL, 0, buffer, length);
}


void
SerialConsole::SetCursorVisible(bool visible)
{
}


void
SerialConsole::SetColors(int32 foreground, int32 background)
{
	static const char kColorMap[] = { 0, 4, 2, 6, 1, 5, 3, 7 };
	char buffer[16];

	if (foreground < 0 || foreground >= 8 || background < 0
		|| background >= 8) {
		return;
	}

	int length = snprintf(buffer, sizeof(buffer), "\033[%d;%dm",
		kColorMap[foreground] + 30, kColorMap[background] + 40);
	WriteAt(NULL, 0, buffer, length);
}


//	#pragma mark -


int
console_wait_for_key(void)
{
	int c;
	while ((c = serial_try_getc()) < 0)
		;

	if (c != 0x1b)
		return c == '\n' ? TEXT_CONSOLE_KEY_RETURN : c;

	// An escape sequence for a cursor key follows at once, a single Esc
	// does not.
	for (int i = 0; i < 200000; i++) {
		int next = serial_try_getc();
		if (next < 0)
			continue;
		if (next != '[')
			return TEXT_CONSOLE_KEY_ESCAPE;
		while ((next = serial_try_getc()) < 0)
			;
		switch (next) {
			case 'A':
				return TEXT_CONSOLE_KEY_UP;
			case 'B':
				return TEXT_CONSOLE_KEY_DOWN;
			case 'C':
				return TEXT_CONSOLE_KEY_RIGHT;
			case 'D':
				return TEXT_CONSOLE_KEY_LEFT;
			default:
				return TEXT_CONSOLE_NO_KEY;
		}
	}
	return TEXT_CONSOLE_KEY_ESCAPE;
}


status_t
console_init(void)
{
	gConsoleNode = &sConsole;
	stdin = (FILE*)&sConsole;
	stdout = (FILE*)&sConsole;
	stderr = (FILE*)&sConsole;
	return B_OK;
}
