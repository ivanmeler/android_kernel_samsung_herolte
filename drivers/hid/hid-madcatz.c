/*
 *  HID driver for some Mad Catz "special" devices
 *
 *  Copyright (c) 1999 Andreas Gal
 *  Copyright (c) 2000-2005 Vojtech Pavlik <vojtech@suse.cz>
 *  Copyright (c) 2005 Michael Haboustak <mike-@cinci.rr.com> for Concept2, Inc
 *  Copyright (c) 2006-2007 Jiri Kosina
 *  Copyright (c) 2007 Paul Walmsley
 *  Copyright (c) 2008 Jiri Slaby
 *  Copyright (c) 2010 Don Prince <dhprince.devel@yahoo.co.uk>
 *
 *
 *  This driver supports several HID devices:
 *
 *  S.U.R.F.R For Samsung
 *  C.T.R.L.R For Samsung
 *
 */

/*
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the Free
 * Software Foundation; either version 2 of the License, or (at your option)
 * any later version.
 */

#include <linux/device.h>
#include <linux/usb.h>
#include <linux/hid.h>
#include <linux/module.h>

#include "hid-ids.h"

/*
 * There are several variants for 0419:0001:
 *
 * 1. 184 byte report descriptor
 * Vendor specific report #4 has a size of 48 bit,
 * and therefore is not accepted when inspecting the descriptors.
 * As a workaround we reinterpret the report as:
 *   Variable type, count 6, size 8 bit, log. maximum 255
 * The burden to reconstruct the data is moved into user space.
 *
 * 2. 203 byte report descriptor
 * Report #4 has an array field with logical range 0..18 instead of 1..15.
 *
 * 3. 135 byte report descriptor
 * Report #4 has an array field with logical range 0..17 instead of 1..14.
 *
 * 4. 171 byte report descriptor
 * Report #3 has an array field with logical range 0..1 instead of 1..3.
 */

#define madcatz_gamepad_map_key_clear(c) \
	hid_map_usage_clear(hi, usage, bit, max, EV_KEY, (c))

static int madcatz_universal_kbd_input_mapping(struct hid_device *hdev,
	struct hid_input *hi, struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max)
{
	if (!(HID_UP_BUTTON == (usage->hid & HID_USAGE_PAGE) ||
			HID_UP_KEYBOARD == (usage->hid & HID_USAGE_PAGE) ||
			HID_UP_CONSUMER == (usage->hid & HID_USAGE_PAGE)))
		return 0;

	dbg_hid("madcatz game controller input mapping event [0x%x], [0x%x], %ld, %ld, [0x%x]\n",
		usage->hid, usage->hid & HID_USAGE, hi->input->evbit[0], hi->input->absbit[0], usage->hid & HID_USAGE_PAGE);

	if (HID_UP_BUTTON == (usage->hid & HID_USAGE_PAGE)) {
		switch(usage->hid & HID_USAGE) {
		/* GAME */
		case 0x10: madcatz_gamepad_map_key_clear(BTN_GAME); break;
		default:
			return 0;
		}
	}

	if (HID_UP_KEYBOARD == (usage->hid & HID_USAGE_PAGE)) {
		switch (usage->hid & HID_USAGE) {
		set_bit(EV_REP, hi->input->evbit);
		/* Only for UK keyboard */
#ifdef CONFIG_HID_KK_UPGRADE
		case 0x32: madcatz_gamepad_map_key_clear(KEY_KBDILLUMTOGGLE); break;
		case 0x64: madcatz_gamepad_map_key_clear(KEY_BACKSLASH); break;
#else
		case 0x32: madcatz_gamepad_map_key_clear(KEY_BACKSLASH); break;
		case 0x64: madcatz_gamepad_map_key_clear(KEY_102ND); break;
#endif
		/* Only for BR keyboard */
		case 0x87: madcatz_gamepad_map_key_clear(KEY_RO); break;
		default:
			return 0;
		}
	}	

	if (HID_UP_CONSUMER == (usage->hid & HID_USAGE_PAGE)) {
		switch (usage->hid & HID_USAGE) {
		/* report 2 */
		/* RECENTAPPS */
		case 0x29d: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY1); break;
		case 0x301: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY1); break;
		/* APPLICATION */
		case 0x302: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY2); break;
		/* Voice search */
		case 0x305: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY4); break;
		/* QPANEL on/off */
		case 0x306: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY5); break;
		/* SIP on/off */
		case 0x307: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY3); break;
		/* LANG */
		case 0x308: madcatz_gamepad_map_key_clear(KEY_LANGUAGE); break;
		/* BRIGHTNESS DOWN */
		case 0x30a: madcatz_gamepad_map_key_clear(KEY_BRIGHTNESSDOWN); break;
		/* BRIGHTNESS UP */        
		case 0x30b: madcatz_gamepad_map_key_clear(KEY_BRIGHTNESSUP); break;
		/* S-Finder */
		case 0x304: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY7); break;
		/* Screen Capture */
		case 0x303: madcatz_gamepad_map_key_clear(KEY_SYSRQ); break;
		/* Multi Window */
		case 0x309: madcatz_gamepad_map_key_clear(BTN_TRIGGER_HAPPY9); break;
		default:
			return 0;
		}
	}

	return 1;
}

static __u8 *madcatz_report_fixup(struct hid_device *hdev, __u8 *rdesc,
	unsigned int *rsize)
{
	return rdesc;
}

static int madcatz_input_mapping(struct hid_device *hdev, struct hid_input *hi,
	struct hid_field *field, struct hid_usage *usage,
	unsigned long **bit, int *max)
{
	int ret = 0;

    if(USB_DEVICE_ID_MADCATZ_SURFR_SMAPP == hdev->product)
		ret = madcatz_universal_kbd_input_mapping(hdev,
			hi, field, usage, bit, max);
	else if(USB_DEVICE_ID_MADCATZ_CTRLR_SMAPP == hdev->product)
		ret = madcatz_universal_kbd_input_mapping(hdev,
			hi, field, usage, bit, max);

	return ret;
}

static int madcatz_probe(struct hid_device *hdev,
		const struct hid_device_id *id)
{
	int ret;
	unsigned int cmask = HID_CONNECT_DEFAULT;

	ret = hid_parse(hdev);
	if (ret) {
		hid_err(hdev, "parse failed\n");
		goto err_free;
	}

	if (USB_DEVICE_ID_SAMSUNG_IR_REMOTE == hdev->product) {
		if (hdev->rsize == 184) {
			/* disable hidinput, force hiddev */
			cmask = (cmask & ~HID_CONNECT_HIDINPUT) |
				HID_CONNECT_HIDDEV_FORCE;
		}
	}

	ret = hid_hw_start(hdev, cmask);
	if (ret) {
		hid_err(hdev, "hw start failed\n");
		goto err_free;
	}

	return 0;
err_free:
	return ret;
}


static const struct hid_device_id madcatz_devices[] = {
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MADCATZ, USB_DEVICE_ID_MADCATZ_SURFR_SMAPP) },
	{ HID_BLUETOOTH_DEVICE(USB_VENDOR_ID_MADCATZ, USB_DEVICE_ID_MADCATZ_CTRLR_SMAPP) },
	{ }
};
MODULE_DEVICE_TABLE(hid, madcatz_devices);

static struct hid_driver madcatz_driver = {
	.name = "madcatz",
	.id_table = madcatz_devices,
	.report_fixup = madcatz_report_fixup,
	.input_mapping = madcatz_input_mapping,
	.probe = madcatz_probe,
};

static int __init madcatz_init(void)
{
	return hid_register_driver(&madcatz_driver);
}

static void __exit madcatz_exit(void)
{
	hid_unregister_driver(&madcatz_driver);
}

module_init(madcatz_init);
module_exit(madcatz_exit);
MODULE_LICENSE("GPL");
