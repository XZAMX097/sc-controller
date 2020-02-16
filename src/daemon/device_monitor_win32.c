/*
 * SC-Controller - Device Monitor - Windows
 *
 * Basically does nothing. Scanning is done by usb_helper
 */
#define LOG_TAG "DevMon"
#include "scc/utils/container_of.h"
#include "scc/utils/logging.h"
#include "scc/utils/hashmap.h"
#include "scc/utils/intmap.h"
#include "scc/utils/assert.h"
#include "scc/utils/math.h"
#include "scc/input_device.h"
#include "daemon.h"
#include <windows.h>
#include <unistd.h>
#include <stdio.h>
#ifdef USE_DINPUT
#include <winbase.h>
#include <dinput.h>
#endif
#if USE_HIDAPI
#include <winbase.h>
#include <setupapi.h>
#include <hidapi.h>

typedef struct HidapiDevice {
	char							fake_syspath[256];
	intmap_t						idx_to_path;
	enum HidapiDeviceType {
		HIDAPIDT_NEW,
		HIDAPIDT_KNOWN,
		HIDAPIDT_REMOVED
	}								type;
} HidapiDevice;

map_t path_to_hidapidevice = NULL;
#endif


void sccd_device_monitor_init() {
	sccd_device_monitor_common_init();
#ifdef USE_HIDAPI
	path_to_hidapidevice = hashmap_new();
	ASSERT(path_to_hidapidevice != NULL);
#endif
}

void sccd_device_monitor_close() {
	sccd_device_monitor_close_common();
}


bool sccd_device_monitor_test_filter(Daemon* d, const InputDeviceData* idev, const HotplugFilter* filter) {
	struct Win32InputDeviceData* wdev = container_of(idev, struct Win32InputDeviceData, idev);
	switch (filter->type) {
	case SCCD_HOTPLUG_FILTER_VENDOR:
		return (idev->subsystem != DINPUT) && (wdev->vendor == filter->vendor);
	case SCCD_HOTPLUG_FILTER_PRODUCT:
		return (idev->subsystem != DINPUT) && (wdev->product == filter->product);
	case SCCD_HOTPLUG_FILTER_IDX:
		return (wdev->idx == filter->idx);
	case SCCD_HOTPLUG_FILTER_NAME:
#ifdef USE_DINPUT
		if (idev->subsystem == DINPUT) {
			const DIDEVICEINSTANCE* d8dev = (const DIDEVICEINSTANCE*)wdev->d8dev;
			return 0 == strcmp(d8dev->tszInstanceName, filter->name);
		}
#endif
		return false;
	case SCCD_HOTPLUG_FILTER_GUID:
#ifdef USE_DINPUT
		if (idev->subsystem == DINPUT) {
			static char buffer[256];
			const DIDEVICEINSTANCE* d8dev = (const DIDEVICEINSTANCE*)wdev->d8dev;
			LPOLESTR guid_str;
			if (S_OK != StringFromCLSID(&d8dev->guidInstance, &guid_str))
				return false;
			snprintf(buffer, 256, "%ls", guid_str);
			CoTaskMemFree(guid_str);
			return 0 == strcmp(buffer, filter->name);
		}
#endif
		return false;
	default:
		return false;
	}
}


static char* input_device_get_name(const InputDeviceData* idev) {
#ifdef USE_DINPUT
	if (idev->subsystem == DINPUT) {
		struct Win32InputDeviceData* wdev = container_of(idev, struct Win32InputDeviceData, idev);
		const DIDEVICEINSTANCE* d8dev = (const DIDEVICEINSTANCE*)wdev->d8dev;
		return strbuilder_cpy(d8dev->tszInstanceName);
	}
#endif
	return NULL;
}

static int input_device_get_idx(const InputDeviceData* idev) {
	struct Win32InputDeviceData* wdev = container_of(idev, struct Win32InputDeviceData, idev);
	return wdev->idx;
}

static InputDevice* input_device_open(const InputDeviceData* idev) {
#ifdef USE_DINPUT
	return sccd_input_dinput_open(idev);
#endif
#ifdef USE_HIDAPI
	if (idev->subsystem == HIDAPI)
		return sccd_input_hidapi_open(idev->path);
#endif
#ifdef USE_LIBUSB
	if (idev->subsystem == USB)
		return sccd_input_libusb_open(idev->path);
#endif
	return NULL;
}

static char* input_device_get_prop(const InputDeviceData* idev, const char* name) {
	return NULL;
}

static void input_device_free(InputDeviceData* idev) {
	free((char*)idev->path);
	free(idev);
}

static InputDeviceData* input_device_copy(const InputDeviceData* idev) {
	return NULL;
}


void sccd_device_monitor_win32_fill_struct(struct Win32InputDeviceData* wdev) {
	wdev->idev.get_prop = input_device_get_prop;
	wdev->idev.get_name = input_device_get_name;
	wdev->idev.get_idx = input_device_get_idx;
	wdev->idev.free = input_device_free;
	wdev->idev.open = input_device_open;
	wdev->idev.copy = input_device_copy;
}

void sccd_device_monitor_rescan() {
#if !defined(USE_LIBUSB) && !defined(USE_HIDAPI)
#error "At least one of USE_LIBUSB, USE_HIDAPI has to be enabled"
#endif
#ifdef USE_LIBUSB
	sccd_input_libusb_rescan();
#endif
#ifdef USE_HIDAPI
	sccd_input_hidapi_rescan();
#endif
#ifdef USE_DINPUT
	sccd_input_dinput_rescan();
#endif
}

