/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <arch/cpu.h>
#include <arch/timer.h>
#include <arch_indy.h>
#include <boot/kernel_args.h>
#include <interrupts.h>
#include <real_time_data.h>
#include <timer.h>
#include <util/AutoLock.h>


/*
 * The timer is CP0 Count and Compare: Count runs freely, an interrupt (IP7)
 * is raised when it equals Compare, and a write to Compare acknowledges it.
 *
 * system_time() extends the 32 bit Count to 64 bits. That works as long as
 * it is called at least once per wrap of Count (64 seconds at 66.7 MHz), so
 * the timer is never armed further than kMaxTimeout ahead.
 */

static const bigtime_t kMaxTimeout = 30 * 1000000LL;

static uint64 sCountFrequency = 33333333;
	// Count ticks per second
static uint64 sConversionFactor;
	// microseconds per tick, 32.32 fixed point
static uint32 sLastCount;
static uint64 sTicks;
static bool sTimerInitialized;


static uint16
read_pit_counter2()
{
	// latch counter 2, then read the low and the high byte
	indy_write8(INDY_PIT_CONTROL, 0x80);
	uint16 low = indy_read8(INDY_PIT_COUNTER2);
	return low | ((uint16)indy_read8(INDY_PIT_COUNTER2) << 8);
}


/*!	Measures the rate of Count against counter 2 of the 8254, which counts
	down at 1 MHz. Count runs at 33 MHz under iris and at half the core clock
	on an R4600.
*/
static void
calibrate_count()
{
	// counter 2, low and high byte, mode 2 (rate generator), binary
	indy_write8(INDY_PIT_CONTROL, 0xb4);
	indy_write8(INDY_PIT_COUNTER2, 0xff);
	indy_write8(INDY_PIT_COUNTER2, 0xff);

	uint16 start = read_pit_counter2();
	for (int i = 0; i < 1000000 && read_pit_counter2() == start; i++)
		;

	start = read_pit_counter2();
	uint32 startCount = mips_read_count();
	uint16 elapsed = 0;
	for (int i = 0; i < 10000000 && elapsed < 20000; i++)
		elapsed = start - read_pit_counter2();
	uint32 ticks = mips_read_count() - startCount;

	if (elapsed < 20000) {
		dprintf("timer: the 8254 does not count, assuming a Count rate of "
			"%" B_PRIu64 " Hz\n", sCountFrequency);
		return;
	}

	sCountFrequency = (uint64)ticks * INDY_PIT_FREQUENCY / elapsed;
}


static inline uint64
current_ticks()
{
	// Called with interrupts disabled.
	uint32 count = mips_read_count();
	sTicks += (uint32)(count - sLastCount);
	sLastCount = count;
	return sTicks;
}


bigtime_t
system_time(void)
{
	InterruptsLocker locker;
	uint64 ticks = current_ticks();
	locker.Unlock();

	// ticks * factor / 2^32 without a 128 bit product
	return (bigtime_t)((ticks >> 32) * sConversionFactor
		+ (((ticks & 0xffffffff) * sConversionFactor) >> 32));
}


void
arch_timer_set_hardware_timer(bigtime_t timeout)
{
	if (timeout > kMaxTimeout)
		timeout = kMaxTimeout;
	if (timeout < 0)
		timeout = 0;

	uint64 delta = (uint64)timeout * sCountFrequency / 1000000;
	if (delta < 64)
		delta = 64;

	InterruptsLocker locker;
	for (;;) {
		uint32 compare = mips_read_count() + (uint32)delta;
		mips_write_compare(compare);
		// If Count has passed the new Compare already, the interrupt
		// would only come after a full wrap.
		if ((int32)(compare - mips_read_count()) > 0)
			break;
		delta *= 2;
	}
}


void
arch_timer_clear_hardware_timer()
{
	// The timer stays armed so that system_time() sees every Count wrap.
	arch_timer_set_hardware_timer(kMaxTimeout);
}


void
mips_timer_interrupt()
{
	// acknowledge, and keep the timer armed
	arch_timer_set_hardware_timer(kMaxTimeout);
	current_ticks();

	if (sTimerInitialized)
		timer_interrupt();
}


uint64
mips_count_frequency()
{
	return sCountFrequency;
}


uint64
mips_system_time_conversion_factor()
{
	return sConversionFactor;
}


int
arch_init_timer(kernel_args* args)
{
	calibrate_count();
	gMipsCpuInfo.count_frequency = sCountFrequency;
	sConversionFactor = ((uint64)1000000 << 32) / sCountFrequency;
	sLastCount = mips_read_count();

	dprintf("timer: Count rate %" B_PRIu64 " Hz\n", sCountFrequency);

	sTimerInitialized = true;
	arch_timer_set_hardware_timer(kMaxTimeout);
	return 0;
}
