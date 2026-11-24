/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	Kernel driver for the Newport (XL) graphics board of the SGI Indy.
	See newport_info.h for the split between driver and accelerant.
*/


#include <stdlib.h>
#include <string.h>

#include <Drivers.h>
#include <graphic_driver.h>
#include <KernelExport.h>
#include <OS.h>

#include <arch/cpu.h>
#include <arch_indy.h>
#include <lock.h>
#include <util/AutoLock.h>
#include <vm/vm.h>

#include <newport_info.h>


#define DEVICE_NAME			"newport"
#define ACCELERANT_NAME		"newport.accelerant"

//#define TRACE_NEWPORT
#ifdef TRACE_NEWPORT
#	define TRACE(x...) dprintf(DEVICE_NAME ": " x)
#else
#	define TRACE(x...) ;
#endif


struct newport_info {
	int32					open_count;
	area_id					shared_area;
	newport_shared_info*	shared_info;
	area_id					registers_area;
	area_id					shadow_area;
};


int32 api_version = B_CUR_DRIVER_API_VERSION;

static const char* sDeviceNames[] = { "graphics/" DEVICE_NAME, NULL };
static newport_info sInfo;
static mutex sLock;


static status_t
newport_init(newport_info& info)
{
	// the shared info
	info.shared_area = create_area("newport shared info",
		(void**)&info.shared_info, B_ANY_KERNEL_ADDRESS,
		ROUNDUP(sizeof(newport_shared_info), B_PAGE_SIZE), B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA | B_CLONEABLE_AREA);
	if (info.shared_area < 0)
		return info.shared_area;

	newport_shared_info& sharedInfo = *info.shared_info;
	memset(&sharedInfo, 0, sizeof(newport_shared_info));

	// the REX3 registers
	void* registers;
	info.registers_area = map_physical_memory("newport registers",
		INDY_REX3_BASE, ROUNDUP(INDY_REX3_SIZE, B_PAGE_SIZE),
		B_ANY_KERNEL_ADDRESS,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA | B_CLONEABLE_AREA,
		&registers);
	if (info.registers_area < 0) {
		delete_area(info.shared_area);
		return info.registers_area;
	}

	// the shadow frame buffer
	void* shadow;
	info.shadow_area = create_area("newport shadow frame buffer", &shadow,
		B_ANY_KERNEL_ADDRESS,
		ROUNDUP(NEWPORT_BYTES_PER_ROW * NEWPORT_HEIGHT, B_PAGE_SIZE),
		B_FULL_LOCK,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA | B_CLONEABLE_AREA);
	if (info.shadow_area < 0) {
		delete_area(info.registers_area);
		delete_area(info.shared_area);
		return info.shadow_area;
	}
	memset(shadow, 0, NEWPORT_BYTES_PER_ROW * NEWPORT_HEIGHT);

	// The colour map's revision register tells the board variant: bit 7
	// is set on the 8 bit board.
	volatile uint32* rex3 = (volatile uint32*)registers;
	while ((rex3[REX3_STATUS / 4] & REX3_STATUS_BACK_BUSY) != 0)
		;
	rex3[REX3_DCBMODE / 4] = REX3_DCB_CMAP_REVISION;
	uint32 revision = rex3[REX3_DCBDATA0 / 4] >> 24;

	sharedInfo.registers_area = info.registers_area;
	sharedInfo.shadow_area = info.shadow_area;
	sharedInfo.mode_list_area = -1;
	sharedInfo.bytes_per_row = NEWPORT_BYTES_PER_ROW;
	sharedInfo.bitplanes = (revision & 0x80) != 0 ? 8 : 24;
	// 64 bit stores to the host data port work under iris. They have not
	// been tried on the machine.
	sharedInfo.double_host_writes = true;

	display_mode& mode = sharedInfo.current_mode;
	mode.virtual_width = NEWPORT_WIDTH;
	mode.virtual_height = NEWPORT_HEIGHT;
	mode.space = B_CMAP8;
	mode.h_display_start = 0;
	mode.v_display_start = 0;
	mode.flags = 0;
	// Nominal timing of the PROM's 1280 x 1024 mode. It is informational:
	// the driver does not program the video timing.
	mode.timing.pixel_clock = 107500;
	mode.timing.h_display = NEWPORT_WIDTH;
	mode.timing.h_sync_start = 1296;
	mode.timing.h_sync_end = 1440;
	mode.timing.h_total = 1688;
	mode.timing.v_display = NEWPORT_HEIGHT;
	mode.timing.v_sync_start = 1025;
	mode.timing.v_sync_end = 1028;
	mode.timing.v_total = 1066;
	mode.timing.flags = 0;

	dprintf(DEVICE_NAME ": %" B_PRIu32 " bitplanes (colour map revision %#"
		B_PRIx32 "), mode 1280 x 1024, 8 bit colour index\n",
		sharedInfo.bitplanes, revision);

	return B_OK;
}


static void
newport_uninit(newport_info& info)
{
	delete_area(info.shadow_area);
	delete_area(info.registers_area);
	delete_area(info.shared_area);
}


//	#pragma mark - device hooks


static status_t
device_open(const char* name, uint32 flags, void** _cookie)
{
	if (strcmp(name, sDeviceNames[0]) != 0)
		return B_BAD_VALUE;

	MutexLocker locker(sLock);

	if (sInfo.open_count == 0) {
		status_t status = newport_init(sInfo);
		if (status != B_OK)
			return status;

		// app_server draws from now on
		mips_newport_console_stop();
	}

	sInfo.open_count++;
	*_cookie = &sInfo;
	return B_OK;
}


static status_t
device_close(void* cookie)
{
	return B_OK;
}


static status_t
device_free(void* cookie)
{
	newport_info* info = (newport_info*)cookie;
	MutexLocker locker(sLock);

	if (info->open_count-- == 1)
		newport_uninit(*info);

	return B_OK;
}


static status_t
device_ioctl(void* cookie, uint32 op, void* buffer, size_t bufferLength)
{
	newport_info* info = (newport_info*)cookie;

	switch (op) {
		case B_GET_ACCELERANT_SIGNATURE:
			if (user_strlcpy((char*)buffer, ACCELERANT_NAME,
					B_FILE_NAME_LENGTH) < B_OK) {
				return B_BAD_ADDRESS;
			}
			return B_OK;

		case NEWPORT_GET_PRIVATE_DATA:
			return user_memcpy(buffer, &info->shared_area, sizeof(area_id));

		case NEWPORT_GET_DEVICE_NAME:
			if (user_strlcpy((char*)buffer, sDeviceNames[0],
					B_PATH_NAME_LENGTH) < B_OK) {
				return B_BAD_ADDRESS;
			}
			return B_OK;
	}

	return B_DEV_INVALID_IOCTL;
}


static status_t
device_read(void* cookie, off_t pos, void* buffer, size_t* _length)
{
	*_length = 0;
	return B_NOT_ALLOWED;
}


static status_t
device_write(void* cookie, off_t pos, const void* buffer, size_t* _length)
{
	*_length = 0;
	return B_NOT_ALLOWED;
}


static device_hooks sDeviceHooks = {
	device_open,
	device_close,
	device_free,
	device_ioctl,
	device_read,
	device_write,
	NULL,
	NULL,
	NULL,
	NULL
};


//	#pragma mark - driver hooks


extern "C" status_t
init_hardware(void)
{
	// The board is absent when the machine runs with a serial console only.
	// REX3 answers with a bus error then.
	uint32 status;
	if (arch_cpu_probe_read32(MIPS_KSEG1(INDY_REX3_BASE + REX3_STATUS),
			&status) != B_OK) {
		dprintf(DEVICE_NAME ": no Newport board\n");
		return B_ERROR;
	}
	return B_OK;
}


extern "C" status_t
init_driver(void)
{
	memset(&sInfo, 0, sizeof(sInfo));
	mutex_init(&sLock, "newport lock");
	return B_OK;
}


extern "C" void
uninit_driver(void)
{
	mutex_destroy(&sLock);
}


extern "C" const char**
publish_devices(void)
{
	return sDeviceNames;
}


extern "C" device_hooks*
find_device(const char* name)
{
	if (strcmp(name, sDeviceNames[0]) == 0)
		return &sDeviceHooks;
	return NULL;
}
