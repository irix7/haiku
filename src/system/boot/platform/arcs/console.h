/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef CONSOLE_H
#define CONSOLE_H


#include <boot/platform/generic/text_console.h>


#ifdef __cplusplus
extern "C" {
#endif

status_t console_init(void);
void console_leave_prom(void);
bool console_prom_graphics(void);

void serial_putc(char c);
void serial_puts(const char* string, size_t size);
	// Returns the next character, or -1 if none is waiting.
int serial_try_getc(void);

#ifdef __cplusplus
}
#endif


#endif	/* CONSOLE_H */
