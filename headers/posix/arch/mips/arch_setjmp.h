/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_SETJMP_H_
#define _ARCH_SETJMP_H_


/*
 * n32 callee-saved state, 8 bytes per slot:
 *   0 to 7    $16 to $23 (s0 to s7)
 *   8         $28 (gp)
 *   9         $29 (sp)
 *   10        $30 (fp)
 *   11        $31 (ra)
 *   12 to 17  $f20, $f22, $f24, $f26, $f28, $f30
 *   18        FCSR
 *   19        reserved
 */
#define _SETJMP_BUF_SZ 20
typedef unsigned long long __jmp_buf[_SETJMP_BUF_SZ];


#endif  /* _ARCH_SETJMP_H_ */
