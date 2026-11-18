/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_KERNEL_H
#define _KERNEL_ARCH_MIPS_KERNEL_H


#ifndef _ASSEMBLER
#	include <arch/cpu.h>
#endif


/*
 * Memory layout
 *
 * 0x00100000 - 0x7ffeffff  user
 * 0x80000000 - 0x9fffffff  kseg0: all physical memory, cached, no TLB.
 *                          The kernel image is linked here.
 * 0xa0000000 - 0xbfffffff  kseg1: the same physical memory, uncached.
 *                          Used for device registers.
 * 0xc0000000 - 0xffffffff  kseg2: managed by the VM through page tables
 *
 * The kernel address space covers all three segments. The parts of kseg0
 * and kseg1 that hold no kernel image are blocked in arch_vm_init_post_area().
 */

// The address the kernel image is linked at (physical 0x09000000).
#define KERNEL_LOAD_BASE	0x89000000

#define KERNEL_BASE			0x80000000
#define KERNEL_SIZE			0x80000000
#define KERNEL_TOP			(KERNEL_BASE + (KERNEL_SIZE - 1))

// Start of the part of the kernel address space that the VM maps.
#define KERNEL_VM_BASE		0xc0000000
#define KERNEL_VM_SIZE		0x40000000

/*
 * User space starts at 1 MB and ends 64 kB below the kernel space. The low
 * megabyte catches NULL pointer accesses. The gap at the top makes sure a
 * buffer passed to a syscall cannot reach into the kernel space.
 */
#define USER_BASE			0x100000
#define USER_BASE_ANY		USER_BASE
#define USER_SIZE			(0x80000000 - (0x10000 + USER_BASE))
#define USER_TOP			(USER_BASE + (USER_SIZE - 1))

#define KERNEL_USER_DATA_BASE	0x70000000
#define USER_STACK_REGION		0x70000000
#define USER_STACK_REGION_SIZE	((USER_TOP - USER_STACK_REGION) + 1)


#endif	/* _KERNEL_ARCH_MIPS_KERNEL_H */
