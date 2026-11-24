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


extern "C" int32 arcs_call(uint32 function, uint32 a0, uint32 a1, uint32 a2,
	uint32 a3);

// The ARCS system parameter block and the firmware vector slot of
// Write(fileID, buffer, length, count). File 1 is the PROM's console: the
// graphics textport, or serial port 1 with console=d.
#define ARCS_SPB				0xa0001000
#define ARCS_SPB_SIGNATURE		0x53435241
#define ARCS_SPB_FIRMWARE_VECTOR	32
#define ARCS_VECTOR_WRITE		27
#define ARCS_VECTOR_GET_ENVIRONMENT	30
#define ARCS_STDOUT				1

static bool sPromConsole = true;
static bool sPromIsSerial = false;
static bool sSerialDead = false;


/*!	Stops the output through the PROM. The loader calls this before it
	replaces the exception vectors of the PROM.
*/
void
console_leave_prom(void)
{
	sPromConsole = false;
}


static void
prom_write(const char* string, size_t size)
{
	if (!sPromConsole
		|| *(volatile uint32*)ARCS_SPB != ARCS_SPB_SIGNATURE) {
		return;
	}

	uint32 vector = *(volatile uint32*)(ARCS_SPB + ARCS_SPB_FIRMWARE_VECTOR);
	uint32 write = *(volatile uint32*)(addr_t)(vector
		+ 4 * ARCS_VECTOR_WRITE);

	// the PROM wants "\r\n"
	static char buffer[128];
	static uint32 count;
	size_t used = 0;
	for (size_t i = 0; i < size; i++) {
		if (string[i] == '\n')
			buffer[used++] = '\r';
		buffer[used++] = string[i];
		if (used >= sizeof(buffer) - 2 || i + 1 == size) {
			arcs_call(write, ARCS_STDOUT, (uint32)(addr_t)buffer, used,
				(uint32)(addr_t)&count);
			used = 0;
		}
	}
}


void
serial_putc(char c)
{
	// Without a transmit clock the transmitter never empties. The wait is
	// bounded: a character takes 1 ms at 9600 baud.
	if (sSerialDead)
		return;
	for (int32 tries = 0; (indy_read8(INDY_SCC_B_CONTROL)
			& INDY_SCC_TX_EMPTY) == 0; tries++) {
		if (tries > 1000000) {
			sSerialDead = true;
			return;
		}
	}
	indy_write8(INDY_SCC_B_DATA, c);
}


void
serial_puts(const char* string, size_t size)
{
	// With console=d the PROM's console is this serial port.
	if (!sPromIsSerial)
		prom_write(string, size);

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
	// "console=d" puts the PROM's console on serial port 1, where the
	// loader writes anyway.
	if (*(volatile uint32*)ARCS_SPB == ARCS_SPB_SIGNATURE) {
		uint32 vector
			= *(volatile uint32*)(ARCS_SPB + ARCS_SPB_FIRMWARE_VECTOR);
		uint32 getEnvironment = *(volatile uint32*)(addr_t)(vector
			+ 4 * ARCS_VECTOR_GET_ENVIRONMENT);
		static const char kName[] = "console";
		const char* value = (const char*)(addr_t)arcs_call(getEnvironment,
			(uint32)(addr_t)kName, 0, 0, 0);
		sPromIsSerial = value != NULL && (value[0] == 'd' || value[0] == 'D');
	}

	gConsoleNode = &sConsole;
	stdin = (FILE*)&sConsole;
	stdout = (FILE*)&sConsole;
	stderr = (FILE*)&sConsole;
	return B_OK;
}
