/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef FRONT_BUFFER_HOOK_H
#define FRONT_BUFFER_HOOK_H


#include <Accelerant.h>


/*
 * Optional accelerant hook for displays whose frame buffer the CPU cannot
 * address. The accelerant returns a buffer in RAM as its frame buffer, and
 * app_server calls this hook for every rectangle it has written to that
 * buffer. The accelerant then copies the rectangle to the screen.
 * The coordinates are inclusive.
 */
enum {
	B_FRONT_BUFFER_UPDATED = B_ACCELERANT_PRIVATE_START + 0x100
};

typedef void (*front_buffer_updated)(int32 left, int32 top, int32 right,
	int32 bottom);


#endif	/* FRONT_BUFFER_HOOK_H */
