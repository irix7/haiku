/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>

#include <syscalls.h>


/*	The CP0 Count register cannot be read in user mode. Task M9.3 of the
	port plan replaces the syscall with the memory controller's counter.
*/
bigtime_t
system_time(void)
{
	return _kern_system_time();
}
