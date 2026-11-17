/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*
 * The SGI Indy has no ISA bus. Its keyboard and mouse controller is
 * compatible with the PC's 8042, so this backend maps the two I/O ports of
 * that controller to its registers. The ps2 bus manager then works as it is.
 * Every other port reads as 0xff.
 */


#include <KernelExport.h>

#include <arch_indy.h>

#include "ISA.h"
#include "isa_arch.h"


#define ISA_PORT_8042_DATA		0x60
#define ISA_PORT_8042_STATUS	0x64


uint8
arch_isa_read_io_8(int mapped_io_addr)
{
	switch (mapped_io_addr) {
		case ISA_PORT_8042_DATA:
			return indy_read8(INDY_KBD_DATA);
		case ISA_PORT_8042_STATUS:
			return indy_read8(INDY_KBD_STATUS);
		default:
			return 0xff;
	}
}


void
arch_isa_write_io_8(int mapped_io_addr, uint8 value)
{
	switch (mapped_io_addr) {
		case ISA_PORT_8042_DATA:
			indy_write8(INDY_KBD_DATA, value);
			break;
		case ISA_PORT_8042_STATUS:
			indy_write8(INDY_KBD_STATUS, value);
			break;
	}
}


uint16
arch_isa_read_io_16(int mapped_io_addr)
{
	return 0xffff;
}


void
arch_isa_write_io_16(int mapped_io_addr, uint16 value)
{
}


uint32
arch_isa_read_io_32(int mapped_io_addr)
{
	return 0xffffffff;
}


void
arch_isa_write_io_32(int mapped_io_addr, uint32 value)
{
}


phys_addr_t
arch_isa_ram_address(phys_addr_t physical_address_in_system_memory)
{
	return physical_address_in_system_memory;
}


status_t
arch_isa_init(void)
{
	return B_OK;
}
