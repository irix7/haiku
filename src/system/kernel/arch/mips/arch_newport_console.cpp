/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	Text console on the Newport (XL) board, for the kernel's debug output.

	Newport has no frame buffer the CPU can address. Every glyph is a block
	the REX3 chip draws from pixels sent through its host data port. The
	sequences are those of the newport accelerant.

	The size of the picture is read from the VC2, see detect_screen_size().

	The console wraps: after the last row it continues at the top. The row
	below the current one is cleared, so the blank row marks the end of the
	text.

	The console is on from the start of the kernel when the PROM reports a
	working display ("gfx=alive"). mips_newport_console_stop() turns it off
	when the newport driver takes the board over. In the kernel debugger it
	draws again.
*/


#include <arch_indy.h>
#include <boot/kernel_args.h>
#include <debug.h>
#include <kernel.h>

#include <newport_info.h>

#include "../../debug/font.h"


#define COLOR_BACKGROUND	0
#define COLOR_TEXT			1

#define IDLE_TRIES			200000


static bool sPresent;
static bool sStopped;
static bool sInDebugger;
static int32 sColumns;
static int32 sRows;
static int32 sColumn;
static int32 sRow;
static bool sWasPresent;
static uint32 sWidth;
static uint32 sHeight;
static uint32 sDetectedWidth;
static uint32 sDetectedHeight;


static inline uint32
read32(uint32 offset)
{
	return *(volatile uint32*)MIPS_KSEG1(INDY_REX3_BASE + offset);
}


static inline void
write32(uint32 offset, uint32 value)
{
	*(volatile uint32*)MIPS_KSEG1(INDY_REX3_BASE + offset) = value;
}


/*!	Waits until the chip accepts a command. The wait is bounded: a board
	that stays busy turns the console off.
*/
static bool
wait_idle(uint32 busy)
{
	for (int32 tries = 0; (read32(REX3_STATUS) & busy) != 0; tries++) {
		if (tries > IDLE_TRIES) {
			sPresent = false;
			return false;
		}
	}
	return true;
}


/*!	Returns the next 16 bit word of what REX3_DCBMODE selects. */
static uint16
vc2_read_ram()
{
	if (!wait_idle(REX3_STATUS_BACK_BUSY))
		return 0;
	return *(volatile uint16*)MIPS_KSEG1(INDY_REX3_BASE + REX3_DCBDATA0 + 2);
}


static uint16
vc2_read_register(uint8 index)
{
	if (!wait_idle(REX3_STATUS_BACK_BUSY))
		return 0;
	write32(REX3_DCBMODE, REX3_DCB_VC2_INDEX);
	*(volatile uint8*)MIPS_KSEG1(INDY_REX3_BASE + REX3_DCBDATA0 + 3) = index;
	write32(REX3_DCBMODE, REX3_DCB_VC2_REGISTER);
	return vc2_read_ram();
}


/*!	Selects the word of the VC2's RAM that the next vc2_read_ram() returns.
*/
static void
vc2_set_ram_address(uint16 address)
{
	if (!wait_idle(REX3_STATUS_BACK_BUSY))
		return;
	write32(REX3_DCBMODE, REX3_DCB_VC2_INDEX_DATA);
	write32(REX3_DCBDATA0,
		((uint32)VC2_RAM_ADDRESS << 24) | ((uint32)address << 8));
	write32(REX3_DCBMODE, REX3_DCB_VC2_RAM);
}


/*!	Reads the size of the picture from the video timing table the PROM has
	loaded into the VC2: a list of (line description, line count) pairs. A
	line description is a list of runs with their length in pixel clocks.
	The method is that of the Linux console driver for the board. The PROM
	chooses the mode by the monitor it senses, 1280 x 1024 or 1024 x 768.
	A result that is no plausible mode gives 1024 x 768.
*/
static void
detect_screen_size()
{
	// every monitor mode of the Indy shows at least this
	sWidth = 1024;
	sHeight = 768;

	uint16 table[128];
	vc2_set_ram_address(vc2_read_register(VC2_VIDEO_ENTRY));
	for (uint32 i = 0; i < B_COUNT_OF(table); i++)
		table[i] = vc2_read_ram();

	uint32 width = 0;
	uint32 height = 0;
	for (uint32 i = 0; i + 1 < B_COUNT_OF(table) && table[i + 1] != 0;
			i += 2) {
		uint32 pixels = 0;
		vc2_set_ram_address(table[i]);
		for (uint32 run = 0; run < 256; run++) {
			uint16 word = vc2_read_ram();
			if ((word & 0x0001) == 0)
				pixels += (word >> 7) & 0xfe;
			if ((word & 0x0080) == 0)
				word = vc2_read_ram();
			if ((word & 0x8000) != 0)
				break;
		}
		if (pixels != 0) {
			if (pixels > width)
				width = pixels;
			height += table[i + 1];
		}
	}

	if (!sPresent && sWasPresent) {
		// the board did not answer
		return;
	}
	// The count is a few pixels over the nominal width, 1282 for 1280.
	if (width >= 640 && width <= NEWPORT_WIDTH + 20 && height >= 480
		&& height <= NEWPORT_HEIGHT) {
		sWidth = width > NEWPORT_WIDTH ? NEWPORT_WIDTH : width & ~3;
		sHeight = height;
	}
	sDetectedWidth = width;
	sDetectedHeight = height;
}


void
mips_newport_screen_size(uint32* _width, uint32* _height)
{
	if (sWidth == 0) {
		sWasPresent = sPresent;
		detect_screen_size();
	}
	*_width = sWidth;
	*_height = sHeight;
}


static void
set_color(uint8 index, uint32 rgb)
{
	if (!wait_idle(REX3_STATUS_BACK_BUSY))
		return;
	write32(REX3_DCBMODE, REX3_DCB_CMAP_ADDRESS);
	write32(REX3_DCBDATA0, (uint32)index << 16);
	write32(REX3_DCBMODE, REX3_DCB_CMAP_PALETTE);
	write32(REX3_DCBDATA0, rgb << 8);
}


/*!	The PROM and the accelerant leave their own drawing state in the chip.
	Coordinates become screen pixels biased by 4096, nothing is clipped.
*/
static void
reset_drawing_state()
{
	if (!wait_idle(REX3_STATUS_GFX_BUSY))
		return;
	write32(REX3_LSMODE, 0);
	write32(REX3_LSPATTERN, 0);
	write32(REX3_LSPATSAVE, 0);
	write32(REX3_ZPATTERN, 0);
	write32(REX3_SMASK0X, 0);
	write32(REX3_SMASK0Y, 0);
	write32(REX3_XSAVE, 0);
	write32(REX3_XYMOVE, 0);
	for (uint32 offset = 0; offset < 0x20; offset += 4)
		write32(REX3_SMASK1X + offset, 0);
	write32(REX3_XYWIN, 0);
	write32(REX3_TOPSCAN, 0x3ff);
	write32(REX3_CLIPMODE, 0xf << 9);

	set_color(COLOR_BACKGROUND, 0x000000);
	set_color(COLOR_TEXT, 0xffffff);
}


static void
fill(int32 x, int32 y, int32 width, int32 height, uint32 index)
{
	if (!wait_idle(REX3_STATUS_GFX_BUSY))
		return;
	write32(REX3_DRAWMODE1, REX3_DRAWMODE1_CI8);
	write32(REX3_WRMASK, 0xffffffff);
	write32(REX3_COLORI, index);
	write32(REX3_XYENDI, REX3_XY(x + width - 1, y + height - 1));
	write32(REX3_XYSTARTI, REX3_XY(x, y));
	write32(REX3_DRAWMODE0 + REX3_GO, REX3_DRAWMODE0_FILL);
}


static void
clear_row(int32 row)
{
	fill(0, row * smallFont.glyphHeight, sWidth, smallFont.glyphHeight,
		COLOR_BACKGROUND);
}


/*!	Draws one glyph. A host word carries four pixels, the first in the most
	significant byte, and every row of the block starts with a new word.
*/
static void
draw_glyph(int32 column, int32 row, uint8 glyph)
{
	const int32 width = smallFont.glyphWidth;
	const int32 height = smallFont.glyphHeight;
	const int32 x = column * width;
	const int32 y = row * height;

	if (glyph > 127)
		glyph = 127;

	if (!wait_idle(REX3_STATUS_GFX_BUSY))
		return;
	write32(REX3_DRAWMODE1, REX3_DRAWMODE1_CI8);
	write32(REX3_WRMASK, 0xffffffff);
	write32(REX3_XYSTARTI, REX3_XY(x, y));
	write32(REX3_XYENDI, REX3_XY(x + width - 1, y + height - 1));
	write32(REX3_DRAWMODE0, REX3_DRAWMODE0_UPLOAD);

	for (int32 line = 0; line < height; line++) {
		// bit 0 is the leftmost pixel
		uint8 bits = smallFont.data[height * glyph + line];
		for (int32 start = 0; start < width; start += 4) {
			uint32 word = 0;
			for (int32 i = 0; i < 4; i++) {
				uint32 pixel = COLOR_BACKGROUND;
				if (start + i < width && (bits & (1 << (start + i))) != 0)
					pixel = COLOR_TEXT;
				word |= pixel << (24 - 8 * i);
			}
			write32(REX3_HOSTRW0 + REX3_GO, word);
		}
	}
}


static void
next_row()
{
	sColumn = 0;
	if (++sRow >= sRows)
		sRow = 0;

	clear_row(sRow);
	clear_row(sRow + 1 < sRows ? sRow + 1 : 0);
}


static void
start()
{
	sColumns = sWidth / smallFont.glyphWidth;
	sRows = sHeight / smallFont.glyphHeight;
	sColumn = 0;
	sRow = 0;

	reset_drawing_state();
	fill(0, 0, sWidth, sHeight, COLOR_BACKGROUND);
}


void
mips_newport_console_init(kernel_args* args)
{
	sPresent = args->platform_args.prom_graphics != 0;
	if (!sPresent)
		return;

	sWasPresent = true;
	detect_screen_size();
	if (!sPresent)
		return;
	start();
	dprintf("newport console: %" B_PRIu32 " x %" B_PRIu32 " (video table: %"
		B_PRIu32 " x %" B_PRIu32 ")\n", sWidth, sHeight, sDetectedWidth,
		sDetectedHeight);
}


/*!	The newport driver calls this when it takes the board over. */
void
mips_newport_console_stop(void)
{
	sStopped = true;
}


void
mips_newport_console_putchar(char c)
{
	if (!sPresent)
		return;

	if (sStopped) {
		// Only the kernel debugger draws over the desktop.
		if (!debug_debugger_running()) {
			sInDebugger = false;
			return;
		}
		if (!sInDebugger) {
			sInDebugger = true;
			start();
			if (!sPresent)
				return;
		}
	}

	switch (c) {
		case '\n':
			next_row();
			return;
		case '\r':
			sColumn = 0;
			return;
		case '\t':
			c = ' ';
			break;
		case '\b':
			if (sColumn > 0)
				draw_glyph(--sColumn, sRow, ' ');
			return;
	}

	if ((uint8)c < ' ')
		return;

	if (sColumn >= sColumns)
		next_row();
	draw_glyph(sColumn++, sRow, (uint8)c);
}
