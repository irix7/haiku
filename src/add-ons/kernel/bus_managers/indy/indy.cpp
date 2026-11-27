/*
 * Copyright 2026, Haiku, Inc. All rights reserved.
 * Distributed under the terms of the MIT License.
 */


/*!	Root bus of the SGI Indy.

	The machine has no bus that can be enumerated. Its devices sit at fixed
	addresses with fixed interrupts (arch_indy.h). This bus manager attaches
	to the device manager's root node and registers one node for each device
	a new-style driver exists for, with that driver as its fixed child. A
	driver recognises its node by the bus "indy" and the INDY_DEVICE_NAME
	attribute, and takes the addresses and the interrupt vector from
	arch_indy.h.
*/


#include <string.h>

#include <device_manager.h>
#include <KernelExport.h>

#include <arch_indy.h>


#define INDY_BUS_MODULE_NAME	"bus_managers/indy/root/driver_v1"

// Attribute of a device node: the device's name (string)
#define INDY_DEVICE_NAME		"indy/device"


struct indy_device {
	const char*	name;
	const char*	prettyName;
	const char*	driver;
};

static const indy_device kDevices[] = {
	{ "wd33c93", "WD33C93 SCSI controller",
		"busses/scsi/wd33c93_hpc3/driver_v1" },
};

static device_manager_info* sDeviceManager;


static float
indy_bus_supports_device(device_node* parent)
{
	// make sure parent is really the device root
	const char* bus;
	if (sDeviceManager->get_attr_string(parent, B_DEVICE_BUS, &bus, false)
			!= B_OK) {
		return B_ERROR;
	}

	if (strcmp(bus, "root") != 0)
		return 0.0;

	return 1.0;
}


static status_t
indy_bus_register_device(device_node* parent)
{
	device_attr attrs[] = {
		{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE, { .string = "SGI Indy" } },
		{ B_DEVICE_FLAGS, B_UINT32_TYPE, { .ui32 = B_KEEP_DRIVER_LOADED } },
		{}
	};

	return sDeviceManager->register_node(parent, INDY_BUS_MODULE_NAME, attrs,
		NULL, NULL);
}


static status_t
indy_bus_init(device_node* node, void** _cookie)
{
	*_cookie = node;
	return B_OK;
}


static void
indy_bus_uninit(void* cookie)
{
}


static status_t
indy_bus_register_child_devices(void* cookie)
{
	device_node* node = (device_node*)cookie;

	for (size_t i = 0; i < sizeof(kDevices) / sizeof(kDevices[0]); i++) {
		device_attr attrs[] = {
			{ B_DEVICE_PRETTY_NAME, B_STRING_TYPE,
				{ .string = kDevices[i].prettyName } },
			{ B_DEVICE_BUS, B_STRING_TYPE, { .string = "indy" } },
			{ INDY_DEVICE_NAME, B_STRING_TYPE,
				{ .string = kDevices[i].name } },
			{ B_DEVICE_FIXED_CHILD, B_STRING_TYPE,
				{ .string = kDevices[i].driver } },
			{}
		};

		status_t status = sDeviceManager->register_node(node,
			"bus_managers/indy/device/driver_v1", attrs, NULL, NULL);
		if (status != B_OK) {
			dprintf("indy: could not register \"%s\": %s\n",
				kDevices[i].name, strerror(status));
		}
	}

	return B_OK;
}


static status_t
indy_device_init(device_node* node, void** _cookie)
{
	*_cookie = node;
	return B_OK;
}


static void
indy_device_uninit(void* cookie)
{
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


static driver_module_info sBusModule = {
	{
		INDY_BUS_MODULE_NAME,
		0,
		std_ops
	},

	indy_bus_supports_device,
	indy_bus_register_device,
	indy_bus_init,
	indy_bus_uninit,
	indy_bus_register_child_devices,
	NULL,	// rescan devices
	NULL,	// device removed
};

// The driver of a device node. It has no function of its own: the node
// exists so that the device's real driver can attach below it.
static driver_module_info sDeviceModule = {
	{
		"bus_managers/indy/device/driver_v1",
		0,
		std_ops
	},

	NULL,	// supports device
	NULL,	// register device (the bus registers the nodes)
	indy_device_init,
	indy_device_uninit,
	NULL,	// register child devices
	NULL,	// rescan devices
	NULL,	// device removed
};


module_dependency module_dependencies[] = {
	{ B_DEVICE_MANAGER_MODULE_NAME, (module_info**)&sDeviceManager },
	{}
};

module_info* modules[] = {
	(module_info*)&sBusModule,
	(module_info*)&sDeviceModule,
	NULL
};
