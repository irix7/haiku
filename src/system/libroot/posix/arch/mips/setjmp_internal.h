/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SETJMP_INTERNAL_H
#define SETJMP_INTERNAL_H


/* Offsets in __jmp_buf, see headers/posix/arch/mips/arch_setjmp.h */
#define JMP_S(n)	((n) * 8)		/* $16 to $23 */
#define JMP_GP		64
#define JMP_SP		72
#define JMP_FP		80
#define JMP_RA		88
#define JMP_F(n)	(96 + (n) * 8)	/* $f20, $f22, ... $f30 */
#define JMP_FCSR	144


#endif	/* SETJMP_INTERNAL_H */
