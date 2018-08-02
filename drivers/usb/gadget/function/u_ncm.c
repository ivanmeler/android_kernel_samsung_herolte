/*
 * File Name : u_ncm.c
 *
 * ncm utilities for composite USB gadgets.
 * This utilitie can support to connect head unit for mirror link
 *
 * Copyright (C) 2011 Samsung Electronics
 * Author: SoonYong, Cho <soonyong.cho@samsung.com>
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include "f_ncm.c"
#include <linux/miscdevice.h>

/* Support dynamic tethering mode.
 * if ncm_connect is true, device is received vendor specific request
 * from head unit.
 */

struct ncm_dev {
	struct work_struct work;
};


static const char mirrorlink_shortname[] = "usb_ncm";
/* Create misc driver for Mirror Link cmd */
static struct miscdevice mirrorlink_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = mirrorlink_shortname,
	//.fops = &mirrorlink_fops,
};

static bool ncm_connect;

/* terminal version using vendor specific request */
u16 terminal_mode_version;
u16 terminal_mode_vendor_id;

struct ncm_function_config {
	u8      ethaddr[ETH_ALEN];
	struct eth_dev *dev;
};

static struct ncm_dev *_ncm_dev;

static void ncm_work(struct work_struct *data)
{
	char *ncm_start[2] = { "NCM_DEVICE=START", NULL };
	char *ncm_release[2] = { "NCM_DEVICE=RELEASE", NULL };
	char **uevent_envp = NULL;

	printk(KERN_DEBUG "usb: %s ncm_connect=%d\n", __func__, ncm_connect);

	if ( ncm_connect==true )
		uevent_envp = ncm_start;
	else
		uevent_envp = ncm_release;

	kobject_uevent_env(&mirrorlink_device.this_device->kobj, KOBJ_CHANGE, uevent_envp);
}

static int ncm_function_init(struct android_usb_function *f,
		struct usb_composite_dev *cdev)
{
	struct ncm_dev *dev;
	int ret=0;

	dev = kzalloc(sizeof(*dev), GFP_KERNEL);
	if (!dev)
		return -ENOMEM;

	f->config = kzalloc(sizeof(struct ncm_function_config), GFP_KERNEL);

	if (!f->config)
	{
		kfree(dev);
		return -ENOMEM;
	}
	INIT_WORK(&dev->work, ncm_work);

	_ncm_dev = dev;

	ret = misc_register(&mirrorlink_device);
	if (ret)
		printk("usb: %s - usb_ncm misc driver fail \n",__func__);
	return 0;
}

static void ncm_function_cleanup(struct android_usb_function *f)
{
	misc_deregister(&mirrorlink_device);
	kfree(_ncm_dev);
	kfree(f->config);
	f->config = NULL;
	_ncm_dev = NULL;
}


static int ncm_function_bind_config(struct android_usb_function *f,
					struct usb_configuration *c)
{
	int ret;
	int i;
	char *src;
	struct ncm_function_config *ncm = f->config;
#ifdef CONFIG_USB_ANDROID_SAMSUNG_COMPOSITE
	struct eth_dev *e_dev;
#endif

	if (!ncm) {
		pr_err("%s: ncm_pdata\n", __func__);
		return -1;
	}

	ncm = f->config;
	if (!f->config)
		return -ENOMEM;

	for (i = 0; i < ETH_ALEN; i++)
		ncm->ethaddr[i] = 0;
	/* create a fake MAC address from our serial number.
	 * first byte is 0x02 to signify locally administered.
	 */
	ncm->ethaddr[0] = 0x02;
	src = serial_string;
	for (i = 0; (i < 256) && *src; i++) {
		/* XOR the USB serial across the remaining bytes */
		ncm->ethaddr[i % (ETH_ALEN - 1) + 1] ^= *src++;
	}

	printk(KERN_DEBUG "usb: %s MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			__func__, ncm->ethaddr[0], ncm->ethaddr[1],
			ncm->ethaddr[2], ncm->ethaddr[3], ncm->ethaddr[4],
			ncm->ethaddr[5]);

	printk(KERN_DEBUG "usb: %s before MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			__func__, ncm->ethaddr[0], ncm->ethaddr[1],
			ncm->ethaddr[2], ncm->ethaddr[3], ncm->ethaddr[4],
			ncm->ethaddr[5]);
	/* we have to use trick.
	 * rndis name will be used for ethernet interface name.
	 */

		e_dev = gether_setup_name(c->cdev->gadget, ncm->ethaddr, "ncm");

	if (IS_ERR(e_dev)) {
		ret = PTR_ERR(e_dev);
		pr_err("%s: gether_setup failed\n", __func__);
		return ret;
	}
	ncm->dev = e_dev;

	printk(KERN_DEBUG "usb: %s after MAC:%02X:%02X:%02X:%02X:%02X:%02X\n",
			__func__, ncm->ethaddr[0], ncm->ethaddr[1],
			ncm->ethaddr[2], ncm->ethaddr[3], ncm->ethaddr[4],
			ncm->ethaddr[5]);
	return ncm_bind_config(c, ncm->ethaddr, ncm->dev);

}

static void ncm_function_unbind_config(struct android_usb_function *f,
						struct usb_configuration *c)
{
	struct ncm_function_config *ncm = f->config;
	gether_cleanup(ncm->dev);
}

static struct android_usb_function ncm_function = {
	.name		= "ncm",
	.init		= ncm_function_init,
	.cleanup	= ncm_function_cleanup,
	.bind_config	= ncm_function_bind_config,
	.unbind_config	= ncm_function_unbind_config,
};

bool is_ncm_ready(char *name)
{
	/* Enable ncm function */
	if (!strcmp(name, "rndis") || !strcmp(name, "ncm")) {
		if (ncm_connect) {
			printk(KERN_DEBUG "usb: %s ncm ready (%s)\n",
					__func__, name);
			return true;
		}
	}
	return false;
}

void set_ncm_device_descriptor(struct usb_device_descriptor *desc)
{
	desc->idProduct = 0x685d;
	desc->bDeviceClass = USB_CLASS_COMM;
	printk(KERN_DEBUG "usb: %s idProduct=0x%x, DeviceClass=0x%x\n",
			__func__, desc->idProduct, desc->bDeviceClass);

}

void set_ncm_ready(bool ready)
{
	if (ready != ncm_connect)
	{
		printk(KERN_DEBUG "usb: %s old status=%d, new status=%d\n",
				__func__, ncm_connect, ready);
		ncm_connect = ready;
		schedule_work(&_ncm_dev->work);
	}
	else
		ncm_connect = ready;

	if (ready == false) {
		terminal_mode_version = 0;
		terminal_mode_vendor_id = 0;
	}
}
EXPORT_SYMBOL(set_ncm_ready);

static ssize_t terminal_version_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	ret = sprintf(buf, "major %x minor %x vendor %x\n",
			terminal_mode_version & 0xff,
			(terminal_mode_version >> 8 & 0xff),
			terminal_mode_vendor_id);
	if(terminal_mode_version)
		printk(KERN_DEBUG "usb: %s terminal_mode %s\n", __func__, buf);
	return ret;
}

static ssize_t terminal_version_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	sscanf(buf, "%x", &value);
	terminal_mode_version = (u16)value;
	printk(KERN_DEBUG "usb: %s buf=%s\n", __func__, buf);
	/* only set ncm ready when terminal verision value is not zero */
	if(value)
		set_ncm_ready(true);
	else
		set_ncm_ready(false);
	return size;
}

static DEVICE_ATTR(terminal_version,  S_IRUGO | S_IWUSR,
		terminal_version_show, terminal_version_store);

static int create_terminal_attribute(struct device **pdev)
{
	int err;
	if (IS_ERR(*pdev)) {
		printk(KERN_DEBUG "usb: %s error pdev(%p)\n",
				__func__, *pdev);
		return PTR_ERR(*pdev);
	}

	err = device_create_file(*pdev, &dev_attr_terminal_version);
	if (err) {
		printk(KERN_DEBUG "usb: %s failed to create attr\n",
				__func__);
		return err;
	}
	return 0;
}

static int terminal_ctrl_request(struct usb_composite_dev *cdev,
				const struct usb_ctrlrequest *ctrl)
{
	int	value = -EOPNOTSUPP;
	u16	w_index = le16_to_cpu(ctrl->wIndex);
	u16	w_value = le16_to_cpu(ctrl->wValue);

	if ((ctrl->bRequestType & USB_TYPE_MASK) == USB_TYPE_VENDOR) {
		/* Handle Terminal mode request */
		if (ctrl->bRequest == 0xf0) {
			terminal_mode_version = w_value;
			terminal_mode_vendor_id = w_index;
			set_ncm_ready(true);
			printk(KERN_DEBUG "usb: %s ver=0x%x vendor_id=0x%x\n",
				__func__, terminal_mode_version,
				terminal_mode_vendor_id);
			value = 0;
		}
	}

	/* respond ZLP */
	if (value >= 0) {
		int rc;
		cdev->req->zero = 0;
		cdev->req->length = value;
		rc = usb_ep_queue(cdev->gadget->ep0, cdev->req, GFP_ATOMIC);
		if (rc < 0)
			printk(KERN_DEBUG "usb: %s failed usb_ep_queue\n",
					__func__);
	}
	return value;
}
