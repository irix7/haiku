/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/debug_console.h>

#include <arch/debug.h>
#include <arch_indy.h>
#include <boot/kernel_args.h>
#include <debug.h>
#include <kernel.h>


/*
 * The debug console has two channels.
 *
 * Serial port 1 (Z85C30 channel B), polled. The PROM has set it to 9600 8N1.
 * Kernel debug output always goes there.
 *
 * UDP port 2159 on the Ethernet controller (arch_debug_net.cpp), inside the
 * kernel debugger only. It carries the input of the kernel debugger and the
 * GDB stub. The text of the kernel debugger is sent to it once its peer has
 * sent a line end, which a terminal does and GDB does not.
 */


static bool sInputFromNetwork;
	// the channel the last input came from
static bool sNetworkTerminal;
	// the peer on the network is a terminal


static int
serial_try_getchar()
{
	if ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_RX_AVAILABLE) == 0)
		return -1;
	return indy_read8(INDY_SCC_B_DATA);
}


static void
serial_putchar(char c)
{
	// Without a transmit clock the transmitter never empties. The wait is
	// bounded: a character takes 1 ms at 9600 baud.
	static bool sDead = false;
	if (sDead)
		return;
	for (int32 tries = 0; (indy_read8(INDY_SCC_B_CONTROL)
			& INDY_SCC_TX_EMPTY) == 0; tries++) {
		if (tries > 1000000) {
			sDead = true;
			return;
		}
	}
	indy_write8(INDY_SCC_B_DATA, c);
}


/*!	Returns whether GDB is attached through the network. The serial port is
	not read then: its input line may be open.
*/
static bool
gdb_on_network()
{
	return sInputFromNetwork && debug_gdb_attached();
}


static bool
network_terminal_output()
{
	return sNetworkTerminal && !debug_gdb_attached();
}


void
arch_debug_remove_interrupt_handler(uint32 line)
{
}


int
arch_debug_blue_screen_try_getchar(void)
{
	return -1;
}


char
arch_debug_blue_screen_getchar(void)
{
	return 0;
}


int
arch_debug_serial_try_getchar(void)
{
	int c = gdb_on_network() ? -1 : serial_try_getchar();
	if (c >= 0) {
		sInputFromNetwork = false;
		return c;
	}

	c = mips_debug_net_try_getchar();
	if (c >= 0) {
		sInputFromNetwork = true;
		if (c == '\r' || c == '\n')
			sNetworkTerminal = true;
	}
	return c;
}


char
arch_debug_serial_getchar(void)
{
	int c;
	while ((c = arch_debug_serial_try_getchar()) < 0)
		;
	return (char)c;
}


void
arch_debug_serial_putchar(const char c)
{
	if (c == '\n')
		serial_putchar('\r');
	serial_putchar(c);

	if (network_terminal_output()) {
		if (c == '\n')
			mips_debug_net_putchar('\r');
		mips_debug_net_putchar(c);
	}
}


void
arch_debug_serial_puts(const char* s)
{
	while (*s != '\0')
		arch_debug_serial_putchar(*s++);

	if (network_terminal_output())
		mips_debug_net_flush();
}


void
arch_debug_serial_early_boot_message(const char* string)
{
	arch_debug_serial_puts(string);
}


status_t
arch_debug_console_init(kernel_args* args)
{
	mips_debug_net_init(args);
	return B_OK;
}


status_t
arch_debug_console_init_settings(kernel_args* args)
{
	return B_OK;
}


//	#pragma mark - kernel debugger


void
arch_debug_enter_debugger(void)
{
	sNetworkTerminal = false;
	mips_debug_net_enter();
}


void
arch_debug_exit_debugger(void)
{
	mips_debug_net_exit();
}


/*!	Reads the break character of the serial port: Control-C, or the start of
	a packet if a GDB is attached there. Called at the end of an interrupt.
	The serial port has no driver, so no other reader loses the character.
*/
bool
mips_debug_serial_break_requested()
{
	if (gdb_on_network())
		return false;

	int c = serial_try_getchar();
	return c == 0x03 || (c == '$' && debug_gdb_attached());
}


//	#pragma mark - channel of the GDB stub


char
arch_debug_gdb_getchar(void)
{
	return arch_debug_serial_getchar();
}


void
arch_debug_gdb_putchar(char c)
{
	if (sInputFromNetwork)
		mips_debug_net_putchar(c);
	else
		serial_putchar(c);
}


void
arch_debug_gdb_flush(void)
{
	if (sInputFromNetwork)
		mips_debug_net_flush();
}


bool
arch_debug_gdb_uses_serial_port(void)
{
	return !sInputFromNetwork;
}
