/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	Accelerant for the Newport (XL) graphics board of the SGI Indy.

	The frame buffer it hands out is a shadow buffer in RAM. app_server
	reports every rectangle it writes there through the
	B_FRONT_BUFFER_UPDATED hook, and the accelerant copies it to the screen.
	The one mode is 1280 x 1024 in B_CMAP8, as the PROM has set it.
*/


#include "accelerant.h"

#include <errno.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <front_buffer_hook.h>


accelerant_info* gInfo;


static status_t
init_common(int device, bool isClone)
{
	gInfo = (accelerant_info*)malloc(sizeof(accelerant_info));
	if (gInfo == NULL)
		return B_NO_MEMORY;

	memset(gInfo, 0, sizeof(accelerant_info));
	gInfo->is_clone = isClone;
	gInfo->device = device;
	gInfo->shared_info_area = -1;
	gInfo->registers_area = -1;
	gInfo->shadow_area = -1;

	status_t status = B_ERROR;
	area_id sharedArea;
	if (ioctl(device, NEWPORT_GET_PRIVATE_DATA, &sharedArea, sizeof(area_id))
			!= 0) {
		goto error;
	}

	status = gInfo->shared_info_area = clone_area("newport shared info",
		(void**)&gInfo->shared_info, B_ANY_ADDRESS,
		B_READ_AREA | B_WRITE_AREA, sharedArea);
	if (status < B_OK)
		goto error;

	status = gInfo->registers_area = clone_area("newport registers",
		(void**)&gInfo->registers, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA,
		gInfo->shared_info->registers_area);
	if (status < B_OK)
		goto error;

	status = gInfo->shadow_area = clone_area("newport shadow frame buffer",
		(void**)&gInfo->shadow, B_ANY_ADDRESS, B_READ_AREA | B_WRITE_AREA,
		gInfo->shared_info->shadow_area);
	if (status < B_OK)
		goto error;

	return B_OK;

error:
	if (gInfo->registers_area >= 0)
		delete_area(gInfo->registers_area);
	if (gInfo->shared_info_area >= 0)
		delete_area(gInfo->shared_info_area);
	free(gInfo);
	gInfo = NULL;
	return status;
}


static void
uninit_common(void)
{
	delete_area(gInfo->shadow_area);
	delete_area(gInfo->registers_area);
	delete_area(gInfo->shared_info_area);

	if (gInfo->is_clone)
		close(gInfo->device);

	free(gInfo);
	gInfo = NULL;
}


//	#pragma mark - general hooks


static status_t
newport_init_accelerant(int device)
{
	status_t status = init_common(device, false);
	if (status != B_OK)
		return status;

	gInfo->shared_info->mode_count = 1;
	newport_reset_drawing_state();
	return B_OK;
}


static ssize_t
newport_accelerant_clone_info_size(void)
{
	return B_PATH_NAME_LENGTH;
}


static void
newport_get_accelerant_clone_info(void* info)
{
	ioctl(gInfo->device, NEWPORT_GET_DEVICE_NAME, info, B_PATH_NAME_LENGTH);
}


static status_t
newport_clone_accelerant(void* info)
{
	char path[MAXPATHLEN];
	strcpy(path, "/dev/");
	strlcat(path, (const char*)info, sizeof(path));

	int fd = open(path, B_READ_WRITE);
	if (fd < 0)
		return errno;

	status_t status = init_common(fd, true);
	if (status != B_OK)
		close(fd);
	return status;
}


static void
newport_uninit_accelerant(void)
{
	uninit_common();
}


static status_t
newport_get_accelerant_device_info(accelerant_device_info* info)
{
	info->version = B_ACCELERANT_VERSION;
	strcpy(info->name, "Newport");
	strcpy(info->chipset, gInfo->shared_info->bitplanes == 8
		? "REX3 (XL, 8 bit)" : "REX3 (XL, 24 bit)");
	strcpy(info->serial_no, "None");
	info->memory = NEWPORT_BYTES_PER_ROW * NEWPORT_HEIGHT;
	info->dac_speed = 0;
	return B_OK;
}


static sem_id
newport_accelerant_retrace_semaphore(void)
{
	return -1;
}


//	#pragma mark - mode hooks


static uint32
newport_accelerant_mode_count(void)
{
	return 1;
}


static status_t
newport_get_mode_list(display_mode* modeList)
{
	*modeList = gInfo->shared_info->current_mode;
	return B_OK;
}


static status_t
newport_propose_display_mode(display_mode* target, const display_mode* low,
	const display_mode* high)
{
	const display_mode& mode = gInfo->shared_info->current_mode;
	if (target->virtual_width != mode.virtual_width
		|| target->virtual_height != mode.virtual_height
		|| target->space != mode.space) {
		return B_BAD_VALUE;
	}

	*target = mode;
	return B_OK;
}


static status_t
newport_set_display_mode(display_mode* mode)
{
	const display_mode& current = gInfo->shared_info->current_mode;
	if (mode == NULL || mode->virtual_width != current.virtual_width
		|| mode->virtual_height != current.virtual_height
		|| mode->space != current.space) {
		return B_UNSUPPORTED;
	}

	return B_OK;
}


static status_t
newport_get_display_mode(display_mode* currentMode)
{
	*currentMode = gInfo->shared_info->current_mode;
	return B_OK;
}


static status_t
newport_get_frame_buffer_config(frame_buffer_config* config)
{
	config->frame_buffer = gInfo->shadow;
	config->frame_buffer_dma = NULL;
	config->bytes_per_row = gInfo->shared_info->bytes_per_row;
	return B_OK;
}


static status_t
newport_get_pixel_clock_limits(display_mode* mode, uint32* _low,
	uint32* _high)
{
	*_low = mode->timing.pixel_clock;
	*_high = mode->timing.pixel_clock;
	return B_OK;
}


//	#pragma mark -


extern "C" void*
get_accelerant_hook(uint32 feature, void* data)
{
	switch (feature) {
		/* general */
		case B_INIT_ACCELERANT:
			return (void*)newport_init_accelerant;
		case B_UNINIT_ACCELERANT:
			return (void*)newport_uninit_accelerant;
		case B_CLONE_ACCELERANT:
			return (void*)newport_clone_accelerant;
		case B_ACCELERANT_CLONE_INFO_SIZE:
			return (void*)newport_accelerant_clone_info_size;
		case B_GET_ACCELERANT_CLONE_INFO:
			return (void*)newport_get_accelerant_clone_info;
		case B_GET_ACCELERANT_DEVICE_INFO:
			return (void*)newport_get_accelerant_device_info;
		case B_ACCELERANT_RETRACE_SEMAPHORE:
			return (void*)newport_accelerant_retrace_semaphore;

		/* mode configuration */
		case B_ACCELERANT_MODE_COUNT:
			return (void*)newport_accelerant_mode_count;
		case B_GET_MODE_LIST:
			return (void*)newport_get_mode_list;
		case B_PROPOSE_DISPLAY_MODE:
			return (void*)newport_propose_display_mode;
		case B_SET_DISPLAY_MODE:
			return (void*)newport_set_display_mode;
		case B_GET_DISPLAY_MODE:
			return (void*)newport_get_display_mode;
		case B_GET_FRAME_BUFFER_CONFIG:
			return (void*)newport_get_frame_buffer_config;
		case B_GET_PIXEL_CLOCK_LIMITS:
			return (void*)newport_get_pixel_clock_limits;
		case B_SET_INDEXED_COLORS:
			return (void*)newport_set_indexed_colors;

		/* the screen is updated from the shadow frame buffer */
		case (uint32)B_FRONT_BUFFER_UPDATED:
			return (void*)newport_front_buffer_updated;
	}

	return NULL;
}
