/* abov_touchkey.c -- Linux driver for abov chip as touchkey
 *
 * Copyright (C) 2013 Samsung Electronics Co.Ltd
 * Author: Junkyeong Kim <jk0430.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License as published by the
 * Free Software Foundation; either version 2, or (at your option) any
 * later version.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <linux/i2c/abov_touchkey.h>
#include <linux/io.h>
#include <asm/unaligned.h>
#include <linux/regulator/consumer.h>
#include <linux/sec_sysfs.h>

#include <linux/pinctrl/consumer.h>

#ifdef CONFIG_OF
#include <linux/of_gpio.h>
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif

#ifdef CONFIG_BATTERY_SAMSUNG
#include <linux/sec_batt.h>
#endif

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
#define SEC_DEBUG_TK_LOG
#endif

#ifdef SEC_DEBUG_TK_LOG
#include <linux/sec_debug.h>
#endif

#ifdef SEC_DEBUG_TK_LOG
#define tk_debug_dbg(mode, dev, fmt, ...)	\
({								\
	dev_dbg(dev, fmt, ## __VA_ARGS__);		\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
})

#define tk_debug_info(mode, dev, fmt, ...)	\
({								\
	dev_info(dev, fmt, ## __VA_ARGS__);		\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
})

#define tk_debug_err(mode, dev, fmt, ...)	\
({								\
	if (mode) {								\
		char msg_buff[10];						\
		snprintf(msg_buff, sizeof(msg_buff), "%s", dev_name(dev));	\
		sec_debug_tsp_log_msg(msg_buff, fmt, ## __VA_ARGS__);	\
	}										\
	dev_err(dev, fmt, ## __VA_ARGS__);		\
})
#else
#define tk_debug_dbg(mode, dev, fmt, ...)	dev_dbg(dev, fmt, ## __VA_ARGS__)
#define tk_debug_info(mode, dev, fmt, ...)	dev_info(dev, fmt, ## __VA_ARGS__)
#define tk_debug_err(mode, dev, fmt, ...)	dev_err(dev, fmt, ## __VA_ARGS__)
#endif

/* registers */
#define ABOV_BTNSTATUS		0x00
#define ABOV_FW_VER			0x01
#define ABOV_PCB_VER		0x02
#define ABOV_COMMAND		0x03
#define ABOV_THRESHOLD		0x04
#define ABOV_SENS			0x05
#define ABOV_SETIDAC		0x06
#define ABOV_DIFFDATA		0x0A
#define ABOV_RAWDATA		0x0E
#define ABOV_VENDORID		0x12
#define ABOV_GLOVE			0x13
#define ABOV_KEYBOARD		0x13
#define ABOV_MODEL_NUMBER	0x14		//Model No.
#define ABOV_FLIP			0x15

/* command */
#define CMD_LED_ON			0x10
#define CMD_LED_OFF			0x20
#define CMD_DATA_UPDATE		0x40
#define CMD_LED_CTRL_ON		0x60
#define CMD_LED_CTRL_OFF	0x70
#define CMD_STOP_MODE		0x80
#define CMD_GLOVE_OFF		0x10
#define CMD_GLOVE_ON		0x20
#define CMD_MOBILE_KBD_OFF	0x10
#define CMD_MOBILE_KBD_ON	0x20
#define CMD_FLIP_OFF		0x10
#define CMD_FLIP_ON			0x20

#define ABOV_BOOT_DELAY		45
#define ABOV_RESET_DELAY	150

#define TK_FW_PATH_BIN "abov/abov_hero.fw"
#define TK_FW_PATH_SDCARD "/sdcard/Firmware/TOUCHKEY/abov_fw.bin"

#define I2C_M_WR 0		/* for i2c */

enum {
	BUILT_IN = 0,
	SDCARD,
};

extern unsigned int system_rev;
extern struct class *sec_class;
static int touchkey_keycode[] = { 0,
	KEY_RECENT, KEY_BACK, KEY_HOMEPAGE
};

struct abov_tk_info {
	struct i2c_client *client;
	struct input_dev *input_dev;
	struct device *dev;
	struct abov_touchkey_platform_data *pdata;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	struct mutex lock;
	struct pinctrl *pinctrl;
	struct pinctrl *pinctrl_det;
	struct pinctrl_state *pins_default;

	const struct firmware *firm_data_bin;
	const u8 *firm_data_ums;
	char phys[32];
	long firm_size;
	int irq;
	u16 menu_s;
	u16 back_s;
	u16 menu_raw;
	u16 back_raw;
	int (*power) (bool on);
	void (*input_event)(void *data);
	int touchkey_count;
	u8 fw_update_state;
	u8 fw_ver;
	u8 fw_ver_bin;
	u8 fw_model_number;
	u8 checksum_h;
	u8 checksum_h_bin;
	u8 checksum_l;
	u8 checksum_l_bin;
	bool enabled;
	bool glovemode;
	bool keyboard_mode;
	bool flip_mode;
};


#ifdef CONFIG_HAS_EARLYSUSPEND
static void abov_tk_early_suspend(struct early_suspend *h);
static void abov_tk_late_resume(struct early_suspend *h);
#endif

#if 1//def CONFIG_INPUT_ENABLED
static int abov_tk_input_open(struct input_dev *dev);
static void abov_tk_input_close(struct input_dev *dev);
#endif

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info);

static int abov_touchkey_led_status;
static int abov_touchled_cmd_reserved;

static int abov_mode_enable(struct i2c_client *client,u8 cmd_reg, u8 cmd)
{
	return i2c_smbus_write_byte_data(client, cmd_reg, cmd);
}

static int abov_tk_i2c_read(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg;
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	msg.addr = client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 1;
	msg.buf = &reg;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0)
			break;

		tk_debug_err(true, &client->dev, "%s fail(address set)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	if (ret < 0) {
		mutex_unlock(&info->lock);
		return ret;
	}
	retry = 3;
	msg.flags = 1;/*I2C_M_RD*/
	msg.len = len;
	msg.buf = val;
	while (retry--) {
		ret = i2c_transfer(client->adapter, &msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		tk_debug_err(true, &client->dev, "%s fail(data read)(%d)\n",
			__func__, retry);
		msleep(10);
	}
	mutex_unlock(&info->lock);
	return ret;
}

static int abov_tk_i2c_write(struct i2c_client *client,
		u8 reg, u8 *val, unsigned int len)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	struct i2c_msg msg[1];
	unsigned char data[2];
	int ret;
	int retry = 3;

	mutex_lock(&info->lock);
	data[0] = reg;
	data[1] = *val;
	msg->addr = client->addr;
	msg->flags = I2C_M_WR;
	msg->len = 2;
	msg->buf = data;

	while (retry--) {
		ret = i2c_transfer(client->adapter, msg, 1);
		if (ret >= 0) {
			mutex_unlock(&info->lock);
			return 0;
		}
		tk_debug_err(true, &client->dev, "%s fail(%d)\n",
			__func__, retry);
		msleep(10);
	}
	mutex_unlock(&info->lock);
	return ret;
}

static void release_all_fingers(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int i;

	tk_debug_dbg(true, &client->dev, "%s called\n", __func__);

	for (i = 1; i < info->touchkey_count; i++) {
		input_report_key(info->input_dev,
			touchkey_keycode[i], 0);
	}
	input_sync(info->input_dev);
}

static int abov_tk_reset_for_bootmode(struct abov_tk_info *info)
{
	int ret=0;

	info->pdata->power(info, false);
	msleep(50);
	info->pdata->power(info, true);

	return ret;

}

static void abov_tk_reset(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;

	if (info->enabled == false)
		return;

	tk_debug_info(true,&client->dev, "%s start\n", __func__);
	disable_irq_nosync(info->irq);

	info->enabled = false;

	release_all_fingers(info);

	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_RESET_DELAY);

	if (info->flip_mode){
		abov_mode_enable(client, ABOV_FLIP, CMD_FLIP_ON);
	} else {
		if (info->glovemode)
			abov_mode_enable(client, ABOV_GLOVE, CMD_GLOVE_ON);
	}
	if (info->keyboard_mode)
		abov_mode_enable(client, ABOV_KEYBOARD, CMD_MOBILE_KBD_ON);

	info->enabled = true;

	enable_irq(info->irq);
	tk_debug_info(true,&client->dev, "%s end\n", __func__);
}

static irqreturn_t abov_tk_interrupt(int irq, void *dev_id)
{
	struct abov_tk_info *info = dev_id;
	struct i2c_client *client = info->client;
	int ret, retry;
	int menu_data, back_data, home_data;
	u8 menu_press, back_press, home_press;
	u8 buf;

	ret = abov_tk_i2c_read(client, ABOV_BTNSTATUS, &buf, 1);
	if (ret < 0) {
		retry = 3;
		while (retry--) {
			tk_debug_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			ret = abov_tk_i2c_read(client, ABOV_BTNSTATUS, &buf, 1);
			if (ret == 0)
				break;
			else
				msleep(10);
		}
		if (retry == 0) {
			abov_tk_reset(info);
			return IRQ_HANDLED;
		}
	}

	{
		if(info->pdata->device_num == 2){
			home_data = buf & 0x03;
			back_data = (buf >> 2) & 0x03;

			home_press = !(home_data % 2);
			back_press = !(back_data % 2);

			if (home_data)
				input_report_key(info->input_dev,
					touchkey_keycode[3], home_press);
			if (back_data)
				input_report_key(info->input_dev,
					touchkey_keycode[2], back_press);
		}else {
			menu_data = buf & 0x03;
			back_data = (buf >> 2) & 0x03;

			menu_press = !(menu_data % 2);
			back_press = !(back_data % 2);

			if (menu_data)
				input_report_key(info->input_dev,
					touchkey_keycode[1], menu_press);
			if (back_data)
				input_report_key(info->input_dev,
					touchkey_keycode[2], back_press);
		}

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
		if(info->pdata->device_num == 2){
			tk_debug_info(true, &client->dev,
				"keycode : %s%s ver 0x%02x\n",
				home_data ? (home_press ? "P" : "R") : "",
				back_data ? (back_press ? "P" : "R") : "",
				info->fw_ver);
		} else {
			tk_debug_info(true, &client->dev,
				"keycode : %s%s ver 0x%02x\n",
				menu_data ? (menu_press ? "P" : "R") : "",
				back_data ? (back_press ? "P" : "R") : "",
				info->fw_ver);
		}
#else
		if(info->pdata->device_num == 2){
			tk_debug_info(true, &client->dev,
				"keycode : %s%s%x ver 0x%02x\n",
				home_data ? (home_press ? "home P " : "home R ") : "",
				back_data ? (back_press ? "back P " : "back R ") : "",
				buf, info->fw_ver);
		} else {
			tk_debug_info(true, &client->dev,
				"keycode : %s%s%x ver 0x%02x\n",
				menu_data ? (menu_press ? "menu P " : "menu R ") : "",
				back_data ? (back_press ? "back P " : "back R ") : "",
				buf, info->fw_ver);
		}
#endif
	}

	input_sync(info->input_dev);

	return IRQ_HANDLED;

}
static int touchkey_led_set(struct abov_tk_info *info, int data)
{
	u8 cmd;
	int ret;

	if (data == 1)
		cmd = CMD_LED_ON;
	else
		cmd = CMD_LED_OFF;

	if (!info->enabled) {
		abov_touchled_cmd_reserved = 1;
		return 1;
	}

	ret = abov_tk_i2c_write(info->client, ABOV_BTNSTATUS, &cmd, 1);
	if (ret < 0) {
		tk_debug_err(true, &info->client->dev, "%s fail(%d)\n", __func__, ret);
		abov_touchled_cmd_reserved = 1;
		return 1;
	}

	return 0;
}

static ssize_t touchkey_led_control(struct device *dev,
		 struct device_attribute *attr, const char *buf,
		 size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	int data;
	u8 cmd;
	int ret;

	ret = sscanf(buf, "%d", &data);
	if (ret != 1) {
		tk_debug_err(true, &info->client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(data == 0 || data == 1)) {
		tk_debug_err(true, &info->client->dev, "%s: wrong command(%d)\n",
			__func__, data);
		return count;
	}

	if (data == 1)
		cmd = CMD_LED_ON;
	else
		cmd = CMD_LED_OFF;

	if(touchkey_led_set(info, data))
		goto out;

	msleep(20);

	abov_touchled_cmd_reserved = 0;
	tk_debug_info(true, &info->client->dev, "%s data(%d)\n",__func__,data);

out:
	abov_touchkey_led_status =  cmd;

	return count;
}

static ssize_t touchkey_threshold_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	u8 r_buf;
	int ret;

	ret = abov_tk_i2c_read(client, ABOV_THRESHOLD, &r_buf, 1);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		r_buf = 0;
	}
	return sprintf(buf, "%d\n", r_buf);
}

static void get_diff_data(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	u8 r_buf[4];
	int ret;

	ret = abov_tk_i2c_read(client, ABOV_DIFFDATA, r_buf, 4);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		info->menu_s = 0;
		info->back_s = 0;
		return;
	}

	info->menu_s = (r_buf[0] << 8) | r_buf[1];
	info->back_s = (r_buf[2] << 8) | r_buf[3];
}

static ssize_t touchkey_menu_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	get_diff_data(info);

	return sprintf(buf, "%d\n", info->menu_s);
}

static ssize_t touchkey_back_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	get_diff_data(info);

	return sprintf(buf, "%d\n", info->back_s);
}

static void get_raw_data(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	u8 r_buf[4];
	int ret;

	ret = abov_tk_i2c_read(client, ABOV_RAWDATA, r_buf, 4);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		info->menu_raw = 0;
		info->back_raw = 0;
		return;
	}

	info->menu_raw = (r_buf[0] << 8) | r_buf[1];
	info->back_raw = (r_buf[2] << 8) | r_buf[3];
}

static ssize_t touchkey_menu_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	get_raw_data(info);

	return sprintf(buf, "%d\n", info->menu_raw);
}

static ssize_t touchkey_back_raw_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	get_raw_data(info);

	return sprintf(buf, "%d\n", info->back_raw);
}

static ssize_t bin_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;

	tk_debug_dbg(true, &client->dev, "fw version bin : 0x%x\n", info->fw_ver_bin);

	return sprintf(buf, "0x%02x\n", info->fw_ver_bin);
}

int get_tk_fw_version(struct abov_tk_info *info, bool bootmode)
{
	struct i2c_client *client = info->client;
	u8 buf;
	int ret;
	int retry = 3;

	ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
	if (ret < 0) {
		while (retry--) {
			tk_debug_err(true, &client->dev, "%s read fail(%d)\n",
				__func__, retry);
			if (!bootmode)
				abov_tk_reset(info);
			else
				return -1;
			ret = abov_tk_i2c_read(client, ABOV_FW_VER, &buf, 1);
			if (ret == 0)
				break;
		}
		if (retry <= 0)
			return -1;
	}

	info->fw_ver = buf;
	tk_debug_info(true, &client->dev, "%s : 0x%x\n", __func__, buf);
	return 0;
}

static ssize_t read_fw_ver(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail\n", __func__);
		info->fw_ver = 0;
	}

	return sprintf(buf, "0x%02x\n", info->fw_ver);
}

static int abov_load_fw(struct abov_tk_info *info, u8 cmd)
{
	struct i2c_client *client = info->client;
	struct file *fp;
	mm_segment_t old_fs;
	long fsize, nread;
	int ret = 0;

	switch(cmd) {
	case BUILT_IN:
		ret = request_firmware(&info->firm_data_bin,
			info->pdata->fw_path, &client->dev);
		if (ret) {
			tk_debug_err(true, &client->dev,
				"%s request_firmware fail(%d)\n", __func__, cmd);
			return ret;
		}
		/* Header info
		* 0x00 0x91 : model info,
		* 0x00 0x00 : module info (Rev 0.0),
		* 0x00 0xF3 : F/W
		* 0x00 0x00 0x17 0x10 : checksum
		* ~ 22byte 0x00 */
		info->fw_model_number = info->firm_data_bin->data[1];
		info->fw_ver_bin = info->firm_data_bin->data[5];
		info->checksum_h_bin = info->firm_data_bin->data[8];
		info->checksum_l_bin = info->firm_data_bin->data[9];
		info->firm_size = info->firm_data_bin->size;

		tk_debug_info(true, &client->dev, "%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
			info->firm_data_bin->data[1], info->firm_data_bin->data[3], info->fw_ver_bin, \
			info->checksum_h_bin, info->checksum_l_bin);
		break;

	case SDCARD:
		old_fs = get_fs();
		set_fs(get_ds());
		fp = filp_open(TK_FW_PATH_SDCARD, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			tk_debug_err(true, &client->dev,
				"%s %s open error\n", __func__, TK_FW_PATH_SDCARD);
			ret = -ENOENT;
			goto fail_sdcard_open;
		}

		fsize = fp->f_path.dentry->d_inode->i_size;
		info->firm_data_ums = kzalloc((size_t)fsize, GFP_KERNEL);
		if (!info->firm_data_ums) {
			tk_debug_err(true, &client->dev,
				"%s fail to kzalloc for fw\n", __func__);
			ret = -ENOMEM;
			goto fail_sdcard_kzalloc;
		}

		nread = vfs_read(fp,
			(char __user *)info->firm_data_ums, fsize, &fp->f_pos);
		if (nread != fsize) {
			tk_debug_err(true, &client->dev,
				"%s fail to vfs_read file\n", __func__);
			ret = -EINVAL;
			goto fail_sdcard_size;
		}
		filp_close(fp, current->files);
		set_fs(old_fs);

		info->firm_size = nread;
		info->checksum_h_bin = info->firm_data_ums[8];
		info->checksum_l_bin = info->firm_data_ums[9];

		tk_debug_info(true, &client->dev,"%s, bin version:%2X,%2X,%2X   crc:%2X,%2X\n", __func__, \
			info->firm_data_ums[1], info->firm_data_ums[3], info->firm_data_ums[5], \
			info->checksum_h_bin, info->checksum_l_bin);
		break;

	default:
		ret = -1;
		break;
	}
	tk_debug_info(true, &client->dev, "fw_size : %lu\n", info->firm_size);
	tk_debug_info(true, &client->dev, "%s success\n", __func__);
	return ret;

fail_sdcard_size:
	kfree(&info->firm_data_ums);
fail_sdcard_kzalloc:
	filp_close(fp, current->files);
fail_sdcard_open:
	set_fs(old_fs);
	return ret;
}

static int abov_tk_check_busy(struct abov_tk_info *info)
{
	int ret, count = 0;
	unsigned char val = 0x00;

	do {
		ret = i2c_master_recv(info->client, &val, sizeof(val));

		if (val & 0x01) {
			count++;
			if (count > 1000)
				tk_debug_err(true, &info->client->dev,
					"%s: busy %d\n", __func__, count);
				return ret;
		} else {
			break;
		}

	} while(1);

	return ret;
}

static int abov_tk_i2c_read_checksum(struct abov_tk_info *info)
{
	unsigned char data[6] = {0xAC, 0x9E, 0x10, 0x00, 0x3F, 0xFF};
	unsigned char checksum[5] = {0, };
	int ret;
	unsigned char reg = 0x00;

	i2c_master_send(info->client, data, 6);

	usleep_range(5000, 5000);

	abov_tk_check_busy(info);

	ret = abov_tk_i2c_read(info->client, reg, checksum, 5);

	tk_debug_info(true, &info->client->dev, "%s: ret:%d [%X][%X][%X][%X][%X]\n",
			__func__, ret, checksum[0], checksum[1], checksum[2]
			, checksum[3], checksum[4]);
	info->checksum_h = checksum[3];
	info->checksum_l = checksum[4];
	return 0;
}

static int abov_tk_fw_write(struct abov_tk_info *info, unsigned char *addrH,
						unsigned char *addrL, unsigned char *val)
{
	int length = 36, ret = 0;
	unsigned char data[36];

	data[0] = 0xAC;
	data[1] = 0x7A;
	memcpy(&data[2], addrH, 1);
	memcpy(&data[3], addrL, 1);
	memcpy(&data[4], val, 32);

	ret = i2c_master_send(info->client, data, length);
	if (ret != length) {
		tk_debug_err(true, &info->client->dev,
			"%s: write fail[%x%x], %d\n", __func__, *addrH, *addrL, ret);
		return ret;
	}

	usleep_range(3000, 3000);

	abov_tk_check_busy(info);

	return 0;
}

static int abov_tk_fw_mode_enter(struct abov_tk_info *info)
{
	unsigned char data[3] = {0xAC, 0x5B, 0x2D};
	int ret = 0;

	ret = i2c_master_send(info->client, data, 3);
	if (ret != 3) {
		tk_debug_err(true, &info->client->dev,
			"%s: write fail\n", __func__);
		return -1;
	}

	return 0;

}

static int abov_tk_fw_update(struct abov_tk_info *info, u8 cmd)
{
	int ret, ii = 0;
	int count;
	unsigned short address;
	unsigned char addrH, addrL;
	unsigned char data[32] = {0, };


	tk_debug_info(true, &info->client->dev, "%s start\n", __func__);

	count = info->firm_size / 32;
	address = 0x1000;

	tk_debug_info(true, &info->client->dev, "%s reset\n", __func__);
	abov_tk_reset_for_bootmode(info);
	msleep(ABOV_BOOT_DELAY);
	ret = abov_tk_fw_mode_enter(info);
	if(ret<0){
		tk_debug_err(true, &info->client->dev,
			"%s:abov_tk_fw_mode_enter fail\n", __func__);
		return ret;
	}
	tk_debug_info(true, &info->client->dev, "%s fw_mode_cmd sended\n", __func__);

	msleep(1100);
	tk_debug_info(true, &info->client->dev, "%s fw_write start\n", __func__);

	for (ii = 1; ii < count; ii++) {
		/* first 32byte is header */
		addrH = (unsigned char)((address >> 8) & 0xFF);
		addrL = (unsigned char)(address & 0xFF);
		if (cmd == BUILT_IN)
			memcpy(data, &info->firm_data_bin->data[ii * 32], 32);
		else if (cmd == SDCARD)
			memcpy(data, &info->firm_data_ums[ii * 32], 32);

		ret = abov_tk_fw_write(info, &addrH, &addrL, data);
		if (ret < 0) {
			tk_debug_err(true, &info->client->dev,
				"%s: err, no device : %d\n", __func__, ret);
			return ret;
		}

		address += 0x20;

		memset(data, 0, 32);
	}
	tk_debug_info(true, &info->client->dev, "%s fw_write end\n", __func__);

	ret = abov_tk_i2c_read_checksum(info);
	tk_debug_dbg(true, &info->client->dev, "%s checksum readed\n", __func__);

	return ret;
}

static void abov_release_fw(struct abov_tk_info *info, u8 cmd)
{
	switch(cmd) {
	case BUILT_IN:
		release_firmware(info->firm_data_bin);
		break;

	case SDCARD:
		kfree(info->firm_data_ums);
		break;

	default:
		break;
	}
}

static int abov_flash_fw(struct abov_tk_info *info, bool probe, u8 cmd)
{
	struct i2c_client *client = info->client;
	int retry = 2;
	int ret;
	int block_count;
	const u8 *fw_data;

	switch(cmd) {
	case BUILT_IN:
		fw_data = info->firm_data_bin->data;
		break;

	case SDCARD:
		fw_data = info->firm_data_ums;
		break;

	default:
		return -1;
		break;
	}

	block_count = (int)(info->firm_size / 32);

	while (retry--) {
		ret = abov_tk_fw_update(info, cmd);
		if (ret < 0)
			break;

		if ((info->checksum_h != info->checksum_h_bin) ||
			(info->checksum_l != info->checksum_l_bin)) {
			tk_debug_err(true, &client->dev,
				"%s checksum fail.(0x%x,0x%x),(0x%x,0x%x) retry:%d\n",
				__func__, info->checksum_h, info->checksum_l,
				info->checksum_h_bin, info->checksum_l_bin, retry);
			ret = -1;
			continue;
		}else
			tk_debug_info(true, &client->dev,"%s checksum successed.\n",__func__);

		abov_tk_reset_for_bootmode(info);
		msleep(ABOV_RESET_DELAY);
		ret = get_tk_fw_version(info, true);
		if (ret) {
			tk_debug_err(true, &client->dev, "%s fw version read fail\n", __func__);
			ret = -1;
			continue;
		}

		if (info->fw_ver == 0) {
			tk_debug_err(true, &client->dev, "%s fw version fail (0x%x)\n",
				__func__, info->fw_ver);
			ret = -1;
			continue;
		}

		if ((cmd == BUILT_IN) && (info->fw_ver != info->fw_ver_bin)){
			tk_debug_err(true, &client->dev, "%s fw version fail 0x%x, 0x%x\n",
				__func__, info->fw_ver, info->fw_ver_bin);
			ret = -1;
			continue;
		}
		ret = 0;
		break;
	}

	return ret;
}

static ssize_t touchkey_fw_update(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int ret;
	u8 cmd;

	switch(*buf) {
	case 's':
	case 'S':
		cmd = BUILT_IN;
		break;
	case 'i':
	case 'I':
		cmd = SDCARD;
		break;
	default:
		info->fw_update_state = 2;
		goto touchkey_fw_update_out;
	}

	ret = abov_load_fw(info, cmd);
	if (ret) {
		tk_debug_err(true, &client->dev,
			"%s fw load fail\n", __func__);
		info->fw_update_state = 2;
		goto touchkey_fw_update_out;
	}

	info->fw_update_state = 1;
	disable_irq(info->irq);
	info->enabled = false;
	ret = abov_flash_fw(info, false, cmd);
	if (info->flip_mode){
		abov_mode_enable(client, ABOV_FLIP, CMD_FLIP_ON);
	} else {
		if (info->glovemode)
			abov_mode_enable(client, ABOV_GLOVE, CMD_GLOVE_ON);
	}
	if (info->keyboard_mode)
		abov_mode_enable(client, ABOV_KEYBOARD, CMD_MOBILE_KBD_ON);

	info->enabled = true;
	enable_irq(info->irq);
	if (ret) {
		tk_debug_err(true, &client->dev, "%s fail\n", __func__);
//		info->fw_update_state = 2;
		info->fw_update_state = 0;

	} else {
		tk_debug_info(true, &client->dev, "%s success\n", __func__);
		info->fw_update_state = 0;
	}

	abov_release_fw(info, cmd);

touchkey_fw_update_out:
	tk_debug_dbg(true, &client->dev, "%s : %d\n", __func__, info->fw_update_state);

	return count;
}

static ssize_t touchkey_fw_update_status(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int count = 0;

	tk_debug_info(true, &client->dev, "%s : %d\n", __func__, info->fw_update_state);

	if (info->fw_update_state == 0)
		count = sprintf(buf, "PASS\n");
	else if (info->fw_update_state == 1)
		count = sprintf(buf, "Downloading\n");
	else if (info->fw_update_state == 2)
		count = sprintf(buf, "Fail\n");

	return count;
}

static ssize_t abov_glove_mode(struct device *dev,
	 struct device_attribute *attr, const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int scan_buffer;
	int ret;
	u8 cmd;

	ret = sscanf(buf, "%d", &scan_buffer);
	tk_debug_info(true, &client->dev, "%s : %d\n", __func__, scan_buffer);


	if (ret != 1) {
		tk_debug_err(true, &client->dev, "%s: cmd read err\n", __func__);
		return count;
	}

	if (!(scan_buffer == 0 || scan_buffer == 1)) {
		tk_debug_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, scan_buffer);
		return count;
	}

	if (!info->enabled)
		return count;

	if (info->glovemode == scan_buffer) {
		tk_debug_dbg(true, &client->dev, "%s same command(%d)\n",
			__func__, scan_buffer);
		return count;
	}

	if (scan_buffer == 1) {
		cmd = CMD_GLOVE_ON;
	} else {
		cmd = CMD_GLOVE_OFF;
	}

	ret = abov_mode_enable(client, ABOV_GLOVE, cmd);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		return count;
	}

	info->glovemode = scan_buffer;

	return count;
}

static ssize_t abov_glove_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);

	return sprintf(buf, "%d\n", info->glovemode);
}

static ssize_t keyboard_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int keyboard_mode_on;
	int ret;
	u8 cmd;

	tk_debug_dbg(true, &client->dev, "%s : Mobile KBD sysfs node called\n",__func__);

	sscanf(buf, "%d", &keyboard_mode_on);
	tk_debug_info(true, &client->dev, "%s : %d\n",
		__func__, keyboard_mode_on);

	if (!(keyboard_mode_on == 0 || keyboard_mode_on == 1)) {
		tk_debug_err(true, &client->dev, "%s: wrong command(%d)\n",
			__func__, keyboard_mode_on);
		return count;
	}

	if (!info->enabled)
		goto out;

	if (info->keyboard_mode == keyboard_mode_on) {
		tk_debug_dbg(true, &client->dev, "%s same command(%d)\n",
			__func__, keyboard_mode_on);
		goto out;
	}

	if (keyboard_mode_on == 1) {
		cmd = CMD_MOBILE_KBD_ON;
	} else {
		cmd = CMD_MOBILE_KBD_OFF;
	}

	/* mobile keyboard use same register with glove mode */
	ret = abov_mode_enable(client, ABOV_KEYBOARD, cmd);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
		goto out;
	}

out:
	info->keyboard_mode = keyboard_mode_on;
	return count;
}

static ssize_t flip_cover_mode_enable(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	struct abov_tk_info *info = dev_get_drvdata(dev);
	struct i2c_client *client = info->client;
	int flip_mode_on;
	int ret;
	u8 cmd;

	sscanf(buf, "%d\n", &flip_mode_on);
	tk_debug_info(true, &client->dev, "%s : %d\n", __func__, flip_mode_on);

	if (!info->enabled)
		goto out;
	/* glove mode control */
	if (flip_mode_on) {
		cmd = CMD_FLIP_ON;
	} else {
		if (info->glovemode)
			cmd = CMD_GLOVE_ON;
		cmd = CMD_FLIP_OFF;
	}

	if (info->glovemode){
		ret = abov_mode_enable(client, ABOV_GLOVE, cmd);
		if (ret < 0) {
			tk_debug_err(true, &client->dev, "%s glove mode fail(%d)\n", __func__, ret);
			goto out;
		}
	} else{
		ret = abov_mode_enable(client, ABOV_FLIP, cmd);
		if (ret < 0) {
			tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
			goto out;
		}
	}

out:
	info->flip_mode = flip_mode_on;
	return count;
}

static DEVICE_ATTR(touchkey_threshold, S_IRUGO, touchkey_threshold_show, NULL);
static DEVICE_ATTR(brightness, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			touchkey_led_control);
static DEVICE_ATTR(touchkey_recent, S_IRUGO, touchkey_menu_show, NULL);
static DEVICE_ATTR(touchkey_back, S_IRUGO, touchkey_back_show, NULL);
static DEVICE_ATTR(touchkey_recent_raw, S_IRUGO, touchkey_menu_raw_show, NULL);
static DEVICE_ATTR(touchkey_back_raw, S_IRUGO, touchkey_back_raw_show, NULL);
static DEVICE_ATTR(touchkey_firm_version_phone, S_IRUGO, bin_fw_ver, NULL);
static DEVICE_ATTR(touchkey_firm_version_panel, S_IRUGO, read_fw_ver, NULL);
static DEVICE_ATTR(touchkey_firm_update, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			touchkey_fw_update);
static DEVICE_ATTR(touchkey_firm_update_status, S_IRUGO | S_IWUSR | S_IWGRP,
			touchkey_fw_update_status, NULL);
static DEVICE_ATTR(glove_mode, S_IRUGO | S_IWUSR | S_IWGRP,
			abov_glove_mode_show, abov_glove_mode);
static DEVICE_ATTR(keyboard_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
			keyboard_cover_mode_enable);
static DEVICE_ATTR(flip_mode, S_IRUGO | S_IWUSR | S_IWGRP, NULL,
		   flip_cover_mode_enable);

static struct attribute *sec_touchkey_attributes[] = {
	&dev_attr_touchkey_threshold.attr,
	&dev_attr_brightness.attr,
	&dev_attr_touchkey_recent.attr,
	&dev_attr_touchkey_back.attr,
	&dev_attr_touchkey_recent_raw.attr,
	&dev_attr_touchkey_back_raw.attr,
	&dev_attr_touchkey_firm_version_phone.attr,
	&dev_attr_touchkey_firm_version_panel.attr,
	&dev_attr_touchkey_firm_update.attr,
	&dev_attr_touchkey_firm_update_status.attr,
	&dev_attr_glove_mode.attr,
	&dev_attr_keyboard_mode.attr,
	&dev_attr_flip_mode.attr,
	NULL,
};

static struct attribute_group sec_touchkey_attr_group = {
	.attrs = sec_touchkey_attributes,
};

extern int get_samsung_lcd_attached(void);

static int abov_tk_fw_check(struct abov_tk_info *info)
{
	struct i2c_client *client = info->client;
	int ret, fw_update = 0;
	u8 buf;

	ret = abov_load_fw(info, BUILT_IN);
	if (ret) {
		tk_debug_err(true, &client->dev,
			"%s fw load fail\n", __func__);
		return ret;
	}

	ret = get_tk_fw_version(info, true);

	//Check Model No.
	ret = abov_tk_i2c_read(client, ABOV_MODEL_NUMBER, &buf, 1);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s fail(%d)\n", __func__, ret);
	}

	if(info->fw_model_number != buf){
		tk_debug_info(true, &client->dev, "fw model number = %x ic model number = %x \n", info->fw_model_number, buf);
		fw_update = 1;
		goto flash_fw;
	}

	if ((info->fw_ver == 0) || info->fw_ver > 0xF0 || info->fw_ver != info->fw_ver_bin) {
		tk_debug_dbg(true, &client->dev, "excute tk firmware update (0x%x -> 0x%x\n",
			info->fw_ver, info->fw_ver_bin);
		fw_update = 1;
	}

flash_fw:
	if(fw_update){
		ret = abov_flash_fw(info, true, BUILT_IN);
		if (ret) {
			tk_debug_err(true, &client->dev,
				"failed to abov_flash_fw (%d)\n", ret);
		} else {
			tk_debug_info(true, &client->dev,
				"fw update success\n");
		}
	}

	abov_release_fw(info, BUILT_IN);

	return ret;
}

int abov_power(void *data, bool on)
{
	struct abov_tk_info *info = (struct abov_tk_info *)data;
	struct i2c_client *client = info->client;
	
	int ret = 0;

	if (!!info->pdata->regulator_avdd) {
		info->pdata->avdd_vreg = regulator_get(NULL, info->pdata->regulator_avdd);
		if (IS_ERR(info->pdata->avdd_vreg)) {
			info->pdata->avdd_vreg = NULL;
			tk_debug_err(true, &client->dev, "%s : avdd_vreg get error, ignoring\n", __func__);
		}
	}

	if (!!info->pdata->regulator_dvdd) {
		info->pdata->dvdd_vreg = regulator_get(NULL, info->pdata->regulator_dvdd);
		if (IS_ERR(info->pdata->dvdd_vreg)) {
			info->pdata->dvdd_vreg = NULL;
			tk_debug_err(true, &client->dev, "%s : dvdd_vreg get error, ignoring\n",__func__);
		}
	}

	if (on) {
		if (info->pdata->dvdd_vreg) {
			ret = regulator_enable(info->pdata->dvdd_vreg);
			if(ret){
				tk_debug_err(true, &client->dev, "%s : dvdd reg enable fail\n", __func__);
			}
		}
		if (info->pdata->avdd_vreg) {
			ret = regulator_enable(info->pdata->avdd_vreg);
			if(ret){
				tk_debug_err(true, &client->dev, "%s : avdd reg enable fail\n", __func__);
			}
		}
	} else {
		if (info->pdata->avdd_vreg) {
			ret = regulator_disable(info->pdata->avdd_vreg);
			if(ret){
				tk_debug_err(true, &client->dev, "%s : avdd reg disable fail\n", __func__);
			}
		}
		if (info->pdata->dvdd_vreg) {
			ret = regulator_disable(info->pdata->dvdd_vreg);
			if(ret){
				tk_debug_err(true, &client->dev, "%s : dvdd reg disable fail\n", __func__);
			}
		}
	}
	
	if (info->pdata->avdd_vreg)
		regulator_put(info->pdata->avdd_vreg);
	if (info->pdata->dvdd_vreg)
		regulator_put(info->pdata->dvdd_vreg);

	tk_debug_info(true, &client->dev, "%s %s\n", __func__, on ? "on" : "off");

	return ret;
}

#if 1
static int abov_pinctrl_configure(struct abov_tk_info *info,
							bool active)
{
	struct pinctrl_state *set_state;
	int retval;

	if (active) {
		set_state =
			pinctrl_lookup_state(info->pinctrl,
						"on_irq");
		if (IS_ERR(set_state)) {
			tk_debug_err(true, &info->client->dev,
				"%s: cannot get ts pinctrl active state\n", __func__);
			return PTR_ERR(set_state);
		}
	} else {
		set_state =
			pinctrl_lookup_state(info->pinctrl,
						"off_irq");
		if (IS_ERR(set_state)) {
			tk_debug_err(true, &info->client->dev,
				"%s: cannot get gpiokey pinctrl sleep state\n", __func__);
			return PTR_ERR(set_state);
		}
	}
	retval = pinctrl_select_state(info->pinctrl, set_state);
	if (retval) {
		tk_debug_err(true, &info->client->dev,
			"%s: cannot set ts pinctrl active state\n", __func__);
		return retval;
	}

	return 0;
}
#endif
int abov_gpio_reg_init(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	int ret = 0;

	ret = gpio_request(pdata->gpio_int, "tkey_gpio_int");
	if(ret < 0){
		tk_debug_err(true, dev,
			"unable to request gpio_int\n");
		return ret;
	}

	pdata->power = abov_power;

	return ret;
}

#ifdef CONFIG_OF
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	struct device_node *np = dev->of_node;
	int ret;
	//u32 tmp[2] = {0, };

	pdata->gpio_int = of_get_named_gpio(np, "abov,irq-gpio", 0);
	if(pdata->gpio_int < 0){
		tk_debug_err(true, dev, "unable to get gpio_int\n");
		return pdata->gpio_int;
	}

	pdata->gpio_scl = of_get_named_gpio(np, "abov,scl-gpio", 0);
	if(pdata->gpio_scl < 0){
		tk_debug_err(true, dev, "unable to get gpio_scl\n");
		return pdata->gpio_scl;
	}

	pdata->gpio_sda = of_get_named_gpio(np, "abov,sda-gpio", 0);
	if(pdata->gpio_sda < 0){
		tk_debug_err(true, dev, "unable to get gpio_sda\n");
		return pdata->gpio_sda;
	}

	pdata->sub_det = of_get_named_gpio(np, "abov,sub-det", 0);
	if (pdata->sub_det < 0) {
		tk_debug_info(true, dev, "unable to get sub_det\n");
	} else {
		tk_debug_info(true, dev, "%s: sub_det:%d\n",__func__,pdata->sub_det);
	}

	if (of_property_read_string(np, "abov,regulator_avdd", &pdata->regulator_avdd)) {
		tk_debug_info(true, dev, "Failed to get regulator_avdd name property\n");
	}

	if (of_property_read_string(np, "abov,regulator_dvdd", &pdata->regulator_dvdd)) {
		tk_debug_info(true, dev, "Failed to get regulator_dvdd name property\n");
	}

	ret = of_property_read_string(np, "abov,fw_path", (const char **)&pdata->fw_path);
	if (ret) {
		tk_debug_err(true, dev, "touchkey:failed to read fw_path %d\n", ret);
		pdata->fw_path = TK_FW_PATH_BIN;
	}
	tk_debug_info(true, dev, "%s: fw path %s\n", __func__, pdata->fw_path);

	pdata->boot_on_ldo = of_property_read_bool(np, "abov,boot-on-ldo");

	if (of_property_read_u32(np, "abov,device_num", &pdata->device_num))
		tk_debug_info(true, dev, "Failed to get device_num property\n");

	/* only for multi tsk models. */
	if(pdata->device_num == 0 || pdata->device_num == 1)
		dev->init_name = "main tsk";
	else
		dev->init_name = "sub  tsk";

	tk_debug_info(true, dev, "%s: gpio_int:%d, gpio_scl:%d, gpio_sda:%d, device_num: %d\n",
			__func__, pdata->gpio_int, pdata->gpio_scl, pdata->gpio_sda, pdata->device_num);

	return 0;
}
#else
static int abov_parse_dt(struct device *dev,
			struct abov_touchkey_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int abov_tk_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct abov_tk_info *info;
	struct input_dev *input_dev;
#if 0
	struct device *touchkey_dev;
	int i;
#endif
	int ret = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		tk_debug_err(true, &client->dev,
			"i2c_check_functionality fail\n");
		return -EIO;
	}

	info = kzalloc(sizeof(struct abov_tk_info), GFP_KERNEL);
	if (!info) {
		tk_debug_err(true, &client->dev, "Failed to allocate memory\n");
		ret = -ENOMEM;
		goto err_alloc;
	}

	input_dev = input_allocate_device();
	if (!input_dev) {
		tk_debug_err(true, &client->dev,
			"Failed to allocate memory for input device\n");
		ret = -ENOMEM;
		goto err_input_alloc;
	}

	info->client = client;
	info->input_dev = input_dev;

	if (client->dev.of_node) {
		struct abov_touchkey_platform_data *pdata;
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct abov_touchkey_platform_data), GFP_KERNEL);
		if (!pdata) {
			tk_debug_err(true, &client->dev, "Failed to allocate memory\n");
			ret = -ENOMEM;
			goto err_config;
		}

		ret = abov_parse_dt(&client->dev, pdata);
		if (ret){
			tk_debug_err(true, &client->dev, "failed to abov_parse_dt\n");
			ret = -ENOMEM;
			goto err_config;
		}

		info->pdata = pdata;
	} else
		info->pdata = client->dev.platform_data;

	if (info->pdata == NULL) {
		tk_debug_err(true, &client->dev, "failed to get platform data\n");
		goto err_config;
	}
#if 1
	/* Get pinctrl if target uses pinctrl */
		info->pinctrl = devm_pinctrl_get(&client->dev);
		if (IS_ERR(info->pinctrl)) {
			if (PTR_ERR(info->pinctrl) == -EPROBE_DEFER)
				goto err_config;

			tk_debug_err(true, &client->dev, "%s: Target does not use pinctrl\n", __func__);
			info->pinctrl = NULL;
		}

		if (info->pinctrl) {
			ret = abov_pinctrl_configure(info, true);
			if (ret)
				tk_debug_err(true, &client->dev,
					"%s: cannot set ts pinctrl active state\n", __func__);
		}

		/* sub-det pinctrl */
		if (gpio_is_valid(info->pdata->sub_det)) {
			info->pinctrl_det = devm_pinctrl_get(&client->dev);
			if (IS_ERR(info->pinctrl_det)) {
				tk_debug_err(true, &client->dev, "%s: Failed to get pinctrl\n", __func__);
				goto err_config;
			}

			info->pins_default = pinctrl_lookup_state(info->pinctrl_det, "sub_det");
			if (IS_ERR(info->pins_default)) {
				tk_debug_err(true, &client->dev, "%s: Failed to get pinctrl state\n", __func__);
				devm_pinctrl_put(info->pinctrl_det);
				goto err_config;
			}

			ret = pinctrl_select_state(info->pinctrl_det, info->pins_default);
			if (ret < 0)
				tk_debug_err(true, &client->dev, "%s: Failed to configure sub_det pin\n", __func__);
		}
#endif
	ret = abov_gpio_reg_init(&client->dev, info->pdata);
	if(ret){
		tk_debug_err(true, &client->dev, "failed to init reg\n");
		goto pwr_config;
	}
	if (info->pdata->power)
		info->pdata->power(info, true);

	if(!info->pdata->boot_on_ldo)
		msleep(ABOV_RESET_DELAY);

	if (gpio_is_valid(info->pdata->sub_det)) {
		ret = gpio_get_value(info->pdata->sub_det);
		if (ret) {
			tk_debug_err(true, &client->dev, "Device wasn't connected to board \n");
			ret = -ENODEV;
			goto err_i2c_check;
		}
	}

	info->enabled = true;
	info->irq = -1;
	client->irq = gpio_to_irq(info->pdata->gpio_int);

	mutex_init(&info->lock);

	info->input_event = info->pdata->input_event;
	info->touchkey_count = sizeof(touchkey_keycode) / sizeof(int);
	i2c_set_clientdata(client, info);

	ret = abov_tk_fw_check(info);
	if (ret) {
		tk_debug_err(true, &client->dev,
			"failed to firmware check (%d)\n", ret);
		goto err_reg_input_dev;
	}

	ret = get_tk_fw_version(info, false);
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "%s read fail\n", __func__);
		goto err_reg_input_dev;
	}

	snprintf(info->phys, sizeof(info->phys),
		 "%s/input0", dev_name(&client->dev));
	if (info->pdata->device_num == 2) {
		input_dev->name = "sec_touchkey2";
	} else {
		input_dev->name = "sec_touchkey";
	}
	input_dev->phys = info->phys;
	input_dev->id.bustype = BUS_HOST;
	input_dev->dev.parent = &client->dev;
#if 1 //def CONFIG_INPUT_ENABLED
	input_dev->open = abov_tk_input_open;
	input_dev->close = abov_tk_input_close;
#endif
	set_bit(EV_KEY, input_dev->evbit);
	set_bit(KEY_RECENT, input_dev->keybit);
	set_bit(KEY_BACK, input_dev->keybit);
	set_bit(KEY_HOMEPAGE, input_dev->keybit);
	set_bit(EV_LED, input_dev->evbit);
	set_bit(LED_MISC, input_dev->ledbit);
	input_set_drvdata(input_dev, info);

	ret = input_register_device(input_dev);
	if (ret) {
		tk_debug_err(true, &client->dev, "failed to register input dev (%d)\n",
			ret);
		goto err_reg_input_dev;
	}

	if (!info->pdata->irq_flag) {
		tk_debug_err(true, &client->dev, "no irq_flag\n");
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT, ABOV_TK_NAME, info);
	} else {
		ret = request_threaded_irq(client->irq, NULL, abov_tk_interrupt,
			info->pdata->irq_flag, ABOV_TK_NAME, info);
	}
	if (ret < 0) {
		tk_debug_err(true, &client->dev, "Failed to register interrupt\n");
		goto err_req_irq;
	}
	info->irq = client->irq;

#ifdef CONFIG_HAS_EARLYSUSPEND
	info->early_suspend.level = EARLY_SUSPEND_LEVEL_STOP_DRAWING;
	info->early_suspend.suspend = abov_tk_early_suspend;
	info->early_suspend.resume = abov_tk_late_resume;
	register_early_suspend(&info->early_suspend);
#endif

	if (info->pdata->device_num == 2) {
		info->dev = sec_device_create(info, "sec_touchkey2");
	} else {
		info->dev = sec_device_create(info, "sec_touchkey");
	}
	if (IS_ERR(info->dev))
		tk_debug_err(true, &client->dev,
		"Failed to create device for the touchkey sysfs\n");

	ret = sysfs_create_group(&info->dev ->kobj,
		&sec_touchkey_attr_group);
	if (ret)
		tk_debug_err(true, &client->dev, "Failed to create sysfs group\n");

	ret = sysfs_create_link(&info->dev ->kobj,
		&info->input_dev->dev.kobj, "input");
	if (ret < 0) {
		tk_debug_err(true, &client->dev,
			"%s: Failed to create input symbolic link\n",
			__func__);
	}

	tk_debug_err(true, &client->dev, "%s done\n", __func__);

	return 0;

err_req_irq:
	input_unregister_device(input_dev);
err_reg_input_dev:
	mutex_destroy(&info->lock);
err_i2c_check:
	if (info->pdata->power)
		info->pdata->power(info, false);
pwr_config:
err_config:
	input_free_device(input_dev);
err_input_alloc:
	kfree(info);
err_alloc:
	tk_debug_err(true, &client->dev, "%s fail\n",__func__);
	return ret;

}

static int abov_tk_remove(struct i2c_client *client)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);

/*	if (info->enabled)
		info->pdata->power(0);
*/
	info->enabled = false;
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&info->early_suspend);
#endif
	if (info->irq >= 0)
		free_irq(info->irq, info);
	input_unregister_device(info->input_dev);
	input_free_device(info->input_dev);
	kfree(info);

	return 0;
}

static void abov_tk_shutdown(struct i2c_client *client)
{
	struct abov_tk_info *info = i2c_get_clientdata(client);
	u8 cmd = CMD_LED_OFF;
	tk_debug_info(true, &client->dev, "Inside abov_tk_shutdown \n");

	if (info->enabled){
		disable_irq(info->irq);
	abov_tk_i2c_write(client, ABOV_BTNSTATUS, &cmd, 1);
		info->pdata->power(info, false);
	}
	info->enabled = false;

// just power off.
//	if (info->irq >= 0)
//		free_irq(info->irq, info);
//	kfree(info);
}

#if defined(CONFIG_PM)
static int abov_tk_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct abov_tk_info *info = i2c_get_clientdata(client);

	if (!info->enabled) {
		tk_debug_info(true, &client->dev, "%s: already power off\n", __func__);
		return 0;
	}

	tk_debug_info(true, &client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	disable_irq(info->irq);
	info->enabled = false;
	release_all_fingers(info);

	if (info->pdata->power)
		info->pdata->power(info, false);
	return 0;
}

static int abov_tk_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct abov_tk_info *info = i2c_get_clientdata(client);
	u8 led_data;

	if (info->enabled) {
		tk_debug_info(true, &client->dev, "%s: already power on\n", __func__);
		return 0;
	}

	tk_debug_info(true, &info->client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	if (info->pdata->power) {
		info->pdata->power(info, true);
		msleep(ABOV_RESET_DELAY);
	} else /* touchkey on by i2c */
		get_tk_fw_version(info, true);

	info->enabled = true;

	if (abov_touchled_cmd_reserved && \
		abov_touchkey_led_status == CMD_LED_ON) {
		abov_touchled_cmd_reserved = 0;
		led_data=abov_touchkey_led_status;

		abov_tk_i2c_write(client, ABOV_BTNSTATUS, &led_data, 1);

		tk_debug_dbg(true, &info->client->dev, "%s: LED reserved on\n", __func__);
	}
	enable_irq(info->irq);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void abov_tk_early_suspend(struct early_suspend *h)
{
	struct abov_tk_info *info;
	info = container_of(h, struct abov_tk_info, early_suspend);
	abov_tk_suspend(&info->client->dev);

}

static void abov_tk_late_resume(struct early_suspend *h)
{
	struct abov_tk_info *info;
	info = container_of(h, struct abov_tk_info, early_suspend);
	abov_tk_resume(&info->client->dev);
}
#endif

#if 1//def CONFIG_INPUT_ENABLED
static int abov_tk_input_open(struct input_dev *dev)
{
	struct abov_tk_info *info = input_get_drvdata(dev);

	tk_debug_dbg(true, &info->client->dev, "%s %d \n",__func__,__LINE__);
	tk_debug_info(true, &info->client->dev, "%s: users=%d, v:0x%02x, g(%d), f(%d), k(%d)\n", __func__,
		info->input_dev->users, info->fw_ver, info->flip_mode, info->glovemode, info->keyboard_mode);

	//gpio_direction_input(info->pdata->gpio_scl);
	//gpio_direction_input(info->pdata->gpio_sda);

	abov_tk_resume(&info->client->dev);
	if (info->pinctrl)
		abov_pinctrl_configure(info, true);

	if (info->flip_mode){
		abov_mode_enable(info->client, ABOV_FLIP, CMD_FLIP_ON);
	} else {
		if (info->glovemode)
			abov_mode_enable(info->client, ABOV_GLOVE, CMD_GLOVE_ON);
	}
	if (info->keyboard_mode)
		abov_mode_enable(info->client, ABOV_KEYBOARD, CMD_MOBILE_KBD_ON);

	return 0;
}
static void abov_tk_input_close(struct input_dev *dev)
{
	struct abov_tk_info *info = input_get_drvdata(dev);

	tk_debug_dbg(true, &info->client->dev, "%s %d \n",__func__,__LINE__);
	tk_debug_info(true, &info->client->dev, "%s: users=%d\n", __func__,
		   info->input_dev->users);

	abov_tk_suspend(&info->client->dev);
	//gpio_set_value(info->pdata->gpio_scl, 1);
	//gpio_set_value(info->pdata->gpio_sda, 1);
	if (info->pinctrl)
		abov_pinctrl_configure(info, false);

}
#endif

#if 0//defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND) &&!defined(CONFIG_INPUT_ENABLED)
static const struct dev_pm_ops abov_tk_pm_ops = {
	.suspend = abov_tk_suspend,
	.resume = abov_tk_resume,
};
#endif

static const struct i2c_device_id abov_tk_id[] = {
	{ABOV_TK_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, abov_tk_id);

#ifdef CONFIG_OF
static struct of_device_id abov_match_table[] = {
	{ .compatible = "abov,mc96ft16xx",},
	{ },
};
#else
#define abov_match_table NULL
#endif

static struct i2c_driver abov_tk_driver = {
	.probe = abov_tk_probe,
	.remove = abov_tk_remove,
	.shutdown = abov_tk_shutdown,
	.driver = {
		   .name = ABOV_TK_NAME,
		   .of_match_table = abov_match_table,
#if 0//defined(CONFIG_PM) && !defined(CONFIG_HAS_EARLYSUSPEND) &&!defined(CONFIG_INPUT_ENABLED)
		   .pm = &abov_tk_pm_ops,
#endif
	},
	.id_table = abov_tk_id,
};

static int __init touchkey_init(void)
{
	pr_err("%s: abov,mc96ft16xx\n", __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
			pr_notice("%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return 0;
	}
#endif

	return i2c_add_driver(&abov_tk_driver);
}

static void __exit touchkey_exit(void)
{
	i2c_del_driver(&abov_tk_driver);
}

module_init(touchkey_init);
module_exit(touchkey_exit);

/* Module information */
MODULE_AUTHOR("Samsung Electronics");
MODULE_DESCRIPTION("Touchkey driver for Abov MF16xx chip");
MODULE_LICENSE("GPL");
