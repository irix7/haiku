/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	Driver for the Seeq 8003 Ethernet controller of the SGI Indy.

	The controller sends and receives through two DMA channels of the HPC3.
	The descriptors and the buffers are in one contiguous area and are
	accessed through kseg1, which is uncached: the HPC3 reads and writes
	memory behind the CPU's caches.
*/


#include <stdlib.h>
#include <string.h>

#include <Drivers.h>
#include <KernelExport.h>
#include <OS.h>

#include <arch/cpu.h>
#include <arch_indy.h>
#include <ether_driver.h>
#include <kernel.h>
#include <lock.h>
#include <net/if_media.h>
#include <util/AutoLock.h>

#include "seeq8003.h"


#define DEVICE_NAME	"seeq8003"

//#define TRACE_SEEQ
#ifdef TRACE_SEEQ
#	define TRACE(x...) dprintf(DEVICE_NAME ": " x)
#else
#	define TRACE(x...) ;
#endif
#define ERROR(x...) dprintf(DEVICE_NAME ": " x)


struct seeq_device {
	int32				openCount;
	uint8				macAddress[6];
	uint32				blockFlag;

	area_id				dmaArea;
	phys_addr_t			dmaPhysical;
	uint8*				dmaBase;			// kseg1

	hpc3_descriptor*	rxDescriptors;
	uint8*				rxBuffers;
	hpc3_descriptor*	txDescriptor;
	uint8*				txBuffer;
	uint32				rxIndex;

	sem_id				rxSem;
	mutex				txLock;
	mutex				rxLock;
};


int32 api_version = B_CUR_DRIVER_API_VERSION;

static const char* sDeviceNames[] = { "net/" DEVICE_NAME "/0", NULL };
static seeq_device sDevice;
static mutex sOpenLock;

static const size_t kDmaSize = ROUNDUP(
	(SEEQ_RX_RING_SIZE + 1) * sizeof(hpc3_descriptor)
		+ (SEEQ_RX_RING_SIZE + 1) * SEEQ_RX_BUFFER_SIZE, B_PAGE_SIZE);


static inline void
seeq_write_register(uint8 index, uint8 value)
{
	indy_write8(INDY_SEEQ_REGISTER(index), value);
}


static inline uint8
seeq_read_register(uint8 index)
{
	return indy_read8(INDY_SEEQ_REGISTER(index));
}


static inline uint32
physical_of(seeq_device& device, const void* address)
{
	return (uint32)(device.dmaPhysical
		+ ((const uint8*)address - device.dmaBase));
}


/*!	Reads the station address from the NVRAM. All ones means that the
	NVRAM's battery is dead.
*/
static void
read_mac_address(uint8* address)
{
	bool valid = false;
	for (int i = 0; i < 6; i++) {
		address[i] = indy_read8(INDY_RTC_REGISTER(INDY_NVRAM_MAC_ADDRESS + i));
		if (address[i] != 0xff && address[i] != 0x00)
			valid = true;
	}

	if (!valid) {
		ERROR("no station address in the NVRAM, using a local one\n");
		static const uint8 kFallback[6] = { 0x0a, 0x00, 0x69, 0x00, 0x00,
			0x01 };
		memcpy(address, kFallback, 6);
	}
}


static void
reset_channel()
{
	// The reset clears the two configuration registers.
	uint32 dmaConfig = indy_read32(INDY_HPC3_ENET_DMACFG);
	uint32 pioConfig = indy_read32(INDY_HPC3_ENET_PIOCFG);

	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_RESET,
		HPC3_ENET_RESET_CHANNEL | HPC3_ENET_RESET_CLRINT);
	spin(1000);
	indy_write32(INDY_HPC3_ENET_RESET, 0);

	indy_write32(INDY_HPC3_ENET_DMACFG, dmaConfig);
	indy_write32(INDY_HPC3_ENET_PIOCFG, pioConfig);
}


static void
start_receiver(seeq_device& device)
{
	for (uint32 i = 0; i < SEEQ_RX_RING_SIZE; i++) {
		hpc3_descriptor& descriptor = device.rxDescriptors[i];
		descriptor.buffer = physical_of(device,
			device.rxBuffers + i * SEEQ_RX_BUFFER_SIZE);
		descriptor.control = SEEQ_RX_ARMED;
		descriptor.next = physical_of(device,
			&device.rxDescriptors[(i + 1) % SEEQ_RX_RING_SIZE]);
	}
	device.rxIndex = 0;

	uint32 first = physical_of(device, &device.rxDescriptors[0]);
	indy_write32(INDY_HPC3_ENET_RX_CBP, first);
	indy_write32(INDY_HPC3_ENET_RX_NDBP, first);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_RX_CTRL, HPC3_ENET_CTRL_ACTIVE);
}


static int32
seeq_interrupt(void* data)
{
	seeq_device* device = (seeq_device*)data;

	// The order matters: read the Seeq status registers, clear the
	// interrupt at the channel, then write only the DMA bits back to the
	// HPC3 interrupt status. Writing the device bit while its line is
	// asserted makes the interrupt fire without end.
	seeq_read_register(SEEQ_RX_COMMAND);
	seeq_read_register(SEEQ_TX_COMMAND);
	indy_write32(INDY_HPC3_ENET_RESET, HPC3_ENET_RESET_CLRINT);
	indy_write32(INDY_HPC3_INTSTAT,
		indy_read32(INDY_HPC3_INTSTAT) & HPC3_INTSTAT_ENET_DMA);

	release_sem_etc(device->rxSem, 1, B_DO_NOT_RESCHEDULE);
	return B_INVOKE_SCHEDULER;
}


//	#pragma mark - device hooks


static status_t
seeq_open(const char* name, uint32 flags, void** _cookie)
{
	if (strcmp(name, sDeviceNames[0]) != 0)
		return B_BAD_VALUE;

	MutexLocker locker(sOpenLock);
	seeq_device& device = sDevice;

	if (device.openCount > 0)
		return B_BUSY;

	void* address;
	device.dmaArea = create_area("seeq8003 dma", &address,
		B_ANY_KERNEL_ADDRESS, kDmaSize, B_32_BIT_CONTIGUOUS,
		B_KERNEL_READ_AREA | B_KERNEL_WRITE_AREA);
	if (device.dmaArea < 0)
		return device.dmaArea;

	physical_entry entry;
	status_t status = get_memory_map(address, kDmaSize, &entry, 1);
	if (status != B_OK) {
		delete_area(device.dmaArea);
		return status;
	}

	// Push the zeroes of the new area out of the cache, then use the
	// memory through kseg1 only.
	memset(address, 0, kDmaSize);
	arch_cpu_dma_sync_for_device(entry.address, kDmaSize);

	device.dmaPhysical = entry.address;
	device.dmaBase = (uint8*)MIPS_KSEG1(entry.address);
	device.rxDescriptors = (hpc3_descriptor*)device.dmaBase;
	device.txDescriptor = &device.rxDescriptors[SEEQ_RX_RING_SIZE];
	device.rxBuffers = device.dmaBase
		+ (SEEQ_RX_RING_SIZE + 1) * sizeof(hpc3_descriptor);
	device.txBuffer = device.rxBuffers
		+ SEEQ_RX_RING_SIZE * SEEQ_RX_BUFFER_SIZE;

	device.rxSem = create_sem(0, "seeq8003 receive");
	if (device.rxSem < 0) {
		delete_area(device.dmaArea);
		return device.rxSem;
	}
	mutex_init(&device.txLock, "seeq8003 transmit");
	mutex_init(&device.rxLock, "seeq8003 receive");
	device.blockFlag = 0;

	read_mac_address(device.macAddress);
	dprintf(DEVICE_NAME ": station address %02x:%02x:%02x:%02x:%02x:%02x\n",
		device.macAddress[0], device.macAddress[1], device.macAddress[2],
		device.macAddress[3], device.macAddress[4], device.macAddress[5]);

	reset_channel();

	// Register bank 0 holds the station address.
	seeq_write_register(SEEQ_TX_COMMAND, 0);
	for (int i = 0; i < 6; i++)
		seeq_write_register(SEEQ_STATION_ADDRESS + i, device.macAddress[i]);
	seeq_write_register(SEEQ_RX_COMMAND,
		SEEQ_RX_STATION_MULTI | SEEQ_RX_INTERRUPT_EOF);

	install_io_interrupt_handler(INDY_IRQ_ETHERNET, seeq_interrupt, &device,
		0);
	start_receiver(device);

	device.openCount = 1;
	*_cookie = &device;
	return B_OK;
}


static status_t
seeq_close(void* cookie)
{
	seeq_device* device = (seeq_device*)cookie;

	// stop both channels and wake up a blocked reader
	indy_write32(INDY_HPC3_ENET_RX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	seeq_write_register(SEEQ_RX_COMMAND, 0);
	remove_io_interrupt_handler(INDY_IRQ_ETHERNET, seeq_interrupt, device);

	delete_sem(device->rxSem);
	device->rxSem = -1;
	return B_OK;
}


static status_t
seeq_free(void* cookie)
{
	seeq_device* device = (seeq_device*)cookie;
	MutexLocker locker(sOpenLock);

	mutex_destroy(&device->txLock);
	mutex_destroy(&device->rxLock);
	delete_area(device->dmaArea);
	device->openCount = 0;
	return B_OK;
}


static status_t
seeq_read(void* cookie, off_t position, void* buffer, size_t* _length)
{
	seeq_device* device = (seeq_device*)cookie;
	MutexLocker locker(device->rxLock);

	for (;;) {
		hpc3_descriptor& descriptor = device->rxDescriptors[device->rxIndex];
		uint32 control = descriptor.control;

		if ((control & HPC3_DESC_OWN) != 0) {
			// Nothing received yet. A channel that has stopped (it ran
			// into a descriptor it does not own) is started again.
			if ((indy_read32(INDY_HPC3_ENET_RX_CTRL) & HPC3_ENET_CTRL_ACTIVE)
					== 0) {
				start_receiver(*device);
			}

			status_t status = acquire_sem_etc(device->rxSem, 1,
				B_CAN_INTERRUPT | device->blockFlag, 0);
			if (status != B_OK) {
				*_length = 0;
				return status;
			}
			continue;
		}

		// The count field holds the bytes that were left over.
		uint32 remaining = control & HPC3_DESC_COUNT_MASK;
		uint32 total = remaining < SEEQ_RX_MAX_COUNT
			? SEEQ_RX_MAX_COUNT - remaining : 0;
		const uint8* data = device->rxBuffers
			+ device->rxIndex * SEEQ_RX_BUFFER_SIZE;

		bool good = false;
		size_t length = 0;
		if (total >= SEEQ_RX_OVERHEAD) {
			length = total - SEEQ_RX_OVERHEAD;
			good = length >= 14 && length <= SEEQ_MAX_FRAME_SIZE
				&& (data[2 + length] & SEEQ_RX_STATUS_GOOD) != 0;
		}

		status_t status = B_OK;
		if (good) {
			if (length > *_length)
				length = *_length;
			status = user_memcpy(buffer, data + 2, length);
		}

		// hand the descriptor back to the HPC3
		descriptor.control = SEEQ_RX_ARMED;
		device->rxIndex = (device->rxIndex + 1) % SEEQ_RX_RING_SIZE;

		if (!good)
			continue;

		*_length = status == B_OK ? length : 0;
		return status;
	}
}


static status_t
seeq_write(void* cookie, off_t position, const void* buffer, size_t* _length)
{
	seeq_device* device = (seeq_device*)cookie;
	size_t length = *_length;
	if (length > SEEQ_MAX_FRAME_SIZE)
		length = SEEQ_MAX_FRAME_SIZE;

	MutexLocker locker(device->txLock);

	status_t status = user_memcpy(device->txBuffer, buffer, length);
	if (status != B_OK) {
		*_length = 0;
		return status;
	}

	size_t count = length;
	if (count < SEEQ_MIN_FRAME_SIZE) {
		memset(device->txBuffer + count, 0, SEEQ_MIN_FRAME_SIZE - count);
		count = SEEQ_MIN_FRAME_SIZE;
	}

	uint32 descriptor = physical_of(*device, device->txDescriptor);
	device->txDescriptor->buffer = physical_of(*device, device->txBuffer);
	device->txDescriptor->control = HPC3_DESC_EOX | HPC3_DESC_EOP | count;
	device->txDescriptor->next = descriptor;

	indy_write32(INDY_HPC3_ENET_TX_CBP, descriptor);
	indy_write32(INDY_HPC3_ENET_TX_NDBP, descriptor);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
	indy_write32(INDY_HPC3_ENET_TX_CTRL, HPC3_ENET_CTRL_ACTIVE);

	// A frame takes about a millisecond at 10 Mbit/s.
	bigtime_t timeout = system_time() + 100000;
	while ((indy_read32(INDY_HPC3_ENET_TX_CTRL) & HPC3_ENET_CTRL_ACTIVE)
			!= 0) {
		if (system_time() > timeout) {
			ERROR("transmit timeout\n");
			indy_write32(INDY_HPC3_ENET_TX_CTRL, 0);
			*_length = 0;
			return B_TIMED_OUT;
		}
		snooze(200);
	}

	*_length = length;
	return B_OK;
}


static status_t
seeq_control(void* cookie, uint32 op, void* argument, size_t length)
{
	seeq_device* device = (seeq_device*)cookie;

	switch (op) {
		case ETHER_INIT:
			return B_OK;

		case ETHER_GETADDR:
			return user_memcpy(argument, device->macAddress, 6);

		case ETHER_NONBLOCK:
		{
			int32 value;
			if (user_memcpy(&value, argument, sizeof(value)) != B_OK)
				return B_BAD_ADDRESS;
			device->blockFlag = value != 0 ? B_RELATIVE_TIMEOUT : 0;
			return B_OK;
		}

		case ETHER_GETFRAMESIZE:
		{
			uint32 frameSize = SEEQ_MAX_FRAME_SIZE;
			return user_memcpy(argument, &frameSize, sizeof(frameSize));
		}

		case ETHER_GET_LINK_STATE:
		{
			// The controller is a 10 Mbit/s half duplex device and cannot
			// report its link.
			ether_link_state_t state;
			state.media = IFM_ACTIVE | IFM_ETHER | IFM_10_T
				| IFM_HALF_DUPLEX;
			state.speed = 10000000;
			state.quality = 1000;
			return user_memcpy(argument, &state, sizeof(state));
		}

		case ETHER_ADDMULTI:
		case ETHER_REMMULTI:
			// All multicast frames are received.
			return B_OK;

		case ETHER_SETPROMISC:
			return B_NOT_SUPPORTED;
	}

	return B_DEV_INVALID_IOCTL;
}


static device_hooks sDeviceHooks = {
	seeq_open,
	seeq_close,
	seeq_free,
	seeq_control,
	seeq_read,
	seeq_write,
	NULL,
	NULL,
	NULL,
	NULL
};


//	#pragma mark - driver hooks


extern "C" status_t
init_hardware(void)
{
	return B_OK;
}


extern "C" status_t
init_driver(void)
{
	memset(&sDevice, 0, sizeof(sDevice));
	mutex_init(&sOpenLock, "seeq8003 open");
	return B_OK;
}


extern "C" void
uninit_driver(void)
{
	mutex_destroy(&sOpenLock);
}


extern "C" const char**
publish_devices(void)
{
	return sDeviceNames;
}


extern "C" device_hooks*
find_device(const char* name)
{
	if (strcmp(name, sDeviceNames[0]) == 0)
		return &sDeviceHooks;
	return NULL;
}
