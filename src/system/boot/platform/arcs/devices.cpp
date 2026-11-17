/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <KernelExport.h>

#include <boot/disk_identifier.h>
#include <boot/partitions.h>
#include <boot/platform.h>
#include <boot/stage2.h>
#include <boot/stdio.h>
#include <boot/vfs.h>
#include <util/kernel_cpp.h>


/*!	The boot device is the archive that was appended to the loader image.
	It holds the kernel and the boot modules.
*/
status_t
platform_add_boot_device(struct stage2_args* args, NodeList* devicesList)
{
	if (args->platform.archive_data == NULL
		|| args->platform.archive_size == 0) {
		dprintf("no archive appended to the loader\n");
		return B_DEVICE_NOT_FOUND;
	}

	MemoryDisk* disk = new(nothrow) MemoryDisk(
		(const uint8*)args->platform.archive_data,
		args->platform.archive_size, "boot.tgz");
	if (disk == NULL)
		return B_NO_MEMORY;

	devicesList->Add(disk);
	return B_OK;
}


status_t
platform_get_boot_partitions(struct stage2_args* args, Node* device,
	NodeList* list, NodeList* partitionList)
{
	NodeIterator iterator = list->GetIterator();
	boot::Partition* partition = (boot::Partition*)iterator.Next();
	if (partition == NULL)
		return B_ENTRY_NOT_FOUND;

	partitionList->Insert(partition);
	dprintf("haiku_loader: archive mounted\n");
	return B_OK;
}


status_t
platform_add_block_devices(stage2_args* args, NodeList* devicesList)
{
	return B_OK;
}


/*!	The archive is not a volume the kernel can mount. The kernel is told
	that it was booted from an image on a hard disk whose identity is
	unknown, so it mounts the first BFS volume it finds: the Haiku image on
	the SCSI disk.
*/
status_t
platform_register_boot_device(Node* device, disk_identifier* defaultDiskID)
{
	disk_identifier identifier;
	identifier.bus_type = UNKNOWN_BUS;
	identifier.device_type = UNKNOWN_DEVICE;
	identifier.device.unknown.size = 0;
	for (int32 i = 0; i < NUM_DISK_CHECK_SUMS; i++) {
		identifier.device.unknown.check_sums[i].offset = -1;
		identifier.device.unknown.check_sums[i].sum = 0;
	}

	gBootParams.SetInt32(BOOT_METHOD, BOOT_METHOD_HARD_DISK);
	gBootParams.SetBool(BOOT_VOLUME_BOOTED_FROM_IMAGE, true);
	gBootParams.SetData(BOOT_VOLUME_DISK_IDENTIFIER, B_RAW_TYPE,
		&identifier, sizeof(disk_identifier));

	return B_OK;
}


void
platform_cleanup_devices()
{
}
