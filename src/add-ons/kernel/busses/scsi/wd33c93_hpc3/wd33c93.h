/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */
#ifndef WD33C93_H
#define WD33C93_H


/* Auxiliary status register (read from the address port) */
#define WD_ASR_DBR				0x01	/* data buffer ready */
#define WD_ASR_CIP				0x10	/* command in progress */
#define WD_ASR_BSY				0x20
#define WD_ASR_INT				0x80	/* interrupt pending */

/* Registers, selected through the address port */
#define WD_OWN_ID				0x00
#define WD_CONTROL				0x01
#define WD_TIMEOUT				0x02
#define WD_CDB_1				0x03
#define WD_TARGET_LUN			0x0f
#define WD_COMMAND_PHASE		0x10
#define WD_SYNCHRONOUS			0x11
#define WD_COUNT_HIGH			0x12
#define WD_COUNT_MIDDLE			0x13
#define WD_COUNT_LOW			0x14
#define WD_DESTINATION_ID		0x15
#define WD_SOURCE_ID			0x16
#define WD_STATUS				0x17
#define WD_COMMAND				0x18
#define WD_DATA					0x19

/* Commands */
#define WD_CMD_RESET			0x00
#define WD_CMD_ABORT			0x01
#define WD_CMD_SELECT_ATN		0x06
#define WD_CMD_SELECT_ATN_XFER	0x08
#define WD_CMD_TRANSFER_INFO	0x20

/* Register values */
#define WD_OWN_ID_EAF			0x08	/* enable advanced features */
#define WD_OWN_ID_RAF			0x20
#define WD_CONTROL_EDI_IDI		0x0c
	/* ending disconnect interrupt, intermediate disconnect interrupt */
#define WD_SOURCE_ID_ER			0x80	/* enable reselection */
#define WD_TIMEOUT_VALUE		63
#define WD_PHASE_STATUS_STARTED	0x46
	/* command phase value: continue with the status phase */

/* SCSI status register values */
#define WD_CSR_PHASE_MASK		0x07
#define WD_CSR_SELECT_XFER_DONE	0x16
#define WD_CSR_DISCONNECT		0x41
#define WD_CSR_TIMEOUT			0x42
#define WD_CSR_DISCONNECT_1		0x85
#define WD_CSR_SERVICE_REQUIRED	0x88	/* | phase: the target asks for it */

/* SCSI bus phases, as in the low bits of the status register */
#define WD_PHASE_DATA_OUT		0x00
#define WD_PHASE_DATA_IN		0x01
#define WD_PHASE_COMMAND		0x02
#define WD_PHASE_STATUS			0x03
#define WD_PHASE_MESSAGE_OUT	0x06
#define WD_PHASE_MESSAGE_IN		0x07

/* HPC3 SCSI channel control register */
#define HPC3_SCSI_CTRL_DIRECTION	0x04
#define HPC3_SCSI_CTRL_FLUSH		0x08
#define HPC3_SCSI_CTRL_ACTIVE		0x10
#define HPC3_SCSI_CTRL_RESET		0x40

#define WD_HOST_ID				7


#endif	/* WD33C93_H */
