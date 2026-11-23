/*
** Copyright 2002, Manuel J. Petit. All rights reserved.
** Distributed under the terms of the NewOS License.
*/
#ifndef _KERNEL_GDB_H
#define _KERNEL_GDB_H


#include <SupportDefs.h>


#ifdef __cplusplus
extern "C" {
#endif

int cmd_gdb(int argc, char** argv);

void gdb_stub_debugger_entered(void);
void gdb_stub_debugger_exiting(void);
void gdb_stub_console_output(const char* string, size_t length);

#ifdef __cplusplus
}
#endif

#endif
