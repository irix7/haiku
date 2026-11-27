/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>

#include <libroot_private.h>
#include <real_time_data.h>


void
__arch_init_time(struct real_time_data* data, bool setDefaults)
{
	if (setDefaults)
		data->arch_data.system_time_conversion_factor = 0;
}


bigtime_t
__arch_get_system_time_offset(struct real_time_data* data)
{
	// A 64 bit load is one instruction, so the value is read atomically.
	return data->arch_data.system_time_offset;
}
