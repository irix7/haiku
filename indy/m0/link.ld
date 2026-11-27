/*
 * Linker script of the M0 test image. The PROM loads netboot images at
 * kseg0 0x88040000 (physical 0x08040000).
 */

OUTPUT_FORMAT("elf32-ntradbigmips")
ENTRY(_start)

SECTIONS
{
	. = 0x88040000;

	.text : { *(.text.start) *(.text*) *(.rodata*) }
	.data : { *(.data*) }
	.bss : {
		*(.bss*) *(COMMON)
		. = ALIGN(16);
		. += 0x4000;
		_stack_top = .;
	}

	/DISCARD/ : {
		*(.reginfo) *(.MIPS.abiflags) *(.MIPS.options) *(.pdr) *(.comment)
		*(.gnu.attributes) *(.mdebug*) *(.eh_frame*)
	}
}
