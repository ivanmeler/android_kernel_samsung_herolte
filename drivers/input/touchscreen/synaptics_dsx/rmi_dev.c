/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#include <linux/kernel.h>
#include <linux/module.h>
#include <asm/unaligned.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/uaccess.h>
#include "synaptics_i2c_rmi.h"

#define CHAR_DEVICE_NAME "rmi"
#define DEVICE_CLASS_NAME "rmidev"
#define ATTRIBUTE_FOLDER_NAME "rmidev"
#define DEV_NUMBER 1
#define REG_ADDR_LIMIT 0xFFFF

static ssize_t rmidev_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidev_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t rmidev_sysfs_open_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_release_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t rmidev_sysfs_attn_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static struct bin_attribute attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = rmidev_sysfs_data_show,
	.write = rmidev_sysfs_data_store,
};

RMI_KOBJ_ATTR(open, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error, rmidev_sysfs_open_store);
RMI_KOBJ_ATTR(release, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error, rmidev_sysfs_release_store);
RMI_KOBJ_ATTR(attn_state, S_IRUGO, rmidev_sysfs_attn_state_show, synaptics_rmi4_store_error);

static struct attribute *attrs[] = {
	&kobj_attr_open.attr,
	&kobj_attr_release.attr,
	&kobj_attr_attn_state.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static struct class *rmidev_device_class;
static int rmidev_major_num;

static irqreturn_t rmidev_sysfs_irq(int irq, void *data)
{
	struct synaptics_rmi4_data *rmi4_data = data;

	sysfs_notify(&rmi4_data->input_dev->dev.kobj,
			ATTRIBUTE_FOLDER_NAME, "attn_state");

	return IRQ_HANDLED;
}

static int rmidev_sysfs_irq_enable(struct synaptics_rmi4_data *rmi4_data,
		bool enable)
{
	int retval = 0;
	unsigned char intr_status[MAX_INTR_REGISTERS];
	unsigned long irq_flags = IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING;
	struct rmidev_handle *rmidev = rmi4_data->rmidev;

	if (enable) {
		if (rmidev->irq_enabled)
			return retval;

		/* Clear interrupts first */
		retval = rmi4_data->i2c_read(rmi4_data,
				rmi4_data->f01_data_base_addr + 1,
				intr_status,
				rmi4_data->num_of_intr_regs);
		if (retval < 0)
			return retval;

		retval = request_threaded_irq(rmi4_data->irq, NULL,
				rmidev_sysfs_irq, irq_flags,
				"synaptics_dsx_rmidev", rmi4_data);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create irq thread\n",
					__func__);
			return retval;
		}

		rmidev->irq_enabled = true;
	} else {
		if (rmidev->irq_enabled) {
			disable_irq(rmi4_data->irq);
			free_irq(rmi4_data->irq, rmi4_data);
			rmidev->irq_enabled = false;
		}
	}

	return retval;
}

static ssize_t rmidev_sysfs_data_show(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	if (length > (REG_ADDR_LIMIT - address)) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Out of register map limit\n",
				__func__);
		return -EINVAL;
	}

	if (length) {
		retval = rmi4_data->i2c_read(rmi4_data,
				address,
				(unsigned char *)buf,
				length);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to read data\n",
					__func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

	return length;
}

static ssize_t rmidev_sysfs_data_store(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	int retval;
	unsigned int length = (unsigned int)count;
	unsigned short address = (unsigned short)pos;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	if (length > (REG_ADDR_LIMIT - address)) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Out of register map limit\n",
				__func__);
		return -EINVAL;
	}

	if (length) {
		retval = rmi4_data->i2c_write(rmi4_data,
				address,
				(unsigned char *)buf,
				length);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to write data\n",
					__func__);
			return retval;
		}
	} else {
		return -EINVAL;
	}

	return length;
}

static ssize_t rmidev_sysfs_open_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	rmi4_data->irq_enable(rmi4_data, false);
	rmidev_sysfs_irq_enable(rmi4_data, true);

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Attention interrupt disabled\n",
			__func__);

	return count;
}

static ssize_t rmidev_sysfs_release_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	rmidev_sysfs_irq_enable(rmi4_data, false);
	rmi4_data->irq_enable(rmi4_data, true);

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Attention interrupt enabled\n",
			__func__);

	return count;
}

static ssize_t rmidev_sysfs_attn_state_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	int attn_state;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	attn_state = gpio_get_value(rmi4_data->board->gpio);

	return snprintf(buf, PAGE_SIZE, "%u\n", attn_state);
}

/*
 * rmidev_llseek - used to set up register address
 *
 * @filp: file structure for seek
 * @off: offset
 *   if whence == SEEK_SET,
 *     high 16 bits: page address
 *     low 16 bits: register address
 *   if whence == SEEK_CUR,
 *     offset from current position
 *   if whence == SEEK_END,
 *     offset from end position (0xFFFF)
 * @whence: SEEK_SET, SEEK_CUR, or SEEK_END
 */
static loff_t rmidev_llseek(struct file *filp, loff_t off, int whence)
{
	loff_t newpos;
	struct rmidev_data *dev_data = filp->private_data;
	struct rmidev_handle *rmidev = NULL;
	struct synaptics_rmi4_data *rmi4_data = NULL;

	if (IS_ERR(dev_data)) {
		pr_err("%s: Pointer of char device data is invalid", __func__);
		return -EBADF;
	}

	rmidev = container_of(dev_data, struct rmidev_handle, dev_data);
	rmi4_data = rmidev->rmi4_data;

	mutex_lock(&(dev_data->file_mutex));

	switch (whence) {
	case SEEK_SET:
		newpos = off;
		break;
	case SEEK_CUR:
		newpos = filp->f_pos + off;
		break;
	case SEEK_END:
		newpos = REG_ADDR_LIMIT + off;
		break;
	default:
		newpos = -EINVAL;
		goto clean_up;
	}

	if (newpos < 0 || newpos > REG_ADDR_LIMIT) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: New position 0x%04x is invalid\n",
				__func__, (unsigned int)newpos);
		newpos = -EINVAL;
		goto clean_up;
	}

	filp->f_pos = newpos;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return newpos;
}

/*
 * rmidev_read: - use to read data from rmi device
 *
 * @filp: file structure for read
 * @buf: user space buffer pointer
 * @count: number of bytes to read
 * @f_pos: offset (starting register address)
 */
static ssize_t rmidev_read(struct file *filp, char __user *buf,
		size_t count, loff_t *f_pos)
{
	ssize_t retval;
	unsigned char tmpbuf[count + 1];
	struct rmidev_data *dev_data = filp->private_data;
	struct rmidev_handle *rmidev = NULL;
	struct synaptics_rmi4_data *rmi4_data = NULL;

	if (IS_ERR(dev_data)) {
		pr_err("%s: Pointer of char device data is invalid", __func__);
		return -EBADF;
	}

	if (count == 0)
		return 0;

	rmidev = container_of(dev_data, struct rmidev_handle, dev_data);
	rmi4_data = rmidev->rmi4_data;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	mutex_lock(&(dev_data->file_mutex));

	retval = rmi4_data->i2c_read(rmi4_data,
			*f_pos,
			tmpbuf,
			count);
	if (retval < 0)
		goto clean_up;

	if (copy_to_user(buf, tmpbuf, count))
		retval = -EFAULT;
	else
		*f_pos += retval;

clean_up:
	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

/*
 * rmidev_write: - used to write data to rmi device
 *
 * @filep: file structure for write
 * @buf: user space buffer pointer
 * @count: number of bytes to write
 * @f_pos: offset (starting register address)
 */
static ssize_t rmidev_write(struct file *filp, const char __user *buf,
		size_t count, loff_t *f_pos)
{
	ssize_t retval;
	unsigned char tmpbuf[count + 1];
	struct rmidev_data *dev_data = filp->private_data;
	struct rmidev_handle *rmidev = NULL;
	struct synaptics_rmi4_data *rmi4_data = NULL;

	if (IS_ERR(dev_data)) {
		pr_err("%s: Pointer of char device data is invalid", __func__);
		return -EBADF;
	}

	if (count == 0)
		return 0;

	rmidev = container_of(dev_data, struct rmidev_handle, dev_data);
	rmi4_data = rmidev->rmi4_data;

	if (count > (REG_ADDR_LIMIT - *f_pos))
		count = REG_ADDR_LIMIT - *f_pos;

	if (copy_from_user(tmpbuf, buf, count))
		return -EFAULT;

	mutex_lock(&(dev_data->file_mutex));

	retval = rmi4_data->i2c_write(rmi4_data,
			*f_pos,
			tmpbuf,
			count);
	if (retval >= 0)
		*f_pos += retval;

	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

/*
 * rmidev_open: enable access to rmi device
 * @inp: inode struture
 * @filp: file structure
 */
static int rmidev_open(struct inode *inp, struct file *filp)
{
	int retval = 0;
	struct rmidev_data *dev_data =
		container_of(inp->i_cdev, struct rmidev_data, main_dev);
	struct rmidev_handle *rmidev = NULL;
	struct synaptics_rmi4_data *rmi4_data = NULL;

	if (!dev_data)
		return -EACCES;

	filp->private_data = dev_data;
	rmidev = container_of(dev_data, struct rmidev_handle, dev_data);
	rmi4_data = rmidev->rmi4_data;

	mutex_lock(&(dev_data->file_mutex));

	rmi4_data->irq_enable(rmi4_data, false);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Attention interrupt disabled\n",
			__func__);

	if (dev_data->ref_count < 1)
		dev_data->ref_count++;
	else
		retval = -EACCES;

	mutex_unlock(&(dev_data->file_mutex));

	return retval;
}

/*
 * rmidev_release: - release access to rmi device
 * @inp: inode structure
 * @filp: file structure
 */
static int rmidev_release(struct inode *inp, struct file *filp)
{
	struct rmidev_data *dev_data =
		container_of(inp->i_cdev, struct rmidev_data, main_dev);
	struct rmidev_handle *rmidev = NULL;
	struct synaptics_rmi4_data *rmi4_data = NULL;

	if (!dev_data)
		return -EACCES;

	rmidev = container_of(dev_data, struct rmidev_handle, dev_data);
	rmi4_data = rmidev->rmi4_data;

	mutex_lock(&(dev_data->file_mutex));

	dev_data->ref_count--;
	if (dev_data->ref_count < 0)
		dev_data->ref_count = 0;

	rmi4_data->irq_enable(rmi4_data, true);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: Attention interrupt enabled\n",
			__func__);

	mutex_unlock(&(dev_data->file_mutex));

	return 0;
}

static const struct file_operations rmidev_fops = {
	.owner = THIS_MODULE,
	.llseek = rmidev_llseek,
	.read = rmidev_read,
	.write = rmidev_write,
	.open = rmidev_open,
	.release = rmidev_release,
};

static void rmidev_device_cleanup(struct rmidev_data *dev_data)
{
	dev_t devno;
	struct rmidev_handle *rmidev = NULL;

	if (dev_data) {
		rmidev = container_of(dev_data, struct rmidev_handle, dev_data);

		devno = dev_data->main_dev.dev;

		if (dev_data->device_class)
			device_destroy(dev_data->device_class, devno);

		cdev_del(&dev_data->main_dev);

		unregister_chrdev_region(devno, 1);

		dev_dbg(&rmidev->rmi4_data->i2c_client->dev,
				"%s: rmidev device removed\n",
				__func__);
	}

	return;
}

static char *rmi_char_devnode(struct device *dev, umode_t *mode)
{
	if (!mode)
		return NULL;

	*mode = (S_IRUSR | S_IWUSR | S_IRGRP | S_IWGRP | S_IROTH | S_IWOTH);

	return kasprintf(GFP_KERNEL, "rmi/%s", dev_name(dev));
}

static int rmidev_create_device_class(struct synaptics_rmi4_data *rmi4_data)
{
	/* If class is not created, create first it. */
	if (!rmidev_device_class) {
		rmidev_device_class = class_create(THIS_MODULE, DEVICE_CLASS_NAME);

		if (IS_ERR(rmidev_device_class)) {
			dev_err(&rmi4_data->i2c_client->dev, "%s: Failed to create /dev/%s\n",
					__func__, CHAR_DEVICE_NAME);
			return -ENODEV;
		}

		rmidev_device_class->devnode = rmi_char_devnode;
	}

	return 0;
}

static int rmidev_init_device(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	dev_t dev_no;
	struct device *device_ptr;
	struct rmidev_handle *rmidev = NULL;

	rmidev = kzalloc(sizeof(struct rmidev_handle), GFP_KERNEL);
	if (!rmidev) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for rmidev\n",
				__func__);
		retval = -ENOMEM;
		goto err_rmidev;
	}

	rmi4_data->rmidev = rmidev;
	rmidev->rmi4_data = rmi4_data;

	retval = rmidev_create_device_class(rmi4_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create device class\n",
				__func__);
		goto err_device_class;
	}

	if (rmidev_major_num) {
		dev_no = MKDEV(rmidev_major_num, DEV_NUMBER);
		retval = register_chrdev_region(dev_no, 1, CHAR_DEVICE_NAME);
	} else {
		retval = alloc_chrdev_region(&dev_no, 0, 1, CHAR_DEVICE_NAME);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to allocate char device region\n",
					__func__);
			goto err_device_region;
		}

		rmidev_major_num = MAJOR(dev_no);
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Major number of rmidev = %d\n",
				__func__, rmidev_major_num);
	}

	mutex_init(&rmidev->dev_data.file_mutex);
	cdev_init(&rmidev->dev_data.main_dev, &rmidev_fops);

	retval = cdev_add(&rmidev->dev_data.main_dev, dev_no, 1);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to add rmi char device\n",
				__func__);
		goto err_char_device;
	}

	dev_set_name(&rmidev->dev, "rmidev%d", MINOR(dev_no));
	rmidev->dev_data.device_class = rmidev_device_class;

	device_ptr = device_create(rmidev->dev_data.device_class, NULL, dev_no,
			NULL, CHAR_DEVICE_NAME"%d", MINOR(dev_no));
	if (IS_ERR(device_ptr)) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create rmi char device\n",
				__func__);
		retval = -ENODEV;
		goto err_char_device;
	}

	retval = gpio_export(rmi4_data->board->gpio, false);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to export attention gpio\n",
				__func__);
	} else {
		retval = gpio_export_link(&(rmi4_data->input_dev->dev),
				"attn", rmi4_data->board->gpio);
		if (retval < 0) {
			dev_err(&rmi4_data->input_dev->dev,
					"%s Failed to create gpio symlink\n",
					__func__);
		} else {
			dev_dbg(&rmi4_data->input_dev->dev,
					"%s: Exported attention gpio %d\n",
					__func__, rmi4_data->board->gpio);
		}
	}

	rmidev->attr_dir = kobject_create_and_add(ATTRIBUTE_FOLDER_NAME,
			&rmi4_data->input_dev->dev.kobj);
	if (!rmidev->attr_dir) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs directory\n",
				__func__);
		retval = -ENODEV;
		goto err_attr_dir;
	}

	retval = sysfs_create_bin_file(rmidev->attr_dir,
			&attr_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto err_sysfs_bin;
	}

	retval = sysfs_create_group(rmidev->attr_dir, &attr_group);
	if (retval < 0) {
		dev_err(&rmi4_data->input_dev->dev,
					"%s: Failed to create sysfs attributes\n", __func__);
			retval = -ENODEV;
			goto err_sysfs_attrs;
	}

	return 0;

err_sysfs_attrs:
	sysfs_remove_group(rmidev->attr_dir, &attr_group);
	sysfs_remove_bin_file(rmidev->attr_dir, &attr_data);

err_sysfs_bin:
	kobject_put(rmidev->attr_dir);

err_attr_dir:
err_char_device:
	rmidev_device_cleanup(&rmidev->dev_data);
	mutex_destroy(&rmidev->dev_data.file_mutex);
	unregister_chrdev_region(dev_no, 1);

err_device_region:
	class_destroy(rmidev_device_class);

err_device_class:
	kfree(rmidev);
	rmi4_data->rmidev = NULL;

err_rmidev:
	return retval;
}

static void rmidev_remove_device(struct synaptics_rmi4_data *rmi4_data)
{
	struct rmidev_handle *rmidev = rmi4_data->rmidev;
	struct rmidev_data *dev_data = NULL;

	if (!rmidev)
		goto exit;

	sysfs_remove_group(rmidev->attr_dir, &attr_group);

	sysfs_remove_bin_file(rmidev->attr_dir, &attr_data);

	kobject_put(rmidev->attr_dir);

	dev_data = &rmidev->dev_data;
	if (dev_data) {
		rmidev_device_cleanup(dev_data);
		mutex_destroy(&dev_data->file_mutex);
	}

	unregister_chrdev_region(rmidev->dev_no, 1);

	class_destroy(rmidev_device_class);

	kfree(rmidev);
	rmi4_data->rmidev = NULL;

exit:
	return;
}

int rmidev_module_register(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_new_function(RMI_DEV,
			rmi4_data,
			rmidev_init_device,
			NULL,
			rmidev_remove_device,
			NULL);

	return retval;
}
