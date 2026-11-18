/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <fenv.h>


/* The environment is the FPU control and status register (FCSR). */

const fenv_t __fe_dfl_env = _FCSR_FLUSH_TO_ZERO;


static inline unsigned int
read_fcsr(void)
{
	unsigned int fcsr;
	__asm__ __volatile__("cfc1 %0, $31" : "=r" (fcsr));
	return fcsr;
}


static inline void
write_fcsr(unsigned int fcsr)
{
	__asm__ __volatile__("ctc1 %0, $31" : : "r" (fcsr));
}


int
feclearexcept(int excepts)
{
	excepts &= FE_ALL_EXCEPT;
	write_fcsr(read_fcsr() & ~(excepts | (excepts << _FCSR_CAUSE_SHIFT)));
	return 0;
}


int
fegetexceptflag(fexcept_t* flagp, int excepts)
{
	*flagp = read_fcsr() & excepts & FE_ALL_EXCEPT;
	return 0;
}


int
fesetexceptflag(const fexcept_t* flagp, int excepts)
{
	unsigned int fcsr = read_fcsr();
	excepts &= FE_ALL_EXCEPT;
	fcsr &= ~excepts;
	fcsr |= *flagp & excepts;
	write_fcsr(fcsr);
	return 0;
}


int
feraiseexcept(int excepts)
{
	/* Setting a cause bit whose enable bit is set raises the exception. */
	excepts &= FE_ALL_EXCEPT;
	write_fcsr(read_fcsr() | excepts | (excepts << _FCSR_CAUSE_SHIFT));
	return 0;
}


int
fetestexcept(int excepts)
{
	return read_fcsr() & excepts & FE_ALL_EXCEPT;
}


int
fegetround(void)
{
	return read_fcsr() & _FCSR_ROUND_MASK;
}


int
fesetround(int round)
{
	if ((round & ~_FCSR_ROUND_MASK) != 0)
		return -1;

	write_fcsr((read_fcsr() & ~_FCSR_ROUND_MASK) | round);
	return 0;
}


int
fegetenv(fenv_t* envp)
{
	*envp = read_fcsr();
	return 0;
}


int
feholdexcept(fenv_t* envp)
{
	unsigned int fcsr = read_fcsr();
	*envp = fcsr;
	fcsr &= ~(FE_ALL_EXCEPT | (FE_ALL_EXCEPT << _FCSR_ENABLE_SHIFT)
		| (FE_ALL_EXCEPT << _FCSR_CAUSE_SHIFT));
	write_fcsr(fcsr);
	return 0;
}


int
fesetenv(const fenv_t* envp)
{
	/* without the cause bits, which would raise exceptions */
	write_fcsr(*envp & ~(FE_ALL_EXCEPT << _FCSR_CAUSE_SHIFT));
	return 0;
}


int
feupdateenv(const fenv_t* envp)
{
	unsigned int excepts = read_fcsr() & FE_ALL_EXCEPT;
	fesetenv(envp);
	feraiseexcept(excepts);
	return 0;
}


int
feenableexcept(int mask)
{
	unsigned int fcsr = read_fcsr();
	int old = (fcsr >> _FCSR_ENABLE_SHIFT) & FE_ALL_EXCEPT;
	write_fcsr(fcsr | ((mask & FE_ALL_EXCEPT) << _FCSR_ENABLE_SHIFT));
	return old;
}


int
fedisableexcept(int mask)
{
	unsigned int fcsr = read_fcsr();
	int old = (fcsr >> _FCSR_ENABLE_SHIFT) & FE_ALL_EXCEPT;
	write_fcsr(fcsr & ~((mask & FE_ALL_EXCEPT) << _FCSR_ENABLE_SHIFT));
	return old;
}


int
fegetexcept(void)
{
	return (read_fcsr() >> _FCSR_ENABLE_SHIFT) & FE_ALL_EXCEPT;
}
