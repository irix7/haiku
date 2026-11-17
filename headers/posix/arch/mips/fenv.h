/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _ARCH_MIPS_FENV_H_
#define _ARCH_MIPS_FENV_H_


#include <sys/cdefs.h>


/* The environment is the value of the FPU control and status register. */
typedef unsigned int fenv_t;
typedef unsigned int fexcept_t;

/* Exception flags (FCSR bits 6 to 2) */
#define FE_INEXACT		0x0004
#define FE_UNDERFLOW	0x0008
#define FE_OVERFLOW		0x0010
#define FE_DIVBYZERO	0x0020
#define FE_INVALID		0x0040
#define FE_ALL_EXCEPT	(FE_DIVBYZERO | FE_INEXACT | FE_INVALID \
							| FE_OVERFLOW | FE_UNDERFLOW)

/* Rounding modes (FCSR bits 1 to 0) */
#define FE_TONEAREST	0x0000
#define FE_TOWARDZERO	0x0001
#define FE_UPWARD		0x0002
#define FE_DOWNWARD		0x0003

/* The enable bits are the flag bits shifted left by 5, the cause bits are
   the flag bits shifted left by 10. */
#define _FCSR_ROUND_MASK	0x0003
#define _FCSR_ENABLE_SHIFT	5
#define _FCSR_CAUSE_SHIFT	10
/* Flush denormal results to zero. Set in the default environment, because
   the R4600 traps to the kernel for denormal operands and results. */
#define _FCSR_FLUSH_TO_ZERO	0x01000000


__BEGIN_DECLS

/* Default floating-point environment */
extern const fenv_t __fe_dfl_env;
#define FE_DFL_ENV	(&__fe_dfl_env)

int feclearexcept(int __excepts);
int fegetexceptflag(fexcept_t* __flagp, int __excepts);
int fesetexceptflag(const fexcept_t* __flagp, int __excepts);
int feraiseexcept(int __excepts);
int fetestexcept(int __excepts);
int fegetround(void);
int fesetround(int __round);
int fegetenv(fenv_t* __envp);
int feholdexcept(fenv_t* __envp);
int fesetenv(const fenv_t* __envp);
int feupdateenv(const fenv_t* __envp);

#if defined(_DEFAULT_SOURCE)

int feenableexcept(int __mask);
int fedisableexcept(int __mask);
int fegetexcept(void);

#endif /* _DEFAULT_SOURCE */

__END_DECLS


#endif /* _ARCH_MIPS_FENV_H_ */
