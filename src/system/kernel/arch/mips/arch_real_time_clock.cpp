/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <arch/real_time_clock.h>

#include <KernelExport.h>

#include <arch_indy.h>
#include <boot/kernel_args.h>
#include <real_time_clock.h>
#include <real_time_data.h>


/* The clock is a DS1386. Its registers hold BCD values. The year register
   counts from 1940. */

uint64 mips_system_time_conversion_factor();


static inline uint32
bcd_to_int(uint8 bcd)
{
	return (bcd >> 4) * 10 + (bcd & 0x0f);
}


static inline uint8
int_to_bcd(uint32 value)
{
	return ((value / 10) << 4) | (value % 10);
}


static inline uint8
rtc_read(uint32 index)
{
	return indy_read8(INDY_RTC_REGISTER(index));
}


static inline void
rtc_write(uint32 index, uint8 value)
{
	indy_write8(INDY_RTC_REGISTER(index), value);
}


status_t
arch_rtc_init(kernel_args* args, struct real_time_data* data)
{
	data->arch_data.system_time_conversion_factor
		= mips_system_time_conversion_factor();
	return B_OK;
}


uint64
arch_rtc_get_hw_time(void)
{
	// Clearing "transfer enable" freezes the registers while they are read.
	uint8 command = rtc_read(INDY_RTC_COMMAND);
	rtc_write(INDY_RTC_COMMAND, command & ~INDY_RTC_COMMAND_TE);

	struct tm t;
	t.tm_sec = bcd_to_int(rtc_read(INDY_RTC_SECONDS) & 0x7f);
	t.tm_min = bcd_to_int(rtc_read(INDY_RTC_MINUTES) & 0x7f);
	t.tm_hour = bcd_to_int(rtc_read(INDY_RTC_HOURS) & 0x3f);
	t.tm_mday = bcd_to_int(rtc_read(INDY_RTC_DATE) & 0x3f);
	t.tm_mon = bcd_to_int(rtc_read(INDY_RTC_MONTH) & 0x1f) - 1;
	t.tm_year = bcd_to_int(rtc_read(INDY_RTC_YEAR)) + INDY_RTC_YEAR_BASE
		- 1900;

	rtc_write(INDY_RTC_COMMAND, command | INDY_RTC_COMMAND_TE);

	if (t.tm_mon < 0 || t.tm_mon > 11 || t.tm_mday < 1 || t.tm_year < 70)
		return 0;

	return rtc_tm_to_secs(&t);
}


void
arch_rtc_set_hw_time(uint64 seconds)
{
	struct tm t;
	rtc_secs_to_tm(seconds, &t);

	int32 year = t.tm_year + 1900 - INDY_RTC_YEAR_BASE;
	if (year < 0 || year > 99)
		return;

	uint8 command = rtc_read(INDY_RTC_COMMAND);
	rtc_write(INDY_RTC_COMMAND, command & ~INDY_RTC_COMMAND_TE);

	rtc_write(INDY_RTC_SECONDS, int_to_bcd(t.tm_sec));
	rtc_write(INDY_RTC_MINUTES, int_to_bcd(t.tm_min));
	rtc_write(INDY_RTC_HOURS, int_to_bcd(t.tm_hour));
	rtc_write(INDY_RTC_DATE, int_to_bcd(t.tm_mday));
	// The upper bits of the month register are control bits.
	rtc_write(INDY_RTC_MONTH,
		(rtc_read(INDY_RTC_MONTH) & 0xe0) | int_to_bcd(t.tm_mon + 1));
	rtc_write(INDY_RTC_YEAR, int_to_bcd(year));

	rtc_write(INDY_RTC_COMMAND, command | INDY_RTC_COMMAND_TE);
}


void
arch_rtc_set_system_time_offset(struct real_time_data* data, bigtime_t offset)
{
	atomic_set64(&data->arch_data.system_time_offset, offset);
}


bigtime_t
arch_rtc_get_system_time_offset(struct real_time_data* data)
{
	return atomic_get64(&data->arch_data.system_time_offset);
}
