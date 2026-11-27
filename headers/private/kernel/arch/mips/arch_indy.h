/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef _KERNEL_ARCH_MIPS_INDY_H
#define _KERNEL_ARCH_MIPS_INDY_H


/*
 * Device addresses and register bits of the SGI Indy (IP24).
 *
 * Addresses are physical. Use MIPS_KSEG1() for uncached access from the
 * kernel and the loader. Every 8 bit register sits in byte lane 3 of a
 * 32 bit word: the byte address is the word address plus 3.
 *
 * This header is included by C, C++ and assembly sources.
 */

#define MIPS_KSEG0(physical)		((physical) + 0x80000000)
#define MIPS_KSEG1(physical)		((physical) + 0xa0000000)

/* RAM */
#define INDY_RAM_BASE				0x08000000
#define INDY_LOW_RAM_ALIAS_SIZE		0x00080000
	/* physical 0 to 0x7ffff is an alias of the first 512 kB of RAM */
#define INDY_RESERVED_LOW_MEMORY	0x00040000
	/* vectors, exception block and PROM data. The PROM loads images above. */

/* Memory controller (MC) */
#define INDY_MC_BASE				0x1fa00000
#define INDY_MC_SYSID				(INDY_MC_BASE + 0x1c)
#define INDY_MC_RPSS_DIVIDER		(INDY_MC_BASE + 0x2c)
#define INDY_MC_MEMCFG0				(INDY_MC_BASE + 0xc4)
#define INDY_MC_MEMCFG1				(INDY_MC_BASE + 0xcc)
#define INDY_MC_RPSS_COUNTER		(INDY_MC_BASE + 0x1004)
#define INDY_MC_MEMCFG_VALID		0x2000
#define INDY_MC_SYSID_REVISION_MASK	0x0f

/* Newport graphics (REX3) */
#define INDY_REX3_BASE				0x1f0f0000
#define INDY_REX3_SIZE				0x00002000

/* HPC3 */
#define INDY_HPC3_BASE				0x1fb80000
#define INDY_HPC3_SCSI0_BASE		(INDY_HPC3_BASE + 0x10000)
#define INDY_HPC3_SCSI0_CBP			(INDY_HPC3_SCSI0_BASE + 0x0)
	/* current buffer pointer */
#define INDY_HPC3_SCSI0_NDBP		(INDY_HPC3_SCSI0_BASE + 0x4)
	/* next descriptor pointer */
#define INDY_HPC3_SCSI0_BC			(INDY_HPC3_SCSI0_BASE + 0x1000)
#define INDY_HPC3_SCSI0_CTRL		(INDY_HPC3_SCSI0_BASE + 0x1004)
#define INDY_HPC3_SCSI0_GIO			(INDY_HPC3_SCSI0_BASE + 0x1008)
#define INDY_HPC3_SCSI0_DEV			(INDY_HPC3_SCSI0_BASE + 0x100c)
#define INDY_HPC3_SCSI0_DMACFG		(INDY_HPC3_SCSI0_BASE + 0x1010)
#define INDY_HPC3_SCSI0_PIOCFG		(INDY_HPC3_SCSI0_BASE + 0x1014)
#define INDY_HPC3_ENET_RX_BASE		(INDY_HPC3_BASE + 0x14000)
#define INDY_HPC3_ENET_RX_CBP		(INDY_HPC3_ENET_RX_BASE + 0x0)
#define INDY_HPC3_ENET_RX_NDBP		(INDY_HPC3_ENET_RX_BASE + 0x4)
#define INDY_HPC3_ENET_RX_BC		(INDY_HPC3_ENET_RX_BASE + 0x1000)
#define INDY_HPC3_ENET_RX_CTRL		(INDY_HPC3_ENET_RX_BASE + 0x1004)
#define INDY_HPC3_ENET_RX_GIO		(INDY_HPC3_ENET_RX_BASE + 0x1008)
#define INDY_HPC3_ENET_RX_DEV		(INDY_HPC3_ENET_RX_BASE + 0x100c)
#define INDY_HPC3_ENET_RESET		(INDY_HPC3_ENET_RX_BASE + 0x1014)
#define INDY_HPC3_ENET_DMACFG		(INDY_HPC3_ENET_RX_BASE + 0x1018)
#define INDY_HPC3_ENET_PIOCFG		(INDY_HPC3_ENET_RX_BASE + 0x101c)
#define INDY_HPC3_ENET_TX_BASE		(INDY_HPC3_BASE + 0x16000)
#define INDY_HPC3_ENET_TX_CBP		(INDY_HPC3_ENET_TX_BASE + 0x0)
#define INDY_HPC3_ENET_TX_NDBP		(INDY_HPC3_ENET_TX_BASE + 0x4)
#define INDY_HPC3_ENET_TX_BC		(INDY_HPC3_ENET_TX_BASE + 0x1000)
#define INDY_HPC3_ENET_TX_CTRL		(INDY_HPC3_ENET_TX_BASE + 0x1004)
#define INDY_HPC3_ENET_TX_GIO		(INDY_HPC3_ENET_TX_BASE + 0x1008)
#define INDY_HPC3_ENET_TX_DEV		(INDY_HPC3_ENET_TX_BASE + 0x100c)
#define INDY_HPC3_INTSTAT			(INDY_HPC3_BASE + 0x30000)
#define INDY_HPC3_INTSTAT1			(INDY_HPC3_BASE + 0x3000c)

/* WD33C93 SCSI controller 0 */
#define INDY_SCSI0_ADDRESS			0x1fbc4003
#define INDY_SCSI0_DATA				0x1fbc4007

/* Seeq 8003 Ethernet controller: register n */
#define INDY_SEEQ_REGISTER(n)		(0x1fbd4000 + ((n) << 2) + 3)

/* IOC2 */
#define INDY_IOC_BASE				0x1fbd9800
#define INDY_SCC_B_CONTROL			(INDY_IOC_BASE + 0x33)
#define INDY_SCC_B_DATA				(INDY_IOC_BASE + 0x37)
#define INDY_SCC_A_CONTROL			(INDY_IOC_BASE + 0x3b)
#define INDY_SCC_A_DATA				(INDY_IOC_BASE + 0x3f)
#define INDY_SCC_RX_AVAILABLE		0x01
#define INDY_SCC_TX_EMPTY			0x04
#define INDY_KBD_DATA				(INDY_IOC_BASE + 0x43)
#define INDY_KBD_STATUS				(INDY_IOC_BASE + 0x47)

/* INT3 interrupt controller (byte registers) */
#define INDY_INT3_L0_STATUS			(INDY_IOC_BASE + 0x83)
#define INDY_INT3_L0_MASK			(INDY_IOC_BASE + 0x87)
#define INDY_INT3_L1_STATUS			(INDY_IOC_BASE + 0x8b)
#define INDY_INT3_L1_MASK			(INDY_IOC_BASE + 0x8f)
#define INDY_INT3_MAP_STATUS		(INDY_IOC_BASE + 0x93)
#define INDY_INT3_MAP_MASK0			(INDY_IOC_BASE + 0x97)
#define INDY_INT3_MAP_MASK1			(INDY_IOC_BASE + 0x9b)
#define INDY_INT3_MAP_POLARITY		(INDY_IOC_BASE + 0x9f)
#define INDY_INT3_TIMER_CLEAR		(INDY_IOC_BASE + 0xa3)
#define INDY_INT3_ERROR_STATUS		(INDY_IOC_BASE + 0xa7)

/* 8254 timer, 1 MHz clock (byte registers) */
#define INDY_PIT_COUNTER0			(INDY_IOC_BASE + 0xb3)
#define INDY_PIT_COUNTER1			(INDY_IOC_BASE + 0xb7)
#define INDY_PIT_COUNTER2			(INDY_IOC_BASE + 0xbb)
#define INDY_PIT_CONTROL			(INDY_IOC_BASE + 0xbf)
#define INDY_PIT_FREQUENCY			1000000

/* DS1386 RTC and NVRAM: register n */
#define INDY_RTC_REGISTER(n)		(0x1fbe0000 + ((n) << 2) + 3)
#define INDY_RTC_SECONDS			0x01
#define INDY_RTC_MINUTES			0x02
#define INDY_RTC_HOURS				0x04
#define INDY_RTC_DATE				0x08
#define INDY_RTC_MONTH				0x09
#define INDY_RTC_YEAR				0x0a
#define INDY_RTC_COMMAND			0x0b
#define INDY_RTC_COMMAND_TE			0x80	/* transfer enable */
#define INDY_RTC_YEAR_BASE			1940
#define INDY_NVRAM_MAC_ADDRESS		314		/* six registers */

/* PROM */
#define INDY_PROM_BASE				0x1fc00000

/*
 * I/O interrupt vectors, as passed to install_io_interrupt_handler().
 * Vectors 1 and 12 are aliases of INDY_IRQ_KEYBOARD_MOUSE: the ps2 bus
 * manager installs its handlers on those two numbers.
 */
#define INDY_IRQ_LOCAL0(bit)		(32 + (bit))
#define INDY_IRQ_LOCAL1(bit)		(40 + (bit))
#define INDY_IRQ_MAPPED(bit)		(48 + (bit))

#define INDY_IRQ_REX3_FIFO			INDY_IRQ_LOCAL0(0)
#define INDY_IRQ_SCSI0				INDY_IRQ_LOCAL0(1)
#define INDY_IRQ_SCSI1				INDY_IRQ_LOCAL0(2)
#define INDY_IRQ_ETHERNET			INDY_IRQ_LOCAL0(3)
#define INDY_IRQ_MC_DMA				INDY_IRQ_LOCAL0(4)
#define INDY_IRQ_PARALLEL			INDY_IRQ_LOCAL0(5)
#define INDY_IRQ_GRAPHICS			INDY_IRQ_LOCAL0(6)
#define INDY_IRQ_POWER_BUTTON		INDY_IRQ_LOCAL1(0)
#define INDY_IRQ_HPC3_DMA			INDY_IRQ_LOCAL1(4)
#define INDY_IRQ_VERTICAL_RETRACE	INDY_IRQ_LOCAL1(7)
#define INDY_IRQ_KEYBOARD_MOUSE		INDY_IRQ_MAPPED(4)
#define INDY_IRQ_SERIAL				INDY_IRQ_MAPPED(5)

#define INDY_INT3_LOCAL0_CASCADE	0x80	/* local 0 bit 7: mapped, mask 0 */
#define INDY_INT3_LOCAL1_CASCADE	0x08	/* local 1 bit 3: mapped, mask 1 */


#ifndef _ASSEMBLER

#include <SupportDefs.h>

/* Device register access through kseg1 */

static inline uint8
indy_read8(uint32 physicalAddress)
{
	return *(volatile uint8*)MIPS_KSEG1(physicalAddress);
}


static inline void
indy_write8(uint32 physicalAddress, uint8 value)
{
	*(volatile uint8*)MIPS_KSEG1(physicalAddress) = value;
}


static inline uint32
indy_read32(uint32 physicalAddress)
{
	return *(volatile uint32*)MIPS_KSEG1(physicalAddress);
}


static inline void
indy_write32(uint32 physicalAddress, uint32 value)
{
	*(volatile uint32*)MIPS_KSEG1(physicalAddress) = value;
}


#ifdef __cplusplus
extern "C" {
#endif

struct kernel_args;

/* text console on the Newport, arch_newport_console.cpp */
void mips_newport_console_init(struct kernel_args* args);
void mips_newport_console_putchar(char c);
void mips_newport_console_stop(void);
void mips_newport_screen_size(uint32* _width, uint32* _height);

#ifdef __cplusplus
}
#endif

#endif	/* !_ASSEMBLER */


#endif	/* _KERNEL_ARCH_MIPS_INDY_H */
