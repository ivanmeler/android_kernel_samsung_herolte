/*
 *  Copyright (C) 2012, Samsung Electronics Co. Ltd. All Rights Reserved.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c/mxtt.h>
#include <asm/unaligned.h>
#include <linux/firmware.h>
#include <linux/string.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_batt.h>
#include <linux/of_gpio.h>
#include <linux/pinctrl/consumer.h>
#include <linux/sec_sysfs.h>

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#if TSP_USE_ATMELDBG
#include <asm/bug.h>
#endif

#define USE_T100_MULTI_SLOT
extern unsigned int system_rev;
static int mxt_read_mem(struct mxt_data *data, u16 reg, u16 len, void *buf)
{
	int ret = 0, i = 0;
	u16 le_reg = cpu_to_le16(reg);
	struct i2c_msg msg[2] = {
		{
			.addr	= data->client->addr,
			.flags	= 0,
			.len	= 2,
			.buf	= (u8 *)&le_reg,
		},
		{
			.addr	= data->client->addr,
			.flags	= I2C_M_RD,
			.len	= len,
			.buf	= buf,
		},
	};

#if TSP_USE_ATMELDBG
	if (data->atmeldbg.block_access)
		return 0;
#endif

	for (i = 0; i < 3 ; i++) {
		ret = i2c_transfer(data->client->adapter, msg, 2);
		if (ret < 0){
			input_err(true, &data->client->dev, "%s fail[%d] address[0x%x]\n",
				__func__, ret, le_reg);
			continue;

		} else if (2 == ret)
			break;
	}

	if (2 != ret)
		input_err(true, &data->client->dev,
			"%s fail[%d] address[0x%x]\n", __func__, ret, le_reg);

	return ret == 2 ? 0 : -EIO;
}

static int mxt_write_mem(struct mxt_data *data,
		u16 reg, u16 len, const u8 *buf)
{
	int ret = 0, i = 0;
	u8 tmp[len + 2];

#if TSP_USE_ATMELDBG
	if (data->atmeldbg.block_access)
		return 0;
#endif

	put_unaligned_le16(cpu_to_le16(reg), tmp);
	memcpy(tmp + 2, buf, len);

	for (i = 0; i < 3 ; i++) {
		ret = i2c_master_send(data->client, tmp, sizeof(tmp));
		if (ret < 0)
			input_err(true, &data->client->dev,	"%s %d times write error on address[0x%x,0x%x]\n",
				__func__, i, tmp[1], tmp[0]);
		else
			break;
	}

	return ret == sizeof(tmp) ? 0 : -EIO;
}

static int mxt_get_object_table(struct mxt_data *data);
static int mxt_read_id_info(struct mxt_data *data);

static struct mxt_object *
	mxt_get_object(struct mxt_data *data, u8 type)
{
	struct mxt_object *object;
	int i;

	if (!data->objects) {
		input_err(true, &data->client->dev,
			"%s object is NULL\n", __func__);
		return NULL;
	}

	for (i = 0; i < data->info.object_num; i++) {
		object = data->objects + i;
		if (object->type == type)
			return object;
	}

	input_err(true, &data->client->dev, "Invalid object type T%d\n",
		type);

	return NULL;
}

static int mxt_read_message(struct mxt_data *data,
				 struct mxt_message *message)
{
	struct mxt_object *object;

	object = mxt_get_object(data, MXT_GEN_MESSAGEPROCESSOR_T5);
	if (!object)
		return -EINVAL;

	return mxt_read_mem(data, object->start_address,
			sizeof(struct mxt_message), message);
}

static int mxt_read_message_reportid(struct mxt_data *data,
	struct mxt_message *message, u8 reportid)
{
	int try = 0;
	int error;
	int fail_count;

	fail_count = data->max_reportid * 2;

	while (++try < fail_count) {
		error = mxt_read_message(data, message);
		if (error)
			return error;

		if (message->reportid == 0xff)
			continue;

		if (message->reportid == reportid)
			return 0;
	}

	return -EINVAL;
}

static int mxt_read_object(struct mxt_data *data,
				u8 type, u16 offset, u8 *val)
{
	struct mxt_object *object;
	int error = 0;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	error = mxt_read_mem(data, object->start_address + offset, 1, val);
	if (error)
		input_err(true, &data->client->dev, "Error to read T[%d] offset[%d] val[%d]\n",
			type, offset, *val);
	return error;
}

static int mxt_write_object(struct mxt_data *data,
				 u8 type, u16 offset, u8 val)
{
	struct mxt_object *object;
	int error = 0;
	u16 reg;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	if (offset >= object->size * object->instances) {
		input_err(true, &data->client->dev, "Tried to write outside object T%d offset:%d, size:%d\n",
			type, offset, object->size);
		return -EINVAL;
	}
	reg = object->start_address;
	error = mxt_write_mem(data, reg + offset, 1, &val);
	if (error)
		input_err(true, &data->client->dev, "Error to write T[%d] offset[%d] val[%d]\n",
			type, offset, val);

	return error;
}

static u32 mxt_make_crc24(u32 crc, u8 byte1, u8 byte2)
{
	static const u32 crcpoly = 0x80001B;
	u32 res;
	u16 data_word;

	data_word = (((u16)byte2) << 8) | byte1;
	res = (crc << 1) ^ (u32)data_word;

	if (res & 0x1000000)
		res ^= crcpoly;

	return res;
}

static int mxt_calculate_infoblock_crc(struct mxt_data *data,
		u32 *crc_pointer)
{
	u32 crc = 0;
	u8 mem[7 + data->info.object_num * 6];
	int ret;
	int i;

	ret = mxt_read_mem(data, 0, sizeof(mem), mem);

	if (ret)
		return ret;

	for (i = 0; i < sizeof(mem) - 1; i += 2)
		crc = mxt_make_crc24(crc, mem[i], mem[i + 1]);

	*crc_pointer = mxt_make_crc24(crc, mem[i], 0) & 0x00FFFFFF;

	return 0;
}

static int mxt_read_info_crc(struct mxt_data *data, u32 *crc_pointer)
{
	u16 crc_address;
	u8 msg[3];
	int ret;

	/* Read Info block CRC address */
	crc_address = MXT_OBJECT_TABLE_START_ADDRESS +
			data->info.object_num * MXT_OBJECT_TABLE_ELEMENT_SIZE;

	ret = mxt_read_mem(data, crc_address, 3, msg);
	if (ret)
		return ret;

	*crc_pointer = msg[0] | (msg[1] << 8) | (msg[2] << 16);

	return 0;
}
static int mxt_read_config_crc(struct mxt_data *data, u32 *crc)
{
	struct device *dev = &data->client->dev;
	struct mxt_message message;
	struct mxt_object *object;
	int error;

	object = mxt_get_object(data, MXT_GEN_COMMANDPROCESSOR_T6);
	if (!object)
		return -EIO;

	/* Try to read the config checksum of the existing cfg */
	mxt_write_object(data, MXT_GEN_COMMANDPROCESSOR_T6,
		MXT_COMMAND_REPORTALL, 1);

	/* Read message from command processor, which only has one report ID */
	error = mxt_read_message_reportid(data, &message, object->max_reportid);
	if (error) {
		input_err(true, dev, "Failed to retrieve CRC\n");
		return error;
	}

	/* Bytes 1-3 are the checksum. */
	*crc = message.message[1] | (message.message[2] << 8) |
		(message.message[3] << 16);

	return 0;
}

static int mxt_check_instance(struct mxt_data *data, u8 type)
{
	int i;

	for (i = 0; i < data->info.object_num; i++) {
		if (data->objects[i].type == type)
			return data->objects[i].instances;
	}
	return 0;
}

static int mxt_init_write_config(struct mxt_data *data,
		u8 type, const u8 *cfg)
{
	struct mxt_object *object;
	u8 *temp;
	int ret;

	object = mxt_get_object(data, type);
	if (!object)
		return -EINVAL;

	if ((object->size == 0) || (object->start_address == 0)) {
		input_err(true, &data->client->dev,	"%s error T%d\n",
			 __func__, type);
		return -ENODEV;
	}

	ret = mxt_write_mem(data, object->start_address,
			object->size, cfg);
	if (ret) {
		input_err(true, &data->client->dev,	"%s write error T%d address[0x%x]\n",
			__func__, type, object->start_address);
		return ret;
	}

	if (mxt_check_instance(data, type)) {
		temp = kzalloc(object->size, GFP_KERNEL);

		if (temp == NULL)
			return -ENOMEM;

		ret |= mxt_write_mem(data, object->start_address + object->size,
			object->size, temp);
		kfree(temp);
	}

	return ret;
}

static int mxt_write_config_from_pdata(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	u8 **tsp_config = (u8 **)data->pdata->config;
	u8 i;
	int ret = 0;

	if (!tsp_config) {
		input_info(true, dev, "No cfg data in pdata\n");
		return 0;
	}

	for (i = 0; tsp_config[i][0] != MXT_RESERVED_T255; i++) {
		ret = mxt_init_write_config(data, tsp_config[i][0],
							tsp_config[i] + 1);
		if (ret)
			return ret;
	}
	return ret;
}

static int mxt_write_config(struct mxt_fw_info *fw_info)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	struct mxt_object *object;
	struct mxt_cfg_data *cfg_data;
	u32 current_crc;
	u8 i, val = 0;
	u16 reg, index;
	int ret;
	input_info(true, &data->client->dev, "%s\n", __func__);

	if (!fw_info->cfg_raw_data) {
		input_info(true, dev, "No cfg data in file\n");
		ret = mxt_write_config_from_pdata(data);
		return ret;
	}

	/* Get config CRC from device */
	ret = mxt_read_config_crc(data, &current_crc);
	if (ret)
		return ret;

	/* Check Version information */
	if (fw_info->fw_ver != data->info.version) {
		input_err(true, dev, "Warning: version mismatch! %s\n", __func__);
		return 0;
	}
	if (fw_info->build_ver != data->info.build) {
		input_err(true, dev, "fw file build info [%d], Binary build info[%d]\n",
							fw_info->build_ver, data->info.build);
		return 0;
	}

	/* Check config CRC */
	if (current_crc == fw_info->cfg_crc) {
		input_info(true, dev, "Skip writing Config:[CRC 0x%06X] as current_crc == fw_info->cfg_crc\n",
			current_crc);
		return 0;
	}

	input_info(true, dev, "Writing Config:[CRC 0x%06X!=0x%06X]\n",
		current_crc, fw_info->cfg_crc);

	/* Write config info */
	for (index = 0; index < fw_info->cfg_len;) {

		if (index + sizeof(struct mxt_cfg_data) >= fw_info->cfg_len) {
			input_err(true, dev, "index(%d) of cfg_data exceeded total size(%d)!!\n",
				(int)(index + sizeof(struct mxt_cfg_data)), (int)fw_info->cfg_len);
			return -EINVAL;
		}

		/* Get the info about each object */
		cfg_data = (struct mxt_cfg_data *)(&fw_info->cfg_raw_data[index]);

		index += sizeof(struct mxt_cfg_data) + cfg_data->size;
		if (index > fw_info->cfg_len) {
			input_err(true, dev, "index(%d) of cfg_data exceeded total size(%d) in T%d object!!\n",
				index, fw_info->cfg_len, cfg_data->type);
			return -EINVAL;
		}

		object = mxt_get_object(data, cfg_data->type);
		if (!object) {
			input_err(true, dev, "T%d is Invalid object type\n",
				cfg_data->type);
			return -EINVAL;
		}

		/* Check and compare the size, instance of each object */
		if (cfg_data->size > object->size) {
			input_err(true, dev, "T%d Object length exceeded!\n",
				cfg_data->type);
			return -EINVAL;
		}
		if (cfg_data->instance >= object->instances) {
			input_err(true, dev, "T%d Object instances exceeded!\n",
				cfg_data->type);
			return -EINVAL;
		}

		input_dbg(false, dev, "Writing config for obj %d len %d instance %d (%d/%d)\n",
			cfg_data->type, object->size,
			cfg_data->instance, index, fw_info->cfg_len);

		reg = object->start_address + object->size * cfg_data->instance;

		/* Write register values of each object */
		ret = mxt_write_mem(data, reg, cfg_data->size, cfg_data->register_val);
		if (ret) {
			input_err(true, dev, "Write T%d Object failed\n",
				object->type);
			return ret;
		}

		/*
		 * If firmware is upgraded, new bytes may be added to end of
		 * objects. It is generally forward compatible to zero these
		 * bytes - previous behaviour will be retained. However
		 * this does invalidate the CRC and will force a config
		 * download every time until the configuration is updated.
		 */
		if (cfg_data->size < object->size) {
			input_err(true, dev, "Warning: zeroing %d byte(s) in T%d\n",
				 object->size - cfg_data->size, cfg_data->type);

			for (i = cfg_data->size + 1; i < object->size; i++) {
				ret = mxt_write_mem(data, reg + i, 1, &val);
				if (ret)
					return ret;
			}
		}
	}
	input_info(true, dev, "Updated configuration\n");

	return ret;
}

#if TSP_PATCH
#include "mxtt_patch.c"
#endif


static void mxt_report_input_data(struct mxt_data *data)
{
	int i;
	int count = 0;
	int report_count = 0;

	for (i = 0; i < MXT_MAX_FINGER; i++) {
		if (data->fingers[i].state == MXT_STATE_INACTIVE)
			continue;

		input_mt_slot(data->input_dev, i);
		if (data->fingers[i].state == MXT_STATE_RELEASE) {
			input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, false);
		} else {
			if (data->fingers[i].type
				 == MXT_T100_TYPE_HOVERING_FINGER)
				/* hover is reported */
				input_report_key(data->input_dev, BTN_TOUCH, 0);
			else
				/* finger or passive stylus are reported */
				input_report_key(data->input_dev, BTN_TOUCH, 1);

			input_mt_report_slot_state(data->input_dev,
					MT_TOOL_FINGER, true);
			input_report_abs(data->input_dev, ABS_MT_POSITION_X,
					data->fingers[i].x);
			input_report_abs(data->input_dev, ABS_MT_POSITION_Y,
					data->fingers[i].y);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MAJOR,
					 data->fingers[i].m);
			input_report_abs(data->input_dev, ABS_MT_TOUCH_MINOR,
					 data->fingers[i].n);
#ifdef REPORT_2D_Z
			input_report_abs(data->input_dev, ABS_MT_PRESSURE,
					 data->fingers[i].z);
#endif

#if TSP_USE_PALM_FLAG
			input_report_abs(data->input_dev, ABS_MT_PALM, data->palm);
#endif
		}
		report_count++;

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
#if TSP_USE_PALM_FLAG
		if (data->fingers[i].state == MXT_STATE_PRESS) {
			input_err(true, &data->client->dev, "[P][%d]: T[%d][%d] X[%4d],Y[%4d] M/m[%2d/%2d] P[%d]\n",
				i, data->fingers[i].type, data->fingers[i].event,
				data->fingers[i].x, data->fingers[i].y,
				data->fingers[i].m, data->fingers[i].n, data->palm);
		}
#else
		if (data->fingers[i].state == MXT_STATE_PRESS) {
			input_err(true, &data->client->dev, "[P][%d]: T[%d][%d] X[%4d],Y[%4d] M/m[%2d/%2d]\n",
				i, data->fingers[i].type, data->fingers[i].event,
				data->fingers[i].x, data->fingers[i].y,
				data->fingers[i].m, data->fingers[i].n);
		}
#endif
#else	/*!defined(CONFIG_SAMSUNG_PRODUCT_SHIP)*/
		if (data->fingers[i].state == MXT_STATE_PRESS) {
			input_info(true, &data->client->dev, "[P][%d]: T[%d][%d]\n",
				i, data->fingers[i].type, data->fingers[i].event);
		}
#endif /*!defined(CONFIG_SAMSUNG_PRODUCT_SHIP)*/

		else if (data->fingers[i].state == MXT_STATE_RELEASE)
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_err(true, &data->client->dev, "[R][%d]: T[%d][%d] M[%d] [%X/%X][%s%s]\n",
				i, data->fingers[i].type, data->fingers[i].event,
				data->fingers[i].mcount, data->info.version, data->info.build,
				data->pdata->paneltype == NULL ? "": data->pdata->paneltype, data->config_date);
#else
			input_err(true, &data->client->dev, "[R][%d]: T[%d][%d] M[%d]\n",
				i, data->fingers[i].type, data->fingers[i].event,
				data->fingers[i].mcount);
#endif
		if (data->fingers[i].state == MXT_STATE_RELEASE) {
			data->fingers[i].state = MXT_STATE_INACTIVE;
			data->fingers[i].mcount = 0;
		} else {
			data->fingers[i].state = MXT_STATE_MOVE;
			count++;
		}
	}

	if (count == 0)
		input_report_key(data->input_dev, BTN_TOUCH, 0);

	if (report_count > 0) {
#if TSP_USE_ATMELDBG
		if (!data->atmeldbg.stop_sync)
#endif
			input_sync(data->input_dev);
	}

	data->finger_mask = 0;
}

static void mxt_release_all_finger(struct mxt_data *data)
{
	int i;
	int count = 0;

	for (i = 0; i < MXT_MAX_FINGER; i++) {
		if (data->fingers[i].state == MXT_STATE_INACTIVE)
			continue;
		data->fingers[i].z = 0;
		data->fingers[i].state = MXT_STATE_RELEASE;
		count++;
	}
	if (count) {
		input_err(true, &data->client->dev, "%s\n", __func__);
		mxt_report_input_data(data);
	}
}

#if ENABLE_TOUCH_KEY
static void mxt_release_all_keys(struct mxt_data *data)
{
	int i = 0, code = 0;
	u8 back_falg, recent_flag;

	back_falg = TOUCH_KEY_BACK_4;
	recent_flag = TOUCH_KEY_RECENT_4;

	if (data->tsp_keystatus != TOUCH_KEY_NULL) {
		if (data->report_dummy_key) {
			for (i = 0 ; i < data->pdata->num_touchkey ; i++) {
				if (data->tsp_keystatus & data->pdata->touchkey[i].value) {
				/* report all touch-key event */
					input_report_key(data->input_dev,
						data->pdata->touchkey[i].keycode, KEY_RELEASE);
					input_info(true, &data->client->dev,
						"[TSP_KEY] %s R!\n", data->pdata->touchkey[i].name);
				}
			}

		} else {
			/* recent key check*/
			if (data->tsp_keystatus & recent_flag) {
				code = KEY_RECENT;
				input_info(true, &data->client->dev, "[TSP_KEY] RECENT R!\n");
			}

			/* back key check*/
			if (data->tsp_keystatus & back_falg) {
				code = KEY_BACK;
				input_info(true, &data->client->dev, "[TSP_KEY] BACK R!\n");
			}
		}

		input_report_key(data->input_dev, code, KEY_RELEASE);
		input_sync(data->input_dev);

		data->tsp_keystatus = TOUCH_KEY_NULL;
	}
}

static void mxt_treat_T15_object(struct mxt_data *data,
						struct mxt_message *message)
{
	struct input_dev *input = data->input_dev;
	u8 input_status = message->message[MXT_MSG_T15_STATUS] & MXT_MSGB_T15_DETECT;
	u8 input_message = message->message[MXT_MSG_T15_KEYSTATE];
	int i = 0, code_recent = 0, code_back = 0;
	u8 change_state = input_message ^ data->tsp_keystatus;
	u8 key_state = 0, key_state_recent = 0, key_state_back = 0;
	u8 back_falg, recent_flag, back_d_flag, recent_d_flag;

	input_info(true, &data->client->dev, "tkey irq handler[%s][0x%X]\n",
		(message->message[MXT_MSG_T15_STATUS] & MXT_MSGB_T15_DETECT) ? "PRESS" : "RELEASE", 
		message->message[MXT_MSG_T15_KEYSTATE]);

	back_falg = TOUCH_KEY_BACK_4;
	recent_flag = TOUCH_KEY_RECENT_4;
	back_d_flag = TOUCH_KEY_D_BACK_4;
	recent_d_flag = TOUCH_KEY_D_RECENT_4;

	/* single key configuration*/
	if (input_status) { /* press */
		if (data->report_dummy_key) {
			for (i = 0 ; i < data->pdata->num_touchkey ; i++) {
				if (change_state & data->pdata->touchkey[i].value) {
					key_state = input_message & data->pdata->touchkey[i].value;
					input_report_key(input, data->pdata->touchkey[i].keycode,
						key_state != 0 ? KEY_PRESS : KEY_RELEASE);
					input_sync(input);
					input_info(true, &data->client->dev, "[TSP_KEY] %s %s\n",
						data->pdata->touchkey[i].name , key_state != 0 ? "P" : "R");
				}
			}
			input_sync(input);
		} else {
			/* recent key check*/
			if (change_state & recent_flag) {
				key_state_recent = input_message & recent_flag;
				code_recent = KEY_RECENT;
			}

			/* back key check*/
			if (change_state & back_falg) {
				key_state_back = input_message & back_falg;
				code_back = KEY_BACK;
			}

			if (code_recent || code_back) {
				if(code_recent) {
					input_report_key(input, code_recent, !!key_state_recent);
					input_info(true, &data->client->dev, "[TSP_KEY] RECENT[%d] %s\n", code_recent, !!key_state_recent ? "P" : "R");
				}
				if(code_back) {
					input_report_key(input, code_back, !!key_state_back);
					input_info(true, &data->client->dev, "[TSP_KEY] BACK[%d] %s\n", code_back, !!key_state_back ? "P" : "R");
				}
				input_sync(input);
			}
		}
	} else {
		mxt_release_all_keys(data);
	}

	data->tsp_keystatus = input_message;

	return;
}
#endif

static void mxt_treat_T6_object(struct mxt_data *data,
		struct mxt_message *message)
{
	/* Normal mode */
	if (message->message[0] == 0x00) {
		input_info(true, &data->client->dev, "Normal mode\n");
	}
	/* I2C checksum error */
	if (message->message[0] & 0x04)
		input_err(true, &data->client->dev, "I2C checksum error\n");
	/* Config error */
	if (message->message[0] & 0x08)
		input_err(true, &data->client->dev, "Config error\n");
	/* Calibration */
	if (message->message[0] & 0x10) {
		input_info(true, &data->client->dev, "Calibration is on going !!\n");
	}
	/* Signal error */
	if (message->message[0] & 0x20)
		input_err(true, &data->client->dev, "Signal error\n");
	/* Overflow */
	if (message->message[0] & 0x40)
		input_err(true, &data->client->dev, "Overflow detected\n");
	/* Reset */
	if (message->message[0] & 0x80) {
		input_info(true, &data->client->dev, "Reset is ongoing\n");

		mxt_release_all_finger(data);
#if ENABLE_TOUCH_KEY
		mxt_release_all_keys(data);
#endif
	}
}

static void mxt_treat_T42_object(struct mxt_data *data,
		struct mxt_message *message)
{
	input_info(true, &data->client->dev, "%s\n", __func__);

	if (message->message[0] & 0x01) {
		/* Palm Press */
		input_info(true, &data->client->dev, "palm touch detected\n");
	} else {
		/* Palm release */
		input_info(true, &data->client->dev, "palm touch released\n");
	}
}

static void mxt_treat_T57_object(struct mxt_data *data,
		struct mxt_message *message)
{
}

static void mxt_treat_T100_object(struct mxt_data *data,
		struct mxt_message *message)
{
	u8 id, index;
	u8 *msg = message->message;
	u8 touch_type = 0, touch_event = 0, touch_detect = 0;

	index = data->reportids[message->reportid].index;

	/* Treate screen messages */
	if (index < MXT_T100_SCREEN_MESSAGE_NUM_RPT_ID) {
		if (index == MXT_T100_SCREEN_MSG_FIRST_RPT_ID)
			/* TODO: Need to be implemeted after fixed protocol
			 * This messages will indicate TCHAREA, ATCHAREA
			 */
			input_dbg(true, &data->client->dev, "SCRSTATUS:[%02X] %02X %04X %04X %04X\n",
				 msg[0], msg[1], (msg[3] << 8) | msg[2],
				 (msg[5] << 8) | msg[4],
				 (msg[7] << 8) | msg[6]);
		return;
	}

	/* Treate touch status messages */
	id = index - MXT_T100_SCREEN_MESSAGE_NUM_RPT_ID;
	touch_detect = msg[0] >> MXT_T100_DETECT_MSG_MASK;
	touch_type = (msg[0] & 0x70) >> 4;
	touch_event = msg[0] & 0x0F;

	input_dbg(false, &data->client->dev, "TCHSTATUS [%d] : DETECT[%d] TYPE[%d] EVENT[%d] %d,%d,%d,%d,%d\n",
		id, touch_detect, touch_type, touch_event,
		msg[1] | (msg[2] << 8),	msg[3] | (msg[4] << 8),
		msg[5], msg[6], msg[7]);

#ifdef USE_T100_MULTI_SLOT
	if (data->finger_mask & (1U << id))
		mxt_report_input_data(data);
#endif
	switch (touch_type)	{
	case MXT_T100_TYPE_FINGER:
	case MXT_T100_TYPE_PASSIVE_STYLUS:
	case MXT_T100_TYPE_HOVERING_FINGER:
	case MXT_T100_TYPE_GLOVE:
	case MXT_T100_TYPE_LARGE:

#if TSP_USE_PALM_FLAG
		if(touch_type==MXT_T100_TYPE_LARGE){
			data->palm =1;
		}
		else{
			data->palm =0;
		}
#endif
		/* There are no touch on the screen */
		if (!touch_detect) {
			if (touch_event == MXT_T100_EVENT_UP
				|| touch_event == MXT_T100_EVENT_SUPPESS) {

				data->fingers[id].z = 0;
				data->fingers[id].n = 0;
				data->fingers[id].m = 0;
				data->fingers[id].state = MXT_STATE_RELEASE;
				data->fingers[id].type = touch_type;
				data->fingers[id].event = touch_event;
#if TSP_USE_PALM_FLAG
				data->palm = 0;
#endif
				mxt_report_input_data(data);
			} else {
				input_err(true, &data->client->dev, "Untreated Undetectd touch : type[%d], event[%d]\n",
					touch_type, touch_event);
			}
			break;
		}

		/* There are touch on the screen */
		if (touch_event == MXT_T100_EVENT_DOWN
			|| touch_event == MXT_T100_EVENT_UNSUPPRESS
			|| touch_event == MXT_T100_EVENT_MOVE
			|| touch_event == MXT_T100_EVENT_NONE) {

			data->fingers[id].x = msg[1] | (msg[2] << 8);
			data->fingers[id].y = msg[3] | (msg[4] << 8);

			if (data->pdata->x_y_chnage) {
				data->fingers[id].x = data->pdata->max_x - data->fingers[id].x;
				data->fingers[id].y = data->pdata->max_y - data->fingers[id].y;
			}

			/* AUXDATA[n]'s order is depended on which values are
			 * enabled or not.
			 */
#ifdef REPORT_2D_Z
			data->fingers[id].z = msg[5];
#endif
			data->fingers[id].n = min(msg[6],msg[7]);
			data->fingers[id].m = max(msg[6],msg[7]);

			if (touch_type == MXT_T100_TYPE_HOVERING_FINGER) {
				data->fingers[id].n = 0;
				data->fingers[id].m = 0;
				data->fingers[id].z = 0;
			}

			if (touch_event == MXT_T100_EVENT_DOWN
				|| touch_event == MXT_T100_EVENT_UNSUPPRESS) {
				data->fingers[id].state = MXT_STATE_PRESS;
				data->fingers[id].mcount = 0;
			} else {
				data->fingers[id].state = MXT_STATE_MOVE;
				data->fingers[id].mcount += 1;
			}
			data->fingers[id].type = touch_type;
			data->fingers[id].event = touch_event;

#ifdef USE_T100_MULTI_SLOT
			data->finger_mask |= 1U << id;
#else
			mxt_report_input_data(data);
#endif

		} else {
			input_err(true, &data->client->dev, "Untreated Detectd touch : type[%d], event[%d]\n",
				touch_type, touch_event);
		}
		break;
	case MXT_T100_TYPE_ACTIVE_STYLUS:
		break;
	}
}

static irqreturn_t mxt_irq_thread(int irq, void *ptr)
{
	struct mxt_data *data = ptr;
	const struct mxt_platform_data *pdata = data->pdata;
	struct mxt_message message;
	struct device *dev = &data->client->dev;
	u8 reportid = 0, type = 0;
	int retry = 3;

	do {
		if (mxt_read_message(data, &message)) {
			input_err(true, dev, "Failed to read message\n");
			if (0 == retry)
				goto end;
			--retry;
		} else {
#if TSP_USE_ATMELDBG
			if (data->atmeldbg.display_log) {
				print_hex_dump(KERN_INFO, "MXT MSG:",
					DUMP_PREFIX_NONE, 16, 1,
					&message,
					sizeof(struct mxt_message), false);
			}
#endif
			reportid = message.reportid;

			if (reportid > data->max_reportid)
				goto end;

			type = data->reportids[reportid].type;

			switch (type) {
			case MXT_RESERVED_T0:
				goto end;
				break;
			case MXT_GEN_COMMANDPROCESSOR_T6:
				mxt_treat_T6_object(data, &message);
				break;
#if ENABLE_TOUCH_KEY
			case MXT_TOUCH_KEYARRAY_T15:
				mxt_treat_T15_object(data, &message);
				break;
#endif
			case MXT_SPT_SELFTEST_T25:
				input_err(true, dev, "Self test fail [0x%x 0x%x 0x%x 0x%x]\n",
					message.message[0], message.message[1],
					message.message[2], message.message[3]);
				break;
			case MXT_PROCI_TOUCHSUPPRESSION_T42:
				mxt_treat_T42_object(data, &message);
				break;
			case MXT_PROCI_EXTRATOUCHSCREENDATA_T57:
				mxt_treat_T57_object(data, &message);
				break;
			case MXT_PROCG_NOISESUPPRESSION_T62:
				break;
			case MXT_TOUCH_MULTITOUCHSCREEN_T100:
				mxt_treat_T100_object(data, &message);
				break;
			default:
				input_dbg(false, dev, "Untreated Object type[%d]\tmessage[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]\n",
					type, message.message[0],
					message.message[1], message.message[2],
					message.message[3], message.message[4],
					message.message[5], message.message[6]);
				break;
			}
#if TSP_PATCH
			mxt_patch_message(data, &message);
#endif
		}
	} while (!gpio_get_value(pdata->gpio_irq));

	if (data->finger_mask)
		mxt_report_input_data(data);
end:
	return IRQ_HANDLED;
}

static int mxt_get_bootloader_version(struct i2c_client *client, u8 val)
{
	u8 buf[3];

	if (val & MXT_BOOT_EXTENDED_ID) {
		if (i2c_master_recv(client, buf, sizeof(buf)) != sizeof(buf)) {
			input_err(true, &client->dev, "%s: i2c recv failed\n",
				 __func__);
			return -EIO;
		}
		input_info(true, &client->dev, "Bootloader ID:%d Version:%d",
			 buf[1], buf[2]);
	} else {
		input_info(true, &client->dev, "Bootloader ID:%d",
			 val & MXT_BOOT_ID_MASK);
	}
	return 0;
}

static int mxt_check_bootloader(struct i2c_client *client,
				unsigned int state)
{
	u8 val;

recheck:
	if (i2c_master_recv(client, &val, 1) != 1) {
		input_err(true, &client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	switch (state) {
	case MXT_WAITING_BOOTLOAD_CMD:
		if (mxt_get_bootloader_version(client, val))
			return -EIO;
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_WAITING_FRAME_DATA:
	case MXT_APP_CRC_FAIL:
		val &= ~MXT_BOOT_STATUS_MASK;
		break;
	case MXT_FRAME_CRC_PASS:
		if (val == MXT_FRAME_CRC_CHECK)
			goto recheck;
		if (val == MXT_FRAME_CRC_FAIL) {
			input_err(true, &client->dev, "Bootloader CRC fail\n");
			return -EINVAL;
		}
		break;
	default:
		return -EINVAL;
	}

	if (val != state) {
		input_err(true, &client->dev,
			 "Invalid bootloader mode state 0x%X\n", val);
		return -EINVAL;
	}

	return 0;
}

static int mxt_unlock_bootloader(struct i2c_client *client)
{
	u8 buf[2] = {MXT_UNLOCK_CMD_LSB, MXT_UNLOCK_CMD_MSB};

	if (i2c_master_send(client, buf, 2) != 2) {
		input_err(true, &client->dev, "%s: i2c send failed\n", __func__);

		return -EIO;
	}

	return 0;
}

static int mxt_probe_bootloader(struct i2c_client *client)
{
	u8 val;

	if (i2c_master_recv(client, &val, 1) != 1) {
		input_err(true, &client->dev, "%s: i2c recv failed\n", __func__);
		return -EIO;
	}

	if (val & (~MXT_BOOT_STATUS_MASK)) {
		if (val & MXT_APP_CRC_FAIL)
			input_err(true, &client->dev, "Application CRC failure\n");
		else
			input_err(true, &client->dev, "Device in bootloader mode\n");
	} else {
		input_err(true, &client->dev, "%s: Unknow status\n", __func__);
		return -EIO;
	}
	return 0;
}

static int mxt_fw_write(struct i2c_client *client,
				const u8 *frame_data, unsigned int frame_size)
{
	if (i2c_master_send(client, frame_data, frame_size) != frame_size) {
		input_err(true, &client->dev, "%s: i2c send failed\n", __func__);
		return -EIO;
	}

	return 0;
}

int mxt_verify_fw(struct mxt_fw_info *fw_info, const struct firmware *fw)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	struct mxt_fw_image *fw_img;

	if (!fw) {
		input_err(true, dev, "could not find firmware file\n");
		return -ENOENT;
	}

	fw_img = (struct mxt_fw_image *)fw->data;

	if (le32_to_cpu(fw_img->magic_code) != MXT_FW_MAGIC) {
		/* In case, firmware file only consist of firmware */
		input_dbg(true, dev, "Firmware file only consist of raw firmware\n");
		fw_info->fw_len = fw->size;
		fw_info->fw_raw_data = fw->data;
	} else {
		/*
		 * In case, firmware file consist of header,
		 * configuration, firmware.
		 */
		input_info(true, dev, "Firmware file consist of header, configuration, firmware\n");
		fw_info->fw_ver = fw_img->fw_ver;
		fw_info->build_ver = fw_img->build_ver;
		fw_info->hdr_len = le32_to_cpu(fw_img->hdr_len);
		fw_info->cfg_len = le32_to_cpu(fw_img->cfg_len);
		fw_info->fw_len = le32_to_cpu(fw_img->fw_len);
		fw_info->cfg_crc = le32_to_cpu(fw_img->cfg_crc);

		/* Check the firmware file with header */
		if (fw_info->hdr_len != sizeof(struct mxt_fw_image)
			|| fw_info->hdr_len + fw_info->cfg_len
				+ fw_info->fw_len != fw->size) {
#if TSP_PATCH
			struct patch_header* ppheader;
			u32 ppos = fw_info->hdr_len + fw_info->cfg_len + fw_info->fw_len;
			ppheader = (struct patch_header*)(fw->data + ppos);
			if(ppheader->magic == MXT_PATCH_MAGIC){
				input_info(true, dev, "Firmware file has patch size: %d\n", ppheader->size);
				if(ppheader->size){
					u8* patch=NULL;
					if(!data->patch.patch){
						kfree(data->patch.patch);
					}
					patch = kzalloc(ppheader->size, GFP_KERNEL);
					memcpy(patch, (u8*)ppheader, ppheader->size);
					data->patch.patch = patch;
				}
			}
			else
#endif
			{
				input_err(true, dev, "Firmware file is invaild !!hdr size[%d] "
						"cfg,fw size[%d,%d] filesize[%d]\n",
						fw_info->hdr_len, fw_info->cfg_len,
						fw_info->fw_len, (int)fw->size);
				return -EINVAL;
			}
		}

		if (!fw_info->cfg_len) {
			input_err(true, dev, "Firmware file dose not include configuration data\n");
			return -EINVAL;
		}
		if (!fw_info->fw_len) {
			input_err(true, dev, "Firmware file dose not include raw firmware data\n");
			return -EINVAL;
		}

		/* Get the address of configuration data */
		fw_info->cfg_raw_data = fw_img->data;

		/* Get the address of firmware data */
		fw_info->fw_raw_data = fw_img->data + fw_info->cfg_len;

#if TSP_SEC_FACTORY
		data->fdata->fw_ver = fw_info->fw_ver;
		data->fdata->build_ver = fw_info->build_ver;
#endif
	}

	return 0;
}

static int mxt_wait_for_chg(struct mxt_data *data, u16 time)
{
	int timeout_counter = 0;
	const struct mxt_platform_data *pdata = data->pdata;

	msleep(time);

	if (gpio_get_value(pdata->gpio_irq)) {
		while (gpio_get_value(pdata->gpio_irq)
			&& timeout_counter++ <= 20) {

			msleep(MXT_RESET_INTEVAL_TIME);
			input_err(true, &data->client->dev, "Spend %d th\n",
				(MXT_RESET_INTEVAL_TIME * timeout_counter)
				 + time);
		}
	}

	return 0;
}

static int mxt_command_reset(struct mxt_data *data, u8 value)
{
	int error;

	mxt_write_object(data, MXT_GEN_COMMANDPROCESSOR_T6,
			MXT_COMMAND_RESET, value);

	error = mxt_wait_for_chg(data, MXT_SW_RESET_TIME);
	if (error)
		input_err(true, &data->client->dev, "Not respond after reset command[%d]\n",
			value);

	return error;
}

static int mxt_command_backup(struct mxt_data *data, u8 value)
{
	mxt_write_object(data, MXT_GEN_COMMANDPROCESSOR_T6,
			MXT_COMMAND_BACKUPNV, value);

	msleep(MXT_BACKUP_TIME);

	return 0;
}

static int mxt_flash_fw(struct mxt_fw_info *fw_info)
{
	struct mxt_data *data = fw_info->data;
	struct i2c_client *client = data->client_boot;
	struct device *dev = &data->client->dev;
	const u8 *fw_data = fw_info->fw_raw_data;
	size_t fw_size = fw_info->fw_len;
	unsigned int frame_size;
	unsigned int pos = 0;
	int ret;
	input_info(true, dev, "%s Enter! :fw_size[%d]\n", __func__, (int)fw_size);

	if (!fw_data) {
		input_err(true, dev, "firmware data is Null\n");
		return -ENOMEM;
	}

	ret = mxt_check_bootloader(client, MXT_WAITING_BOOTLOAD_CMD);
	if (ret) {
		/*may still be unlocked from previous update attempt */
		ret = mxt_check_bootloader(client, MXT_WAITING_FRAME_DATA);
		if (ret)
			goto out;
	} else {
		input_info(true, dev, "Unlocking bootloader\n");
		/* Unlock bootloader */
		mxt_unlock_bootloader(client);
	}

	while (pos < fw_size) {
		ret = mxt_check_bootloader(client, MXT_WAITING_FRAME_DATA);
		if (ret) {
			input_err(true, dev, "Fail updating firmware. wating_frame_data err\n");
			goto out;
		}

		frame_size = ((*(fw_data + pos) << 8) | *(fw_data + pos + 1));

		/*
		* We should add 2 at frame size as the the firmware data is not
		* included the CRC bytes.
		*/

		frame_size += 2;

		/* Write one frame to device */
		mxt_fw_write(client, fw_data + pos, frame_size);

		ret = mxt_check_bootloader(client, MXT_FRAME_CRC_PASS);
		if (ret) {
			input_err(true, dev, "Fail updating firmware. frame_crc err\n");
			goto out;
		}

		pos += frame_size;

		input_dbg(false, dev, "Updated %d bytes / %zd bytes\n", pos, fw_size);

		msleep(20);
	}

	ret = mxt_wait_for_chg(data, MXT_SW_RESET_TIME);
	if (ret) {
		input_err(true, dev, "Not respond after F/W finish reset\n");
		goto out;
	}

	input_info(true, dev, "success updating firmware\n");
out:
	return ret;
}
#if 0
static void mxt_handle_T62_object(struct mxt_data *data)
{
	int ret;
	u8 value;

	ret = mxt_read_object(data, MXT_PROCG_NOISESUPPRESSION_T62, 0, &value);
	if (ret) {
		input_err(true, &data->client->dev, "%s: failed to read T62 object.\n",
				__func__);
		return;
	}

	value &= ~(0x02);

	ret = mxt_write_object(data, MXT_PROCG_NOISESUPPRESSION_T62,
						0, value);
	if (ret)
		input_err(true, &data->client->dev, "%s: failed to write T62 object.\n",
				__func__);
	else
		input_err(true, &data->client->dev, "%s: Setting T62 report disable.\n",
				__func__);
}
static void mxt_handle_init_data(struct mxt_data *data)
{
/*
 * Caution : This function is called before backup NV. So If you write
 * register vaules directly without config file in this function, it can
 * be a cause of that configuration CRC mismatch or unintended values are
 * stored in Non-volatile memory in IC. So I would recommed do not use
 * this function except for bring up case. Please keep this in your mind.
 */

/* disable T62 report bit. */
	mxt_handle_T62_object(data);

	return;
}
#endif
static int mxt_read_id_info(struct mxt_data *data)
{
	int ret = 0;
	u8 id[MXT_INFOMATION_BLOCK_SIZE];

	/* Read IC information */
	ret = mxt_read_mem(data, 0, MXT_INFOMATION_BLOCK_SIZE, id);
	if (ret) {
		input_err(true, &data->client->dev, "Read fail. IC information\n");
		goto out;
	} else {
		input_info(true, &data->client->dev,
			"family: 0x%x variant: 0x%x version: 0x%x"
			" build: 0x%x matrix X,Y size:  %d,%d"
			" number of obect: %d\n"
			, id[0], id[1], id[2], id[3], id[4], id[5], id[6]);
		data->info.family_id = id[0];
		data->info.variant_id = id[1];
		data->info.version = id[2];
		data->info.build = id[3];
		data->info.matrix_xsize = id[4];
		data->info.matrix_ysize = id[5];
		data->info.object_num = id[6];
	}

out:
	return ret;
}

static int mxt_get_object_table(struct mxt_data *data)
{
	int error;
	int i;
	u16 reg;
	u8 reportid = 0;
	u8 buf[MXT_OBJECT_TABLE_ELEMENT_SIZE];

	input_info(true, &data->client->dev, "%s called!\n", __func__);

	for (i = 0; i < data->info.object_num; i++) {
		struct mxt_object *object = data->objects + i;

		reg = MXT_OBJECT_TABLE_START_ADDRESS +
				MXT_OBJECT_TABLE_ELEMENT_SIZE * i;
		error = mxt_read_mem(data, reg,
				MXT_OBJECT_TABLE_ELEMENT_SIZE, buf);
		if (error)
			return error;

		object->type = buf[0];
		object->start_address = (buf[2] << 8) | buf[1];
		/* the real size of object is buf[3]+1 */
		object->size = buf[3] + 1;
		/* the real instances of object is buf[4]+1 */
		object->instances = buf[4] + 1;
		object->num_report_ids = buf[5];

		input_dbg(false, &data->client->dev,
			"Object:T%d\t\t\t Address:0x%x\tSize:%d\tInstance:%d\tReport Id's:%d\n",
			object->type, object->start_address, object->size,
			object->instances, object->num_report_ids);

		if (object->num_report_ids) {
			reportid += object->num_report_ids * object->instances;
			object->max_reportid = reportid;
		}
	}

	/* Store maximum reportid */
	data->max_reportid = reportid;
	input_info(true, &data->client->dev, "maXTouch: %d report ID\n",
			data->max_reportid);

	return 0;
}

static void mxt_make_reportid_table(struct mxt_data *data)
{
	struct mxt_object *objects = data->objects;
	struct mxt_reportid *reportids = data->reportids;
	int i, j;
	int id = 0;

	for (i = 0; i < data->info.object_num; i++) {
		for (j = 0; j < objects[i].num_report_ids *
				objects[i].instances; j++) {
			id++;

			reportids[id].type = objects[i].type;
			reportids[id].index = j;

			input_dbg(false, &data->client->dev, "Report_id[%d]:\tT%d\tIndex[%d]\n",
				id, reportids[id].type, reportids[id].index);
		}
	}
}

static int mxt_initialize(struct mxt_data *data)
{
	struct i2c_client *client = data->client;

	u32 read_info_crc, calc_info_crc;
	int ret;
	input_dbg(true, &client->dev, "%s\n", __func__);

	ret = mxt_read_id_info(data);
	if (ret)
		return ret;

	data->objects = kcalloc(data->info.object_num,
				sizeof(struct mxt_object),
				GFP_KERNEL);
	if (!data->objects) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Get object table infomation */
	ret = mxt_get_object_table(data);
	if (ret)
		goto out;

	data->reportids = kcalloc(data->max_reportid + 1,
			sizeof(struct mxt_reportid),
			GFP_KERNEL);
	if (!data->reportids) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto out;
	}

	/* Make report id table */
	mxt_make_reportid_table(data);

	/* Verify the info CRC */
	ret = mxt_read_info_crc(data, &read_info_crc);
	if (ret)
		goto out;

	ret = mxt_calculate_infoblock_crc(data, &calc_info_crc);
	if (ret)
		goto out;

	if (read_info_crc != calc_info_crc) {
		input_err(true, &data->client->dev, "Infomation CRC error :[CRC 0x%06X!=0x%06X]\n",
				read_info_crc, calc_info_crc);
		ret = -EFAULT;
		goto out;
	}
	return 0;

out:
	return ret;
}

static int mxt_check_config_crc(struct mxt_fw_info *fw_info)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	u32 current_crc;
	int ret = CONFIG_IS_MISMATCH;

	/* Get config CRC from device */
	ret = mxt_read_config_crc(data, &current_crc);
	if (ret) {
		input_err(true, dev, "%s : Fail to read config crc.\n", __func__);
		return CONFIG_IS_MISMATCH;
	}

	/* Check config CRC */
	if (current_crc == fw_info->cfg_crc) {
		input_info(true, dev, "Config is Same:[CRC 0x%06X]\n",
			current_crc);

		ret = CONFIG_IS_SAME;
	} else if ((current_crc != fw_info->cfg_crc) && fw_info->cfg_crc != 0) {
		input_info(true, dev, "Config is Mismatch:[CRC IC:0x%06X != BIN:0x%06X]\n",
			current_crc, fw_info->cfg_crc);

		ret = CONFIG_IS_MISMATCH;
	} else {
		input_info(true, dev, "Config is Empty:[CRC IC:0x%06X != BIN:0x%06X]\n",
			current_crc, fw_info->cfg_crc);

		ret = CONFIG_IS_EMPTY;
	}

	return ret;
}

static int mxt_rest_initialize(struct mxt_fw_info *fw_info)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	int ret = 0;
	input_dbg(true, &data->client->dev, "[TSP] %s\n", __func__);

	if (mxt_check_config_crc(fw_info) == CONFIG_IS_MISMATCH) {
		/* Restore memory and stop event handing */
		ret = mxt_command_backup(data, MXT_DISALEEVT_VALUE);
		if (ret) {
			input_err(true, dev, "Failed Restore NV and stop event\n");
			goto out;
		}

		/* Write config */
		ret = mxt_write_config(fw_info);
		if (ret) {
			input_err(true, dev, "Failed to write config from file\n");
			goto out;
		}
#if 0
	/* Handle data for init */
	mxt_handle_init_data(data);
#endif
		/* Backup to memory */
		ret = mxt_command_backup(data, MXT_BACKUP_VALUE);
		if (ret) {
			input_err(true, dev, "Failed backup NV data\n");
			goto out;
		}

		/* Soft reset */
		ret = mxt_command_reset(data, MXT_RESET_VALUE);
		if (ret) {
			input_err(true, dev, "Failed Reset IC\n");
			goto out;
		}
	}

#if TSP_PATCH
	if(data->patch.patch)
		ret = mxt_patch_init(data, data->patch.patch);
	else
		input_info(true, dev, "Firmware file does not have a patch.\n");
#endif

out:
	return ret;
}

static int mxt_power_on(struct mxt_data *data)
{
/*
 * If do not turn off the power during suspend, you can use deep sleep
 * or disable scan to use T7, T9 Object. But to turn on/off the power
 * is better.
 */
	int error = 0;

	input_dbg(true, &data->client->dev, "[TSP] %s(%d)\n",
					__func__, data->mxt_enabled);

	if (data->mxt_enabled)
		return 0;

	if (!data->pdata->power_ctrl) {
		input_err(true, &data->client->dev, "Power on function is not defined\n");
		error = -EINVAL;
		goto out;
	}

	error = data->pdata->power_ctrl(data, true);
	if (error) {
		input_err(true, &data->client->dev, "Failed to power on\n");
		goto out;
	}

	data->mxt_enabled = true;

out:
	return error;
}

static int mxt_power_off(struct mxt_data *data)
{
	int error = 0;

	input_dbg(true, &data->client->dev, "[TSP] %s(%d)\n",
					__func__, data->mxt_enabled);

	if (!data->mxt_enabled)
		return 0;

	if (!data->pdata->power_ctrl) {
		input_info(true, &data->client->dev, "Power off function is not defined\n");
		error = -EINVAL;
		goto out;
	}

	error = data->pdata->power_ctrl(data, false);
	if (error) {
		input_err(true, &data->client->dev, "Failed to power off\n");
		goto out;
	}

	data->mxt_enabled = false;

out:
	return error;
}

/* Need to be called by function that is blocked with mutex */
static int mxt_start(struct mxt_data *data)
{
	int error = 0;
	input_dbg(true, &data->client->dev, "%s\n", __func__);

	if (data->mxt_enabled) {
		input_err(true, &data->client->dev,
			"%s. but touch already on\n", __func__);
		return error;
	}

	error = mxt_power_on(data);
	if (error)
		input_err(true, &data->client->dev, "Fail to start touch\n");
	else
		enable_irq(data->client->irq);

	return error;
}

/* Need to be called by function that is blocked with mutex */
static int mxt_stop(struct mxt_data *data)
{
	int error = 0;
	input_dbg(true, &data->client->dev, "%s\n", __func__);

	if (!data->mxt_enabled) {
		input_err(true, &data->client->dev,
			"%s. but touch already off\n", __func__);
		return error;
	}
	disable_irq(data->client->irq);

	error = mxt_power_off(data);
	if (error) {
		input_err(true, &data->client->dev, "Fail to stop touch\n");
		goto err_power_off;
	}
	mxt_release_all_finger(data);

#if ENABLE_TOUCH_KEY
	mxt_release_all_keys(data);
#endif
	return 0;

err_power_off:
	enable_irq(data->client->irq);
	return error;
}

static int mxt_input_open(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	int ret;
	input_info(true, &data->client->dev, "%s\n", __func__);

	ret = mxt_start(data);
	if (ret) {
		input_info(true, &data->client->dev, "%s fail\n", __func__);
		return ret;
	}

	return 0;
}

static void mxt_input_close(struct input_dev *dev)
{
	struct mxt_data *data = input_get_drvdata(dev);
	input_info(true, &data->client->dev, "[TSP] %s\n", __func__);

	mxt_stop(data);
}

#if FOR_BRINGUP
static int mxt_make_highchg(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_message message;
	int count = data->max_reportid * 2;
	int error;

	/* Read dummy message to make high CHG pin */
	do {
		error = mxt_read_message(data, &message);
		if (error)
			return error;
	} while (message.reportid != 0xff && --count);

	if (!count) {
		input_err(true, dev, "CHG pin isn't cleared\n");
		return -EBUSY;
	}

	return 0;
}
#endif

static int mxt_touch_finish_init(struct mxt_data *data)
{
	struct i2c_client *client = data->client;

	input_dbg(true, &client->dev, "%s\n", __func__);

	/*
	* to prevent unnecessary report of touch event
	* it will be enabled in open function
	*/
	//mxt_stop(data);
#if FOR_BRINGUP
	error = mxt_make_highchg(data);
	if (error) {
		input_err(true, &client->dev, "Failed to clear CHG pin\n");
		goto err_req_irq;
	}
#endif
	input_info(true, &client->dev, "Mxt touch controller initialized\n");

	return 0;
}

static int mxt_enter_bootloader(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	int error;

	input_dbg(true, dev, "%s enter!!\n", __func__);

	data->objects = kcalloc(data->info.object_num,
			sizeof(struct mxt_object),
			GFP_KERNEL);
	if (!data->objects) {
		input_err(true, dev, "%s Failed to allocate memory\n",
			__func__);
		error = -ENOMEM;
		goto out;
	}

	/* Get object table information*/
	error = mxt_get_object_table(data);
	if (error)
		goto err_free_mem;

	/* Change to the bootloader mode */
	error = mxt_command_reset(data, MXT_BOOT_VALUE);
	if (error)
		goto err_free_mem;

err_free_mem:
	input_err(true, dev, "%s err_free_mem\n", __func__);
	kfree(data->objects);
	data->objects = NULL;

out:
	return error;
}

static void mxt_print_ic_version(struct mxt_data *data)
{
	struct i2c_client *client = data->client;
	struct mxt_object *user_object;

	u32 current_crc;
	int ret = 0;

	/* Get config CRC from device */
	ret = mxt_read_config_crc(data, &current_crc);
	if (ret) {
		input_err(true, &client->dev, "%s : Fail to read config crc.\n", __func__);
	}

	user_object = mxt_get_object(data, MXT_SPT_USERDATA_T38);
	if (!user_object) {
		input_err(true, &client->dev, "fail to get object_info\n");
		return;
	}
	mxt_read_mem(data, user_object->start_address, MXT_CONFIG_VERSION_LENGTH, data->config_date);

	input_info(true, &client->dev, "%s : TSP IC Ver : [0x%02X/0x%2X][%s][0x%X]\n",
		__func__, data->info.version, data->info.build, data->config_date, current_crc);
}

/* fw config update */
static int mxt_fw_config_update(struct mxt_fw_info *fw_info)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	int error;

	input_info(true, dev, "%s called!\n", __func__);

	/* mxt_initialize() : Check FW CRC value( read tsp ic value & caluator tsp ic value) */
	error = mxt_initialize(data);
	if (error) {
		input_err(true, dev, "Failed to initialize(TSP FW is not validate)\n");
		goto err_free_mem;
	}

	/* mxt_rest_initialize() : Check config CRC value and update */
	error = mxt_rest_initialize(fw_info);
	if (error) {
		input_err(true, dev, "Failed to rest initialize\n");
		goto err_free_mem;
	}
	return 0;

err_free_mem:
	if (data->objects != NULL) {
		kfree(data->objects);
		data->objects = NULL;
	}
	if (data->reportids != NULL) {
		kfree(data->reportids);
		data->reportids = NULL;
	}
	return error;
}


/* fw update (not config) */
static int mxt_fw_update(struct mxt_fw_info *fw_info, const struct firmware *fw)
{
	struct mxt_data *data = fw_info->data;
	struct device *dev = &data->client->dev;
	int error;

	input_info(true, dev, "%s called!\n", __func__);

	/* Read fw info & Read patch data */
	error = mxt_verify_fw(fw_info, fw);
	if (error) //SKIP firmware update during bringup..NEed to change once get new firmware
		return -1;

	/* Skip update on boot up if firmware file does not have a header */
	if (!fw_info->hdr_len){
		input_err(true, dev, "Firmware file does not have a header!\n");
		return -1;
	}

	/* Read TSP IC FW info(data->info) */
	error = mxt_read_id_info(data);
	if (error) {
		/* need to check IC is in boot mode */
		error = mxt_probe_bootloader(data->client_boot);
		if (error) {
			input_err(true, dev, "Failed to verify bootloader's status\n");
			return -1;
		}

		input_info(true, dev, "Updating firmware from boot-mode\n");
		goto load_fw;
	}

	input_info(true, dev, "TSP Firmware Info.: IC:0x%x,0x%x  KERNEL:0x%x,0x%x\n",
			data->info.version, data->info.build,
			fw_info->fw_ver, fw_info->build_ver);

	/* compare the version to verify necessity of firmware updating */
	if (data->info.version > fw_info->fw_ver){
		input_info(true, dev, "%s : F/W Ver on IC is higher than Kernel\n", __func__);
		return 0;
	}
	else if (data->info.version == fw_info->fw_ver){
		if(data->info.build >= fw_info->build_ver){
			input_info(true, dev, "%s : F/W Build Ver on IC is higher than Kernel\n", __func__);
			return 0;
		}
	}

	error = mxt_enter_bootloader(data);
	if (error) {
		input_err(true, dev, "Failed updating firmware\n");
		return -1;
	}

load_fw:
	input_info(true, dev, "%s : TSP Loading Firmware\n", __func__);
	error = mxt_flash_fw(fw_info);
	if (error)
		input_err(true, dev, "%s : Failed updating firmware\n", __func__);
	else
		input_dbg(true, dev, "%s : succeeded updating firmware\n", __func__);

	return 0;
}

#if 0 /* Disable check_signal_limit() for checking panel connector */
static int check_signal_limit(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	struct mxt_message message;
	struct mxt_object *object;
	int error;
	int retry_cnt = 2;

	if (data->objects == NULL){
		input_info(true, dev, "%s object is NULL!\n", __func__);

		mxt_read_id_info(data);

		data->objects = kcalloc(data->info.object_num,
					sizeof(struct mxt_object), GFP_KERNEL);
		if (!data->objects) {
			input_err(true, dev, "%s : Failed to allocate memory\n",__func__);
		}

		/* Get object table infomation */
		mxt_get_object_table(data);
	}

	object = mxt_get_object(data, MXT_GEN_COMMANDPROCESSOR_T6);
	if (!object)
		return -EIO;

	/* send calibration command to the chip */
	mxt_write_object(data, MXT_GEN_COMMANDPROCESSOR_T6,
			MXT_COMMAND_CALIBRATE, 1);

	do {
		error = mxt_read_message_reportid(data, &message, object->max_reportid);
		if (error) {
			input_err(true, dev, "%s : Failed to retrieve CRC message[0x%x], error[%d]\n",
									__func__, message.message[0], error);
			return error;
		}
		input_info(true, dev, "%s [%d]: message[0]=[0x%x]\n",
									__func__, retry_cnt, message.message[0]);

		if (message.message[0] & 0x10) {
			break;
		}
	} while(retry_cnt--);

	/* Check calibration is done */
	if (message.message[0] & 0x10) {
		object = mxt_get_object(data, MXT_SPT_SELFTEST_T25);

		if (!object)
			return -EIO;

		/* Enable T25 Self test feature */
		mxt_write_object(data, MXT_SPT_SELFTEST_T25, 0, 0x03);

		/* Run the signal limit test */
		mxt_write_object(data, MXT_SPT_SELFTEST_T25, 1, 0x17);

		/* Read message from self test result, which only has one report ID */
		error = mxt_read_message_reportid(data, &message, object->max_reportid);
		if (error) {
			input_err(true, dev, "Failed to received t25 message\n");
			return 1;
		}

		if(message.message[0] == 0x17) {
			input_info(true, dev, "%s : T25 signal limit failed & panel is off!\n", __func__);
			return 1;
		} else if (message.message[0] == 0xFE) {
			input_info(true, dev, "%s : T25 all tests passed!\n", __func__);
			return 0;
		} else {
			input_err(true, dev, "%s : Abnormal T25 return value[0x%x]\n",
					__func__, message.message[0]);
			return 1;
		}
	}

	input_err(true, dev, "%s : Fail to check!\n", __func__);
	return 1;
}
#endif

static int mxt_fw_update_on_probe(struct mxt_data *data)
{
	struct device *dev = &data->client->dev;
	const struct firmware *fw = NULL;
	struct mxt_fw_info fw_info;
	int ret = 0;

	input_info(true, dev, "%s\n", __func__);

	memset(&fw_info, 0, sizeof(struct mxt_fw_info));
	fw_info.data = data;

#if 0 /* Disable check_signal_limit() for checking panel connector */
	/* Panel condition check(COB Type).                                            */
	/* Skip fw update when use Multi Panel by TSP ID pins & Panel is off. */
	ret = check_signal_limit(data);
	if (ret) {
		input_err(true, dev, "%s : TSP panel is off!\n", __func__);
		goto ts_rest_init;
	}
#endif

	if (data->pdata->firmware_name == NULL) {
		input_err(true, dev, "%s : tsp fw name is null, not update tsp fw!\n", __func__);
		goto ts_rest_init;
	}

	ret = request_firmware(&fw, data->pdata->firmware_name, dev);
	if (ret) {
		input_err(true, dev, "error requesting built-in firmware\n");
		goto ts_rest_init;
	}

	ret = mxt_fw_update(&fw_info, fw);
	if (ret) {
		input_err(true, dev, "Fail firmware update\n");
		goto ts_rest_init;
	}

ts_rest_init:
	ret = mxt_fw_config_update(&fw_info);
	if (ret) {
		input_err(true, dev, "Fail firmware config update\n");
		goto out;
	}

	mxt_touch_finish_init(data);

	mxt_print_ic_version(data);

out:

	if (fw != NULL)
		release_firmware(fw);
	return ret;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
#define mxt_suspend	NULL
#define mxt_resume	NULL

static void mxt_early_suspend(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data,
								early_suspend);
	mutex_lock(&data->input_dev->mutex);

	mxt_stop(data);

	mutex_unlock(&data->input_dev->mutex);
}

static void mxt_late_resume(struct early_suspend *h)
{
	struct mxt_data *data = container_of(h, struct mxt_data,
								early_suspend);
	mutex_lock(&data->input_dev->mutex);

	mxt_start(data);

	mutex_unlock(&data->input_dev->mutex);
}
#else
static int mxt_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&data->input_dev->mutex);

	if (input_dev->users)
		mxt_stop(data);

	mutex_unlock(&data->input_dev->mutex);
	return 0;
}

static int mxt_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct mxt_data *data = i2c_get_clientdata(client);
	struct input_dev *input_dev = data->input_dev;

	mutex_lock(&data->input_dev->mutex);

	if (input_dev->users)
		mxt_start(data);

	mutex_unlock(&data->input_dev->mutex);
	return 0;
}
#endif

static void mxt_shutdown(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

	input_info(true, &client->dev, "%s called!\n", __func__);

	mxt_stop(data);
#if ENABLE_TOUCH_KEY
//	data->pdata->led_power_ctrl(data, 0);
#endif
}

/* Added for samsung dependent codes such as Factory test,
 * Touch booster, Related debug sysfs.
 */
#if 1//FOR_BRINGUP
#include "mxtt_sec.c"
#endif

#if ENABLE_TOUCH_KEY
struct mxt_touchkey mxt_touchkey_data[] = {
	{
		.value = TOUCH_KEY_RECENT,
		.keycode = KEY_RECENT,
		.name = "recent",
		.xnode = 4,
		.ynode = 43,
		.deltaobj = 0,
	},
	{
		.value = TOUCH_KEY_BACK,
		.keycode = KEY_BACK,
		.name = "back",
		.xnode = 5,
		.ynode = 43,
		.deltaobj = 1,
	},
};
#endif


#ifdef CONFIG_OF
static int mxt_power_ctrl(void *ddata, bool on)
{
	struct mxt_data *info = (struct mxt_data *)ddata;
	struct device *dev = &info->client->dev;

	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;
	struct pinctrl_state *pinctrl_state;
	int retval = 0;

	input_info(true, dev, "%s %s(%d)\n", __func__,
					on ? "on" : "off", info->tsp_pwr_enabled);

	if (info->tsp_pwr_enabled == on)
		return retval;

	regulator_dvdd = regulator_get(NULL, info->pdata->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, info->pdata->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}

	regulator_avdd = regulator_get(NULL, info->pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, info->pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	if (on) {
		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "on_state");
		if (IS_ERR(pinctrl_state)) {
			input_err(true, dev, "%s: Failed to lookup pinctrl.\n", __func__);
		} else {
			retval = pinctrl_select_state(info->pinctrl, pinctrl_state);
			if (retval) {
				input_err(true, dev, "%s: Failed to configure pinctrl.\n", __func__);
				goto fail_pwr_ctrl;
			}
		}
		if (gpio_is_valid(info->pdata->gpio_reset)) {
			/* Use TSP RESET */
			input_info(true, dev, "%s: Use TSP RESET.\n", __func__);
			gpio_direction_output(info->pdata->gpio_reset, 0);

			retval = regulator_enable(regulator_dvdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
				goto fail_pwr_ctrl;
			}

			retval = regulator_enable(regulator_avdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
				goto fail_pwr_ctrl;
			}

			usleep_range(1000,1200);
			input_info(true, dev, "%s: set_direction for info->pdata->gpio_reset[%d]\n",
						__func__,info->pdata->gpio_reset);

			retval = gpio_direction_output(info->pdata->gpio_reset, 1);
			if (retval) {
				input_err(true, dev, "%s: unable to set_direction for info->pdata->gpio_reset[%d]\n",
							__func__,info->pdata->gpio_reset);
				goto fail_pwr_ctrl;
			}

		} else {
			/* Not use TSP RESET */
			input_info(true, dev, "%s: Not use TSP RESET.\n", __func__);
			retval = regulator_enable(regulator_avdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
				goto fail_pwr_ctrl;
			}
			usleep_range(1000,1200);
			retval = regulator_enable(regulator_dvdd);
			if (retval) {
				input_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
				goto fail_pwr_ctrl;
			}
		}

		msleep(MXT_HW_RESET_TIME);

	} else {
		if (gpio_is_valid(info->pdata->gpio_reset)) {
			retval = gpio_direction_output(info->pdata->gpio_reset, 0);
			if (retval) {
				input_err(true, dev, "%s: unable to set_direction for info->pdata->gpio_reset[%d]\n",
							__func__,info->pdata->gpio_reset);
				goto fail_pwr_ctrl;
			}
		}

		pinctrl_state = pinctrl_lookup_state(info->pinctrl, "off_state");
		if (IS_ERR(pinctrl_state)) {
			input_err(true, dev, "%s: Failed to lookup pinctrl.\n", __func__);
		} else {
			retval = pinctrl_select_state(info->pinctrl, pinctrl_state);
			if (retval) {
				input_err(true, dev, "%s: Failed to configure pinctrl.\n", __func__);
				goto fail_pwr_ctrl;
			}
		}

		if (regulator_is_enabled(regulator_dvdd))
			regulator_disable(regulator_dvdd);

		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);
	}

fail_pwr_ctrl:
	if(!retval)
		info->tsp_pwr_enabled = on;

	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}

static int mxt_parse_dt(struct i2c_client *client, struct mxt_platform_data *pdata)
{
	struct device *dev = &client->dev;
	struct device_node *np = dev->of_node;
	const char *model = NULL;
	u32 coords[4];
	int ret;

	/* irq gpio info */
	pdata->gpio_irq= of_get_named_gpio(np, "mxts,irq-gpio", 0);
	ret = gpio_request(pdata->gpio_irq, "mxts,irq-gpio");
	if (ret) {
		input_err(true, dev, "%s: unable to request tsp_int [%d]\n",
				__func__, pdata->gpio_irq);
		return ret;
	}

	/* rst gpio info */
	pdata->gpio_reset = of_get_named_gpio(np, "mxts,rst-gpio", 0);

	if (gpio_is_valid(pdata->gpio_reset)) {
		ret = gpio_request(pdata->gpio_reset, "mxts,rst");
		if (ret) {
			input_err(true, dev, "%s: unable to request tsp_rst [%d]\n",
					__func__, pdata->gpio_reset);
			return ret;
		}
	} else {
		input_info(true, dev, "%s: Not use tsp_rst pin.\n",	__func__);
	}

	ret = of_property_read_u32_array(np, "mxts,tsp_coord", coords, 4);
	if (ret && (ret != -EINVAL)) {
		input_err(true, dev, "%s: Unable to read mxts,tsp_coord\n", __func__);
		return ret;
	}

	pdata->num_xnode = coords[0];
	pdata->num_ynode = coords[1];
	pdata->max_x = coords[2];
	pdata->max_y = coords[3];

	ret = of_property_read_u32(np, "mxts,x_y_chnage", &pdata->x_y_chnage);
	input_info(true, dev, "%s: Read x_y_chnage flag [%d][%d]!\n",
							__func__, pdata->x_y_chnage, ret);

	ret = of_property_read_string(np, "mxts,pname", &model);
	if (ret && (ret != -EINVAL)) {
		input_err(true, dev, "%s: Unable to read mxts,pname\n", __func__);
	}
	pdata->model_name = model;

	if (of_property_read_string(np, "mxts,regulator_dvdd", &pdata->regulator_dvdd)) {
		input_err(true, dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}
	if (of_property_read_string(np, "mxts,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}
	pdata->power_ctrl = mxt_power_ctrl;

	of_property_read_string(np, "mxts,firmware_name", &pdata->firmware_name);
	input_info(true, dev, "%s : fw_name[%s]\n",	__func__, pdata->firmware_name);

#if ENABLE_TOUCH_KEY
	pdata->num_touchkey = ARRAY_SIZE(mxt_touchkey_data);
	pdata->touchkey = mxt_touchkey_data;
#endif

	input_info(true, dev, "%s : addr=[0X%02X], tsp_int[%d], rst[%d], xnode[%d], ynode[%d],"
							" max_x=[%d], max_y[%d], model_name[%s]\n",
			__func__, client->addr, pdata->gpio_irq, pdata->gpio_reset, pdata->num_xnode, pdata->num_ynode,
			pdata->max_x, pdata->max_y,	pdata->model_name);

	return 0;
}
#endif

static int mxt_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct mxt_platform_data *pdata = client->dev.platform_data;
	struct mxt_data *data;
	struct input_dev *input_dev;
	int error = 0;
#if ENABLE_TOUCH_KEY
	int i = 0;
#endif

#ifdef CONFIG_OF
	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev, sizeof(struct mxt_platform_data), GFP_KERNEL);
		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		error = mxt_parse_dt(client,pdata);
		if (error) {
			input_err(true, &client->dev, "Failed to parse dt\n");
			devm_kfree(&client->dev, pdata);
			return -EINVAL;
		}
	} else {
		pdata = client->dev.platform_data;
	}
#else
	pdata = client->dev.platform_data;
#endif
	if (!pdata) {
		input_err(true, &client->dev, "Platform data is not proper\n");
		return -EINVAL;
	}

	data = kzalloc(sizeof(struct mxt_data), GFP_KERNEL);
	if (!data) {
		input_err(true, &client->dev, "Failed to allocate memory\n");
		error = -ENOMEM;
		goto err_allocate_data;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		error = -ENOMEM;
		input_err(true, &client->dev, "Input device allocation failed\n");
		goto err_allocate_input_device;
	}

	input_dev->name = "sec_touchscreen";
	input_dev->id.bustype = BUS_I2C;
	input_dev->dev.parent = &client->dev;
	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;

	data->client = client;
	data->input_dev = input_dev;
	data->pdata = pdata;

	/* Get pinctrl if target uses pinctrl */
	data->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(data->pinctrl)) {
		if (PTR_ERR(data->pinctrl) == -EPROBE_DEFER) {
			input_info(true, &data->client->dev,"%s: Target does not use pinctrl\n", __func__);
			data->pinctrl = NULL;
		}
	}

	input_dev->open = mxt_input_open;
	input_dev->close = mxt_input_close;
	set_bit(EV_SYN, input_dev->evbit);
	set_bit(EV_ABS, input_dev->evbit);
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(BTN_TOUCH, input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, input_dev->keybit);

#if ENABLE_TOUCH_KEY
	for (i = 0 ; i < data->pdata->num_touchkey ; i++){
		set_bit(data->pdata->touchkey[i].keycode, input_dev->keybit);
	}
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
#endif

	input_mt_init_slots(input_dev, MXT_MAX_FINGER, INPUT_MT_DIRECT);
	input_set_abs_params(input_dev, ABS_MT_POSITION_X,
							0, pdata->max_x, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_POSITION_Y,
							0, pdata->max_y, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MAJOR,
							0, MXT_AREA_MAX, 0, 0);
	input_set_abs_params(input_dev, ABS_MT_TOUCH_MINOR,
							0, MXT_AREA_MIN, 0, 0);
#ifdef REPORT_2D_Z
	input_set_abs_params(input_dev, ABS_MT_PRESSURE,
							0, MXT_AMPLITUDE_MAX, 0, 0);
#endif
#if TSP_USE_PALM_FLAG
	input_set_abs_params(input_dev, ABS_MT_PALM,
							0, MXT_PALM_MAX, 0, 0);
#endif

	input_set_drvdata(input_dev, data);
	i2c_set_clientdata(client, data);

	if (client->addr == 0x4a) {
		data->client_boot = i2c_new_dummy(client->adapter, MXT_BOOT_ADDRESS);
	} else if (client->addr == 0x4b) {
		data->client_boot = i2c_new_dummy(client->adapter, MXT_BOOT_ADDRESS2);
	}
	if (!data->client_boot) {
		input_err(true, &client->dev, "Fail to register sub client[0x%x]\n",
			 MXT_BOOT_ADDRESS);
		error = -ENODEV;
		goto err_create_sub_client;
	}

	/* regist input device */
	error = input_register_device(input_dev);
	if (error)
		goto err_register_input_device;

	error = mxt_sysfs_init(client);
	if (error < 0) {
		input_err(true, &client->dev, "Failed to create sysfs\n");
		goto err_sysfs_init;
	}

	client->irq = gpio_to_irq(data->pdata->gpio_irq);
	input_info(true, &data->client->dev, "%s: tsp int gpio is %d : gpio_to_irq : %d\n",
			__func__, data->pdata->gpio_irq, client->irq);

	error = mxt_power_on(data);
	if (error) {
		input_err(true, &client->dev, "Failed to power_on\n");
		goto err_power_on;
	}

	error = mxt_fw_update_on_probe(data);
	if (error) {
		input_err(true, &client->dev, "Failed to init driver\n");
		goto err_touch_init;
	}

	error = request_threaded_irq(client->irq, NULL, mxt_irq_thread,
		IRQF_TRIGGER_LOW | IRQF_ONESHOT, client->dev.driver->name, data);

	if (error) {
		input_err(true, &client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	data->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	data->early_suspend.suspend = mxt_early_suspend;
	data->early_suspend.resume = mxt_late_resume;
	register_early_suspend(&data->early_suspend);
#endif

	return 0;

	free_irq(client->irq,data);
err_req_irq:
err_touch_init:
	mxt_power_off(data);
err_power_on:
	mxt_sysfs_remove(data);
err_sysfs_init:
	input_unregister_device(input_dev);
	input_dev = NULL;
err_register_input_device:
	i2c_unregister_device(data->client_boot);
err_create_sub_client:
	input_free_device(input_dev);
err_allocate_input_device:
	kfree(data);
err_allocate_data:
	if (client->dev.of_node) {
#if ENABLE_TOUCH_KEY
		devm_kfree(&client->dev, (void *)pdata->touchkey);
#endif
		devm_kfree(&client->dev, (void *)pdata);
	}

	return error;
}

static int mxt_remove(struct i2c_client *client)
{
	struct mxt_data *data = i2c_get_clientdata(client);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&data->early_suspend);
#endif
	free_irq(client->irq, data);
	kfree(data->objects);
	kfree(data->reportids);
	input_unregister_device(data->input_dev);
	i2c_unregister_device(data->client_boot);
	mxt_sysfs_remove(data);
	mxt_power_off(data);
	if (client->dev.of_node) {
#if ENABLE_TOUCH_KEY
		devm_kfree(&client->dev, (void *)data->pdata->touchkey);
#endif
		devm_kfree(&client->dev, (void *)data->pdata);
	}
	kfree(data);

	return 0;
}

static struct i2c_device_id mxt_idtable[] = {
	{MXT_DEV_NAME, 0},
};

MODULE_DEVICE_TABLE(i2c, mxt_idtable);

#ifdef CONFIG_OF
static struct of_device_id mxt_dt_ids[] = {
	{ .compatible = "atmel,mxt_t"},
	{ }
};
#endif

static const struct dev_pm_ops mxt_pm_ops = {
	.suspend = mxt_suspend,
	.resume = mxt_resume,
};

static struct i2c_driver mxt_i2c_driver = {
	.id_table = mxt_idtable,
	.probe = mxt_probe,
	.remove = mxt_remove,
	.shutdown = mxt_shutdown,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= MXT_DEV_NAME,
#ifdef CONFIG_PM
		.pm	= &mxt_pm_ops,
#endif
#ifdef CONFIG_OF
		.of_match_table = mxt_dt_ids,
#endif
	},
};

static int mxt_i2c_init(void)
{
	printk(KERN_INFO "%s %s\n", SECLOG, __func__);

	if (lpcharge == 1) {
		pr_err("%s %s : lpcharge mode!\n", SECLOG, __func__);
		return -ENODEV;
	}

	return i2c_add_driver(&mxt_i2c_driver);
}

static void mxt_i2c_exit(void)
{
	printk(KERN_INFO "%s %s\n", SECLOG, __func__);
	i2c_del_driver(&mxt_i2c_driver);
}

module_init(mxt_i2c_init);
module_exit(mxt_i2c_exit);

MODULE_DESCRIPTION("Atmel MaXTouch driver");
MODULE_AUTHOR("bumwoo.lee<bw365.lee@samsung.com>");
MODULE_LICENSE("GPL");
