/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_ELF_H
#define _KERNEL_ARCH_MIPS_ELF_H


/* Relocation types (System V ABI, MIPS processor supplement) */
#define R_MIPS_NONE				0
#define R_MIPS_16				1
#define R_MIPS_32				2
#define R_MIPS_REL32			3
#define R_MIPS_26				4
#define R_MIPS_HI16				5
#define R_MIPS_LO16				6
#define R_MIPS_GPREL16			7
#define R_MIPS_LITERAL			8
#define R_MIPS_GOT16			9
#define R_MIPS_PC16				10
#define R_MIPS_CALL16			11
#define R_MIPS_GPREL32			12
#define R_MIPS_64				18
#define R_MIPS_GOT_DISP			19
#define R_MIPS_GOT_PAGE			20
#define R_MIPS_GOT_OFST			21
#define R_MIPS_JALR				37
#define R_MIPS_TLS_DTPMOD32		38
#define R_MIPS_TLS_DTPREL32		39
#define R_MIPS_TLS_DTPMOD64		40
#define R_MIPS_TLS_DTPREL64		41
#define R_MIPS_TLS_GD			42
#define R_MIPS_TLS_LDM			43
#define R_MIPS_TLS_DTPREL_HI16	44
#define R_MIPS_TLS_DTPREL_LO16	45
#define R_MIPS_TLS_GOTTPREL		46
#define R_MIPS_TLS_TPREL32		47
#define R_MIPS_TLS_TPREL64		48
#define R_MIPS_TLS_TPREL_HI16	49
#define R_MIPS_TLS_TPREL_LO16	50
#define R_MIPS_GLOB_DAT			51
#define R_MIPS_COPY				126
#define R_MIPS_JUMP_SLOT		127

/*
 * Dynamic section tags. The global offset table of a MIPS object has no
 * relocation entries. Its first DT_MIPS_LOCAL_GOTNO entries are local: the
 * loader adds the load offset to each (the first two are reserved). The rest
 * belong, in order, to the dynamic symbols from index DT_MIPS_GOTSYM to
 * DT_MIPS_SYMTABNO - 1.
 */
#define DT_MIPS_RLD_VERSION		0x70000001
#define DT_MIPS_FLAGS			0x70000005
#define DT_MIPS_BASE_ADDRESS	0x70000006
#define DT_MIPS_LOCAL_GOTNO		0x7000000a
#define DT_MIPS_SYMTABNO		0x70000011
#define DT_MIPS_UNREFEXTNO		0x70000012
#define DT_MIPS_GOTSYM			0x70000013
#define DT_MIPS_RLD_MAP			0x70000016
#define DT_MIPS_PLTGOT			0x70000032
#define DT_MIPS_RLD_MAP_REL		0x70000035

/* e_flags */
#define EF_MIPS_NOREORDER		0x00000001
#define EF_MIPS_PIC				0x00000002
#define EF_MIPS_CPIC			0x00000004
#define EF_MIPS_ABI2			0x00000020	/* n32 */
#define EF_MIPS_ARCH_MASK		0xf0000000
#define EF_MIPS_ARCH_3			0x20000000

/* st_other: the function expects its own address in $25 */
#define STO_MIPS_PIC			0x08
#define STO_MIPS_PLT			0x08

/* The value a TLS symbol offset is biased by in the GOT entries of
   R_MIPS_TLS_DTPREL32. */
#define TLS_DTV_OFFSET			0x8000


#endif	/* _KERNEL_ARCH_MIPS_ELF_H */
