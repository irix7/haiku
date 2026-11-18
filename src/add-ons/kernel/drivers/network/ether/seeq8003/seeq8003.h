/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef SEEQ8003_H
#define SEEQ8003_H


#include <SupportDefs.h>


/* Seeq 8003 registers */
#define SEEQ_STATION_ADDRESS	0	/* 0 to 5 */
#define SEEQ_RX_COMMAND			6	/* write: command, read: status */
#define SEEQ_TX_COMMAND			7

#define SEEQ_RX_STATION_MULTI	0xc0
	/* accept frames for the station, broadcasts and multicasts */
#define SEEQ_RX_INTERRUPT_EOF	0x10	/* interrupt on a good end of frame */

/* HPC3 Ethernet channel control register */
#define HPC3_ENET_CTRL_ACTIVE	0x0200
#define HPC3_ENET_CTRL_OVERFLOW	0x0800	/* receive buffer overflow */

/* HPC3 Ethernet reset register */
#define HPC3_ENET_RESET_CHANNEL	0x01
#define HPC3_ENET_RESET_CLRINT	0x02

/* HPC3 interrupt status: the Ethernet DMA bits, written back to clear them */
#define HPC3_INTSTAT_ENET_DMA	0x60

/* DMA descriptor, 16 bytes, in memory the HPC3 reads and writes */
struct hpc3_descriptor {
	uint32	buffer;		/* physical address */
	uint32	control;	/* flags and byte count */
	uint32	next;		/* physical address of the next descriptor */
	uint32	_reserved;
};

#define HPC3_DESC_EOX			0x80000000	/* end of the chain */
#define HPC3_DESC_EOP			0x40000000	/* end of packet */
#define HPC3_DESC_XIE			0x20000000	/* interrupt when done */
#define HPC3_DESC_OWN			0x00004000	/* receive: the HPC3 owns it */
#define HPC3_DESC_COUNT_MASK	0x00003fff

/* A received buffer holds two pad bytes, the frame and one status byte. */
#define SEEQ_RX_BUFFER_SIZE		2048
#define SEEQ_RX_MAX_COUNT		1517
#define SEEQ_RX_OVERHEAD		3
#define SEEQ_RX_STATUS_GOOD		0x20
#define SEEQ_RX_ARMED \
	(HPC3_DESC_EOP | HPC3_DESC_XIE | HPC3_DESC_OWN | SEEQ_RX_MAX_COUNT)

/* A ring of 16 did not work under the iris emulator. */
#define SEEQ_RX_RING_SIZE		8

#define SEEQ_MAX_FRAME_SIZE		1514
#define SEEQ_MIN_FRAME_SIZE		60


#endif	/* SEEQ8003_H */
