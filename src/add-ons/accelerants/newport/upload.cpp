/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	REX3 access: palette, and the copy of a rectangle from the shadow frame
	buffer to the screen through the host data port. The sequences are those
	of the test image in indy/m0, which was verified on the iris emulator.
*/


#include "accelerant.h"

#include <string.h>


static inline uint32
read32(uint32 offset)
{
	return *(volatile uint32*)(gInfo->registers + offset);
}


static inline void
write32(uint32 offset, uint32 value)
{
	*(volatile uint32*)(gInfo->registers + offset) = value;
}


static inline void
write64(uint32 offset, uint64 value)
{
	// one 64 bit store: the ABI is n32 and the CPU has 64 bit registers
	*(volatile uint64*)(gInfo->registers + offset) = value;
}


static void
wait_idle(void)
{
	while ((read32(REX3_STATUS) & REX3_STATUS_GFX_BUSY) != 0)
		;
}


/*!	The PROM leaves its own window offset, clip mode and screen masks in the
	chip. Reset the drawing state so that coordinates are screen pixels
	biased by 4096 and nothing is clipped.
*/
void
newport_reset_drawing_state(void)
{
	wait_idle();
	write32(REX3_LSMODE, 0);
	write32(REX3_LSPATTERN, 0);
	write32(REX3_LSPATSAVE, 0);
	write32(REX3_ZPATTERN, 0);
	write32(REX3_SMASK0X, 0);
	write32(REX3_SMASK0Y, 0);
	write32(REX3_XSAVE, 0);
	write32(REX3_XYMOVE, 0);
	// screen masks 1 to 4, x and y
	for (uint32 offset = 0; offset < 0x20; offset += 4)
		write32(REX3_SMASK1X + offset, 0);
	write32(REX3_XYWIN, 0);
	write32(REX3_TOPSCAN, 0x3ff);
	// no screen mask enabled, every CID value passes
	write32(REX3_CLIPMODE, 0xf << 9);
}


void
newport_set_indexed_colors(uint count, uint8 first, uint8* colorData,
	uint32 flags)
{
	for (uint i = 0; i < count && first + i < 256; i++) {
		uint32 rgb = ((uint32)colorData[0] << 16)
			| ((uint32)colorData[1] << 8) | colorData[2];
		colorData += 3;

		while ((read32(REX3_STATUS) & REX3_STATUS_BACK_BUSY) != 0)
			;
		write32(REX3_DCBMODE, REX3_DCB_CMAP_ADDRESS);
		write32(REX3_DCBDATA0, (uint32)(first + i) << 16);
		write32(REX3_DCBMODE, REX3_DCB_CMAP_PALETTE);
		write32(REX3_DCBDATA0, rgb << 8);
	}
}


/*!	Copies the rectangle (inclusive coordinates) from the shadow to the
	screen. A host word carries 8 pixels (64 bit store) or 4 pixels (32 bit
	store), the first pixel in the most significant byte. Every row starts
	with a new word, and the last word of a row may be partly used.
*/
void
newport_front_buffer_updated(int32 left, int32 top, int32 right, int32 bottom)
{
	if (left < 0)
		left = 0;
	if (top < 0)
		top = 0;
	const display_mode& mode = gInfo->shared_info->current_mode;
	if (right >= mode.virtual_width)
		right = mode.virtual_width - 1;
	if (bottom >= mode.virtual_height)
		bottom = mode.virtual_height - 1;
	if (left > right || top > bottom)
		return;

	bool doubleWrites = gInfo->shared_info->double_host_writes;
	uint32 width = right - left + 1;
	const uint8* row = gInfo->shadow + top * NEWPORT_BYTES_PER_ROW + left;

	wait_idle();
	write32(REX3_DRAWMODE1, REX3_DRAWMODE1_CI8
		| (doubleWrites ? REX3_DRAWMODE1_RWDOUBLE : 0));
	write32(REX3_WRMASK, 0xffffffff);
	write32(REX3_XYSTARTI, REX3_XY(left, top));
	write32(REX3_XYENDI, REX3_XY(right, bottom));
	write32(REX3_DRAWMODE0, REX3_DRAWMODE0_UPLOAD);

	for (int32 y = top; y <= bottom; y++, row += NEWPORT_BYTES_PER_ROW) {
		const uint8* source = row;
		uint32 remaining = width;

		if (doubleWrites) {
			for (; remaining >= 8; remaining -= 8, source += 8) {
				uint64 word;
				memcpy(&word, source, 8);
				write64(REX3_HOSTRW0 + REX3_GO, word);
			}
			if (remaining > 0) {
				uint64 word = 0;
				memcpy(&word, source, remaining);
				write64(REX3_HOSTRW0 + REX3_GO, word);
			}
		} else {
			for (; remaining >= 4; remaining -= 4, source += 4) {
				uint32 word;
				memcpy(&word, source, 4);
				write32(REX3_HOSTRW0 + REX3_GO, word);
			}
			if (remaining > 0) {
				uint32 word = 0;
				memcpy(&word, source, remaining);
				write32(REX3_HOSTRW0 + REX3_GO, word);
			}
		}
	}
}
