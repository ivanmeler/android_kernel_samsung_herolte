/*
 * Copyright (C) 2015-2017 Samsung Electronics Co. Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

  /* usb notify layer v3.0 */


#ifndef __LINUX_USB_NOTIFY_SYSFS_H__
#define __LINUX_USB_NOTIFY_SYSFS_H__

#define MAX_DISABLE_STR_LEN 32


struct usb_notify_dev {
	const char *name;
	struct device *dev;
	struct otg_notify *o_notify;
	int index;
	unsigned long disable_state;
	char disable_state_cmd[MAX_DISABLE_STR_LEN];
	int (*set_disable)(struct usb_notify_dev *, int);
};

extern int usb_notify_dev_register(struct usb_notify_dev *ndev);
extern void usb_notify_dev_unregister(struct usb_notify_dev *ndev);
extern int usb_notify_class_init(void);
extern void usb_notify_class_exit(void);
#endif

