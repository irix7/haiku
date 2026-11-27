/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	SIM for the WD33C93 SCSI controller of the SGI Indy.

	This version executes one request at a time with polled programmed I/O:
	the CPU moves every byte through the chip's data register. It uses no
	interrupt and no HPC3 DMA. A request's data is reached through kseg0 by
	its physical address, so no mapping is needed.

	The controller's registers are selected through an address port. The
	address advances after most data port accesses, so every access selects
	its register again.
*/


#include <stdlib.h>
#include <string.h>

#include <new>

#include <bus/SCSI.h>
#include <device_manager.h>
#include <KernelExport.h>
#include <lock.h>

#include <arch/cpu.h>
#include <arch_indy.h>
#include <arch_vm_translation_map.h>

#include "wd33c93.h"


//#define TRACE_WD33C93
#ifdef TRACE_WD33C93
#	define TRACE(x...) dprintf("wd33c93: " x)
#else
#	define TRACE(x...) ;
#endif
#define ERROR(x...) dprintf("wd33c93: " x)

#define WD33C93_DEVICE_MODULE_NAME	"busses/scsi/wd33c93_hpc3/driver_v1"
#define WD33C93_SIM_MODULE_NAME		"busses/scsi/wd33c93_hpc3/sim/driver_v1"

#define INDY_DEVICE_NAME			"indy/device"

static const uint32 kPollLimit = 20000000;
	// polls of the status port before a wait gives up


static device_manager_info* gDeviceManager;
static scsi_for_sim_interface* gSCSI;


struct wd33c93_controller {
	device_node*	node;
	scsi_bus		bus;
	mutex			lock;
};


// A position in the data of a request. The data is a list of physical
// ranges.
struct data_cursor {
	const physical_entry*	entry;
	uint32					entriesLeft;
	uint32					offset;
	uint32					bytesLeft;

	void SetTo(scsi_ccb* request)
	{
		entry = request->sg_list;
		entriesLeft = request->sg_count;
		offset = 0;
		bytesLeft = request->data_length;
	}

	uint8* Next()
	{
		while (entriesLeft > 0 && offset >= entry->size) {
			entry++;
			entriesLeft--;
			offset = 0;
		}
		if (entriesLeft == 0 || bytesLeft == 0)
			return NULL;

		bytesLeft--;
		return (uint8*)mips_physical_to_kseg0(entry->address + offset++);
	}
};


//	#pragma mark - register access


static inline uint8
read_asr()
{
	return indy_read8(INDY_SCSI0_ADDRESS);
}


static inline void
write_register(uint8 index, uint8 value)
{
	indy_write8(INDY_SCSI0_ADDRESS, index);
	indy_write8(INDY_SCSI0_DATA, value);
}


static inline uint8
read_register(uint8 index)
{
	indy_write8(INDY_SCSI0_ADDRESS, index);
	return indy_read8(INDY_SCSI0_DATA);
}


static void
write_count(uint32 count)
{
	write_register(WD_COUNT_HIGH, count >> 16);
	write_register(WD_COUNT_MIDDLE, count >> 8);
	write_register(WD_COUNT_LOW, count);
}


static bool
wait_command_accepted()
{
	for (uint32 i = 0; i < kPollLimit; i++) {
		if ((read_asr() & WD_ASR_CIP) == 0)
			return true;
	}
	return false;
}


static bool
wait_interrupt()
{
	for (uint32 i = 0; i < kPollLimit; i++) {
		if ((read_asr() & WD_ASR_INT) != 0)
			return true;
	}
	return false;
}


/*!	Reads the status register, which acknowledges the interrupt. */
static uint8
read_status()
{
	uint8 status = read_register(WD_STATUS);
	for (int i = 0; i < 8; i++) {
		spin(1);
		if ((read_asr() & WD_ASR_INT) == 0)
			break;
		status = read_register(WD_STATUS);
	}
	return status;
}


static bool
issue_command(uint8 command)
{
	if (!wait_command_accepted())
		return false;
	write_register(WD_COMMAND, command);
	return wait_command_accepted();
}


//	#pragma mark - bus phases


static status_t
reset_controller()
{
	// reset the HPC3 channel
	indy_write32(INDY_HPC3_SCSI0_CTRL, HPC3_SCSI_CTRL_RESET);
	spin(100);
	indy_write32(INDY_HPC3_SCSI0_CTRL, 0);
	spin(100);

	write_register(WD_COMMAND, WD_CMD_ABORT);
	if (!wait_command_accepted())
		return B_TIMED_OUT;

	write_register(WD_OWN_ID, WD_HOST_ID | WD_OWN_ID_EAF | WD_OWN_ID_RAF);
	write_register(WD_COMMAND, WD_CMD_RESET);
	spin(100);
	if (!wait_interrupt())
		return B_TIMED_OUT;
	read_status();

	write_register(WD_CONTROL, WD_CONTROL_EDI_IDI);
	write_register(WD_SOURCE_ID, 0);
		// no reselection: targets are told not to disconnect
	write_register(WD_SYNCHRONOUS, 0);
	write_register(WD_TIMEOUT, WD_TIMEOUT_VALUE);

	return B_OK;
}


/*!	Selects the target with ATN. Returns the status register value the
	selection ends with: a service request for the message out or the
	command phase, or WD_CSR_TIMEOUT.
*/
static status_t
select_target(uint8 target, uint8* _status)
{
	write_count(0);
	write_register(WD_DESTINATION_ID, target);
	write_register(WD_TIMEOUT, WD_TIMEOUT_VALUE);
	if (!issue_command(WD_CMD_SELECT_ATN))
		return B_TIMED_OUT;

	for (int i = 0; i < 8; i++) {
		if (!wait_interrupt())
			return B_TIMED_OUT;

		uint8 status = read_status();
		if (status == WD_CSR_TIMEOUT
			|| (status & 0xf8) == WD_CSR_SERVICE_REQUIRED) {
			*_status = status;
			return B_OK;
		}
		spin(1);
	}

	return B_ERROR;
}


/*!	Transfers up to \a length bytes in the current bus phase. The bytes come
	from or go to \a buffer or, if that is NULL, \a cursor. Bytes that have
	no place to go are dropped, missing bytes are sent as 0.
	Returns the status register value the transfer ends with.
*/
static status_t
transfer(uint32 length, bool outbound, uint8* buffer, data_cursor* cursor,
	uint32* _transferred, uint8* _status)
{
	write_register(WD_CONTROL, WD_CONTROL_EDI_IDI);
	write_count(length);
	if (!issue_command(WD_CMD_TRANSFER_INFO))
		return B_TIMED_OUT;

	uint32 done = 0;
	uint32 idle = 0;
	for (;;) {
		uint8 asr = read_asr();
		if ((asr & WD_ASR_DBR) != 0) {
			uint8* byte = NULL;
			if (buffer != NULL) {
				if (done < length)
					byte = &buffer[done];
			} else if (cursor != NULL)
				byte = cursor->Next();

			indy_write8(INDY_SCSI0_ADDRESS, WD_DATA);
			if (outbound) {
				indy_write8(INDY_SCSI0_DATA, byte != NULL ? *byte : 0);
			} else {
				uint8 value = indy_read8(INDY_SCSI0_DATA);
				if (byte != NULL)
					*byte = value;
			}
			done++;
			idle = 0;
			continue;
		}
		if ((asr & WD_ASR_INT) != 0)
			break;
		if (++idle > kPollLimit) {
			write_count(0);
			return B_TIMED_OUT;
		}
	}

	*_status = read_status();
	write_count(0);
	if (_transferred != NULL)
		*_transferred = done;
	return B_OK;
}


/*!	Lets the controller run the status and message in phases. Returns the
	target's status byte.
*/
static status_t
finish_command(uint8* _deviceStatus)
{
	write_count(0);
	write_register(WD_COMMAND_PHASE, WD_PHASE_STATUS_STARTED);
	if (!issue_command(WD_CMD_SELECT_ATN_XFER))
		return B_TIMED_OUT;

	for (int i = 0; i < 8; i++) {
		if (!wait_interrupt())
			return B_TIMED_OUT;

		uint8 status = read_status();
		if (status == WD_CSR_SELECT_XFER_DONE || status == WD_CSR_DISCONNECT
			|| status == WD_CSR_DISCONNECT_1) {
			*_deviceStatus = read_register(WD_TARGET_LUN);
			return B_OK;
		}
		spin(1);
	}

	return B_ERROR;
}


static void
execute_request(wd33c93_controller* controller, scsi_ccb* request)
{
	request->data_resid = request->data_length;
	request->device_status = 0;

	if (request->target_id >= 8 || request->target_id == WD_HOST_ID
		|| request->target_lun > 7) {
		request->subsys_status = SCSI_TID_INVALID;
		return;
	}

	TRACE("request: target %d, lun %d, command %#x, %" B_PRIu32 " bytes\n",
		request->target_id, request->target_lun, request->cdb[0],
		request->data_length);

	uint8 status;
	status_t error = select_target(request->target_id, &status);
	if (error != B_OK) {
		ERROR("selection of target %d failed: %s\n", request->target_id,
			strerror(error));
		reset_controller();
		request->subsys_status = SCSI_HBA_ERR;
		return;
	}
	if (status == WD_CSR_TIMEOUT) {
		request->subsys_status = SCSI_SEL_TIMEOUT;
		return;
	}

	data_cursor cursor;
	cursor.SetTo(request);
	bool dataOut = (request->flags & SCSI_DIR_MASK) == SCSI_DIR_OUT;
	uint32 dataDone = 0;

	// Follow the phases the target asks for.
	for (int32 steps = 0; steps < 64; steps++) {
		uint8 phase = status & WD_CSR_PHASE_MASK;
		uint32 transferred = 0;

		if (status == WD_CSR_SELECT_XFER_DONE || status == WD_CSR_DISCONNECT
			|| status == WD_CSR_DISCONNECT_1) {
			// The target has left the bus without a status phase.
			error = B_ERROR;
			break;
		}

		if (phase == WD_PHASE_MESSAGE_OUT) {
			// identify, no disconnection allowed
			uint8 identify = 0x80 | request->target_lun;
			error = transfer(1, true, &identify, NULL, NULL, &status);
		} else if (phase == WD_PHASE_COMMAND) {
			error = transfer(request->cdb_length, true, request->cdb, NULL,
				NULL, &status);
		} else if (phase == WD_PHASE_DATA_IN || phase == WD_PHASE_DATA_OUT) {
			if ((phase == WD_PHASE_DATA_OUT) != dataOut
				&& request->data_length != 0) {
				ERROR("target %d: wrong data direction\n",
					request->target_id);
				error = B_ERROR;
				break;
			}
			// A request without data, or one whose data is used up, still
			// has to feed the phase: transfer bytes that go nowhere.
			uint32 length = cursor.bytesLeft > 0 ? cursor.bytesLeft : 512;
			error = transfer(length, phase == WD_PHASE_DATA_OUT, NULL,
				&cursor, &transferred, &status);
			dataDone += transferred;
		} else if (phase == WD_PHASE_STATUS) {
			uint8 deviceStatus;
			error = finish_command(&deviceStatus);
			if (error == B_OK) {
				request->device_status = deviceStatus;
				if (dataDone > request->data_length)
					dataDone = request->data_length;
				request->data_resid = request->data_length - dataDone;
				request->subsys_status = deviceStatus == 0	// GOOD
					? SCSI_REQ_CMP : SCSI_REQ_CMP_ERR;
				return;
			}
			break;
		} else {
			ERROR("target %d: unexpected phase %d (status %#x)\n",
				request->target_id, phase, status);
			error = B_ERROR;
			break;
		}

		if (error != B_OK)
			break;
	}

	ERROR("request for target %d failed (status %#x): %s\n",
		request->target_id, status, strerror(error));
	reset_controller();
	request->subsys_status = SCSI_SEQUENCE_FAIL;
}


//	#pragma mark - SIM interface


static void
set_scsi_bus(scsi_sim_cookie cookie, scsi_bus bus)
{
	wd33c93_controller* controller = (wd33c93_controller*)cookie;
	controller->bus = bus;
}


static void
scsi_io(scsi_sim_cookie cookie, scsi_ccb* request)
{
	wd33c93_controller* controller = (wd33c93_controller*)cookie;

	mutex_lock(&controller->lock);
	execute_request(controller, request);
	mutex_unlock(&controller->lock);

	TRACE("  -> status %#x, device status %#x, residual %" B_PRId32 "\n",
		request->subsys_status, request->device_status, request->data_resid);

	gSCSI->finished(request, 1);
}


static uchar
abort_io(scsi_sim_cookie cookie, scsi_ccb* request)
{
	// Requests complete before scsi_io() returns.
	return SCSI_REQ_CMP;
}


static uchar
reset_device(scsi_sim_cookie cookie, uchar targetID, uchar targetLUN)
{
	return SCSI_REQ_CMP;
}


static uchar
terminate_io(scsi_sim_cookie cookie, scsi_ccb* request)
{
	return SCSI_REQ_CMP;
}


static uchar
path_inquiry(scsi_sim_cookie cookie, scsi_path_inquiry* info)
{
	wd33c93_controller* controller = (wd33c93_controller*)cookie;
	if (controller->bus == NULL)
		return SCSI_NO_HBA;

	info->hba_inquiry = 0;
	info->hba_misc = 0;
	info->initiator_id = WD_HOST_ID;
	info->hba_queue_size = 1;
	memset(info->vuhba_flags, 0, sizeof(info->vuhba_flags));

	strlcpy(info->sim_vid, "Muse", SCSI_SIM_ID);
	strlcpy(info->hba_vid, "SGI", SCSI_HBA_ID);
	strlcpy(info->sim_version, "1.0", SCSI_VERS);
	strlcpy(info->hba_version, "1.0", SCSI_VERS);
	strlcpy(info->controller_family, "WD33C93", SCSI_FAM_ID);
	strlcpy(info->controller_type, "WD33C93B", SCSI_TYPE_ID);

	return SCSI_REQ_CMP;
}


static uchar
scan_bus(scsi_sim_cookie cookie)
{
	return SCSI_REQ_CMP;
}


static uchar
reset_bus(scsi_sim_cookie cookie)
{
	wd33c93_controller* controller = (wd33c93_controller*)cookie;

	mutex_lock(&controller->lock);
	status_t status = reset_controller();
	mutex_unlock(&controller->lock);

	return status == B_OK ? SCSI_REQ_CMP : SCSI_HBA_ERR;
}


static void
get_restrictions(scsi_sim_cookie cookie, uchar targetID, bool* isATAPI,
	bool* noAutoSense, uint32* maxBlocks)
{
	*isATAPI = false;
	// The SCSI bus manager sends REQUEST SENSE itself.
	*noAutoSense = true;
	*maxBlocks = 255;
}


static status_t
ioctl(scsi_sim_cookie cookie, uint8 targetID, uint32 op, void* buffer,
	size_t length)
{
	return B_DEV_INVALID_IOCTL;
}


static status_t
sim_init_bus(device_node* node, void** _cookie)
{
	wd33c93_controller* controller
		= new(std::nothrow) wd33c93_controller;
	if (controller == NULL)
		return B_NO_MEMORY;

	controller->node = node;
	controller->bus = NULL;
	mutex_init(&controller->lock, "wd33c93");

	status_t status = reset_controller();
	if (status != B_OK) {
		ERROR("controller reset failed: %s\n", strerror(status));
		mutex_destroy(&controller->lock);
		delete controller;
		return status;
	}

	dprintf("wd33c93: controller ready, host ID %d, polled I/O\n",
		WD_HOST_ID);

	*_cookie = controller;
	return B_OK;
}


static void
sim_uninit_bus(void* cookie)
{
	wd33c93_controller* controller = (wd33c93_controller*)cookie;
	mutex_destroy(&controller->lock);
	delete controller;
}


//	#pragma mark - device module


static float
wd33c93_supports_device(device_node* parent)
{
	const char* bus;
	const char* name;

	if (gDeviceManager->get_attr_string(parent, B_DEVICE_BUS, &bus, false)
			!= B_OK) {
		return -1;
	}
	if (strcmp(bus, "indy") != 0)
		return 0.0;

	if (gDeviceManager->get_attr_string(parent, INDY_DEVICE_NAME, &name,
			false) != B_OK || strcmp(name, "wd33c93") != 0) {
		return 0.0;
	}

	return 0.8;
}


static status_t
wd33c93_register_device(device_node* parent)
{
	device_attr attrs[] = {
		{ SCSI_DEVICE_MAX_TARGET_COUNT, B_UINT32_TYPE, { .ui32 = 8 } },
		{ SCSI_DEVICE_MAX_LUN_COUNT, B_UINT32_TYPE, { .ui32 = 1 } },
		{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
			{ .string = "WD33C93 SCSI controller" } },
		{}
	};

	return gDeviceManager->register_node(parent, WD33C93_DEVICE_MODULE_NAME,
		attrs, NULL, NULL);
}


static status_t
wd33c93_init_driver(device_node* node, void** _cookie)
{
	*_cookie = node;
	return B_OK;
}


static status_t
wd33c93_register_child_devices(void* cookie)
{
	device_node* node = (device_node*)cookie;

	device_attr attrs[] = {
		{ B_DEVICE_FIXED_CHILD, B_STRING_TYPE,
			{ .string = SCSI_FOR_SIM_MODULE_NAME } },
		{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
			{ .string = "WD33C93 SCSI bus" } },
		{ SCSI_DESCRIPTION_CONTROLLER_NAME, B_STRING_TYPE,
			{ .string = WD33C93_DEVICE_MODULE_NAME } },
		{ B_DMA_MAX_TRANSFER_BLOCKS, B_UINT32_TYPE, { .ui32 = 255 } },
		{}
	};

	return gDeviceManager->register_node(node, WD33C93_SIM_MODULE_NAME, attrs,
		NULL, NULL);
}


static status_t
std_ops(int32 op, ...)
{
	switch (op) {
		case B_MODULE_INIT:
		case B_MODULE_UNINIT:
			return B_OK;

		default:
			return B_ERROR;
	}
}


static scsi_sim_interface sSimInterface = {
	{
		{
			WD33C93_SIM_MODULE_NAME,
			0,
			std_ops
		},
		NULL,	// supported devices
		NULL,	// register node
		sim_init_bus,
		sim_uninit_bus,
		NULL,	// register child devices
		NULL,	// rescan
		NULL	// bus_removed
	},
	set_scsi_bus,
	scsi_io,
	abort_io,
	reset_device,
	terminate_io,
	path_inquiry,
	scan_bus,
	reset_bus,
	get_restrictions,
	ioctl
};


static driver_module_info sDeviceModule = {
	{
		WD33C93_DEVICE_MODULE_NAME,
		0,
		std_ops
	},
	wd33c93_supports_device,
	wd33c93_register_device,
	wd33c93_init_driver,
	NULL,	// uninit_driver
	wd33c93_register_child_devices,
	NULL,	// rescan
	NULL,	// device_removed
};


module_dependency module_dependencies[] = {
	{ B_DEVICE_MANAGER_MODULE_NAME, (module_info**)&gDeviceManager },
	{ SCSI_FOR_SIM_MODULE_NAME, (module_info**)&gSCSI },
	{}
};


module_info* modules[] = {
	(module_info*)&sDeviceModule,
	(module_info*)&sSimInterface,
	NULL
};
