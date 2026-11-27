/*
 * Linker script of the boot loader for the SGI ARCS PROM (platform "arcs").
 * The PROM loads netboot images at kseg0 0x88040000 (physical 0x08040000).
 */

OUTPUT_FORMAT("elf32-ntradbigmips")
OUTPUT_ARCH(mips)

ENTRY(_start)

SECTIONS
{
	. = 0x88040000;

	/* text/read-only data */
	.text : {
		*(.text.head)
		*(.text .text.* .gnu.linkonce.t.*)
		*(.MIPS.stubs)
	}

	. = ALIGN(0x8);
	__ctor_list = .;
	.init_array : { *(.ctors) *(.init_array) }
	__ctor_end = .;
	__dtor_list = .;
	.fini_array : { *(.dtors) *(.fini_array) }
	__dtor_end = .;

	.rodata : { *(.rodata .rodata.* .gnu.linkonce.r.*) }

	/* writable data */
	. = ALIGN(0x4000);
	__data_start = .;
	.data : { *(.data .data.* .gnu.linkonce.d.*) }

	. = ALIGN(16);
	_gp = . + 0x7ff0;
	.got : { *(.got.plt) *(.got) }
	.sdata : { *(.sdata .sdata.* .lit8 .lit4) }

	/* uninitialized data (in same segment as writable data) */
	. = ALIGN(16);
	__bss_start = .;
	.sbss : { *(.sbss .sbss.* .scommon) }
	.bss : { *(.bss .bss.* COMMON) }

	. = ALIGN(16);
	_end = . ;

	/* Strip unnecessary stuff */
	/DISCARD/ : {
		*(.comment .note .eh_frame .stab .stabstr .debug*)
		*(.reginfo .MIPS.abiflags .MIPS.options .pdr .mdebug* .gnu.attributes)
		*(.dynamic .dynsym .dynstr .hash .gnu.hash .interp .rel.dyn)
	}
}
