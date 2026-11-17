/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <OS.h>

#include <arch/cpu.h>
#include <arch_indy.h>
#include <boot/arch/mips/arch_cpu.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/vfs.h>


// CP0 Count ticks per second
static uint64 sCountFrequency = 33000000;
static uint32 sLastCount;
static uint64 sTicks;


static uint16
read_pit_counter2()
{
	// latch counter 2, then read the low and the high byte
	indy_write8(INDY_PIT_CONTROL, 0x80);
	uint16 low = indy_read8(INDY_PIT_COUNTER2);
	return low | ((uint16)indy_read8(INDY_PIT_COUNTER2) << 8);
}


/*!	Measures the rate of CP0 Count against counter 2 of the 8254, which
	counts down at 1 MHz.
*/
static void
calibrate_count()
{
	// counter 2, low and high byte, mode 2 (rate generator), binary
	indy_write8(INDY_PIT_CONTROL, 0xb4);
	indy_write8(INDY_PIT_COUNTER2, 0xff);
	indy_write8(INDY_PIT_COUNTER2, 0xff);

	// wait for the counter to load and to start counting
	uint16 start = read_pit_counter2();
	for (int i = 0; i < 1000000 && read_pit_counter2() == start; i++)
		;

	start = read_pit_counter2();
	uint32 startCount = mips_read_count();
	uint16 elapsed = 0;
	for (int i = 0; i < 10000000 && elapsed < 10000; i++)
		elapsed = start - read_pit_counter2();
	uint32 ticks = mips_read_count() - startCount;

	if (elapsed < 10000) {
		dprintf("8254 does not count, assuming a Count rate of 33 MHz\n");
		return;
	}

	sCountFrequency = (uint64)ticks * INDY_PIT_FREQUENCY / elapsed;
	dprintf("CP0 Count rate: %" B_PRIu64 " Hz\n", sCountFrequency);
}


extern "C" void
cpu_init()
{
	boot_arch_cpu_init();
	calibrate_count();
	sLastCount = mips_read_count();
}


extern "C" bigtime_t
system_time()
{
	// Count wraps after about a minute. The loader polls often enough.
	uint32 count = mips_read_count();
	sTicks += count - sLastCount;
	sLastCount = count;
	return (bigtime_t)(sTicks * 1000000 / sCountFrequency);
}


extern "C" void
spin(bigtime_t microseconds)
{
	bigtime_t end = system_time() + microseconds;
	while (system_time() < end)
		;
}


void
platform_load_ucode(BootVolume& volume)
{
	arch_ucode_load(volume);
}
