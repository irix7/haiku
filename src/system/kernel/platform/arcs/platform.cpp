/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

/*
 * Kernel platform code for machines with the SGI ARCS PROM.
 *
 * The kernel makes no firmware calls: the PROM is a 32 bit o32 program and
 * the kernel uses the n32 ABI. The machine specific code is in
 * src/system/kernel/arch/mips.
 */
