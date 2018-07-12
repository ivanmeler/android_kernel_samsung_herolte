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
//#include <mach/cpufreq.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include "synaptics_i2c_rmi.h"

#define DO_STARTUP_FW_UPDATE
#define STARTUP_FW_UPDATE_DELAY_MS 1000 /* ms */
#define FORCE_UPDATE false
#define DO_LOCKDOWN false

#define MAX_IMAGE_NAME_LEN 256
#define MAX_FIRMWARE_ID_LEN 10

#define F01_DEVICE_STATUS	0X0004

#define BOOTLOADER_ID_OFFSET 0
#define BLOCK_NUMBER_OFFSET 0

#define V5_PROPERTIES_OFFSET 2
#define V5_BLOCK_SIZE_OFFSET 3
#define V5_BLOCK_COUNT_OFFSET 5
#define V5_BLOCK_DATA_OFFSET 2

#define V6_PROPERTIES_OFFSET 1
#define V6_BLOCK_SIZE_OFFSET 2
#define V6_BLOCK_COUNT_OFFSET 3
#define V6_BLOCK_DATA_OFFSET 1
#define V6_FLASH_COMMAND_OFFSET 2
#define V6_FLASH_STATUS_OFFSET 3

#define V7_FLASH_STATUS_OFFSET 0
#define V7_PARTITION_ID_OFFSET 1
#define V7_BLOCK_NUMBER_OFFSET 2
#define V7_TRANSFER_LENGTH_OFFSET 3
#define V7_COMMAND_OFFSET 4
#define V7_PAYLOAD_OFFSET 5

#define V7_PARTITION_SUPPORT_BYTES 4

#define IMG_VERSION_OFFSET 0x07
#define IMG_X10_TOP_CONTAINER_OFFSET 0x0C
#define IMG_X0_X6_FW_OFFSET 0x100

#define UI_CONFIG_AREA 0x00
#define PERM_CONFIG_AREA 0x01
#define BL_CONFIG_AREA 0x02
#define DISP_CONFIG_AREA 0x03

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define MIN_SLEEP_TIME_US 50
#define MAX_SLEEP_TIME_US 100
#define STATUS_POLLING_PERIOD_US 3000

#define POLLING_MODE_DEFAULT 0

#define ATTRIBUTE_FOLDER_NAME "fwu"

enum f34_version {
	F34_V0 = 0,
	F34_V1,
	F34_V2,
};

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_do_reflash_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_write_config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_read_config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_config_area_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_image_size_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_block_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_firmware_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_configuration_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_perm_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_bl_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_disp_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_guest_code_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf);

static ssize_t fwu_sysfs_write_guest_code_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count);

static void fwu_img_parse_format(struct synaptics_rmi4_data *rmi4_data);
static int fwu_do_write_guest_code(struct synaptics_rmi4_data *rmi4_data);


static struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = fwu_sysfs_show_image,
	.write = fwu_sysfs_store_image,
};

RMI_KOBJ_ATTR(doreflash, S_IWUSR | S_IWGRP,	synaptics_rmi4_show_error, fwu_sysfs_do_reflash_store);
RMI_KOBJ_ATTR(writeconfig, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error, fwu_sysfs_write_config_store);
RMI_KOBJ_ATTR(readconfig, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error,	fwu_sysfs_read_config_store);
RMI_KOBJ_ATTR(configarea, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error,	fwu_sysfs_config_area_store);
RMI_KOBJ_ATTR(imagesize, S_IWUSR | S_IWGRP,	synaptics_rmi4_show_error, fwu_sysfs_image_size_store);
RMI_KOBJ_ATTR(blocksize, S_IRUGO, fwu_sysfs_block_size_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(fwblockcount, S_IRUGO, fwu_sysfs_firmware_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(configblockcount, S_IRUGO, fwu_sysfs_configuration_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(permconfigblockcount, S_IRUGO, fwu_sysfs_perm_config_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(blconfigblockcount, S_IRUGO, fwu_sysfs_bl_config_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(dispconfigblockcount, S_IRUGO, fwu_sysfs_disp_config_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(guestcodeblockcount, S_IRUGO, fwu_sysfs_guest_code_block_count_show, synaptics_rmi4_store_error);
RMI_KOBJ_ATTR(writeguestcode, S_IWUSR | S_IWGRP, synaptics_rmi4_show_error, fwu_sysfs_write_guest_code_store);

static struct attribute *attrs[] = {
	&kobj_attr_doreflash.attr,
	&kobj_attr_writeconfig.attr,
	&kobj_attr_readconfig.attr,
	&kobj_attr_configarea.attr,
	&kobj_attr_imagesize.attr,
	&kobj_attr_blocksize.attr,
	&kobj_attr_fwblockcount.attr,
	&kobj_attr_configblockcount.attr,
	&kobj_attr_permconfigblockcount.attr,
	&kobj_attr_blconfigblockcount.attr,
	&kobj_attr_dispconfigblockcount.attr,
	&kobj_attr_guestcodeblockcount.attr,
	&kobj_attr_writeguestcode.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attrs,
};

static unsigned int extract_uint_le(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
		(unsigned int)ptr[1] * 0x100 +
		(unsigned int)ptr[2] * 0x10000 +
		(unsigned int)ptr[3] * 0x1000000;
}

#ifdef FW_UPDATE_GO_NOGO
static unsigned int extract_uint_be(const unsigned char *ptr)
{
	return (unsigned int)ptr[3] +
			(unsigned int)ptr[2] * 0x100 +
			(unsigned int)ptr[1] * 0x10000 +
			(unsigned int)ptr[0] * 0x1000000;
}
#endif

static unsigned int le_to_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

static unsigned short extract_ushort_le(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] + (unsigned int)ptr[1] * 0x100;
}

static int fwu_read_f01_device_status(struct synaptics_rmi4_data *rmi4_data, struct synaptics_rmi4_f01_device_status *status)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f01_fd.data_base_addr,
			status->data,
			sizeof(status->data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device status\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_wait_for_idle(struct synaptics_rmi4_data *rmi4_data, int timeout_ms);


static int fwu_write_f34_command_single_transaction_v7(struct synaptics_rmi4_data *rmi4_data, unsigned char cmd)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	int retval;
	unsigned char base;
	struct f34_v7_data_1_5 data_1_5;

	base = fwu->f34_fd.data_base_addr;

	memset(data_1_5.data, 0x00, sizeof(data_1_5.data));

	switch (cmd) {
	case v7_CMD_ERASE_ALL:
		data_1_5.partition_id = CORE_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE_AP;
		break;
	case v7_CMD_ERASE_UI_FIRMWARE:
		data_1_5.partition_id = CORE_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ERASE_BL_CONFIG:
		data_1_5.partition_id = GLOBAL_PARAMETERS_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ERASE_UI_CONFIG:
		data_1_5.partition_id = CORE_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ERASE_DISP_CONFIG:
		data_1_5.partition_id = DISPLAY_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ERASE_FLASH_CONFIG:
		data_1_5.partition_id = FLASH_CONFIG_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ERASE_GUEST_CODE:
		data_1_5.partition_id = GUEST_CODE_PARTITION;
		data_1_5.command = CMD_V7_ERASE;
		break;
	case v7_CMD_ENABLE_FLASH_PROG:
		data_1_5.partition_id = BOOTLOADER_PARTITION;
		data_1_5.command = CMD_V7_ENTER_BL;
		break;
	};

	data_1_5.payload_0 = fwu->bootloader_id[0];
	data_1_5.payload_1 = fwu->bootloader_id[1];

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.partition_id,
			data_1_5.data,
			sizeof(data_1_5.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write single transaction command\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_write_f34_command_v7(struct synaptics_rmi4_data *rmi4_data, unsigned char cmd)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	int retval;
	unsigned char base;
	unsigned char command;

	base = fwu->f34_fd.data_base_addr;

	switch (cmd) {
	case v7_CMD_WRITE_FW:
	case v7_CMD_WRITE_CONFIG:
	case v7_CMD_WRITE_GUEST_CODE:
		command = CMD_V7_WRITE;
		break;
	case v7_CMD_READ_CONFIG:
		command = CMD_V7_READ;
		break;
	case v7_CMD_ERASE_ALL:
		command = CMD_V7_ERASE_AP;
		break;
	case v7_CMD_ERASE_UI_FIRMWARE:
	case v7_CMD_ERASE_BL_CONFIG:
	case v7_CMD_ERASE_UI_CONFIG:
	case v7_CMD_ERASE_DISP_CONFIG:
	case v7_CMD_ERASE_FLASH_CONFIG:
	case v7_CMD_ERASE_GUEST_CODE:
		command = CMD_V7_ERASE;
		break;
	case v7_CMD_ENABLE_FLASH_PROG:
		command = CMD_V7_ENTER_BL;
		break;
	default:
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Invalid command 0x%02x\n",
				__func__, cmd);
		return -EINVAL;
	};

	fwu->command = command;

	switch (cmd) {
	case v7_CMD_ERASE_ALL:
	case v7_CMD_ERASE_UI_FIRMWARE:
	case v7_CMD_ERASE_BL_CONFIG:
	case v7_CMD_ERASE_UI_CONFIG:
	case v7_CMD_ERASE_DISP_CONFIG:
	case v7_CMD_ERASE_FLASH_CONFIG:
	case v7_CMD_ERASE_GUEST_CODE:
	case v7_CMD_ENABLE_FLASH_PROG:
		retval = fwu_write_f34_command_single_transaction_v7(rmi4_data, cmd);
		if (retval < 0)
			return retval;
		else
			return 0;
	default:
		break;
	};

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write flash command\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_write_f34_v7_partition_id(struct synaptics_rmi4_data *rmi4_data, unsigned char cmd)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	int retval;
	unsigned char base;
	unsigned char partition;

	base = fwu->f34_fd.data_base_addr;

	switch (cmd) {
	case v7_CMD_WRITE_FW:
		partition = CORE_CODE_PARTITION;
		break;
	case v7_CMD_WRITE_CONFIG:
	case v7_CMD_READ_CONFIG:
		if (fwu->config_area == v7_UI_CONFIG_AREA)
			partition = CORE_CONFIG_PARTITION;
		else if (fwu->config_area == v7_DP_CONFIG_AREA)
			partition = DISPLAY_CONFIG_PARTITION;
		else if (fwu->config_area == v7_PM_CONFIG_AREA)
			partition = GUEST_SERIALIZATION_PARTITION;
		else if (fwu->config_area == v7_BL_CONFIG_AREA)
			partition = GLOBAL_PARAMETERS_PARTITION;
		else if (fwu->config_area == v7_FLASH_CONFIG_AREA)
			partition = FLASH_CONFIG_PARTITION;
		break;
	case v7_CMD_WRITE_GUEST_CODE:
		partition = GUEST_CODE_PARTITION;
		break;
	case v7_CMD_ERASE_ALL:
		partition = CORE_CODE_PARTITION;
		break;
	case v7_CMD_ERASE_BL_CONFIG:
		partition = GLOBAL_PARAMETERS_PARTITION;
		break;
	case v7_CMD_ERASE_UI_CONFIG:
		partition = CORE_CONFIG_PARTITION;
		break;
	case v7_CMD_ERASE_DISP_CONFIG:
		partition = DISPLAY_CONFIG_PARTITION;
		break;
	case v7_CMD_ERASE_FLASH_CONFIG:
		partition = FLASH_CONFIG_PARTITION;
		break;
	case v7_CMD_ERASE_GUEST_CODE:
		partition = GUEST_CODE_PARTITION;
		break;
	case v7_CMD_ENABLE_FLASH_PROG:
		partition = BOOTLOADER_PARTITION;
		break;
	default:
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Invalid command 0x%02x\n",
				__func__, cmd);
		return -EINVAL;
	};

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.partition_id,
			&partition,
			sizeof(partition));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write partition ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_write_f34_partition_id(struct synaptics_rmi4_data *rmi4_data, unsigned char cmd)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	int retval;

	if (fwu->bl_version == BL_V7)
		retval = fwu_write_f34_v7_partition_id(rmi4_data, cmd);
	else
		retval = 0;

	return retval;
}

static int fwu_read_f34_v7_partition_table(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	int retval;
	unsigned char base;
	unsigned char length[2];
	unsigned short block_number = 0;

	base = fwu->f34_fd.data_base_addr;

	fwu->config_area = v7_FLASH_CONFIG_AREA;

	retval = fwu_write_f34_partition_id(rmi4_data, v7_CMD_READ_CONFIG);
	if (retval < 0)
		return retval;

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.block_number,
			(unsigned char *)&block_number,
			sizeof(block_number));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write block number\n",
				__func__);
		return retval;
	}

	length[0] = (unsigned char)(fwu->flash_config_length & MASK_8BIT);
	length[1] = (unsigned char)(fwu->flash_config_length >> 8);

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.transfer_length,
			length,
			sizeof(length));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write transfer length\n",
				__func__);
		return retval;
	}

	retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_READ_CONFIG);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write command\n",
				__func__);
		return retval;
	}

	fwu->polling_mode = true;
	retval = fwu_wait_for_idle(rmi4_data, WRITE_WAIT_MS);
	fwu->polling_mode = false;
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to wait for idle status\n",
				__func__);
		return retval;
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			base + fwu->off.payload,
			fwu->read_config_buf,
			fwu->partition_table_bytes);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read block data\n",
				__func__);
		return retval;
	}

	return 0;
}

static void fwu_parse_partition_table(struct synaptics_rmi4_data *rmi4_data,
		const unsigned char *partition_table,
		struct block_count *blkcount, struct physical_address *phyaddr)
{
	unsigned char ii;
	unsigned char index;
	unsigned char offset;
	unsigned short partition_length;
	unsigned short physical_address;
	struct partition_table *ptable;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	for (ii = 0; ii < fwu->partitions; ii++) {
		index = ii * 8 + 2;
		ptable = (struct partition_table *)&partition_table[index];
		partition_length = ptable->partition_length_15_8 << 8 |
				ptable->partition_length_7_0;
		physical_address = ptable->start_physical_address_15_8 << 8 |
				ptable->start_physical_address_7_0;
		tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
				"%s: Partition entry %d:\n",
				__func__, ii);
		for (offset = 0; offset < 8; offset++) {
			tsp_debug_dbg(true, &rmi4_data->i2c_client->dev,
					"%s: 0x%02x\n",
					__func__,
					partition_table[index + offset]);
		}
		switch (ptable->partition_id) {
		case CORE_CODE_PARTITION:
			blkcount->ui_firmware = partition_length;
			phyaddr->ui_firmware = physical_address;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Core code block count: %d\n",
					__func__, blkcount->ui_firmware);
			break;
		case CORE_CONFIG_PARTITION:
			blkcount->ui_config = partition_length;
			phyaddr->ui_config = physical_address;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Core config block count: %d\n",
					__func__, blkcount->ui_config);
			break;
		case DISPLAY_CONFIG_PARTITION:
			blkcount->dp_config = partition_length;
			phyaddr->dp_config = physical_address;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Display config block count: %d\n",
					__func__, blkcount->dp_config);
			break;
		case FLASH_CONFIG_PARTITION:
			blkcount->fl_config = partition_length;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Flash config block count: %d\n",
					__func__, blkcount->fl_config);
			break;
		case GUEST_CODE_PARTITION:
			blkcount->guest_code = partition_length;
			phyaddr->guest_code = physical_address;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Guest code block count: %d\n",
					__func__, blkcount->guest_code);
			break;
		case GUEST_SERIALIZATION_PARTITION:
			blkcount->pm_config = partition_length;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Guest serialization block count: %d\n",
					__func__, blkcount->pm_config);
			break;
		case GLOBAL_PARAMETERS_PARTITION:
			blkcount->bl_config = partition_length;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Global parameters block count: %d\n",
					__func__, blkcount->bl_config);
			break;
		case DEVICE_CONFIG_PARTITION:
			blkcount->lockdown = partition_length;
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Device config block count: %d\n",
					__func__, blkcount->lockdown);
			break;
		};
	}

	return;
}

static int fwu_read_f34_queries_bl_version(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	int retval;
	unsigned char base;
	unsigned char offset;
	struct f34_v7_query_0 query_0;
	struct f34_v7_query_1_7 query_1_7;

	base = rmi4_data->fwu->f34_fd.query_base_addr;

	tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: base[%x]\n",
					__func__, base);

	retval = rmi4_data->i2c_read(rmi4_data,
			base,
			query_0.data,
			sizeof(query_0.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read query 0\n",
				__func__);
		return retval;
	}

	offset = query_0.subpacket_1_size + 1;
	tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: offset[%x]\n",
					__func__, offset);

	retval = rmi4_data->i2c_read(rmi4_data,
			base + offset,
			query_1_7.data,
			sizeof(query_1_7.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read queries 1 to 7\n",
				__func__);
		return retval;
	}

	fwu->bootloader_id[0] = query_1_7.bl_minor_revision;
	fwu->bootloader_id[1] = query_1_7.bl_major_revision;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: Bootloader ver : [%d,%d]\n",
					__func__, fwu->bootloader_id[0],fwu->bootloader_id[1]);
	return 0;
}

static int fwu_read_f34_queries_v7(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	int retval;
	unsigned char ii;
	unsigned char base;
	unsigned char index;
	unsigned char offset;
	unsigned char *ptable;
	struct f34_v7_query_0 query_0;
	struct f34_v7_query_1_7 query_1_7;

	base = rmi4_data->fwu->f34_fd.query_base_addr;

	retval = rmi4_data->i2c_read(rmi4_data,
			base,
			query_0.data,
			sizeof(query_0.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read query 0\n",
				__func__);
		return retval;
	}

	offset = query_0.subpacket_1_size + 1;

	retval = rmi4_data->i2c_read(rmi4_data,
			base + offset,
			query_1_7.data,
			sizeof(query_1_7.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read queries 1 to 7\n",
				__func__);
		return retval;
	}

	fwu->bootloader_id_ic[0] = fwu->bootloader_id[0] = query_1_7.bl_minor_revision;
	fwu->bootloader_id_ic[1] = fwu->bootloader_id[1] = query_1_7.bl_major_revision;

	fwu->block_size = query_1_7.block_size_15_8 << 8 |
			query_1_7.block_size_7_0;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: fwu->block_size = %d, [%d,%d]\n",
					__func__, fwu->block_size, fwu->bootloader_id[0],fwu->bootloader_id[1]);

	fwu->flash_config_length = query_1_7.flash_config_length_15_8 << 8 |
			query_1_7.flash_config_length_7_0;

	fwu->payload_length = query_1_7.payload_length_15_8 << 8 |
			query_1_7.payload_length_7_0;

	fwu->off.flash_status = V7_FLASH_STATUS_OFFSET;
	fwu->off.partition_id = V7_PARTITION_ID_OFFSET;
	fwu->off.block_number = V7_BLOCK_NUMBER_OFFSET;
	fwu->off.transfer_length = V7_TRANSFER_LENGTH_OFFSET;
	fwu->off.flash_cmd = V7_COMMAND_OFFSET;
	fwu->off.payload = V7_PAYLOAD_OFFSET;

	fwu->flash_properties.has_disp_config = query_1_7.has_display_config;
	fwu->flash_properties.has_perm_config = query_1_7.has_guest_serialization;
	fwu->flash_properties.has_bl_config = query_1_7.has_global_parameters;

	fwu->has_guest_code = query_1_7.has_guest_code;

	index = sizeof(query_1_7.data) - V7_PARTITION_SUPPORT_BYTES;

	fwu->partitions = 0;
	for (offset = 0; offset < V7_PARTITION_SUPPORT_BYTES; offset++) {
		for (ii = 0; ii < 8; ii++) {
			if (query_1_7.data[index + offset] & (1 << ii))
				fwu->partitions++;
		}

		tsp_debug_dbg(true, &rmi4_data->i2c_client->dev,
				"%s: Supported partitions: 0x%02x\n",
				__func__, query_1_7.data[index + offset]);
	}

	fwu->partition_table_bytes = fwu->partitions * 8 + 2;

	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->partition_table_bytes, GFP_KERNEL);
	if (!fwu->read_config_buf) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu->read_config_buf\n",
				__func__);
		fwu->read_config_buf_size = 0;
		return -ENOMEM;
	}
	fwu->read_config_buf_size = fwu->partition_table_bytes;
	ptable = fwu->read_config_buf;

	retval = fwu_read_f34_v7_partition_table(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read partition table\n",
				__func__);
		return retval;
	}

	fwu_parse_partition_table(rmi4_data, ptable, &fwu->blkcount, &fwu->phyaddr);

	return 0;
}


static int fwu_read_f34_queries(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char count;
	unsigned char buf[10];
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if ( fwu->bl_version == BL_V7) {
		dev_info(&rmi4_data->i2c_client->dev,
				"%s: bl_version == BL_V7\n",__func__);
		retval = fwu_read_f34_queries_v7(rmi4_data);
	} else {

		retval = rmi4_data->i2c_read(rmi4_data,
				fwu->f34_fd.query_base_addr + BOOTLOADER_ID_OFFSET,
				fwu->bootloader_id,
				sizeof(fwu->bootloader_id));
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to read bootloader ID\n",
					__func__);
			return retval;
		}

		if (fwu->bootloader_id[1] == '5') {
			fwu->bl_version = V5;
		} else if (fwu->bootloader_id[1] == '6') {
			fwu->bl_version = V6;
		} else if (fwu->bootloader_id[1] == 7) {
			fwu->bl_version = BL_V7;
		} else {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Unrecognized bootloader version\n",
					__func__);
			return -EINVAL;
		}
	}

	tsp_debug_info(false, &rmi4_data->i2c_client->dev,
			"%s: F34 Query : ID: %s\n", __func__, fwu->bootloader_id);

	if (fwu->bl_version == V5) {
		fwu->properties_off = V5_PROPERTIES_OFFSET;
		fwu->blk_size_off = V5_BLOCK_SIZE_OFFSET;
		fwu->blk_count_off = V5_BLOCK_COUNT_OFFSET;
		fwu->blk_data_off = V5_BLOCK_DATA_OFFSET;
	} else if (fwu->bl_version == V6) {
		fwu->properties_off = V6_PROPERTIES_OFFSET;
		fwu->blk_size_off = V6_BLOCK_SIZE_OFFSET;
		fwu->blk_count_off = V6_BLOCK_COUNT_OFFSET;
		fwu->blk_data_off = V6_BLOCK_DATA_OFFSET;
		fwu->properties2_off = fwu->blk_count_off + 1;
		fwu->guest_blk_count_off = fwu->properties2_off + 1;
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->properties_off,
			fwu->flash_properties.data,
			sizeof(fwu->flash_properties.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash properties\n",
				__func__);
		return retval;
	}

	count = 4;

	if (fwu->flash_properties.has_perm_config) {
		fwu->has_perm_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_bl_config) {
		fwu->has_bl_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_disp_config) {
		fwu->has_disp_config = 1;
		count += 2;
	}

	if (fwu->flash_properties.has_flash_query4) {
		struct synaptics_rmi4_f34_query_04 query4;

		retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->properties2_off,
			query4.data,
			sizeof(query4.data));
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to read block size info\n",
					__func__);
			return retval;
		}

		if (query4.has_guest_code) {
			retval = rmi4_data->i2c_read(rmi4_data,
				fwu->f34_fd.query_base_addr + fwu->guest_blk_count_off,
				buf,
				2);
			if (retval < 0) {
				tsp_debug_err(true, &rmi4_data->i2c_client->dev,
						"%s: Failed to read block size info\n",
						__func__);
				return retval;
			}
			batohs(&fwu->guest_code_block_count, buf);
			fwu->has_guest_code = 1;
		} else {
			tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: query data do not supply quest image.\n",
					__func__);

		}
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->blk_size_off,
			buf,
			2);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read block size info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->block_size, &(buf[0]));

	if (fwu->bl_version == V5) {
		fwu->flash_cmd_off = fwu->blk_data_off + fwu->block_size;
		fwu->flash_status_off = fwu->flash_cmd_off;
	} else if (fwu->bl_version == V6) {
		fwu->flash_cmd_off = V6_FLASH_COMMAND_OFFSET;
		fwu->flash_status_off = V6_FLASH_STATUS_OFFSET;
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.query_base_addr + fwu->blk_count_off,
			buf,
			count);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read block count info\n",
				__func__);
		return retval;
	}

	batohs(&fwu->fw_block_count, &(buf[0]));
	batohs(&fwu->config_block_count, &(buf[2]));

	count = 4;

	if (fwu->has_perm_config) {
		batohs(&fwu->perm_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->has_bl_config) {
		batohs(&fwu->bl_config_block_count, &(buf[count]));
		count += 2;
	}

	if (fwu->has_disp_config)
		batohs(&fwu->disp_config_block_count, &(buf[count]));

	return 0;
}

static int fwu_read_flash_status_v7(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char status;
	unsigned char command;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.flash_status,
			&status,
			sizeof(status));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}

	fwu->in_bl_mode = status >> 7;

	fwu->flash_status = status & MASK_5BIT;

	if (fwu->flash_status != 0x00) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Flash status = %d, command = 0x%02x\n",
				__func__, fwu->flash_status, fwu->command);
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->off.flash_cmd,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash command\n",
				__func__);
		return retval;
	}

	fwu->command = command;

	return 0;
}


static int fwu_read_f34_flash_status(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char status;
	unsigned char command;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_status_off,
			&status,
			sizeof(status));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}

	fwu->program_enabled = status >> 7;

	if (fwu->bl_version == V5)
		fwu->flash_status = (status >> 4) & MASK_3BIT;
	else if (fwu->bl_version == V6)
		fwu->flash_status = status & MASK_3BIT;
	else if (fwu->bl_version == BL_V7)
		fwu->flash_status = status & MASK_5BIT;

	if (fwu->flash_status != 0x00) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Flash status = %d, command = 0x%02x\n",
				__func__, fwu->flash_status, fwu->command);
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_cmd_off,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read flash command\n",
				__func__);
		return retval;
	}

//	fwu->command = command & MASK_4BIT;
//	if (fwu->bl_version == BL_V7)
		fwu->command = command;

	return 0;
}

static int fwu_write_f34_command(struct synaptics_rmi4_data *rmi4_data, unsigned char cmd)
{
	int retval;
	unsigned char command = cmd & MASK_4BIT;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	fwu->command = cmd;

	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->flash_cmd_off,
			&command,
			sizeof(command));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write command 0x%02x\n",
				__func__, command);
		return retval;
	}

	return 0;
}

static int fwu_wait_for_idle(struct synaptics_rmi4_data *rmi4_data, int timeout_ms)
{
	int count = 0;
	int timeout_count = ((timeout_ms * 1000) / MAX_SLEEP_TIME_US) + 1;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	tsp_debug_dbg(true, &rmi4_data->i2c_client->dev,
			"%s: fwu->polling_mode = %d, timeout_count = %d\n",
			__func__, fwu->polling_mode, timeout_count );
#if 1
	do {
		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);

		count++;
		if (fwu->polling_mode || (count == timeout_count))
			fwu_read_flash_status_v7(rmi4_data);

		if ((fwu->command == CMD_IDLE) && (fwu->flash_status == 0x00))
			return 0;
	} while (count < timeout_count);

#else
	do {
		if (fwu->polling_mode || count == timeout_count)
//			fwu_read_f34_flash_status(rmi4_data);
			fwu_read_flash_status_v7(rmi4_data);
		if ((fwu->command == 0x00) && (fwu->flash_status == 0x00)) {
			if (count == timeout_count)
				fwu->polling_mode = true;
			return 0;
		}
		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);
		count++;

	} while (count <= timeout_count);
#endif
	tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Timed out waiting for idle status\n",
			__func__);

	return -ETIMEDOUT;
}

#ifdef FW_UPDATE_GO_NOGO
static enum flash_area fwu_go_nogo(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	enum flash_area flash_area = NONE;
	unsigned char index = 0;
	unsigned char config_id[4];
	unsigned int device_config_id;
	unsigned int image_config_id;
	unsigned int device_fw_id;
	unsigned long image_fw_id;
	char *strptr;
	char *firmware_id;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (fwu->force_update) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Update both UI and config if device is in bootloader mode */
	if (fwu->in_flash_prog_mode) {
		flash_area = UI_FIRMWARE;
		goto exit;
	}

	/* Get device firmware ID */
	device_fw_id = rmi4_data->rmi4_mod_info.build_id[0] +
			rmi4_data->rmi4_mod_info.build_id[1] * 0x100 +
			rmi4_data->rmi4_mod_info.build_id[2] * 0x10000;
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Device firmware ID = %d\n",
			__func__, device_fw_id);

	/* Get image firmware ID */
	if (fwu->img.firmwareId != NULL) {
		image_fw_id = extract_uint_le(fwu->img.firmwareId);
	} else {
		strptr = strstr(fwu->img.image_name, "PR");
		if (!strptr) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: No valid PR number (PRxxxxxxx) "
					"found in image file name (%s)\n",
					__func__, fwu->img.image_name);
			flash_area = NONE;
			goto exit;
		}

		strptr += 2;
		firmware_id = kzalloc(MAX_FIRMWARE_ID_LEN, GFP_KERNEL);
		while (strptr[index] >= '0' && strptr[index] <= '9') {
			firmware_id[index] = strptr[index];
			index++;
		}

		retval = kstrtoul(firmware_id, 10, &image_fw_id);
		kfree(firmware_id);
		if (retval) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to obtain image firmware ID\n",
					__func__);
			flash_area = NONE;
			goto exit;
		}
	}
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Image firmware ID = %d\n",
			__func__, (unsigned int)image_fw_id);

	if (image_fw_id > device_fw_id) {
		flash_area = UI_FIRMWARE;
		goto exit;
	} else if (image_fw_id < device_fw_id) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Image firmware ID older than device firmware ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}

	/* Get device config ID */
	retval = rmi4_data->i2c_read(rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				config_id,
				sizeof(config_id));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read device config ID\n",
				__func__);
		flash_area = NONE;
		goto exit;
	}
	device_config_id = extract_uint_be(config_id);
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Device config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			config_id[0],
			config_id[1],
			config_id[2],
			config_id[3]);

	/* Get image config ID */
	image_config_id = extract_uint_be(fwu->img.configId);
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Image config ID = 0x%02x 0x%02x 0x%02x 0x%02x\n",
			__func__,
			fwu->img.configId[0],
			fwu->img.configId[1],
			fwu->img.configId[2],
			fwu->img.configId[3]);

	if (image_config_id > device_config_id) {
		flash_area = CONFIG_AREA;
		goto exit;
	}

exit:
	if (flash_area == NONE) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: No need to do reflash\n",
				__func__);
	} else {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Updating %s\n",
				__func__,
				flash_area == UI_FIRMWARE ?
				"UI firmware" :
				"config only");
	}
	return flash_area;
}
#endif

static int fwu_scan_pdt(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	bool f01found = false;
	bool f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = rmi4_data->i2c_read(rmi4_data,
				addr,
				(unsigned char *)&rmi_fd,
				sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
					"%s: Found F%02x\n",
					__func__, rmi_fd.fn_number);
			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				f01found = true;
				fwu->f01_fd.query_base_addr =
					rmi_fd.query_base_addr;
				fwu->f01_fd.ctrl_base_addr =
					rmi_fd.ctrl_base_addr;
				fwu->f01_fd.data_base_addr =
					rmi_fd.data_base_addr;
				fwu->f01_fd.cmd_base_addr =
					rmi_fd.cmd_base_addr;
				break;
			case SYNAPTICS_RMI4_F34:
				f34found = true;
				fwu->f34_fd.query_base_addr =
					rmi_fd.query_base_addr;
				fwu->f34_fd.ctrl_base_addr =
					rmi_fd.ctrl_base_addr;
				fwu->f34_fd.data_base_addr =
					rmi_fd.data_base_addr;

				switch (rmi_fd.fn_version) {
				case F34_V0:
					fwu->bl_version = BL_V5;
					break;
				case F34_V1:
					fwu->bl_version = BL_V6;
					break;
				case F34_V2:
					fwu->bl_version = BL_V7;
					break;
				default:
					tsp_debug_err(true, &rmi4_data->i2c_client->dev,
							"%s: Unrecognized F34 version\n",
							__func__);
					return -EINVAL;
				}

				fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off;	ii < ((intr_src & MASK_3BIT) + intr_off); ii++)
					fwu->intr_mask |= 1 << ii;
				break;
			}
		} else {
			break;
		}

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to find both F01 and F34\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}

static int fwu_write_blocks(struct synaptics_rmi4_data *rmi4_data, unsigned char *block_ptr, unsigned int block_size,
		enum flash_command command)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	unsigned short block_cnt = block_size / fwu->block_size;

	if (block_ptr == NULL) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Cannot find block data (%x)\n",
				__func__, command);
		return -EINVAL;
	}

	if (command == CMD_WRITE_CONFIG_BLOCK)
		block_offset[1] |= (fwu->config_area << 5);

	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f34_fd.data_base_addr + BLOCK_NUMBER_OFFSET,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		return retval;
	}

	for (block_num = 0; block_num < block_cnt; block_num++) {
		retval = rmi4_data->i2c_write(rmi4_data,
				fwu->f34_fd.data_base_addr + fwu->blk_data_off,
				block_ptr,
				fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write block data (block %d)\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_write_f34_command(rmi4_data, command);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write command for block %d\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_wait_for_idle(rmi4_data, WRITE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status (block %d)\n",
					__func__, block_num);
			return retval;
		}

		block_ptr += fwu->block_size;
	}

	return 0;
}

static int fwu_write_config_block(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: size : %d, cmd : %d\n", __func__, fwu->img.uiConfig.size, CMD_WRITE_CONFIG_BLOCK);

	return fwu_write_blocks(rmi4_data, fwu->img.uiConfig.data,
		fwu->img.uiConfig.size, CMD_WRITE_CONFIG_BLOCK);
}

static int fwu_write_guest_code_block(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return fwu_write_blocks(rmi4_data, fwu->img.guestCode.data,
		fwu->img.guestCode.size, CMD_WRITE_GUEST_CODE);
}

static int fwu_write_bootloader_id(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f34_fd.data_base_addr + fwu->blk_data_off,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write bootloader ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static int fwu_enter_flash_prog(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_f01_device_status f01_device_status;
	struct synaptics_rmi4_f01_device_control f01_device_control;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	unsigned char int_enable = 0x00;

	retval = fwu_read_f34_flash_status(rmi4_data);
	if (retval < 0)
		return retval;

	if (fwu->program_enabled)
		return 0;

	/* To block interrupt from finger or hovering during enter flash program mode
	 * 1. F01_RMI_CTRL1(Interrupt Enable 0) register is cleared on this position.
	 * 2. After enter flash program mode, interrupt occured only by Flash bit(0x01)
	 * 3. After finish all flashing and reset, Interrupt enable register will be set as default value
	 */
	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f01_fd.ctrl_base_addr + 1,
			&int_enable, sizeof(int_enable));

	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write interrupt enable register\n", __func__);
		return retval;
	}
	msleep(20);

	retval = fwu_write_bootloader_id(rmi4_data);
	if (retval < 0)
		return retval;

	retval = fwu_write_f34_command(rmi4_data, CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	if (!fwu->program_enabled) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Program enabled bit not set\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_scan_pdt(rmi4_data);
	if (retval < 0)
		return retval;

	retval = fwu_read_f01_device_status(rmi4_data, &f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Not in flash prog mode\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries(rmi4_data);
	if (retval < 0)
		return retval;

	retval = rmi4_data->i2c_read(rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f01_fd.ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}

	return retval;
}

static int fwu_check_ui_firmware_size(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	block_count = fwu->v7_img.ui_firmware.size / fwu->block_size;

	if (block_count != fwu->blkcount.ui_firmware) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: UI firmware size mismatch, block_count=%d,fwu->blkcount.ui_firmware=%d\n",
				__func__, block_count, fwu->blkcount.ui_firmware);
		return -EINVAL;
	}

	return 0;
}

static int fwu_check_ui_configuration_size(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	block_count = fwu->v7_img.ui_config.size / fwu->block_size;

	if (block_count != fwu->blkcount.ui_config) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: UI configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}
	return 0;
}
static int fwu_check_dp_configuration_size(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;


	block_count = fwu->v7_img.dp_config.size / fwu->block_size;

	if (block_count != fwu->blkcount.dp_config) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Display configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}
static int fwu_check_guest_code_size(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;


	block_count = fwu->v7_img.guest_code.size / fwu->block_size;
	if (block_count != fwu->blkcount.guest_code) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Guest code size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}
static int fwu_check_bl_configuration_size(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;


	block_count = fwu->v7_img.bl_config.size / fwu->block_size;

	if (block_count != fwu->blkcount.bl_config) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Bootloader configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	return 0;
}
static int fwu_erase_configuration(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;


	switch (fwu->config_area) {
	case v7_UI_CONFIG_AREA:
		retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ERASE_UI_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case v7_DP_CONFIG_AREA:
		retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ERASE_DISP_CONFIG);
		if (retval < 0)
			return retval;
		break;
	case v7_BL_CONFIG_AREA:
		retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ERASE_BL_CONFIG);
		if (retval < 0)
			return retval;
		break;
	}

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	return retval;
}
static int fwu_erase_guest_code(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = fwu_write_f34_command_v7(rmi4_data, CMD_ERASE_GUEST_CODE);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	return 0;
}

static int fwu_erase_all(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ERASE_UI_FIRMWARE);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	fwu->config_area = v7_UI_CONFIG_AREA;
	retval = fwu_erase_configuration(rmi4_data);
	if (retval < 0)
		return retval;

	if (fwu->flash_properties.has_disp_config) {
		fwu->config_area = v7_DP_CONFIG_AREA;
		retval = fwu_erase_configuration(rmi4_data);
		if (retval < 0)
			return retval;
	}

	if (fwu->new_partition_table && fwu->has_guest_code) {
		retval = fwu_erase_guest_code(rmi4_data);
		if (retval < 0)
			return retval;
	}

	return 0;
}

static int fwu_read_f34_v7_blocks(struct synaptics_rmi4_data *rmi4_data,
		unsigned short block_cnt,
		unsigned char command)
{
	int retval;
	unsigned char base;
	unsigned char length[2];
	unsigned short transfer;
	unsigned short max_transfer;
	unsigned short remaining = block_cnt;
	unsigned short block_number = 0;
	unsigned short index = 0;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;


	base = fwu->f34_fd.data_base_addr;

	retval = fwu_write_f34_partition_id(rmi4_data, command);
	if (retval < 0)
		return retval;

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.block_number,
			(unsigned char *)&block_number,
			sizeof(block_number));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write block number\n",
				__func__);
		return retval;
	}

	if (fwu->payload_length > (PAGE_SIZE / fwu->block_size))
		max_transfer = PAGE_SIZE / fwu->block_size;
	else
		max_transfer = fwu->payload_length;

	do {
		if (remaining / max_transfer)
			transfer = max_transfer;
		else
			transfer = remaining;

		length[0] = (unsigned char)(transfer & MASK_8BIT);
		length[1] = (unsigned char)(transfer >> 8);

		retval = rmi4_data->i2c_write(rmi4_data,
				base + fwu->off.transfer_length,
				length,
				sizeof(length));
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write transfer length (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = fwu_write_f34_command_v7(rmi4_data, command);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write command (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = rmi4_data->i2c_read(rmi4_data,
				base + fwu->off.payload,
				&fwu->read_config_buf[index],
				transfer * fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to read block data (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		index += (transfer * fwu->block_size);
		remaining -= transfer;
	} while (remaining);

	return 0;
}

static int fwu_write_f34_v7_blocks(struct synaptics_rmi4_data *rmi4_data,
		unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char command)
{
	int retval;
	unsigned char base;
	unsigned char length[2];
	unsigned short transfer;
	unsigned short max_transfer;
	unsigned short remaining = block_cnt;
	unsigned short block_number = 0;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	base = fwu->f34_fd.data_base_addr;

	retval = fwu_write_f34_partition_id(rmi4_data, command);
	if (retval < 0)
		return retval;

	retval = rmi4_data->i2c_write(rmi4_data,
			base + fwu->off.block_number,
			(unsigned char *)&block_number,
			sizeof(block_number));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write block number\n",
				__func__);
		return retval;
	}

	if (fwu->payload_length > (PAGE_SIZE / fwu->block_size))
		max_transfer = PAGE_SIZE / fwu->block_size;
	else
		max_transfer = fwu->payload_length;

	do {
		if (remaining / max_transfer)
			transfer = max_transfer;
		else
			transfer = remaining;

		length[0] = (unsigned char)(transfer & MASK_8BIT);
		length[1] = (unsigned char)(transfer >> 8);

		retval = rmi4_data->i2c_write(rmi4_data,
				base + fwu->off.transfer_length,
				length,
				sizeof(length));
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write transfer length (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = fwu_write_f34_command_v7(rmi4_data, command);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write command (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = rmi4_data->i2c_write(rmi4_data,
				base + fwu->off.payload,
				block_ptr,
				transfer * fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write block data (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status (%d blocks remaining)\n",
					__func__, remaining);
			return retval;
		}

		block_ptr += (transfer * fwu->block_size);
		remaining -= transfer;
	} while (remaining);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
						"%s: Write END !!!!\n", __func__);

	return 0;
}

static int fwu_write_f34_blocks(struct synaptics_rmi4_data *rmi4_data,
		unsigned char *block_ptr,
		unsigned short block_cnt, unsigned char cmd)
{
	int retval;

	retval = fwu_write_f34_v7_blocks(rmi4_data, block_ptr, block_cnt, cmd);

	return retval;
}

static int fwu_write_configuration(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return fwu_write_f34_blocks(rmi4_data, (unsigned char *)fwu->config_data,
			fwu->config_block_count, v7_CMD_WRITE_CONFIG);
}

static int fwu_write_ui_configuration(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	fwu->config_area = v7_UI_CONFIG_AREA;
	fwu->config_data = fwu->v7_img.ui_config.data;
	fwu->config_size = fwu->v7_img.ui_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	return fwu_write_configuration(rmi4_data);
}

static int fwu_write_dp_configuration(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	fwu->config_area = v7_DP_CONFIG_AREA;
	fwu->config_data = fwu->v7_img.dp_config.data;
	fwu->config_size = fwu->v7_img.dp_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	return fwu_write_configuration(rmi4_data);
}
static int fwu_write_guest_code(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	int retval;
	unsigned short guest_code_block_count;

	guest_code_block_count = fwu->v7_img.guest_code.size / fwu->block_size;

	retval = fwu_write_f34_blocks(rmi4_data, (unsigned char *)fwu->v7_img.guest_code.data,
			guest_code_block_count, CMD_WRITE_GUEST_CODE);
	if (retval < 0)
		return retval;

	return 0;
}
static int fwu_write_flash_configuration(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	fwu->config_area = v7_FLASH_CONFIG_AREA;
	fwu->config_data = fwu->v7_img.fl_config.data;
	fwu->config_size = fwu->v7_img.fl_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	if (fwu->config_block_count != fwu->blkcount.fl_config) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Flash configuration size mismatch\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ERASE_FLASH_CONFIG);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase flash configuration command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);//fwu_wait_for_idle(ERASE_WAIT_MS, false);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	retval = fwu_write_configuration(rmi4_data);
	if (retval < 0)
		return retval;

	rmi4_data->reset_device(rmi4_data);

	return 0;
}

static int fwu_write_partition_table(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	block_count = fwu->blkcount.bl_config;
	fwu->config_area = v7_BL_CONFIG_AREA;
	fwu->config_size = fwu->block_size * block_count;
	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->config_size, GFP_KERNEL);
	if (!fwu->read_config_buf) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu->read_config_buf\n",
				__func__);
		fwu->read_config_buf_size = 0;
		return -ENOMEM;
	}
	fwu->read_config_buf_size = fwu->config_size;

	retval = fwu_read_f34_v7_blocks(rmi4_data, block_count, v7_CMD_READ_CONFIG);
	if (retval < 0)
		return retval;

	retval = fwu_erase_configuration(rmi4_data);
	if (retval < 0)
		return retval;

	retval = fwu_write_flash_configuration(rmi4_data);
	if (retval < 0)
		return retval;

	fwu->config_area = v7_BL_CONFIG_AREA;
	fwu->config_data = fwu->read_config_buf;
	fwu->config_size = fwu->v7_img.bl_config.size;
	fwu->config_block_count = fwu->config_size / fwu->block_size;

	retval = fwu_write_configuration(rmi4_data);
	if (retval < 0)
		return retval;

	return 0;
}
static int fwu_write_firmware(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	unsigned short firmware_block_count;

	firmware_block_count = fwu->v7_img.ui_firmware.size / fwu->block_size;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: Write Run!!!\n",
					__func__);

	return fwu_write_f34_blocks(rmi4_data, (unsigned char *)fwu->v7_img.ui_firmware.data,
			firmware_block_count, v7_CMD_WRITE_FW);
}

static int fwu_do_reflash_v7(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu->new_partition_table) {
		retval = fwu_check_ui_firmware_size(rmi4_data);
		if (retval < 0)
			return retval;

		retval = fwu_check_ui_configuration_size(rmi4_data);
		if (retval < 0)
			return retval;

		if (fwu->flash_properties.has_disp_config &&
				fwu->v7_img.contains_disp_config) {
			retval = fwu_check_dp_configuration_size(rmi4_data);
			if (retval < 0)
				return retval;
		}

		if (fwu->has_guest_code && fwu->v7_img.contains_guest_code) {
			retval = fwu_check_guest_code_size(rmi4_data);
			if (retval < 0)
				return retval;
		}
	} else {
		retval = fwu_check_bl_configuration_size(rmi4_data);
		if (retval < 0)
			return retval;
	}

	retval = fwu_erase_all(rmi4_data);
	if (retval < 0)
		return retval;

	if (fwu->new_partition_table) {
		retval = fwu_write_partition_table(rmi4_data);
		if (retval < 0)
			return retval;
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Partition table programmed\n", __func__);
	}

	retval = fwu_write_firmware(rmi4_data);
	if (retval < 0)
		return retval;
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Firmware programmed\n", __func__);

	fwu->config_area = v7_UI_CONFIG_AREA;
	retval = fwu_write_ui_configuration(rmi4_data);
	if (retval < 0)
		return retval;
	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Configuration programmed\n", __func__);

	if (fwu->flash_properties.has_disp_config &&
			fwu->v7_img.contains_disp_config) {
		retval = fwu_write_dp_configuration(rmi4_data);
		if (retval < 0)
			return retval;
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Display configuration programmed\n", __func__);
	}

	if (fwu->new_partition_table) {
		if (fwu->has_guest_code && fwu->v7_img.contains_guest_code) {
			retval = fwu_write_guest_code(rmi4_data);
			if (retval < 0)
				return retval;
			tsp_debug_info(true, &rmi4_data->i2c_client->dev,
					"%s: Guest code programmed\n", __func__);
		}
	}

	return retval;
}

static int fwu_do_write_config(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = fwu_enter_flash_prog(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	if (fwu->config_area == PERM_CONFIG_AREA) {
		fwu->config_block_count = fwu->perm_config_block_count;
		goto write_config;
	}

	retval = fwu_write_bootloader_id(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written, AREA : %d\n",
			__func__, fwu->config_area);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(rmi4_data, CMD_ERASE_CONFIG);
		break;
	case BL_CONFIG_AREA:
		retval = fwu_write_f34_command(rmi4_data, CMD_ERASE_BL_CONFIG);
		fwu->config_block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		retval = fwu_write_f34_command(rmi4_data, CMD_ERASE_DISP_CONFIG);
		fwu->config_block_count = fwu->disp_config_block_count;
		break;
	}
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

write_config:
	retval = fwu_write_config_block(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Config written\n",
		__func__);

	return retval;
}

static void fwu_parse_image_header_10_bl_container(struct synaptics_rmi4_data *rmi4_data, unsigned char *image)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int container_id;
	unsigned int length;
	unsigned char *content;
	struct container_descriptor *descriptor;

	num_of_containers = (fwu->v7_img.bootloader.size - 4) / 4;

	for (ii = 1; ii <= num_of_containers; ii++) {
		addr = le_to_uint(fwu->v7_img.bootloader.data + (ii * 4));
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);
		switch (container_id) {
		case BL_CONFIG_CONTAINER:
		case GLOBAL_PARAMETERS_CONTAINER:
			fwu->v7_img.bl_config.data = content;
			fwu->v7_img.bl_config.size = length;
			break;
		case BL_LOCKDOWN_INFO_CONTAINER:
		case DEVICE_CONFIG_CONTAINER:
			fwu->v7_img.lockdown.data = content;
			fwu->v7_img.lockdown.size = length;
			break;
		default:
			break;
		};
	}

	return;
}

void fwu_parse_image_header_10_simple(struct synaptics_rmi4_data *rmi4_data, unsigned char *image)
{
	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int offset;
	unsigned int container_id;
	unsigned int length;
	unsigned char *content;
	struct container_descriptor *descriptor;
	struct image_header_10 *header;

	header = (struct image_header_10 *)image;

	/* address of top level container */
	offset = le_to_uint(header->top_level_container_start_addr);
	descriptor = (struct container_descriptor *)(image + offset);

	/* address of top level container content */
	offset = le_to_uint(descriptor->content_address);
	num_of_containers = le_to_uint(descriptor->content_length) / 4;

	for (ii = 0; ii < num_of_containers; ii++) {
		addr = le_to_uint(image + offset);
		offset += 4;
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);

		if(container_id == UI_CONFIG_CONTAINER || container_id == CORE_CONFIG_CONTAINER){
			tsp_debug_info(true, &rmi4_data->i2c_client->dev,
								"%s: container_id=%d, length=%d data=[0x%02x/0x%02x/0x%02x/0x%02x]\n",
								__func__, container_id, length,
								content[0], content[1], content[2], content[3]);

			rmi4_data->ic_revision_of_bin = (int)content[2];
			rmi4_data->fw_version_of_bin = (int)content[3];
		}
		if(container_id == GENERAL_INFORMATION_CONTAINER){
			snprintf(rmi4_data->product_id_string_of_bin, SYNAPTICS_RMI4_PRODUCT_ID_SIZE, "%c%c%c%c%c",
				content[24], content[25], content[26], content[27], content[28]);

			tsp_debug_info(true, &rmi4_data->i2c_client->dev,
							"%s: container_id=%d, length=%d data=[%s]\n",
							__func__, container_id, length,
							rmi4_data->product_id_string_of_bin);
		}
	}

	return;
}
EXPORT_SYMBOL(fwu_parse_image_header_10_simple);


static void fwu_parse_image_header_10(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	unsigned char ii;
	unsigned char num_of_containers;
	unsigned int addr;
	unsigned int offset;
	unsigned int container_id;
	unsigned int length;
	unsigned char *image;
	unsigned char *content;
	struct container_descriptor *descriptor;
	struct image_header_10 *header;

	image = fwu->image;
	header = (struct image_header_10 *)image;

	fwu->v7_img.checksum = le_to_uint(header->checksum);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
						"%s: fwu->v7_img.checksum=%d\n",
						__func__, fwu->v7_img.checksum);

	/* address of top level container */
	offset = le_to_uint(header->top_level_container_start_addr);
	descriptor = (struct container_descriptor *)(image + offset);

	/* address of top level container content */
	offset = le_to_uint(descriptor->content_address);
	num_of_containers = le_to_uint(descriptor->content_length) / 4;

	for (ii = 0; ii < num_of_containers; ii++) {
		addr = le_to_uint(image + offset);
		offset += 4;
		descriptor = (struct container_descriptor *)(image + addr);
		container_id = descriptor->container_id[0] |
				descriptor->container_id[1] << 8;
		content = image + le_to_uint(descriptor->content_address);
		length = le_to_uint(descriptor->content_length);

		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
							"%s: container_id=%d, length=%d\n",
							__func__, container_id, length);

		
		switch (container_id) {
		case UI_CONTAINER:
		case CORE_CODE_CONTAINER:
			fwu->v7_img.ui_firmware.data = content;
			fwu->v7_img.ui_firmware.size = length;
			break;
		case UI_CONFIG_CONTAINER:
		case CORE_CONFIG_CONTAINER:
			fwu->v7_img.ui_config.data = content;
			fwu->v7_img.ui_config.size = length;
			break;
		case BL_CONTAINER:
			fwu->v7_img.bl_version = *content;
			fwu->v7_img.bootloader.data = content;
			fwu->v7_img.bootloader.size = length;
			fwu_parse_image_header_10_bl_container(rmi4_data, image);
			break;
		case GUEST_CODE_CONTAINER:
			fwu->v7_img.contains_guest_code = true;
			fwu->v7_img.guest_code.data = content;
			fwu->v7_img.guest_code.size = length;
			break;
		case DISPLAY_CONFIG_CONTAINER:
			fwu->v7_img.contains_disp_config = true;
			fwu->v7_img.dp_config.data = content;
			fwu->v7_img.dp_config.size = length;
			break;
		case FLASH_CONFIG_CONTAINER:
			fwu->v7_img.contains_flash_config = true;
			fwu->v7_img.fl_config.data = content;
			fwu->v7_img.fl_config.size = length;
			break;
		case GENERAL_INFORMATION_CONTAINER:
			fwu->v7_img.contains_firmware_id = true;
			fwu->v7_img.firmware_id = le_to_uint(content + 4);
			break;
		default:
			break;
		}
	}

	return;
}

static void fwu_compare_partition_tables(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (fwu->phyaddr.ui_firmware != fwu->v7_img.phyaddr.ui_firmware) {
		fwu->new_partition_table = true;
		return;
	}

	if (fwu->phyaddr.ui_config != fwu->v7_img.phyaddr.ui_config) {
		fwu->new_partition_table = true;
		return;
	}

	if (fwu->flash_properties.has_disp_config) {
		if (fwu->phyaddr.dp_config != fwu->v7_img.phyaddr.dp_config) {
			fwu->new_partition_table = true;
			return;
		}
	}

	if (fwu->flash_properties.has_disp_config) {
		if (fwu->phyaddr.dp_config != fwu->v7_img.phyaddr.dp_config) {
			fwu->new_partition_table = true;
			return;
		}
	}

	if (fwu->has_guest_code) {
		if (fwu->phyaddr.guest_code != fwu->v7_img.phyaddr.guest_code) {
			fwu->new_partition_table = true;
			return;
		}
	}

	fwu->new_partition_table = false;

	return;
}

static int fwu_parse_image_info_v7(struct synaptics_rmi4_data *rmi4_data)
{
	struct image_header_10 *header;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	header = (struct image_header_10 *)fwu->image;

	memset(&fwu->v7_img, 0x00, sizeof(fwu->v7_img));

	tsp_debug_info(false, &rmi4_data->i2c_client->dev,
						"%s: header->major_header_version = %d\n",
						__func__, header->major_header_version);

	switch (header->major_header_version) {
	case IMAGE_HEADER_VERSION_10:
		fwu_parse_image_header_10(rmi4_data);
		break;
	default:
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: Unsupported image file format (0x%02x)\n",
				__func__, header->major_header_version);
		return -EINVAL;
	}

	if (fwu->bl_version == BL_V7) {
		if (!fwu->v7_img.contains_flash_config) {
			tsp_debug_err(false, &rmi4_data->i2c_client->dev,
					"%s: No flash config found in firmware image\n",
					__func__);
			return -EINVAL;
		}

		fwu_parse_partition_table(rmi4_data, fwu->v7_img.fl_config.data,
				&fwu->v7_img.blkcount, &fwu->v7_img.phyaddr);

		fwu_compare_partition_tables(rmi4_data);
	} else {
		fwu->new_partition_table = false;
	}

	return 0;
}

static int fwu_enter_flash_prog_v7(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct f01_device_control f01_device_control;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	unsigned char int_enable = 0x00;

	retval = fwu_read_flash_status_v7(rmi4_data);
	if (retval < 0)
		return retval;

	if (fwu->in_bl_mode)
		return 0;

	
	/* To block interrupt from finger or hovering during enter flash program mode
	 * 1. F01_RMI_CTRL1(Interrupt Enable 0) register is cleared on this position.
	 * 2. After enter flash program mode, interrupt occured only by Flash bit(0x01)
	 * 3. After finish all flashing and reset, Interrupt enable register will be set as default value
	 */
	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f01_fd.ctrl_base_addr + 1,
			&int_enable, sizeof(int_enable));

	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write interrupt enable register\n", __func__);
		return retval;
	}
	msleep(20);

	retval = fwu_write_f34_command_v7(rmi4_data, v7_CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(rmi4_data, ENABLE_WAIT_MS);
	if (retval < 0)
		return retval;

	if (!fwu->in_bl_mode) {
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: BL mode not entered\n",
				__func__);
		return -EINVAL;
	}

	retval = fwu_scan_pdt(rmi4_data);
	if (retval < 0)
		return retval;

	/* TSP IC Bootloader sub version will use programming key on Bootloader mode.
	 * UI mode BL sub version and BL sub version can be different,
	 * so read and use BL sub version after entering BL mode.
	    After flash fw, restore BL version for UI mode.
	 */
	fwu_read_f34_queries_bl_version(rmi4_data);

	retval = rmi4_data->i2c_read(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: Failed to read F01 device control\n",
				__func__);
		return retval;
	}

	f01_device_control.nosleep = true;
	f01_device_control.sleep_mode = SLEEP_MODE_NORMAL;

	retval = rmi4_data->i2c_write(rmi4_data,
			rmi4_data->f01_ctrl_base_addr,
			f01_device_control.data,
			sizeof(f01_device_control.data));
	if (retval < 0) {
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: Failed to write F01 device control\n",
				__func__);
		return retval;
	}

	msleep(20);

	return retval;
}

static int fwu_start_reflash_v7(struct synaptics_rmi4_data *rmi4_data)
{
	int retval = 0;
	enum flash_area flash_area;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (rmi4_data->sensor_sleep) {
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: Sensor sleeping\n",
				__func__);
		return -ENODEV;
	}

	rmi4_data->stay_awake = true;

	mutex_lock(&rmi4_data->rmi4_reflash_mutex);

	tsp_debug_info(false, &rmi4_data->i2c_client->dev, 
					"%s: Start of reflash process\n", __func__);

	retval = fwu_parse_image_info_v7(rmi4_data);
	if (retval < 0)
		goto exit;

	if (fwu->bl_version != fwu->v7_img.bl_version) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Bootloader version mismatch\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	fwu->force_update = true;

	if (!fwu->force_update && fwu->new_partition_table) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Partition table mismatch\n",
				__func__);
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_read_flash_status_v7(rmi4_data);
	if (retval < 0)
		goto exit;

	if (fwu->in_bl_mode) {
		tsp_debug_info(false, &rmi4_data->i2c_client->dev,
				"%s: Device in bootloader mode\n",
				__func__);
	}

	flash_area = UI_FIRMWARE;
	rmi4_data->doing_reflash = true;
	fwu_enter_flash_prog_v7(rmi4_data);

	retval = fwu_do_reflash_v7(rmi4_data);

	if (retval < 0) {
		tsp_debug_err(false, &rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
	}

	rmi4_data->reset_device(rmi4_data);
	rmi4_data->doing_reflash = false;

	fwu->bootloader_id[0] = fwu->bootloader_id_ic[0];
	fwu->bootloader_id[1] = fwu->bootloader_id_ic[1];

exit:
	tsp_debug_info(false, &rmi4_data->i2c_client->dev,
			"%s: End of reflash process\n", __func__);

	mutex_unlock(&rmi4_data->rmi4_reflash_mutex);

	return retval;
}

static int fwu_do_write_guest_code(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu->has_guest_code) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Firmware does not support Guest Code.\n",
			__func__);
		retval = -EINVAL;
	}
	if (fwu->guest_code_block_count !=
		(fwu->img.guestCode.size/fwu->block_size)) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
			"%s: Size of Guest Code not match (dev: %x, img: %x).\n",
			__func__, fwu->guest_code_block_count,
			fwu->img.guestCode.size/fwu->block_size);
		retval = -EINVAL;
	}

	retval = fwu_enter_flash_prog(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	retval = fwu_write_bootloader_id(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(rmi4_data, CMD_ERASE_GUEST_CODE);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	retval = fwu_write_guest_code_block(rmi4_data);
	if (retval < 0)
		return retval;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: guest code written\n",
			__func__);

	return retval;
}

static int fwu_start_write_config(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned short block_count;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->has_perm_config)
			return -EINVAL;
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->has_bl_config)
			return -EINVAL;
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->has_disp_config)
			return -EINVAL;
		block_count = fwu->disp_config_block_count;
		break;
	default:
		return -EINVAL;
	}

	if (fwu->ext_data_source)
		fwu->img.uiConfig.data = fwu->ext_data_source;
	else
		return -EINVAL;

	fwu->config_size = fwu->block_size * block_count;

	/* Jump to the config area if given a packrat image */
	if ((fwu->config_area == UI_CONFIG_AREA) &&
			(fwu->config_size != fwu->img.image_size)) {
		fwu_img_parse_format(rmi4_data);
	}

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Start of write config process\n",
		__func__);

	rmi4_data->doing_reflash = true;
	retval = fwu_do_write_config(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	rmi4_data->reset_device(rmi4_data);
	rmi4_data->doing_reflash = false;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: End of write config process\n",
		__func__);

	return retval;
}

#define CHECKSUM_SIZE	4
static void synaptics_rmi_calculate_checksum(unsigned short *data,
				unsigned short len, unsigned long *result)
{
	unsigned long temp;
	unsigned long sum1 = 0xffff;
	unsigned long sum2 = 0xffff;

	*result = 0xffffffff;

	while (len--) {
		temp = *data;
		sum1 += temp;
		sum2 += sum1;
		sum1 = (sum1 & 0xffff) + (sum1 >> 16);
		sum2 = (sum2 & 0xffff) + (sum2 >> 16);
		data++;
	}

	*result = sum2 << 16 | sum1;

	return;
}

static void synaptics_rmi_rewrite_checksum(unsigned char *dest,
				unsigned long src)
{
	dest[0] = (unsigned char)(src & 0xff);
	dest[1] = (unsigned char)((src >> 8) & 0xff);
	dest[2] = (unsigned char)((src >> 16) & 0xff);
	dest[3] = (unsigned char)((src >> 24) & 0xff);

	return;
}

int synaptics_rmi4_set_tsp_test_result_in_config(struct synaptics_rmi4_data *rmi4_data, int value)

{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	int retval;
	unsigned char buf[10] = {0, };
	unsigned long checksum;

	/* read config from IC */
	memset(buf, 0, sizeof(buf));
	snprintf(buf, 2, "%u\n", 1);
	fwu_sysfs_read_config_store(fwu->attr_dir, NULL, buf, 1);

	/* set test result value
	 * MSB 4bit of Customr derined config ID 0 used for factory test in TSP.
	 * PASS : 2, FAIL : 1, NONE: 0.
	 */
	fwu->read_config_buf[0] &= 0x0F;
	fwu->read_config_buf[0] |= value << 4;

	/* check CRC checksum value and re-write checksum in config */
	synaptics_rmi_calculate_checksum((unsigned short *)fwu->read_config_buf,
			(fwu->config_size - CHECKSUM_SIZE) / 2, &checksum);

	synaptics_rmi_rewrite_checksum(&fwu->read_config_buf[fwu->config_size - CHECKSUM_SIZE],
			checksum);

	rmi4_data->doing_reflash = true;

	retval = fwu_enter_flash_prog(rmi4_data);
	if (retval < 0)
		goto err_config_write;


	retval = fwu_write_bootloader_id(rmi4_data);
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(rmi4_data, CMD_ERASE_CONFIG);
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(rmi4_data, ERASE_WAIT_MS);
	if (retval < 0)
		goto err_config_write;

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	fwu_write_blocks(rmi4_data, fwu->read_config_buf,
			fwu->config_size, CMD_WRITE_CONFIG_BLOCK);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev, "%s: Config written\n",
			__func__);

err_config_write:
	rmi4_data->reset_device(rmi4_data);
	rmi4_data->doing_reflash = false;

	return retval;
}

static int fwu_start_write_guest_code(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu->ext_data_source)
		return -EINVAL;

	fwu_img_parse_format(rmi4_data);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: Start of update guest code process\n", __func__);

	retval = fwu_do_write_guest_code(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	rmi4_data->reset_device(rmi4_data);

	tsp_debug_info(true, &rmi4_data->i2c_client->dev,
			"%s: End of write guest code process\n", __func__);

	return retval;
}

static int fwu_do_read_config(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short block_count;
	unsigned short index = 0;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = fwu_enter_flash_prog(rmi4_data);
	if (retval < 0)
		goto exit;

	tsp_debug_dbg(false, &rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!fwu->has_perm_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!fwu->has_bl_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!fwu->has_disp_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->disp_config_block_count;
		break;
	default:
		retval = -EINVAL;
		goto exit;
	}

	fwu->config_size = fwu->block_size * block_count;

	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->config_size, GFP_KERNEL);
	if (!fwu->read_config_buf) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for config size data\n",
				__func__);
		return -ENOMEM;
	}

	block_offset[1] |= (fwu->config_area << 5);

	retval = rmi4_data->i2c_write(rmi4_data,
			fwu->f34_fd.data_base_addr + BLOCK_NUMBER_OFFSET,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		goto exit;
	}

	for (block_num = 0; block_num < block_count; block_num++) {
		retval = fwu_write_f34_command(rmi4_data, CMD_READ_CONFIG_BLOCK);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to write read config command\n",
					__func__);
			goto exit;
		}

		retval = fwu_wait_for_idle(rmi4_data, WRITE_WAIT_MS);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status\n",
					__func__);
			goto exit;
		}

		retval = rmi4_data->i2c_read(rmi4_data,
				fwu->f34_fd.data_base_addr + fwu->blk_data_off,
				&fwu->read_config_buf[index],
				fwu->block_size);
		if (retval < 0) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: Failed to read block data (block %d)\n",
					__func__, block_num);
			goto exit;
		}

		index += fwu->block_size;
	}

exit:
	rmi4_data->reset_device(rmi4_data);

	return retval;
}

int synaptics_fw_updater(struct synaptics_rmi4_data *rmi4_data, unsigned char *fw_data)
{
	int retval;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu)
		return -ENODEV;

	if (!fwu->initialized)
		return -ENODEV;

	if (!fw_data) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Firmware data is NULL\n", __func__);
		return -ENODEV;
	}

	fwu->ext_data_source = fw_data;
	fwu->config_area = UI_CONFIG_AREA;

	fwu->image = fw_data;
	retval = fwu_start_reflash_v7(rmi4_data);
	if (retval < 0)
		goto out_fw_update;

out_fw_update:
	return retval;
}
EXPORT_SYMBOL(synaptics_fw_updater);

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (count < fwu->config_size) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Not enough space (%d bytes) in buffer\n",
				__func__, (int)count);
		return -EINVAL;
	}

	memcpy(buf, fwu->read_config_buf, fwu->config_size);

	return fwu->config_size;
}

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	memcpy((void *)(&fwu->ext_data_source[fwu->data_pos]),
			(const void *)buf,
			count);

	fwu->data_pos += count;
	fwu->img.fw_image = fwu->ext_data_source;
	return count;
}

static ssize_t fwu_sysfs_do_reflash_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input & UPDATE_MODE_LOCKDOWN) {
		fwu->do_lockdown = true;
		input &= ~UPDATE_MODE_LOCKDOWN;
	}

	if ((input != UPDATE_MODE_NORMAL) && (input != UPDATE_MODE_FORCE)) {
		retval = -EINVAL;
		goto exit;
	}

	if (input == UPDATE_MODE_FORCE)
		fwu->force_update = true;

	retval = synaptics_fw_updater(rmi4_data, fwu->ext_data_source);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = DO_LOCKDOWN;
	return retval;
}

static ssize_t fwu_sysfs_write_config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_config(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static ssize_t fwu_sysfs_read_config_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	retval = fwu_do_read_config(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read config\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t fwu_sysfs_config_area_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long config_area;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = kstrtoul(buf, 10, &config_area);
	if (retval)
		return retval;

	fwu->config_area = config_area;

	return count;
}

static ssize_t fwu_sysfs_image_size_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long size;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	retval = kstrtoul(buf, 10, &size);
	if (retval)
		return retval;

	fwu->img.image_size = size;
	fwu->data_pos = 0;

	kfree(fwu->ext_data_source);
	fwu->ext_data_source = kzalloc(fwu->img.image_size, GFP_KERNEL);
	if (!fwu->ext_data_source) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image data\n",
				__func__);
		return -ENOMEM;
	}

	return count;
}

static ssize_t fwu_sysfs_block_size_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->block_size);
}

static ssize_t fwu_sysfs_firmware_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->fw_block_count);
}

static ssize_t fwu_sysfs_configuration_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->config_block_count);
}

static ssize_t fwu_sysfs_perm_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->perm_config_block_count);
}

static ssize_t fwu_sysfs_bl_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->bl_config_block_count);
}

static ssize_t fwu_sysfs_disp_config_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->disp_config_block_count);
}

static ssize_t fwu_sysfs_guest_code_block_count_show(struct kobject *kobj,
		struct kobj_attribute *attr, char *buf)
{
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	return snprintf(buf, PAGE_SIZE, "%u\n", fwu->guest_code_block_count);
}

static ssize_t fwu_sysfs_write_guest_code_store(struct kobject *kobj,
		struct kobj_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = rmi_attr_kobj_to_drvdata(kobj);
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_guest_code(rmi4_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to write guest code\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static void synaptics_rmi4_fwu_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu)
		return;

	if (fwu->intr_mask & intr_mask)
		fwu_read_flash_status_v7(rmi4_data);

	return;
}

static void fwu_img_scan_x10_container(struct synaptics_rmi4_data *rmi4_data,
		unsigned int list_length, unsigned char *start_addr)
{
	unsigned int i;
	unsigned int length;
	unsigned int contentAddr, addr;
	unsigned short containerId;
	struct block_data block;
	struct img_x10_descriptor *containerDescriptor;
	struct img_x10_bl_container *blContainer;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	for (i = 0; i < list_length; i += 4) {
		contentAddr = extract_uint_le(start_addr + i);
		containerDescriptor =
			(struct img_x10_descriptor *)(fwu->img.fw_image + contentAddr);
		containerId = extract_ushort_le(containerDescriptor->containerID);
		addr = extract_uint_le(containerDescriptor->contentAddress);
		length = extract_uint_le(containerDescriptor->contentLength);
		block.data = fwu->img.fw_image + addr;
		block.size = length;
		switch (containerId) {
		case ID_UI_CONTAINER:
			fwu->img.uiFirmware = block;
			break;
		case ID_UI_CONFIGURATION:
			fwu->img.uiConfig = block;
			fwu->img.configId = fwu->img.uiConfig.data;
			break;
		case ID_BOOTLOADER_LOCKDOWN_INFORMATION_CONTAINER:
			fwu->img.lockdown = block;
			break;
		case ID_GUEST_CODE_CONTAINER:
			fwu->img.guestCode = block;
			break;
		case ID_BOOTLOADER_CONTAINER:
			blContainer =
			(struct img_x10_bl_container *)(fwu->img.fw_image + addr);
			fwu->img.blMajorVersion = blContainer->majorVersion;
			fwu->img.blMinorVersion = blContainer->minorVersion;
			fwu->img.bootloaderInfo = block;
			break;
		case ID_PERMANENT_CONFIGURATION_CONTAINER:
			fwu->img.permanent = block;
			break;
		case ID_GENERAL_INFORMATION_CONTAINER:
			fwu->img.packageId = fwu->img.fw_image + addr + 0;
			fwu->img.firmwareId = fwu->img.fw_image + addr + 4;
			fwu->img.dsFirmwareInfo = fwu->img.fw_image + addr + 8;
			break;
		default:
			break;
		}
	}
}

static void fwu_img_parse_x10_topcontainer(struct synaptics_rmi4_data *rmi4_data)
{
	struct img_x10_descriptor *descriptor;
	unsigned int topAddr;
	unsigned int list_length, bl_length;
	unsigned char *start_addr;
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	topAddr = extract_uint_le(fwu->img.fw_image +
				IMG_X10_TOP_CONTAINER_OFFSET);
	descriptor = (struct img_x10_descriptor *)
			(fwu->img.fw_image + topAddr);
	list_length = extract_uint_le(descriptor->contentLength);
	start_addr = fwu->img.fw_image +
			extract_uint_le(descriptor->contentAddress);
	fwu_img_scan_x10_container(rmi4_data, list_length, start_addr);
	/* scan sub bootloader container (lockdown container) */
	if (fwu->img.bootloaderInfo.data != NULL) {
		bl_length = fwu->img.bootloaderInfo.size - 4;
		if (bl_length)
			fwu_img_scan_x10_container(rmi4_data, bl_length,
					fwu->img.bootloaderInfo.data);
	}
}

static void fwu_img_parse_x10(struct synaptics_rmi4_data *rmi4_data)
{
	fwu_img_parse_x10_topcontainer(rmi4_data);
}

static void fwu_img_parse_x0_x6(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;
	struct img_x0x6_header *header = (struct img_x0x6_header *)fwu->img.fw_image;

	if (header->bootloader_version > 6)
		return;
	fwu->img.blMajorVersion = header->bootloader_version;
	fwu->img.uiFirmware.size = extract_uint_le(header->firmware_size);
	fwu->img.uiFirmware.data = fwu->img.fw_image + IMG_X0_X6_FW_OFFSET;
	fwu->img.uiConfig.size = extract_uint_le(header->config_size);
	fwu->img.uiConfig.data = fwu->img.uiFirmware.data + fwu->img.uiFirmware.size;
	fwu->img.configId = fwu->img.uiConfig.data;
	switch (fwu->img.imageFileVersion) {
	case 0x2:
		fwu->img.lockdown.size = 0x30;
		break;
	case 0x3:
	case 0x4:
		fwu->img.lockdown.size = 0x40;
		break;
	case 0x5:
	case 0x6:
		fwu->img.lockdown.size = 0x50;
		if (header->options_firmware_id) {
			fwu->img.firmwareId = header->firmware_id;
			fwu->img.packageId = header->package_id;
			fwu->img.dsFirmwareInfo = header->ds_firmware_info;
		}
		break;
	default:
		break;
	}
	fwu->img.lockdown.data = fwu->img.fw_image +
		IMG_X0_X6_FW_OFFSET - fwu->img.lockdown.size;
}

static void fwu_img_parse_format(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	fwu->polling_mode = POLLING_MODE_DEFAULT;
	fwu->img.firmwareId = NULL;
	fwu->img.packageId = NULL;
	fwu->img.dsFirmwareInfo = NULL;
	fwu->img.uiFirmware.data = NULL;
	fwu->img.uiConfig.data = NULL;
	fwu->img.lockdown.data = NULL;
	fwu->img.guestCode.data = NULL;
	fwu->img.uiConfig.size = 0;
	fwu->img.uiFirmware.size = 0;
	fwu->img.lockdown.size = 0;
	fwu->img.guestCode.size =	 0;

	fwu->img.imageFileVersion = fwu->img.fw_image[IMG_VERSION_OFFSET];

	switch (fwu->img.imageFileVersion) {
	case 0x10:
		fwu_img_parse_x10(rmi4_data);
		break;
	case 0x5:
	case 0x6:
		fwu_img_parse_x0_x6(rmi4_data);
		break;
	default:
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Unsupported image file format $%X\n",
				__func__, fwu->img.imageFileVersion);
		break;
	}

	if (fwu->bl_version == BL_V7) {

		memset(&fwu->v7_img, 0x00, sizeof(fwu->v7_img));

		if (!fwu->v7_img.contains_flash_config) {
			tsp_debug_err(true, &rmi4_data->i2c_client->dev,
					"%s: No flash config found in firmware image\n",
					__func__);
			return;
		}

		fwu_parse_partition_table(rmi4_data, fwu->v7_img.fl_config.data,
				&fwu->v7_img.blkcount, &fwu->v7_img.phyaddr);

		fwu_compare_partition_tables(rmi4_data);
	} else {
		fwu->new_partition_table = false;
	}
}

static int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	struct pdt_properties pdt_props;
	struct synaptics_rmi4_fwu_handle *fwu = NULL;

	fwu = kzalloc(sizeof(struct synaptics_rmi4_fwu_handle), GFP_KERNEL);
	if (!fwu) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu\n",
				__func__);
		retval = -ENOMEM;
		goto exit;
	}

	rmi4_data->fwu = fwu;
	fwu->rmi4_data = rmi4_data;

	memset(&fwu->img, 0, sizeof(struct img_file_content));

	fwu->img.image_name = kzalloc(MAX_IMAGE_NAME_LEN, GFP_KERNEL);
	if (!fwu->img.image_name) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image name\n",
				__func__);
		retval = -ENOMEM;
		goto err_free_fwu;
	}

	retval = rmi4_data->i2c_read(rmi4_data,
			PDT_PROPS,
			pdt_props.data,
			sizeof(pdt_props.data));
	if (retval < 0) {
		tsp_debug_info(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to read PDT properties, assuming 0x00\n",
				__func__);
	} else if (pdt_props.has_bsr) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Reflash for LTS not currently supported\n",
				__func__);
		retval = -ENODEV;
		goto err_free_mem;
	}

	retval = fwu_scan_pdt(rmi4_data);
	if (retval < 0)
		goto err_free_mem;

	memset(&fwu->blkcount, 0x00, sizeof(fwu->blkcount));
	memset(&fwu->phyaddr, 0x00, sizeof(fwu->phyaddr));
	fwu_read_f34_queries_v7(rmi4_data);

	fwu->force_update = FORCE_UPDATE;
	fwu->do_lockdown = DO_LOCKDOWN;
	fwu->initialized = true;

	fwu->attr_dir = kobject_create_and_add(ATTRIBUTE_FOLDER_NAME,
			&rmi4_data->input_dev->dev.kobj);
	if (!fwu->attr_dir) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs directory\n",
				__func__);
		retval = -ENODEV;
		goto err_attr_dir;
	}

	retval = sysfs_create_bin_file(fwu->attr_dir, &dev_attr_data);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto err_sysfs_bin;
	}

	retval = sysfs_create_group(fwu->attr_dir, &attr_group);
	if (retval < 0) {
		tsp_debug_err(true, &rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs attributes\n", __func__);
		retval = -ENODEV;
		goto err_sysfs_attrs;
	}

	return 0;

err_sysfs_attrs:
	sysfs_remove_group(fwu->attr_dir, &attr_group);
	sysfs_remove_bin_file(fwu->attr_dir, &dev_attr_data);

err_sysfs_bin:
	kobject_put(fwu->attr_dir);

err_attr_dir:
err_free_mem:
	kfree(fwu->img.image_name);

err_free_fwu:
	kfree(fwu);
	rmi4_data->fwu = NULL;

exit:
	return retval;
}

static void synaptics_rmi4_fwu_remove(struct synaptics_rmi4_data *rmi4_data)
{
	struct synaptics_rmi4_fwu_handle *fwu = rmi4_data->fwu;

	if (!fwu)
		goto exit;

	sysfs_remove_group(fwu->attr_dir, &attr_group);
	sysfs_remove_bin_file(fwu->attr_dir, &dev_attr_data);

	kobject_put(fwu->attr_dir);

	kfree(fwu->read_config_buf);
	kfree(fwu->img.image_name);
	kfree(fwu);
	rmi4_data->fwu = NULL;

exit:
	return;
}

int rmi4_fw_update_module_register(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;

	retval = synaptics_rmi4_new_function(RMI_FW_UPDATER,
			rmi4_data,
			synaptics_rmi4_fwu_init,
			NULL,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn);

	return retval;
}
