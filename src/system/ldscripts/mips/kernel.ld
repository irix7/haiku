/*
 * Linker script of the kernel for the SGI Indy.
 *
 * The kernel is linked at a fixed address in kseg0 (physical 0x09000000) and
 * is not relocated. It is a dynamic executable: add-ons resolve their
 * symbols through its dynamic symbol table.
 */

OUTPUT_FORMAT("elf32-ntradbigmips")
OUTPUT_ARCH(mips)

ENTRY(_start)

SECTIONS
{
  . = 0x89000000 + SIZEOF_HEADERS;

  .interp         : { *(.interp) }
  .MIPS.abiflags  : { *(.MIPS.abiflags) }
  .reginfo        : { *(.reginfo) }
  .note.gnu.build-id  : { *(.note.gnu.build-id) }
  .dynamic        : { *(.dynamic) }
  .hash           : { *(.hash) }
  .gnu.hash       : { *(.gnu.hash) }
  .dynsym         : { *(.dynsym) }
  .dynstr         : { *(.dynstr) }
  .gnu.version    : { *(.gnu.version) }
  .gnu.version_d  : { *(.gnu.version_d) }
  .gnu.version_r  : { *(.gnu.version_r) }
  .rel.dyn        : { *(.rel.dyn) *(.rel.*) }
  .rela.dyn       : { *(.rela.dyn) *(.rela.*) }
  .init           : { KEEP (*(SORT_NONE(.init))) }
  .plt            : { *(.plt) }
  .text           :
  {
    *(.text.unlikely .text.*_unlikely .text.unlikely.*)
    *(.text.exit .text.exit.*)
    *(.text.startup .text.startup.*)
    *(.text.hot .text.hot.*)
    *(.text .stub .text.* .gnu.linkonce.t.*)
    *(.gnu.warning)
    *(.mips16.fn.*) *(.mips16.call.*)
  }
  .fini           : { KEEP (*(SORT_NONE(.fini))) }
  .MIPS.stubs     : { *(.MIPS.stubs) }
  PROVIDE (__etext = .);
  PROVIDE (_etext = .);
  PROVIDE (etext = .);
  .rodata         : { *(.rodata .rodata.* .gnu.linkonce.r.*) }
  .rodata1        : { *(.rodata1) }
  .eh_frame_hdr   : { *(.eh_frame_hdr) }
  .eh_frame       : ONLY_IF_RO { KEEP (*(.eh_frame)) *(.eh_frame.*) }
  .gcc_except_table   : ONLY_IF_RO { *(.gcc_except_table .gcc_except_table.*) }

  /* The data segment starts on its own page (16 kB). */
  . = ALIGN(0x4000);

  .eh_frame       : ONLY_IF_RW { KEEP (*(.eh_frame)) *(.eh_frame.*) }
  .gcc_except_table   : ONLY_IF_RW { *(.gcc_except_table .gcc_except_table.*) }
  .tdata          : { *(.tdata .tdata.* .gnu.linkonce.td.*) }
  .tbss           : { *(.tbss .tbss.* .gnu.linkonce.tb.*) *(.tcommon) }
  .preinit_array  :
  {
    PROVIDE_HIDDEN (__preinit_array_start = .);
    KEEP (*(.preinit_array))
    PROVIDE_HIDDEN (__preinit_array_end = .);
  }
  .init_array     :
  {
    PROVIDE_HIDDEN (__init_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.init_array.*) SORT_BY_INIT_PRIORITY(.ctors.*)))
    KEEP (*(.init_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .ctors))
    PROVIDE_HIDDEN (__init_array_end = .);
  }
  .fini_array     :
  {
    PROVIDE_HIDDEN (__fini_array_start = .);
    KEEP (*(SORT_BY_INIT_PRIORITY(.fini_array.*) SORT_BY_INIT_PRIORITY(.dtors.*)))
    KEEP (*(.fini_array EXCLUDE_FILE (*crtbegin.o *crtbegin?.o *crtend.o *crtend?.o ) .dtors))
    PROVIDE_HIDDEN (__fini_array_end = .);
  }
  .ctors          :
  {
    KEEP (*crtbegin.o(.ctors))
    KEEP (*crtbegin?.o(.ctors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .ctors))
    KEEP (*(SORT(.ctors.*)))
    KEEP (*(.ctors))
  }
  .dtors          :
  {
    KEEP (*crtbegin.o(.dtors))
    KEEP (*crtbegin?.o(.dtors))
    KEEP (*(EXCLUDE_FILE (*crtend.o *crtend?.o ) .dtors))
    KEEP (*(SORT(.dtors.*)))
    KEEP (*(.dtors))
  }
  .jcr            : { KEEP (*(.jcr)) }
  .data.rel.ro    : { *(.data.rel.ro.local* .gnu.linkonce.d.rel.ro.local.*) *(.data.rel.ro .data.rel.ro.* .gnu.linkonce.d.rel.ro.*) }
  .data           :
  {
    *(.data .data.* .gnu.linkonce.d.*)
    SORT(CONSTRUCTORS)
  }
  .data1          : { *(.data1) }
  .rld_map        : { *(.rld_map) }
  . = ALIGN(16);
  HIDDEN (_gp = . + 0x7ff0);
  .got            : { *(.got.plt) *(.got) }
  .sdata          : { *(.sdata .sdata.* .gnu.linkonce.s.*) }
  .lit8           : { *(.lit8) }
  .lit4           : { *(.lit4) }
  _edata = .; PROVIDE (edata = .);
  __bss_start = .;
  .sbss           : { *(.dynsbss) *(.sbss .sbss.* .gnu.linkonce.sb.*) *(.scommon) }
  .bss            :
  {
    *(.dynbss)
    *(.bss .bss.* .gnu.linkonce.b.*)
    *(COMMON)
    . = ALIGN(16);
  }
  . = ALIGN(16);
  _end = .; PROVIDE (end = .);

  /DISCARD/ : { *(.note.GNU-stack) *(.gnu_debuglink) *(.gnu.lto_*) *(.pdr) *(.mdebug*) *(.gnu.attributes) *(.MIPS.options) }
}
