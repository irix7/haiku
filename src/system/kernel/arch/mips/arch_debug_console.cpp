/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/debug_console.h>
#include <arch_indy.h>
#include <boot/kernel_args.h>
#include <kernel.h>


/*
 * The debug console is serial port 1 (Z85C30 channel B), polled. The PROM
 * has set it to 9600 8N1.
 */


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
	if ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_RX_AVAILABLE) == 0)
		return -1;
	return indy_read8(INDY_SCC_B_DATA);
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
	if (c == '\n') {
		while ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_TX_EMPTY) == 0)
			;
		indy_write8(INDY_SCC_B_DATA, '\r');
	}
	while ((indy_read8(INDY_SCC_B_CONTROL) & INDY_SCC_TX_EMPTY) == 0)
		;
	indy_write8(INDY_SCC_B_DATA, c);
}


void
arch_debug_serial_puts(const char* s)
{
	while (*s != '\0')
		arch_debug_serial_putchar(*s++);
}


void
arch_debug_serial_early_boot_message(const char* string)
{
	arch_debug_serial_puts(string);
}


status_t
arch_debug_console_init(kernel_args* args)
{
	return B_OK;
}


status_t
arch_debug_console_init_settings(kernel_args* args)
{
	return B_OK;
}
