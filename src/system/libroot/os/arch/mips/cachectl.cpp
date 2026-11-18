/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <image.h>


/*!	Called by code the compiler emits for trampolines and for
	__builtin___clear_cache().
*/
extern "C" void
_flush_cache(char* buffer, int size, int flag)
{
	clear_caches(buffer, size, B_INVALIDATE_ICACHE | B_FLUSH_DCACHE);
}
