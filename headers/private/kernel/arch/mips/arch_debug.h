/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_DEBUG_H
#define _KERNEL_ARCH_MIPS_DEBUG_H


#include <SupportDefs.h>


struct iframe;
struct kernel_args;


struct arch_debug_registers {
	addr_t	fp;
	addr_t	sp;
	addr_t	pc;
};


// What cpu_ent::fault_handler_stack_pointer points to while a function runs
// under arch_debug_call_with_fault_handler(). The fault handler gets the jump
// buffer as its argument and a stack below the link.
struct mips_debug_fault_link {
	void*	jump_buffer;
};


#ifdef __cplusplus

// arch_debug.cpp
iframe* mips_debug_set_frame(iframe* frame);
void mips_debug_check_break(iframe* frame);

// arch_debug_console.cpp
bool mips_debug_serial_break_requested();

// arch_debug_net.cpp: the debug console on the network
void mips_debug_net_init(kernel_args* args);
void mips_debug_net_enter();
void mips_debug_net_exit();
int mips_debug_net_try_getchar();
void mips_debug_net_putchar(char c);
void mips_debug_net_flush();
bool mips_debug_net_take_break_request();

extern "C" bool mips_debug_net_input(const uint8* frame, size_t length);
	// for the seeq8003 driver

#endif	// __cplusplus


#endif	// _KERNEL_ARCH_MIPS_DEBUG_H
