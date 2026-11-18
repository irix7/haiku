/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _SYSTEM_ARCH_MIPS_CONFIG_H
#define _SYSTEM_ARCH_MIPS_CONFIG_H


/* n32: every argument slot is 64 bits wide. */
#define FUNCTION_CALL_PARAMETER_ALIGNMENT_TYPE	unsigned long long

#define STACK_GROWS_DOWNWARDS

/* 32 and 64 bit atomics are inline ll/sc and lld/scd sequences. */
//#define ATOMIC_FUNCS_ARE_SYSCALLS
//#define ATOMIC64_FUNCS_ARE_SYSCALLS


#endif	/* _SYSTEM_ARCH_MIPS_CONFIG_H */
