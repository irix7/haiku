/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef NEWPORT_ACCELERANT_H
#define NEWPORT_ACCELERANT_H


#include <newport_info.h>


typedef struct accelerant_info {
	int						device;
	bool					is_clone;

	area_id					shared_info_area;
	newport_shared_info*	shared_info;
	area_id					registers_area;
	volatile uint8*			registers;
	area_id					shadow_area;
	uint8*					shadow;
} accelerant_info;

extern accelerant_info* gInfo;


#ifdef __cplusplus
extern "C" {
#endif

// upload.cpp
void newport_reset_drawing_state(void);
void newport_set_indexed_colors(uint count, uint8 first, uint8* colorData,
	uint32 flags);
void newport_front_buffer_updated(int32 left, int32 top, int32 right,
	int32 bottom);

#ifdef __cplusplus
}
#endif


#endif	/* NEWPORT_ACCELERANT_H */
