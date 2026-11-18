/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

// n32: every argument and the return value occupy a 64 bit slot.
#define SYSCALL_RETURN_TYPE_ALIGNMENT_TYPE	long long
#define SYSCALL_PARAMETER_ALIGNMENT_TYPE	long long
