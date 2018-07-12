#ifndef __CCIC_SYSFS__
#define __CCIC_SYSFS__

extern const struct attribute_group ccic_sysfs_group;

#define CCIC_MAX_FW_PATH	64
#define CCIC_DEFAULT_FW		"usbpd/s2mm005.bin"
#define CCIC_DEFAULT_FULL_FW		"usbpd/USB_PD_FULL_DRIVER.bin"
#define CCIC_DEFAULT_UMS_FW			"/sdcard/Firmware/usbpd/s2mm005.bin"
#define CCIC_DEFAULT_FULL_UMS_FW	"/sdcard/Firmware/usbpd/USB_PD_FULL_DRIVER.bin"

enum {
	BUILT_IN = 0,
	UMS,
};

#endif
