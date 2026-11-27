/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef NEWPORT_INFO_H
#define NEWPORT_INFO_H


#include <Accelerant.h>
#include <Drivers.h>


/*
 * Interface between the newport kernel driver and its accelerant.
 *
 * Newport (REX3) has no frame buffer the CPU can address. The driver creates
 * a shadow buffer in RAM, which the accelerant hands out as the frame
 * buffer, and maps the REX3 registers. The accelerant copies changed
 * rectangles from the shadow to the screen through the REX3 host data port.
 * The only mode is the one the PROM has set: 1280 x 1024 or 1024 x 768,
 * 8 bit colour index. The 8 bit Newport XL board has no other depth.
 * NEWPORT_WIDTH and NEWPORT_HEIGHT are the largest picture; the shadow
 * buffer always has this size. current_mode holds the size in use.
 */

#define NEWPORT_WIDTH			1280
#define NEWPORT_HEIGHT			1024
#define NEWPORT_BYTES_PER_ROW	1280

struct newport_shared_info {
	area_id			registers_area;		// REX3 registers
	area_id			shadow_area;		// the shadow frame buffer
	area_id			mode_list_area;		// set by the accelerant
	uint32			mode_count;
	display_mode	current_mode;
	uint32			bytes_per_row;
	uint32			bitplanes;			// 8 or 24, as the board reports
	bool			double_host_writes;
		// whether the accelerant sends 8 pixels per 64 bit store
};

enum {
	NEWPORT_GET_PRIVATE_DATA = B_DEVICE_OP_CODES_END + 1,
	NEWPORT_GET_DEVICE_NAME,
};


/* REX3 registers: offsets from the start of the register area */
#define REX3_DRAWMODE1			0x0000
#define REX3_DRAWMODE0			0x0004
#define REX3_LSMODE				0x0008
#define REX3_LSPATTERN			0x000c
#define REX3_LSPATSAVE			0x0010
#define REX3_ZPATTERN			0x0014
#define REX3_SMASK0X			0x0028
#define REX3_SMASK0Y			0x002c
#define REX3_XSAVE				0x0110
#define REX3_XYMOVE				0x0114
#define REX3_XYSTARTI			0x0150
#define REX3_XYENDI				0x0154
#define REX3_WRMASK				0x0220
#define REX3_COLORI				0x0224
#define REX3_HOSTRW0			0x0230
#define REX3_DCBMODE			0x0238
#define REX3_DCBDATA0			0x0240
#define REX3_SMASK1X			0x1300
#define REX3_TOPSCAN			0x1320
#define REX3_XYWIN				0x1324
#define REX3_CLIPMODE			0x1328
#define REX3_STATUS				0x1338
/* A write to a register at this offset from its address starts the command. */
#define REX3_GO					0x0800

#define REX3_STATUS_GFX_BUSY	0x08
#define REX3_STATUS_BACK_BUSY	0x10

/* planes RGB/CI, draw depth 8, packed host words, host depth 8, compare
   always, logic op source */
#define REX3_DRAWMODE1_CI8		0x30007189
#define REX3_DRAWMODE1_RWDOUBLE	0x00000400
/* draw, block, stop on x and y, with iterator setup */
#define REX3_DRAWMODE0_FILL		0x00000326
/* draw, block, colour from the host data port */
#define REX3_DRAWMODE0_UPLOAD	0x00000046

/* DCB access to the colour map: address register, then palette register */
#define REX3_DCB_CMAP_ADDRESS \
	((1 << 7) | (0 << 4) | 2 | 0x00848000 | (1 << 28) | (1 << 3))
#define REX3_DCB_CMAP_PALETTE	((1 << 7) | (2 << 4) | 3 | 0x00848000)
/* DCB access to the VC2 video controller: the index register (one byte,
   written to the low byte of the data register), the index register with
   the 16 bit value of the register in one write, the value of the indexed
   register, the RAM at VC2_RAM_ADDRESS (which counts up). Values are read
   from the low half of the data register. */
#define REX3_DCB_VC2_PROTOCOL	0x00841000
#define REX3_DCB_VC2_INDEX		(0x00 | 0x08 | 1 | REX3_DCB_VC2_PROTOCOL)
#define REX3_DCB_VC2_INDEX_DATA	(0x00 | 0x08 | 3 | REX3_DCB_VC2_PROTOCOL)
#define REX3_DCB_VC2_REGISTER	(0x10 | 0x08 | 2 | REX3_DCB_VC2_PROTOCOL)
#define REX3_DCB_VC2_RAM		(0x30 | 2 | REX3_DCB_VC2_PROTOCOL)
#define VC2_VIDEO_ENTRY			0x00	/* start of the video timing table */
#define VC2_RAM_ADDRESS			0x07
/* DCB access to the colour map's revision register, one byte */
#define REX3_DCB_CMAP_REVISION	((2 << 7) | (6 << 4) | 1 | 0x00848000)

/* Coordinates are screen pixels plus 4096 once XYWIN is 0. */
#define REX3_XY(x, y)			((((x) + 4096) << 16) | ((y) + 4096))


#endif	/* NEWPORT_INFO_H */
