/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 *
 * M0 test image for the SGI Indy. It prints a marker on the serial console.
 * Built with -DREX3_TEST it also loads a palette and uploads two gradients
 * through the REX3 host data port, which is the path the newport accelerant
 * uses.
 */

#define SCC_B_CONTROL	(*(volatile unsigned char*)0xBFBD9833)
#define SCC_B_DATA		(*(volatile unsigned char*)0xBFBD9837)
#define SCC_TX_EMPTY	0x04


static void
put_char(char c)
{
	while ((SCC_B_CONTROL & SCC_TX_EMPTY) == 0)
		;
	SCC_B_DATA = c;
}


static void
put_string(const char* string)
{
	while (*string != '\0')
		put_char(*string++);
}


#ifdef REX3_TEST

#define REX3_BASE			0xBF0F0000
#define REX3(offset)		(*(volatile unsigned int*)(REX3_BASE + (offset)))
#define REX3_64(offset) \
	(*(volatile unsigned long long*)(REX3_BASE + (offset)))

#define REX3_DRAWMODE1		0x0000
#define REX3_DRAWMODE0		0x0004
#define REX3_LSMODE			0x0008
#define REX3_LSPATTERN		0x000c
#define REX3_LSPATSAVE		0x0010
#define REX3_ZPATTERN		0x0014
#define REX3_SMASK0X		0x0028
#define REX3_SMASK0Y		0x002c
#define REX3_XSAVE			0x0110
#define REX3_XYMOVE			0x0114
#define REX3_XYSTARTI		0x0150
#define REX3_XYENDI			0x0154
#define REX3_WRMASK			0x0220
#define REX3_COLORI			0x0224
#define REX3_HOSTRW0		0x0230
#define REX3_DCBMODE		0x0238
#define REX3_DCBDATA0		0x0240
#define REX3_SMASK1X		0x1300
#define REX3_TOPSCAN		0x1320
#define REX3_XYWIN			0x1324
#define REX3_CLIPMODE		0x1328
#define REX3_STATUS			0x1338
#define REX3_GO				0x0800

#define STATUS_GFX_BUSY		0x08
#define STATUS_BACK_BUSY	0x10

/* planes RGB/CI, draw depth 8, packed host words, host depth 8, compare
   always, logic op source */
#define DRAWMODE1_CI8		0x30007189
#define DRAWMODE1_RWDOUBLE	0x00000400

/* draw, block, stop on x and y, with iterator setup */
#define DRAWMODE0_FILL		0x00000326
/* draw, block, colour from the host port */
#define DRAWMODE0_UPLOAD	0x00000046

#define DCB_CMAP_ADDRESS \
	((1 << 7) | (0 << 4) | 2 | 0x00848000 | (1 << 28) | (1 << 3))
#define DCB_CMAP_PALETTE	((1 << 7) | (2 << 4) | 3 | 0x00848000)

#define XY(x, y)	((((x) + 4096) << 16) | ((y) + 4096))


static void
wait_idle(void)
{
	while ((REX3(REX3_STATUS) & STATUS_GFX_BUSY) != 0)
		;
}


/* The PROM leaves its own window offset, clip mode and screen masks in the
   chip. Reset the drawing state so that coordinates are screen pixels biased
   by 4096 and nothing is clipped. */
static void
rex3_init(void)
{
	unsigned int offset;

	wait_idle();
	REX3(REX3_LSMODE) = 0;
	REX3(REX3_LSPATTERN) = 0;
	REX3(REX3_LSPATSAVE) = 0;
	REX3(REX3_ZPATTERN) = 0;
	REX3(REX3_SMASK0X) = 0;
	REX3(REX3_SMASK0Y) = 0;
	REX3(REX3_XSAVE) = 0;
	REX3(REX3_XYMOVE) = 0;
	/* screen masks 1 to 4, x and y */
	for (offset = 0; offset < 0x20; offset += 4)
		REX3(REX3_SMASK1X + offset) = 0;
	REX3(REX3_XYWIN) = 0;
	REX3(REX3_TOPSCAN) = 0x3ff;
	/* no screen mask enabled, every CID value passes */
	REX3(REX3_CLIPMODE) = 0xf << 9;
}


static void
set_palette_entry(unsigned int index, unsigned int rgb)
{
	while ((REX3(REX3_STATUS) & STATUS_BACK_BUSY) != 0)
		;
	REX3(REX3_DCBMODE) = DCB_CMAP_ADDRESS;
	REX3(REX3_DCBDATA0) = index << 16;
	REX3(REX3_DCBMODE) = DCB_CMAP_PALETTE;
	REX3(REX3_DCBDATA0) = rgb << 8;
}


static void
fill(int x, int y, int width, int height, unsigned int index)
{
	wait_idle();
	REX3(REX3_DRAWMODE1) = DRAWMODE1_CI8;
	REX3(REX3_WRMASK) = 0xffffffff;
	REX3(REX3_COLORI) = index;
	REX3(REX3_XYENDI) = XY(x + width - 1, y + height - 1);
	REX3(REX3_XYSTARTI) = XY(x, y);
	REX3(REX3_DRAWMODE0 + REX3_GO) = DRAWMODE0_FILL;
}


/* The pixel of the test pattern: the palette index rises with x + y. */
static inline unsigned int
pattern(int x, int y)
{
	return ((x + y) >> 1) & 0xff;
}


/* Uploads the pattern with 32-bit stores, 4 pixels per store. */
static void
upload_32(int left, int top, int width, int height)
{
	int x, y;

	wait_idle();
	REX3(REX3_DRAWMODE1) = DRAWMODE1_CI8;
	REX3(REX3_WRMASK) = 0xffffffff;
	REX3(REX3_XYSTARTI) = XY(left, top);
	REX3(REX3_XYENDI) = XY(left + width - 1, top + height - 1);
	REX3(REX3_DRAWMODE0) = DRAWMODE0_UPLOAD;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x += 4) {
			unsigned int word = (pattern(x, y) << 24)
				| (pattern(x + 1, y) << 16) | (pattern(x + 2, y) << 8)
				| pattern(x + 3, y);
			REX3(REX3_HOSTRW0 + REX3_GO) = word;
		}
	}
}


/* Uploads the pattern with 64-bit stores, 8 pixels per store. */
static void
upload_64(int left, int top, int width, int height)
{
	int x, y, i;

	wait_idle();
	REX3(REX3_DRAWMODE1) = DRAWMODE1_CI8 | DRAWMODE1_RWDOUBLE;
	REX3(REX3_WRMASK) = 0xffffffff;
	REX3(REX3_XYSTARTI) = XY(left, top);
	REX3(REX3_XYENDI) = XY(left + width - 1, top + height - 1);
	REX3(REX3_DRAWMODE0) = DRAWMODE0_UPLOAD;

	for (y = 0; y < height; y++) {
		for (x = 0; x < width; x += 8) {
			unsigned long long word = 0;
			for (i = 0; i < 8; i++)
				word = (word << 8) | pattern(x + i, y);
			REX3_64(REX3_HOSTRW0 + REX3_GO) = word;
		}
	}
}


static void
rex3_test(void)
{
	unsigned int i;

	rex3_init();

	/* a grey ramp in entries 0 to 127, a red ramp in 128 to 255 */
	for (i = 0; i < 128; i++)
		set_palette_entry(i, (i * 2) * 0x010101);
	for (i = 128; i < 256; i++)
		set_palette_entry(i, ((i - 128) * 2) << 16);

	fill(0, 0, 1280, 1024, 40);
	upload_32(64, 64, 256, 256);
	upload_64(384, 64, 256, 256);
	/* an odd width: each row ends with a partly used word */
	upload_32(64, 384, 253, 64);
	upload_64(384, 384, 253, 64);
	wait_idle();

	put_string("haiku: m0 rex3 done\r\n");
}

#endif	/* REX3_TEST */


void
hello_main(void)
{
	put_string("haiku: m0 alive\r\n");

#ifdef REX3_TEST
	rex3_test();
#endif

	for (;;)
		;
}
