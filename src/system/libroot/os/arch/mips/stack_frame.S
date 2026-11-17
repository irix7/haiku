/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */

	.abicalls
#if defined(_KERNEL_MODE) || defined(_BOOT_MODE)
	// The kernel and the boot loader are not position independent.
	.option	pic0
#endif
	.set	noreorder
	.text

/* void* get_stack_frame(void) */
	.globl	get_stack_frame
	.type	get_stack_frame, @function
	.ent	get_stack_frame
get_stack_frame:
	jr	$31
	 move	$2, $30
	.end	get_stack_frame
	.size	get_stack_frame, . - get_stack_frame
