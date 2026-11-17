/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * n32 syscall stubs. The syscall number is in $2. The arguments are where
 * the C calling convention puts them: the first eight in $4 to $11, the
 * rest on the stack. The result is in $2.
 */

#define _SYSCALL(name, n) \
	.globl name; \
	.type name, @function; \
	.ent name; \
	.align 2; \
name: \
	.set push; \
	.set noreorder; \
	li	$2, n; \
	syscall; \
	jr	$31; \
	 nop; \
	.set pop; \
	.end name; \
	.size name, . - name

#define SYSCALL0(name, n) _SYSCALL(name, n)
#define SYSCALL1(name, n) _SYSCALL(name, n)
#define SYSCALL2(name, n) _SYSCALL(name, n)
#define SYSCALL3(name, n) _SYSCALL(name, n)
#define SYSCALL4(name, n) _SYSCALL(name, n)
#define SYSCALL5(name, n) _SYSCALL(name, n)
#define SYSCALL6(name, n) _SYSCALL(name, n)
#define SYSCALL7(name, n) _SYSCALL(name, n)
#define SYSCALL8(name, n) _SYSCALL(name, n)
#define SYSCALL9(name, n) _SYSCALL(name, n)
#define SYSCALL10(name, n) _SYSCALL(name, n)
#define SYSCALL11(name, n) _SYSCALL(name, n)
#define SYSCALL12(name, n) _SYSCALL(name, n)
#define SYSCALL13(name, n) _SYSCALL(name, n)
#define SYSCALL14(name, n) _SYSCALL(name, n)
#define SYSCALL15(name, n) _SYSCALL(name, n)
#define SYSCALL16(name, n) _SYSCALL(name, n)
#define SYSCALL17(name, n) _SYSCALL(name, n)
#define SYSCALL18(name, n) _SYSCALL(name, n)
#define SYSCALL19(name, n) _SYSCALL(name, n)
#define SYSCALL20(name, n) _SYSCALL(name, n)
