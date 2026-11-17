/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <boot/platform.h>
#include <boot/platform/generic/video.h>
#include <boot/stage2.h>


/*!	Newport has no frame buffer the CPU can address, so the loader shows no
	boot logo and the kernel gets no frame buffer console. The loader's
	console is the serial port.
*/


extern "C" void
platform_set_palette(const uint8* palette)
{
}


extern "C" void
platform_blit4(addr_t frameBuffer, const uint8* data, uint16 width,
	uint16 height, uint16 imageWidth, uint16 left, uint16 top)
{
}


extern "C" void
platform_switch_to_logo(void)
{
}


extern "C" void
platform_switch_to_text_mode(void)
{
}


extern "C" status_t
platform_init_video(void)
{
	gKernelArgs.frame_buffer.enabled = false;
	return B_OK;
}
