/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _MIPS_LIMITS_H
#define _MIPS_LIMITS_H

/* The page size is 16 kB: each data cache way of the supported CPUs is at
   most 16 kB, so a page has the same cache index at every virtual address. */
#define PAGESIZE 16384

#endif
