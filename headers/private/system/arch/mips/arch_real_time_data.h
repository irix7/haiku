/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_REAL_TIME_DATA_H
#define _KERNEL_ARCH_REAL_TIME_DATA_H


#include <StorageDefs.h>
#include <SupportDefs.h>


/* 64 bit loads and stores are single instructions on this architecture, so
   system_time_offset needs no versioning. */
struct arch_real_time_data {
	bigtime_t	system_time_offset;
	uint64		system_time_conversion_factor;
		// microseconds per CP0 Count tick, as a 32.32 fixed point value
};


#endif	/* _KERNEL_ARCH_REAL_TIME_DATA_H */
