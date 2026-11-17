/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


#include <stdlib.h>
#include <string.h>

#include <KernelExport.h>

#include <arch_indy.h>
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


/*!	Returns the value of an ARCS environment variable, or NULL. The PROM
	passes the environment as an array of 32 bit string pointers.
*/
static const char*
arcs_environment(const char* name)
{
	const uint32* environment
		= (const uint32*)(addr_t)gKernelArgs.platform_args.arcs_envp;
	if (environment == NULL)
		return NULL;

	size_t length = strlen(name);
	for (int32 i = 0; i < 128 && environment[i] != 0; i++) {
		const char* entry = (const char*)(addr_t)environment[i];
		if (strncmp(entry, name, length) == 0 && entry[length] == '=')
			return entry + length + 1;
	}
	return NULL;
}


/*!	Whether the PROM command line has the given word as an argument, as in
	"boot -f bootp()haiku disk".
*/
static bool
arcs_has_argument(const char* word)
{
	const uint32* arguments
		= (const uint32*)(addr_t)gKernelArgs.platform_args.arcs_argv;
	uint32 count = gKernelArgs.platform_args.arcs_argc;
	if (arguments == NULL || count > 32)
		return false;

	for (uint32 i = 1; i < count; i++) {
		const char* argument = (const char*)(addr_t)arguments[i];
		if (argument != NULL && strcasecmp(argument, word) == 0)
			return true;
	}
	return false;
}


/*!	Returns the value of a "name=value" argument of the PROM command line,
	or NULL.
*/
static const char*
arcs_argument_value(const char* name)
{
	const uint32* arguments
		= (const uint32*)(addr_t)gKernelArgs.platform_args.arcs_argv;
	uint32 count = gKernelArgs.platform_args.arcs_argc;
	if (arguments == NULL || count > 32)
		return NULL;

	size_t length = strlen(name);
	for (uint32 i = 1; i < count; i++) {
		const char* argument = (const char*)(addr_t)arguments[i];
		if (argument != NULL && strncmp(argument, name, length) == 0
			&& argument[length] == '=') {
			return argument + length + 1;
		}
	}
	return NULL;
}


/*!	Hands the kernel a driver settings file "remote_disk" that names the
	server.
*/
static void
add_remote_disk_settings(const char* server)
{
	char settings[64];
	snprintf(settings, sizeof(settings), "server %s\n", server);
	size_t length = strlen(settings);

	char* buffer = (char*)kernel_args_malloc(length + 1);
	driver_settings_file* file = (driver_settings_file*)kernel_args_malloc(
		sizeof(driver_settings_file));
	if (buffer == NULL || file == NULL)
		return;

	strlcpy(file->name, "remote_disk", sizeof(file->name));
	memcpy(buffer, settings, length + 1);
	file->buffer = buffer;
	file->size = length;
	file->next = gKernelArgs.driver_settings;
	gKernelArgs.driver_settings = file;

	dprintf("remote disk server: %s\n", server);
}


static bool
parse_ip_address(const char* string, uint32* _address)
{
	uint32 address = 0;
	for (int i = 0; i < 4; i++) {
		char* end;
		unsigned long part = strtoul(string, &end, 10);
		if (end == string || part > 255)
			return false;
		address = (address << 8) | part;
		if (i < 3) {
			if (*end != '.')
				return false;
			string = end + 1;
		}
	}
	*_address = address;
	return true;
}


/*!	The station address, as the seeq8003 driver determines it: from the
	NVRAM, or a fixed local address if the NVRAM holds none.
*/
static uint64
station_address()
{
	uint8 bytes[6];
	bool valid = false;
	for (int i = 0; i < 6; i++) {
		bytes[i] = indy_read8(INDY_RTC_REGISTER(INDY_NVRAM_MAC_ADDRESS + i));
		if (bytes[i] != 0xff && bytes[i] != 0x00)
			valid = true;
	}
	if (!valid) {
		static const uint8 kFallback[6] = { 0x0a, 0x00, 0x69, 0x00, 0x00,
			0x01 };
		memcpy(bytes, kFallback, 6);
	}

	uint64 address = 0;
	for (int i = 0; i < 6; i++)
		address = (address << 8) | bytes[i];
	return address;
}


/*!	The archive is not a volume the kernel can mount, so the kernel is told
	where to find one.

	Default: the network. The kernel brings up the Ethernet interface with
	the address the PROM used ($netaddr) and mounts the BFS volume that a
	remote_disk_server on the local network offers.

	With the argument "disk" on the PROM command line, or without a
	$netaddr: a hard disk whose identity is unknown, so the kernel mounts the
	first BFS volume it finds on a SCSI disk.
*/
status_t
platform_register_boot_device(Node* device, disk_identifier* defaultDiskID)
{
	uint32 clientIP;
	const char* netAddress = arcs_environment("netaddr");
	if (!arcs_has_argument("disk") && netAddress != NULL
		&& parse_ip_address(netAddress, &clientIP)) {
		uint64 clientMAC = station_address();
		dprintf("boot volume: remote disk, client IP %s, MAC %012" B_PRIx64
			"\n", netAddress, clientMAC);

		gBootParams.SetInt32(BOOT_METHOD, BOOT_METHOD_NET);
		gBootParams.SetInt64("client MAC", clientMAC);
		gBootParams.SetInt32("client IP", clientIP);

		// The remote_disk driver finds its server with a broadcast. A
		// server that a broadcast does not reach is named with
		// "server=ADDRESS" on the PROM command line or with the PROM
		// variable "haikuserver".
		const char* server = arcs_argument_value("server");
		if (server == NULL)
			server = arcs_environment("haikuserver");
		uint32 serverIP;
		if (server != NULL && parse_ip_address(server, &serverIP))
			add_remote_disk_settings(server);

		return B_OK;
	}

	dprintf("boot volume: first BFS volume on a SCSI disk\n");

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
