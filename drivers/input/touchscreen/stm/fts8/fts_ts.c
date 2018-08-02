/******************** (C) COPYRIGHT 2012 STMicroelectronics ********************
*
* File Name		: fts.c
* Authors		: AMS(Analog Mems Sensor) Team
* Description	: FTS Capacitive touch screen controller (FingerTipS)
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
* THIS SOFTWARE IS SPECIFICALLY DESIGNED FOR EXCLUSIVE USE WITH ST PARTS.
********************************************************************************
* REVISON HISTORY
* DATE		| DESCRIPTION
* 03/09/2012| First Release
* 08/11/2012| Code migration
* 23/01/2013| SEC Factory Test
* 29/01/2013| Support Hover Events
* 08/04/2013| SEC Factory Test Add more - hover_enable, glove_mode, clear_cover_mode, fast_glove_mode
* 09/04/2013| Support Blob Information
*******************************************************************************/

#include <linux/init.h>
#include <linux/errno.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/serio.h>
#include <linux/init.h>
#include <linux/pm.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/i2c-dev.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/power_supply.h>
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/input/mt.h>
#ifdef CONFIG_SEC_SYSFS
#include <linux/sec_sysfs.h>
#endif
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/t-base-tui.h>
#endif
#include "fts_ts.h"

#if defined(CONFIG_SECURE_TOUCH)
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/errno.h>
#include <linux/atomic.h>
#include <soc/qcom/scm.h>
/*#include <asm/system.h>*/

enum subsystem {
	TZ = 1,
	APSS = 3
};

#define TZ_BLSP_MODIFY_OWNERSHIP_ID		3

#endif

#ifdef CONFIG_OF
#ifndef USE_OPEN_CLOSE
#define USE_OPEN_CLOSE
#undef CONFIG_PM
#endif
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
struct fts_touchkey fts_touchkeys[] = {
	{
		.value = 0x01,
		.keycode = KEY_RECENT,
		.name = "recent",
	},
	{
		.value = 0x02,
		.keycode = KEY_BACK,
		.name = "back",
	},
};
#endif

#ifdef CONFIG_GLOVE_TOUCH
enum TOUCH_MODE {
	FTS_TM_NORMAL = 0,
	FTS_TM_GLOVE,
};
#endif

#ifdef USE_OPEN_CLOSE
static int fts_input_open(struct input_dev *dev);
static void fts_input_close(struct input_dev *dev);
#ifdef USE_OPEN_DWORK
static void fts_open_work(struct work_struct *work);
#endif
#endif

static int fts_stop_device(struct fts_ts_info *info);
static int fts_start_device(struct fts_ts_info *info);
static void fts_reset_work(struct work_struct *work);
static void fts_delay(unsigned int ms);

static void dump_tsp_rawdata(struct work_struct *work);
struct delayed_work *p_debug_work;

#if (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
static int fts_suspend(struct i2c_client *client, pm_message_t mesg);
static int fts_resume(struct i2c_client *client);
#endif

#if defined(CONFIG_SECURE_TOUCH)
static void fts_secure_touch_notify(struct fts_ts_info *info);

static irqreturn_t fts_filter_interrupt(struct fts_ts_info *info);

static irqreturn_t fts_interrupt_handler(int irq, void *handle);

static ssize_t fts_secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static ssize_t fts_secure_touch_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fts_secure_touch_show(struct device *dev,
			struct device_attribute *attr, char *buf);

static struct device_attribute attrs[] = {
	__ATTR(secure_touch_enable, (S_IRUGO | S_IWUSR | S_IWGRP),
			fts_secure_touch_enable_show,
			fts_secure_touch_enable_store),
	__ATTR(secure_touch, (S_IRUGO),
			fts_secure_touch_show,
			NULL),
};

static int fts_change_pipe_owner(struct fts_ts_info *info, enum subsystem subsystem)
{
	/* scm call disciptor */
	struct scm_desc desc;
	int ret = 0;

	/* number of arguments */
	desc.arginfo = SCM_ARGS(2);
	/* BLSPID (1 - 12) */
	desc.args[0] = info->client->adapter->nr - 1;
	/* Owner if TZ or APSS */
	desc.args[1] = subsystem;

	ret = scm_call2(SCM_SIP_FNID(SCM_SVC_TZ, TZ_BLSP_MODIFY_OWNERSHIP_ID), &desc);
	if (ret) {
		dev_err(&info->client->dev, "%s: return: %d\n", __func__, ret);
		return ret;
	}

	return desc.ret[0];
}


static int fts_secure_touch_clk_prepare_enable(struct fts_ts_info *info)
{
	int ret;

	if (!info->iface_clk || !info->core_clk) {
		input_err(true, &info->client->dev,
			"%s: error clk. iface:%d, core:%d\n", __func__,
			IS_ERR_OR_NULL(info->iface_clk), IS_ERR_OR_NULL(info->core_clk));
		return -ENODEV;
	}

	ret = clk_prepare_enable(info->iface_clk);
	if (ret) {
		input_err(true, &info->client->dev,
			"%s: error on clk_prepare_enable(iface_clk):%d\n", __func__, ret);
		return ret;
	}

	ret = clk_prepare_enable(info->core_clk);
	if (ret) {
		clk_disable_unprepare(info->iface_clk);
		input_err(true, &info->client->dev,
			"%s: error clk_prepare_enable(core_clk):%d\n", __func__, ret);
		return ret;
	}
	return ret;
}

static void fts_secure_touch_clk_disable_unprepare(
		struct fts_ts_info *info)
{
	clk_disable_unprepare(info->core_clk);
	clk_disable_unprepare(info->iface_clk);
}

static ssize_t fts_secure_touch_enable_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	return scnprintf(buf, PAGE_SIZE, "%d", atomic_read(&info->st_enabled));
}

/*
 * Accept only "0" and "1" valid values.
 * "0" will reset the st_enabled flag, then wake up the reading process.
 * The bus driver is notified via pm_runtime that it is not required to stay
 * awake anymore.
 * It will also make sure the queue of events is emptied in the controller,
 * in case a touch happened in between the secure touch being disabled and
 * the local ISR being ungated.
 * "1" will set the st_enabled flag and clear the st_pending_irqs flag.
 * The bus driver is requested via pm_runtime to stay awake.
 */
static ssize_t fts_secure_touch_enable_store(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	unsigned long value;
	int err = 0;

	if (count > 2)
		return -EINVAL;

	err = kstrtoul(buf, 10, &value);
	if (err != 0)
		return err;

	err = count;

	switch (value) {
	case 0:
		if (atomic_read(&info->st_enabled) == 0)
			break;

		fts_change_pipe_owner(info, APSS);

		fts_secure_touch_clk_disable_unprepare(info);

		pm_runtime_put_sync(info->client->adapter->dev.parent);

		atomic_set(&info->st_enabled, 0);

		fts_secure_touch_notify(info);

		fts_delay(10);

		fts_interrupt_handler(info->client->irq, info);

		complete(&info->st_powerdown);
		complete(&info->st_interrupt);

		break;

	case 1:
		if (atomic_read(&info->st_enabled)) {
			err = -EBUSY;
			break;
		}

		/* syncronize_irq -> disable_irq + enable_irq
		 * concern about timing issue.
		 */
		disable_irq(info->client->irq);

		/* Release All Finger */
		fts_release_all_finger(info);

		if (pm_runtime_get_sync(info->client->adapter->dev.parent) < 0) {
			input_err(true, &info->client->dev, "pm_runtime_get failed\n");
			err = -EIO;
			enable_irq(info->client->irq);
			break;
		}

		if (fts_secure_touch_clk_prepare_enable(info) < 0) {
			pm_runtime_put_sync(info->client->adapter->dev.parent);
			err = -EIO;
			enable_irq(info->client->irq);
			break;
		}

		fts_change_pipe_owner(info, TZ);

		reinit_completion(&info->st_powerdown);
		reinit_completion(&info->st_interrupt);

		atomic_set(&info->st_enabled, 1);
		atomic_set(&info->st_pending_irqs, 0);

		enable_irq(info->client->irq);

		break;

	default:
		input_err(true, &info->client->dev, "unsupported value: %lu\n", value);
		err = -EINVAL;
		break;
	}

	return err;
}

static ssize_t fts_secure_touch_show(struct device *dev,
						struct device_attribute *attr, char *buf)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);
	int val = 0;

	input_err(true, &info->client->dev, "%s: SECURETOUCH_fts_secure_touch_show &info->st_pending_irqs=%d ",
								__func__, atomic_read(&info->st_pending_irqs));

	if (atomic_read(&info->st_enabled) == 0)
		return -EBADF;

	if (atomic_cmpxchg(&info->st_pending_irqs, -1, 0) == -1)
		return -EINVAL;

	if (atomic_cmpxchg(&info->st_pending_irqs, 1, 0) == 1)
		val = 1;

	input_err(true, &info->client->dev, "%s %d: SECURETOUCH_fts_secure_touch_show", __func__, val);
	input_err(true, &info->client->dev, "%s: SECURETOUCH_fts_secure_touch_show &info->st_pending_irqs=%d ",
								__func__, atomic_read(&info->st_pending_irqs));
	complete(&info->st_interrupt);

	return scnprintf(buf, PAGE_SIZE, "%u", val);
}

static void fts_secure_touch_init(struct fts_ts_info *info)
{
	int ret;

	init_completion(&info->st_powerdown);
	init_completion(&info->st_interrupt);

	info->core_clk = clk_get(&info->client->adapter->dev, "core_clk");
	if (IS_ERR(info->core_clk)) {
		ret = PTR_ERR(info->core_clk);
		input_err(true, &info->client->dev, "%s: error on clk_get(core_clk):%d\n",
			__func__, ret);
		return;
	}

	info->iface_clk = clk_get(&info->client->adapter->dev, "iface_clk");
	if (IS_ERR(info->iface_clk)) {
		ret = PTR_ERR(info->core_clk);
		input_err(true, &info->client->dev, "%s: error on clk_get(iface_clk):%d\n",
			__func__, ret);
		goto err_iface_clk;
	}

	return;

err_iface_clk:
	clk_put(info->core_clk);
	info->core_clk = NULL;
}

static void fts_secure_touch_stop(struct fts_ts_info *info, int blocking)
{
	if (atomic_read(&info->st_enabled)) {
		atomic_set(&info->st_pending_irqs, -1);
		fts_secure_touch_notify(info);

		if (blocking)
			wait_for_completion_interruptible(&info->st_powerdown);
	}
}

static void fts_secure_touch_notify(struct fts_ts_info *info)
{
	input_info(true, &info->client->dev, "%s: SECURETOUCH_NOTIFY", __func__);
	sysfs_notify(&info->input_dev->dev.kobj, NULL, "secure_touch");
}

static irqreturn_t fts_filter_interrupt(struct fts_ts_info *info)
{
	if (atomic_read(&info->st_enabled)) {
		if (atomic_cmpxchg(&info->st_pending_irqs, 0, 1) == 0) {
			input_info(true, &info->client->dev, "%s: st_pending_irqs: %d\n",
								__func__, atomic_read(&info->st_pending_irqs));
			fts_secure_touch_notify(info);
		}
		return IRQ_HANDLED;
	}
	return IRQ_NONE;
}
#endif

int fts_write_reg(struct fts_ts_info *info,
		  unsigned char *reg, unsigned short num_com)
{
	struct i2c_msg xfer_msg[2];
	int ret;

	if (info->touch_stopped) {
		input_err(true, &info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&info->st_enabled)) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = num_com;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	ret = i2c_transfer(info->client->adapter, xfer_msg, 1);

	mutex_unlock(&info->i2c_mutex);
	return ret;

 exit:
	return 0;
}

int fts_read_reg(struct fts_ts_info *info, unsigned char *reg, int cnum,
		 unsigned char *buf, int num)
{
	struct i2c_msg xfer_msg[2];
	int ret;

	if (info->touch_stopped) {
		input_err(true, &info->client->dev, "%s: Sensor stopped\n", __func__);
		goto exit;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&info->st_enabled)) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	mutex_lock(&info->i2c_mutex);

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = cnum;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = reg;

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = num;
	xfer_msg[1].flags = I2C_M_RD;
	xfer_msg[1].buf = buf;

	ret = i2c_transfer(info->client->adapter, xfer_msg, 2);

	mutex_unlock(&info->i2c_mutex);
	return ret;

 exit:
	return 0;
}

#ifdef FTS_SUPPORT_STRINGLIB
#ifdef CONFIG_SEC_FACTORY
static void fts_disable_string(struct fts_ts_info *info)
{
	unsigned char regAdd[4] = {0xb0, 0x01, 0x10, 0x8f};
	int ret = 0;

	ret = fts_write_reg(info, &regAdd[0], 4);
	input_info(true, &info->client->dev, "String Library Disabled , ret = %d\n", ret);
}
#endif

static int fts_read_from_string(struct fts_ts_info *info,
					unsigned short *reg, unsigned char *data, int length)
{
	unsigned char string_reg[3];
	unsigned char *buf;
	int rtn;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif
#ifdef CONFIG_SECURE_TOUCH
	if (atomic_read(&info->st_enabled)) {
		input_err(true, &info->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	string_reg[0] = 0xD0;
	string_reg[1] = (*reg >> 8) & 0xFF;
	string_reg[2] = *reg & 0xFF;

	buf = kzalloc(length + 1, GFP_KERNEL);
	if (buf == NULL) {
		input_info(true, &info->client->dev,
				"%s: kzalloc error.\n", __func__);
		return -ENOMEM;
	}

	rtn = fts_read_reg(info, string_reg, 3, buf, length + 1);
	if (rtn >= 0)
		memcpy(data, &buf[1], length);

	kfree(buf);
	return rtn;
}

/*
 * int fts_write_to_string(struct fts_ts_info *, unsigned short *, unsigned char *, int)
 * send command or write specfic value to the string area.
 * string area means guest image or display lab firmware.. etc..
 */
static int fts_write_to_string(struct fts_ts_info *info,
					unsigned short *reg, unsigned char *data, int length)
{
	struct i2c_msg xfer_msg[3];
	unsigned char *regAdd;
	int ret;

	if (info->touch_stopped) {
		input_err(true, &info->client->dev, "%s: Sensor stopped\n", __func__);
		return 0;
	}

	regAdd = kzalloc(length + 6, GFP_KERNEL);
	if (regAdd == NULL) {
		input_info(true, &info->client->dev, "%s: kzalloc error.\n", __func__);
		return -ENOMEM;
	}

	mutex_lock(&info->i2c_mutex);

/* msg[0], length 3*/
	regAdd[0] = 0xB3;
	regAdd[1] = 0x20;
	regAdd[2] = 0x01;

	xfer_msg[0].addr = info->client->addr;
	xfer_msg[0].len = 3;
	xfer_msg[0].flags = 0;
	xfer_msg[0].buf = &regAdd[0];
/* msg[0], length 3*/

/* msg[1], length 4*/
	regAdd[3] = 0xB1;
	regAdd[4] = (*reg >> 8) & 0xFF;
	regAdd[5] = *reg & 0xFF;

	memcpy(&regAdd[6], data, length);

/*regAdd[3] : B1 address, [4], [5] : String Address, [6]...: data */

	xfer_msg[1].addr = info->client->addr;
	xfer_msg[1].len = 3 + length;
	xfer_msg[1].flags = 0;
	xfer_msg[1].buf = &regAdd[3];
/* msg[1], length 4*/

	ret = i2c_transfer(info->client->adapter, xfer_msg, 2);
	if (ret == 2) {
		input_info(true, &info->client->dev,
				"%s: string command is OK.\n", __func__);

		regAdd[0] = FTS_CMD_NOTIFY;
		regAdd[1] = *reg & 0xFF;
		regAdd[2] = (*reg >> 8) & 0xFF;

		xfer_msg[0].addr = info->client->addr;
		xfer_msg[0].len = 3;
		xfer_msg[0].flags = 0;
		xfer_msg[0].buf = regAdd;

		ret = i2c_transfer(info->client->adapter, xfer_msg, 1);
		if (ret != 1)
			input_info(true, &info->client->dev,
					"%s: string notify is failed.\n", __func__);
		else
			input_info(true, &info->client->dev,
					"%s: string notify is OK[%X].\n", __func__, *data);

	} else
		input_info(true, &info->client->dev,
				"%s: string command is failed. ret: %d\n", __func__, ret);

	mutex_unlock(&info->i2c_mutex);
	kfree(regAdd);

	return ret;
}
#endif

static void fts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

void fts_command(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd = 0;
	int ret = 0;

	regAdd = cmd;
	ret = fts_write_reg(info, &regAdd, 1);
	input_info(true, &info->client->dev, "FTS Command (%02X) , ret = %d\n", cmd, ret);
}

void fts_enable_feature(struct fts_ts_info *info, unsigned char cmd, int enable)
{
	unsigned char regAdd[2] = {0xC1, 0x00};
	int ret = 0;

	if (!enable)
		regAdd[0] = 0xC2;
	regAdd[1] = cmd;

	ret = fts_write_reg(info, &regAdd[0], 2);
	input_info(true, &info->client->dev, "FTS %s Feature (%02X %02X) , ret = %d\n",
		(enable) ? "Enable":"Disable", regAdd[0], regAdd[1], ret);
}

static void fts_set_cover_type(struct fts_ts_info *info, bool enable)
{
	input_info(true, &info->client->dev, "%s: %d\n", __func__, info->cover_type);

	switch (info->cover_type) {
	case FTS_VIEW_WIRELESS:
	case FTS_VIEW_COVER:
		fts_enable_feature(info, FTS_FEATURE_COVER_GLASS, enable);
		break;
	case FTS_VIEW_WALLET:
		fts_enable_feature(info, FTS_FEATURE_COVER_WALLET, enable);
		break;
	case FTS_FLIP_WALLET:
	case FTS_LED_COVER:
	case FTS_MONTBLANC_COVER:
		fts_enable_feature(info, FTS_FEATURE_COVER_LED, enable);
		break;
	case FTS_CLEAR_FLIP_COVER:
		fts_enable_feature(info, FTS_FEATURE_COVER_CLEAR_FLIP, enable);
		break;
	case FTS_QWERTY_KEYBOARD_EUR:
	case FTS_QWERTY_KEYBOARD_KOR:
		fts_enable_feature(info, 0x0D, enable);
		break;
	case FTS_CHARGER_COVER:
	case FTS_COVER_NOTHING1:
	case FTS_COVER_NOTHING2:
	default:
		input_err(true, &info->client->dev, "%s: not change touch state, %d\n",
				__func__, info->cover_type);
		break;
	}
}

void fts_change_scan_rate(struct fts_ts_info *info, unsigned char cmd)
{
	unsigned char regAdd[2] = {0xC3, 0x00};
	int ret = 0;

	regAdd[1] = cmd;
	ret = fts_write_reg(info, &regAdd[0], 2);

	input_dbg(true, &info->client->dev, "FTS %s Scan Rate (%02X %02X) , ret = %d\n",
		(cmd == FTS_CMD_FAST_SCAN) ? "90Hz" : (cmd == FTS_CMD_SLOW_SCAN) ? "60Hz" : "30Hz",
		regAdd[0], regAdd[1], ret);
}

void fts_systemreset(struct fts_ts_info *info)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x28, 0x80 };

	fts_write_reg(info, &regAdd[0], 4);
	fts_delay(10);
}

void fts_interrupt_set(struct fts_ts_info *info, int enable)
{
	unsigned char regAdd[4] = { 0xB6, 0x00, 0x2C, enable };

	if (enable) {
		regAdd[3] = INT_ENABLE_D3;
		input_info(true, &info->client->dev, "FTS INT Enable\n");
	} else {
		regAdd[3] = INT_DISABLE_D3;
		input_info(true, &info->client->dev, "FTS INT Disable\n");
	}

	fts_write_reg(info, &regAdd[0], 4);
}

static int fts_read_chip_id(struct fts_ts_info *info)
{
	unsigned char regAdd[3] = {0xB6, 0x00, 0x04};
	unsigned char val[7] = {0};
	int ret;

	ret = fts_read_reg(info, regAdd, 3, (unsigned char *)val, 7);
	if (ret < 0) {
		input_err(true, &info->client->dev, "%s failed. ret: %d\n",
			__func__, ret);
		return ret;
	}

	input_info(true, &info->client->dev, "FTS %02X%02X%02X =  %02X %02X %02X %02X %02X %02X\n",
	       regAdd[0], regAdd[1], regAdd[2], val[1], val[2], val[3], val[4], val[5], val[6]);

	if (val[1] == FTS_ID0 && val[2] == FTS_ID1)
		input_info(true, &info->client->dev, "FTS Chip ID : %02X %02X\n", val[1], val[2]);
	else
		return -FTS_ERROR_INVALID_CHIP_ID;

	return ret;
}

int fts_read_analog_chip_id(struct fts_ts_info *info, unsigned char id)
{
	unsigned char regAdd[6] = {0xB3, 0x20, 0x07, 0x00, 0x00, 0x00};
	unsigned char val[4] = {0};
	int ret;

	switch(id) {
	case ANALOG_ID_FTS8:
		ret = fts_write_reg(info, &regAdd[0], 3);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		regAdd[0] = 0xB1;
		regAdd[1] = 0x00;
		regAdd[2] = 0x84;
		ret = fts_write_reg(info, &regAdd[0], 6);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		regAdd[0] = 0xB1;
		regAdd[1] = 0x00;
		regAdd[2] = 0x04;
		ret = fts_read_reg(info, regAdd, 3, (unsigned char *)val, 4);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		input_info(true, &info->client->dev, "FTS %02X%02X%02X =  %02X %02X %02X\n",
			   regAdd[0], regAdd[1], regAdd[2], val[1], val[2], val[3]);

		if (val[1] == id) {
			input_info(true, &info->client->dev, "FTS8 %02X %02X\n", val[1], val[2]);
			ret = 1;
		}
		break;
	case ANALOG_ID_FTS9:
		regAdd[0] = 0xB6;
		regAdd[1] = 0x00;
		regAdd[2] = 0x89;
		regAdd[3] = 0x14;
		ret = fts_write_reg(info, &regAdd[0], 4);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		regAdd[0] = 0xB3;
		regAdd[1] = 0x20;
		regAdd[2] = 0x03;
		ret = fts_write_reg(info, &regAdd[0], 3);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		regAdd[0] = 0xB1;
		regAdd[1] = 0xF8;
		regAdd[2] = 0x00;
		ret = fts_read_reg(info, regAdd, 3, (unsigned char *)val, 4);
		if (ret < 0) {
			input_err(true, &info->client->dev, "%s failed. ret: %d\n",
				__func__, ret);
			return ret;
		}

		input_info(true, &info->client->dev, "FTS %02X%02X%02X =  %02X %02X %02X\n",
		       regAdd[0], regAdd[1], regAdd[2], val[1], val[2], val[3]);

		if (val[1] == id) {
			input_info(true, &info->client->dev, "FTS9 %02X %02X\n", val[1], val[2]);
			ret = 0;
		}
		break;
	default:
		break;
	}
	return ret;
}

static int fts_wait_for_ready(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd;
	unsigned char data[FTS_EVENT_SIZE];
	int retry = 0;
	int err_cnt = 0;

	memset(data, 0x0, FTS_EVENT_SIZE);

	regAdd = READ_ONE_EVENT;
	rc = -1;
	while (fts_read_reg(info, &regAdd, 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
		if (data[0] == EVENTID_CONTROLLER_READY) {
			rc = 0;
			break;
		}

		if (data[0] == EVENTID_ERROR) {
			if (err_cnt++ > 32) {
				rc = -FTS_ERROR_EVENT_ID;
				break;
			}
			continue;
		}

		if (retry++ > FTS_RETRY_COUNT) {
			rc = -FTS_ERROR_TIMEOUT;
			input_err(true, &info->client->dev, "%s: Time Over\n", __func__);
			if (info->lowpower_mode)
				schedule_delayed_work(&info->reset_work, msecs_to_jiffies(10));

			break;
		}
		fts_delay(20);
	}

	input_info(true, &info->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X\n",
		__func__, data[0], data[1], data[2], data[3],
		data[4], data[5], data[6], data[7]);

	return rc;
}

int fts_get_sysinfo_data(struct fts_ts_info *info, unsigned char sysinfo_addr, unsigned char read_cnt, unsigned char *data)
{
	int ret;
	unsigned char regAdd[3] = { 0xD0, 0x00, sysinfo_addr };
	unsigned char *buff;

	buff = kzalloc(read_cnt + 1, GFP_KERNEL);
	if (!buff)
		return -ENOMEM;

	ret = fts_read_reg(info, &regAdd[0], 3, buff, read_cnt + 1);
	if (ret <= 0) {
		input_err(true, &info->client->dev, "%s failed. ret: %d\n",
			__func__, ret);
		kfree(buff);
		return ret;
	}

	memcpy(data, &buff[1], read_cnt);

	kfree(buff);
	return ret;
}

int fts_get_version_info(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd[3];
	unsigned char data[FTS_EVENT_SIZE] = { 0 };
	int retry = 0;

	fts_command(info, FTS_CMD_RELEASEINFO);

	memset(data, 0x0, FTS_EVENT_SIZE);

	regAdd[0] = READ_ONE_EVENT;
	rc = -1;
	while (fts_read_reg(info, &regAdd[0], 1, (unsigned char *)data, FTS_EVENT_SIZE)) {
		if (data[0] == EVENTID_INTERNAL_RELEASE_INFO) {
			/* Internal release Information */
			info->fw_version_of_ic = (data[3] << 8) + data[4];
			info->config_version_of_ic = (data[6] << 8) + data[5];
			info->ic_product_id = data[2];
		} else if (data[0] == EVENTID_EXTERNAL_RELEASE_INFO) {
			/* External release Information */
			info->fw_main_version_of_ic = (data[1] << 8) + data[2];
			rc = 0;
			break;
		}

		if (retry++ > FTS_RETRY_COUNT) {
			rc = -FTS_ERROR_TIMEOUT;
			input_err(true, &info->client->dev, "%s: Time Over\n", __func__);
			break;
		}
	}

	rc = fts_get_sysinfo_data(info, FTS_SI_PURE_AUTOTUNE_CONFIG, 4, data);
	if (rc <= 0) {
		info->afe_ver = 0;
		input_info(true, info->dev, "%s: Read Fail - Final AFE [Data : %2X] AFE Ver [Data : %2X]\n",
			__func__, data[0], data[1]);
	} else {
		info->afe_ver = data[1];
	}

	input_info(true, &info->client->dev,
		"[IC] product id: 0x%02X, Firmware Ver: 0x%04X, Config Ver: 0x%04X, Main Ver: 0x%04X, AFE Ver: 0x%02X\n",
		info->ic_product_id, info->fw_version_of_ic,
		info->config_version_of_ic, info->fw_main_version_of_ic,
		info->afe_ver);

	return rc;
}

#ifdef FTS_SUPPORT_NOISE_PARAM
int fts_get_noise_param_address(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd[3];
	unsigned char data[6] = { 0 };
	struct fts_noise_param *noise_param;

	noise_param = (struct fts_noise_param *)&info->noise_param;

	regAdd[0] = 0xD0;
	regAdd[1] = 0x00;
	regAdd[2] = FTS_SI_NOISE_PARAM_ADDR;

	rc = fts_read_reg(info, regAdd, 3, (unsigned char *)data, 4);

	noise_param->pAddr = (unsigned short)(data[0] + (data[1] << 8));

	return rc;
}

static int fts_get_noise_param(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd[3];
	unsigned char data[6] = { 0 };
	struct fts_noise_param *noise_param;

	noise_param = (struct fts_noise_param *)&info->noise_param;

	regAdd[0] = 0xB3;
	regAdd[1] = 0x00;
	regAdd[2] = 0x10;
	rc = fts_write_reg(info, regAdd, 3);

	regAdd[0] = 0xB1;
	regAdd[1] = (noise_param->pAddr >> 8) & 0xff;
	regAdd[2] = noise_param->pAddr & 0xff;
	rc = fts_read_reg(info, regAdd, 3, data, 5);

	noise_param->bestRIdx = data[1];
	noise_param->mtNoiseLvl = data[2];
	noise_param->sstNoiseLvl = data[3];
	noise_param->bestRMutual = data[4];

	return rc;
}

static int fts_set_noise_param(struct fts_ts_info *info)
{
	int rc;
	unsigned char regAdd[7];
	struct fts_noise_param *noise_param;

	noise_param = (struct fts_noise_param *)&info->noise_param;

	regAdd[0] = 0xB3;
	regAdd[1] = 0x00;
	regAdd[2] = 0x10;
	rc = fts_write_reg(info, regAdd, 3);

	regAdd[0] = 0xB1;
	regAdd[1] = (noise_param->pAddr >> 8) & 0xff;
	regAdd[2] = noise_param->pAddr & 0xff;
	regAdd[3] = noise_param->bestRIdx;
	regAdd[4] = noise_param->mtNoiseLvl;
	regAdd[5] = noise_param->sstNoiseLvl;
	regAdd[6] = noise_param->bestRMutual;

	rc = fts_write_reg(info, regAdd, 7);

	return 0;
}
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
void fts_release_all_key(struct fts_ts_info *info)
{
	unsigned char key_recent = TOUCH_KEY_RECENT;
	unsigned char key_back = TOUCH_KEY_BACK;

	if (info->board->support_mskey && info->tsp_keystatus != TOUCH_KEY_NULL) {
		if (info->tsp_keystatus & key_recent) {
			input_report_key(info->input_dev, KEY_RECENT, KEY_RELEASE);
			input_info(true, &info->client->dev, "[TSP_KEY] Recent R!\n");
		}

		if (info->tsp_keystatus & key_back) {
			input_report_key(info->input_dev, KEY_BACK, KEY_RELEASE);
			input_info(true, &info->client->dev, "[TSP_KEY] back R!\n");
		}

		input_sync(info->input_dev);

		info->tsp_keystatus = TOUCH_KEY_NULL;
	}
}
#endif

/* Added for samsung dependent codes such as Factory test,
 * Touch booster, Related debug sysfs.
 */
#include "fts_sec.c"

static int fts_init(struct fts_ts_info *info)
{
	unsigned char retry = 3;
	unsigned char val[16];
	unsigned char regAdd[8];
	int rc;

	do {
		fts_systemreset(info);

		rc = fts_wait_for_ready(info);
		if (rc != FTS_NOT_ERROR) {
			if (info->board->power)
				info->board->power(info, false);

			fts_delay(20);

			if (info->board->power)
				info->board->power(info, true);

			fts_delay(5);

			if (rc == -FTS_ERROR_EVENT_ID) {
				info->fw_version_of_ic = 0;
				info->config_version_of_ic = 0;
				info->fw_main_version_of_ic = 0;
			}
		} else {
			fts_get_version_info(info);
			break;
		}
	} while (retry--);

	if (retry == 0) {
		input_info(true, &info->client->dev, "%s: Failed to system reset\n", __func__);
		return FTS_ERROR_TIMEOUT;
	}

	rc = fts_read_chip_id(info);
	if (rc < 0) {
		bool temp_lpm;

		temp_lpm = info->lowpower_mode;
		/* Reset-routine must go to power off state  */
		info->lowpower_mode = 0;
		input_info(true, &info->client->dev, "%s, Call Power-Off to recover IC, lpm:%d\n", __func__, temp_lpm);
		fts_stop_device(info);
		fts_delay(500);	/* Delay to discharge the IC from ESD or On-state.*/
		if (fts_start_device(info) < 0)
			input_err(true, &info->client->dev, "%s: Failed to start device\n", __func__);

		info->lowpower_mode = temp_lpm;
		input_err(true, &info->client->dev, "%s: Reset caused by chip id error\n",
				__func__);
	}

	rc = fts_read_chip_id(info);
	if (rc < 0)
		return 1;

	rc  = fts_fw_update_on_probe(info);
	if (rc  < 0)
		input_err(true, &info->client->dev, "%s: Failed to firmware update\n",
				__func__);

#ifdef SEC_TSP_FACTORY_TEST
		rc = fts_get_channel_info(info);
		if (rc >= 0) {
			input_info(true, &info->client->dev, "FTS Sense(%02d) Force(%02d)\n",
				   info->SenseChannelLength, info->ForceChannelLength);
		} else {
			input_info(true, &info->client->dev, "FTS read failed rc = %d\n", rc);
			input_info(true, &info->client->dev, "FTS Initialise Failed\n");
			return 1;
		}
		info->pFrame =
		kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2, GFP_KERNEL);
		if (info->pFrame == NULL) {
			input_info(true, &info->client->dev, "FTS pFrame kzalloc Failed\n");
			return 1;
		}
		info->cx_data = kzalloc(info->SenseChannelLength * info->ForceChannelLength, GFP_KERNEL);
		if (!info->cx_data)
			input_err(true, &info->client->dev, "%s: cx_data kzalloc Failed\n", __func__);
#if defined(FTS_SUPPORT_STRINGLIB) && defined(CONFIG_SEC_FACTORY)
		fts_disable_string(info);
#endif
#endif
	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_PRESSURE_SENSOR
	fts_command(info, FTS_CMD_PRESSURE_SENSE_ON);
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_get_noise_param_address(info);
#endif
	/* fts driver set functional feature */
	info->touch_count = 0;
#ifdef FTS_SUPPORT_HOVER
	info->hover_enabled = false;
	info->hover_ready = false;
#endif
	info->flip_enable = false;
	info->mainscr_disable = false;

	info->deepsleep_mode = false;
	info->wirelesscharger_mode = false;
//	info->lowpower_mode = true;
	info->lowpower_mode = false;

	info->lowpower_flag = 0x00;
	info->fts_power_state = 0;

#ifdef FTS_SUPPORT_TOUCH_KEY
	info->tsp_keystatus = 0x00;
#endif

	fts_command(info, FORCECALIBRATION);

	fts_interrupt_set(info, INT_ENABLE);

	memset(val, 0x0, 4);
	regAdd[0] = READ_STATUS;
	fts_read_reg(info, regAdd, 1, (unsigned char *)val, 4);
	input_info(true, &info->client->dev, "FTS ReadStatus(0x84) : %02X %02X %02X %02X\n", val[0],
	       val[1], val[2], val[3]);

	memset(val, 0x0, 4);
	fts_get_sysinfo_data(info, FTS_SI_REPORT_PRESSURE_RAW_DATA, 2, val);
	if (val[0] == 1)
		info->report_pressure = true;

	input_info(true, &info->client->dev, "FTS Initialized\n");

	return 0;
}

static void fts_debug_msg_event_handler(struct fts_ts_info *info,
				      unsigned char data[])
{
	input_dbg(true, &info->client->dev,
	       "%s: %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
	       data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
}

static unsigned char fts_event_handler_type_b(struct fts_ts_info *info,
					      unsigned char data[],
					      unsigned char LeftEvent)
{
	unsigned char EventNum = 0;
	unsigned char NumTouches = 0;
	unsigned char TouchID = 0, EventID = 0, status = 0, event_type = 0;
	unsigned char LastLeftEvent = 0;
	int x = 0, y = 0, z = 0;
	int bw = 0, bh = 0, palm = 0, sumsize = 0;
#ifdef FTS_SUPPORT_PRESSURE_SENSOR
	int pressure = 0;
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	static int longpress_release[FINGER_MAX] = {0, };
#endif
#ifdef FTS_SUPPORT_STRINGLIB
	unsigned short string_addr;
	unsigned char string;
#endif

	for (EventNum = 0; EventNum < LeftEvent; EventNum++) {

		/* for event debugging */
		if (info->debug_string & 0x1)
			input_info(true, &info->client->dev, "[%d] %2x %2x %2x %2x %2x %2x %2x %2x\n",
				EventNum, data[EventNum * FTS_EVENT_SIZE], data[EventNum * FTS_EVENT_SIZE+1],
				data[EventNum * FTS_EVENT_SIZE+2], data[EventNum * FTS_EVENT_SIZE+3],
				data[EventNum * FTS_EVENT_SIZE+4], data[EventNum * FTS_EVENT_SIZE+5],
				data[EventNum * FTS_EVENT_SIZE+6], data[EventNum * FTS_EVENT_SIZE+7]);

		EventID = data[EventNum * FTS_EVENT_SIZE] & 0x0F;

		if ((EventID >= 3) && (EventID <= 5)) {
			LastLeftEvent = 0;
			NumTouches = 1;

			TouchID = (data[EventNum * FTS_EVENT_SIZE] >> 4) & 0x0F;
		} else {
			LastLeftEvent =
			    data[7 + EventNum * FTS_EVENT_SIZE] & 0x0F;
			NumTouches =
			    (data[1 + EventNum * FTS_EVENT_SIZE] & 0xF0) >> 4;
			TouchID = data[1 + EventNum * FTS_EVENT_SIZE] & 0x0F;
			EventID = data[EventNum * FTS_EVENT_SIZE] & 0xFF;
			status = data[1 + EventNum * FTS_EVENT_SIZE] & 0xFF;
		}

		event_type = data[1 + EventNum * FTS_EVENT_SIZE];

		switch (EventID) {
		case EVENTID_NO_EVENT:
			break;

		case EVENTID_GESTURE_WAKEUP:
			input_info(true, &info->client->dev, "EVENTID_GESTURE_WAKEUP detected![EventID=%x]\n", EventID);

			/* defined in Istor & JF-synaptics */
			info->scrub_id = 0x07;

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
			input_sync(info->input_dev);
			break;

		case EVENTID_PRESSURE:
			if (event_type == 0x01)
				info->scrub_id = SPECIAL_EVENT_TYPE_PRESSURE_TOUCHED;
			else
				info->scrub_id = SPECIAL_EVENT_TYPE_PRESSURE_RELEASED;

			info->scrub_x = (data[4] & 0xFF) << 8 | (data[3] & 0xFF);
			info->scrub_y = (data[6] & 0xFF) << 8 | (data[5] & 0xFF);

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

			input_info(true, &info->client->dev,
				"%s: [PRESSURE] %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
				data[EventNum * FTS_EVENT_SIZE],
				data[EventNum * FTS_EVENT_SIZE+1],
				data[EventNum * FTS_EVENT_SIZE+2],
				data[EventNum * FTS_EVENT_SIZE+3],
				data[EventNum * FTS_EVENT_SIZE+4],
				data[EventNum * FTS_EVENT_SIZE+5],
				data[EventNum * FTS_EVENT_SIZE+6],
				data[EventNum * FTS_EVENT_SIZE+7]);
			break;

		case EVENTID_GESTURE_HOME:
			info->scrub_id = SPECIAL_EVENT_TYPE_AOD_HOMEKEY;
			info->scrub_x = (data[4] & 0xFF) << 8 | (data[3] & 0xFF);
			info->scrub_y = (data[6] & 0xFF) << 8 | (data[5] & 0xFF);

			if (event_type == 0x01)
				input_report_key(info->input_dev, KEY_HOMEPAGE, 1);
			else
				input_report_key(info->input_dev, KEY_HOMEPAGE, 0);

			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);

			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_HOMEPAGE, 0);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

			input_info(true, &info->client->dev,
				"%s: [HOME key] %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
				data[EventNum * FTS_EVENT_SIZE],
				data[EventNum * FTS_EVENT_SIZE+1],
				data[EventNum * FTS_EVENT_SIZE+2],
				data[EventNum * FTS_EVENT_SIZE+3],
				data[EventNum * FTS_EVENT_SIZE+4],
				data[EventNum * FTS_EVENT_SIZE+5],
				data[EventNum * FTS_EVENT_SIZE+6],
				data[EventNum * FTS_EVENT_SIZE+7]);
			break;

#ifdef FTS_SUPPORT_TOUCH_KEY
		case EVENTID_MSKEY:
			if (info->board->support_mskey) {
				unsigned char input_keys;

				input_keys = data[2 + EventNum * FTS_EVENT_SIZE];

				if (input_keys == 0x00)
					fts_release_all_key(info);
				else {
					unsigned char change_keys;
					unsigned char key_state;
					unsigned char key_recent = TOUCH_KEY_RECENT;
					unsigned char key_back = TOUCH_KEY_BACK;

					change_keys = input_keys ^ info->tsp_keystatus;

					if (change_keys & key_recent) {
						key_state = input_keys & key_recent;

						input_report_key(info->input_dev, KEY_RECENT, key_state != 0 ? KEY_PRESS : KEY_RELEASE);
						input_info(true, &info->client->dev, "[TSP_KEY] RECENT %s\n", key_state != 0 ? "P" : "R");
					}

					if (change_keys & key_back) {
						key_state = input_keys & key_back;

						input_report_key(info->input_dev, KEY_BACK, key_state != 0 ? KEY_PRESS : KEY_RELEASE);
						input_info(true, &info->client->dev, "[TSP_KEY] BACK %s\n", key_state != 0 ? "P" : "R");
					}

					input_sync(info->input_dev);
				}

				info->tsp_keystatus = input_keys;
			}
			break;
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
		case EVENTID_SIDE_TOUCH:
		case EVENTID_SIDE_TOUCH_DEBUG:
			if ((event_type == FTS_SIDEGESTURE_EVENT_SINGLE_STROKE) ||
				(event_type == FTS_SIDEGESTURE_EVENT_DOUBLE_STROKE) ||
				(event_type == FTS_SIDEGESTURE_EVENT_INNER_STROKE)) {
				int direction, distance;
				direction = data[2 + EventNum * FTS_EVENT_SIZE];
				distance = *(int *)&data[3 + EventNum * FTS_EVENT_SIZE];

				if (direction)
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 1);
				else
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 1);


				input_info(true, &info->client->dev,
					"%s: [Gesture] %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
					data[EventNum * FTS_EVENT_SIZE],
					data[EventNum * FTS_EVENT_SIZE+1],
					data[EventNum * FTS_EVENT_SIZE+2],
					data[EventNum * FTS_EVENT_SIZE+3],
					data[EventNum * FTS_EVENT_SIZE+4],
					data[EventNum * FTS_EVENT_SIZE+5],
					data[EventNum * FTS_EVENT_SIZE+6],
					data[EventNum * FTS_EVENT_SIZE+7]);

				input_sync(info->input_dev);
				fts_delay(1);

				if (direction)
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
				else
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
			}
			else if (event_type == FTS_SIDETOUCH_EVENT_LONG_PRESS) {
				 int sideLongPressfingerID = 0;
				 sideLongPressfingerID = data[2 + EventNum * FTS_EVENT_SIZE];

				 //Todo : event processing
				longpress_release[sideLongPressfingerID - 1] = 1;

				input_info(true, &info->client->dev,
					"%s: [Side Long Press] id:%d %02X %02X %02X %02X %02X %02X %02X %02X\n",
					__func__, sideLongPressfingerID,
					data[EventNum * FTS_EVENT_SIZE],
					data[EventNum * FTS_EVENT_SIZE+1],
					data[EventNum * FTS_EVENT_SIZE+2],
					data[EventNum * FTS_EVENT_SIZE+3],
					data[EventNum * FTS_EVENT_SIZE+4],
					data[EventNum * FTS_EVENT_SIZE+5],
					data[EventNum * FTS_EVENT_SIZE+6],
					data[EventNum * FTS_EVENT_SIZE+7]);
			}
			else if(event_type == FTS_SIDETOUCH_EVENT_REBOOT_BY_ESD) {
				input_info(true, &info->client->dev,
					"%s: ESD detected!  %02X %02X %02X %02X %02X %02X %02X %02X\n", __func__,
					data[EventNum * FTS_EVENT_SIZE],
					data[EventNum * FTS_EVENT_SIZE+1],
					data[EventNum * FTS_EVENT_SIZE+2],
					data[EventNum * FTS_EVENT_SIZE+3],
					data[EventNum * FTS_EVENT_SIZE+4],
					data[EventNum * FTS_EVENT_SIZE+5],
					data[EventNum * FTS_EVENT_SIZE+6],
					data[EventNum * FTS_EVENT_SIZE+7]);
				
				schedule_delayed_work(&info->reset_work, msecs_to_jiffies(10));
			}
			else {
				fts_debug_msg_event_handler(info,
						  &data[EventNum *
							FTS_EVENT_SIZE]);
			}
			break;
#endif
		case EVENTID_ERROR:
			if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x08) {
				/* Get Auto tune fail event */
				if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x00)
					input_info(true, &info->client->dev, "[FTS] Fail Mutual Auto tune\n");
				else if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x01)
					input_info(true, &info->client->dev, "[FTS] Fail Self Auto tune\n");
			} else if (data[1 + EventNum * FTS_EVENT_SIZE] == 0x09) {
				/* Get detect SYNC fail event */
				input_info(true, &info->client->dev, "[FTS] Fail detect SYNC\n");
			}
			break;
#ifdef FTS_SUPPORT_HOVER
		case EVENTID_HOVER_ENTER_POINTER:
		case EVENTID_HOVER_MOTION_POINTER:
			x = ((data[4 + EventNum * FTS_EVENT_SIZE] & 0xF0) >> 4)
			    | ((data[2 + EventNum * FTS_EVENT_SIZE]) << 4);
			y = ((data[4 + EventNum * FTS_EVENT_SIZE] & 0x0F) |
			     ((data[3 + EventNum * FTS_EVENT_SIZE]) << 4));

			z = data[5 + EventNum * FTS_EVENT_SIZE];

			input_mt_slot(info->input_dev, 0);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 1);

			input_report_key(info->input_dev, BTN_TOUCH, 0);
			input_report_key(info->input_dev, BTN_TOOL_FINGER, 1);

			input_report_abs(info->input_dev, ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev, ABS_MT_POSITION_Y, y);
			input_report_abs(info->input_dev, ABS_MT_DISTANCE, 255 - z);
			break;

		case EVENTID_HOVER_LEAVE_POINTER:
			input_mt_slot(info->input_dev, 0);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);
			break;
#endif
		case EVENTID_ENTER_POINTER:
			if (info->fts_power_state == FTS_POWER_STATE_LOWPOWER)
				break;

			info->touch_count++;
		case EVENTID_MOTION_POINTER:
			if (info->fts_power_state == FTS_POWER_STATE_LOWPOWER) {
				input_info(true, &info->client->dev, "%s: low power mode\n", __func__);
				fts_release_all_finger(info);
				break;
			}

			if (info->touch_count == 0) {
				input_info(true, &info->client->dev, "%s: count 0\n", __func__);
				fts_release_all_finger(info);
				break;
			}

			if ((EventID == EVENTID_MOTION_POINTER) &&
				(info->finger[TouchID].state == EVENTID_LEAVE_POINTER)) {
				input_info(true, &info->client->dev, "%s: state leave but point is moved.\n", __func__);
				break;
			}

			if (info->fts_power_state == FTS_POWER_STATE_LOWPOWER)
				break;

			x = data[1 + EventNum * FTS_EVENT_SIZE] +
			    ((data[2 + EventNum * FTS_EVENT_SIZE] &
			      0x0f) << 8);
			y = ((data[2 + EventNum * FTS_EVENT_SIZE] &
			      0xf0) >> 4) + (data[3 +
						  EventNum *
						  FTS_EVENT_SIZE] << 4);
			bw = data[4 + EventNum * FTS_EVENT_SIZE];
			bh = data[5 + EventNum * FTS_EVENT_SIZE];
			palm =
			    (data[6 + EventNum * FTS_EVENT_SIZE] >> 7) & 0x01;

			sumsize = (data[6 + EventNum * FTS_EVENT_SIZE] & 0x7f) << 1;

			if ((info->touch_count == 1) && (sumsize < 40))
				sumsize = 39;

			z = data[7 + EventNum * FTS_EVENT_SIZE];

			input_report_key(info->input_dev, BTN_TOUCH, 1);
			input_mt_slot(info->input_dev, TouchID);
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER,
						   1 + (palm << 1));

			input_report_key(info->input_dev,
					 BTN_TOOL_FINGER, 1);
			input_report_abs(info->input_dev,
					 ABS_MT_POSITION_X, x);
			input_report_abs(info->input_dev,
					 ABS_MT_POSITION_Y, y);

			input_report_abs(info->input_dev,
					 ABS_MT_TOUCH_MAJOR, max(bw,
								 bh));

			input_report_abs(info->input_dev,
					 ABS_MT_TOUCH_MINOR, min(bw,
								 bh));

#ifdef FTS_SUPPORT_SEC_SWIPE
			input_report_abs(info->input_dev, ABS_MT_PALM,
					 palm);
#endif
#if defined(FTS_SUPPORT_SIDE_GESTURE)
			if (info->board->support_sidegesture)
				input_report_abs(info->input_dev, ABS_MT_GRIP, 0);
#endif

#ifdef FTS_SUPPORT_PRESSURE_SENSOR
			pressure = fts_read_pressure_data(info);
			if (pressure <= 0)
				pressure = 1;

			if ((info->finger[TouchID].lx != x) || (info->finger[TouchID].ly != y))
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, pressure);
			else
				input_report_abs(info->input_dev, ABS_MT_PRESSURE, info->finger[TouchID].lp);
#endif

#ifdef CONFIG_SEC_FACTORY
			input_report_abs(info->input_dev,
					 ABS_MT_PRESSURE, z & 0xFF);
#endif

			info->finger[TouchID].lx = x;
			info->finger[TouchID].ly = y;
#ifdef FTS_SUPPORT_PRESSURE_SENSOR
			info->finger[TouchID].lp = pressure;
#endif

			break;

		case EVENTID_LEAVE_POINTER:
			if (info->fts_power_state == FTS_POWER_STATE_LOWPOWER)
				break;

			if (info->touch_count <= 0) {
				input_info(true, &info->client->dev, "%s: count 0\n", __func__);
				fts_release_all_finger(info);
				break;
			}

			info->touch_count--;

			input_mt_slot(info->input_dev, TouchID);

#if defined(FTS_SUPPORT_SIDE_GESTURE)
			if (info->board->support_sidegesture) {
				if (longpress_release[TouchID] == 1) {
					input_report_abs(info->input_dev, ABS_MT_GRIP, 1);
					input_info(true, &info->client->dev,
						"[FTS] GRIP [%d] %s\n", TouchID,
						longpress_release[TouchID] ? "LONGPRESS" : "RELEASE");
					longpress_release[TouchID] = 0;

					input_sync(info->input_dev);
				}
			}
#endif
#if defined(CONFIG_SEC_FACTORY) || defined(FTS_SUPPORT_PRESSURE_SENSOR)
			input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
#endif
			input_mt_report_slot_state(info->input_dev,
						   MT_TOOL_FINGER, 0);

			if (info->touch_count == 0) {
				/* Clear BTN_TOUCH when All touch are released  */
				input_report_key(info->input_dev, BTN_TOUCH, 0);
				input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);
#ifdef FTS_SUPPORT_SIDE_GESTURE
				if (info->board->support_sidegesture) {
					input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
					input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
				}
#endif
			}
			break;
		case EVENTID_STATUS_EVENT:
			if (status == STATUS_EVENT_GLOVE_MODE) {
#ifdef CONFIG_GLOVE_TOUCH
				int tm;

				if (data[2 + EventNum * FTS_EVENT_SIZE] == 0x01)
					info->touch_mode = FTS_TM_GLOVE;
				else
					info->touch_mode = FTS_TM_NORMAL;

				tm = info->touch_mode;
				input_report_switch(info->input_dev, SW_GLOVE, tm);
#endif
			} else if (status == STATUS_EVENT_RAW_DATA_READY) {
				unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x01};

				fts_write_reg(info, &regAdd[0], 4);
#ifdef FTS_SUPPORT_HOVER
				info->hover_ready = true;
#endif

				input_info(true, &info->client->dev, "[FTS] Received the Hover Raw Data Ready Event\n");
			} else if (status == STATUS_EVENT_FORCE_CAL_MUTUAL) {
				input_dbg(true, &info->client->dev, "[FTS] Received Force Calibration Mutual only Event\n");
			} else if (status == STATUS_EVENT_FORCE_CAL_SELF) {
				input_dbg(true, &info->client->dev, "[FTS] Received Force Calibration Self only Event\n");
			} else if (status == STATUS_EVENT_WATERMODE_ON) {
				input_info(true, &info->client->dev, "[FTS] Received Water Mode On Event\n");
			} else if (status == STATUS_EVENT_WATERMODE_OFF) {
				input_info(true, &info->client->dev, "[FTS] Received Water Mode Off Event\n");
			} else if (status == STATUS_EVENT_MUTUAL_CAL_FRAME_CHECK) {
				input_info(true, &info->client->dev, "[FTS] Received Mutual Calib Frame Check Event\n");
			} else if (status == STATUS_EVENT_SELF_CAL_FRAME_CHECK) {
				input_info(true, &info->client->dev, "[FTS] Received Self Calib Frame Check Event\n");
			} else {
				fts_debug_msg_event_handler(info,
						  &data[EventNum *
							FTS_EVENT_SIZE]);
			}
			break;

#ifdef SEC_TSP_FACTORY_TEST
		case EVENTID_RESULT_READ_REGISTER:

			break;
#endif

#ifdef FTS_SUPPORT_STRINGLIB
		case EVENTID_FROM_STRING:
			string_addr = FTS_CMD_STRING_ACCESS + 1;
			fts_read_from_string(info, &string_addr, &string, sizeof(string));
			input_info(true, &info->client->dev,
					"%s: [String] %02X %02X %02X %02X %02X %02X %02X %02X || %04X: %02X\n",
					__func__, data[0], data[1], data[2], data[3],
					data[4], data[5], data[6], data[7],
					string_addr, string);

			switch (string) {
			case FTS_STRING_EVENT_AOD_TRIGGER:
				input_info(true, &info->client->dev, "%s: AOD[%X]\n", __func__, string);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				info->scrub_id = SPECIAL_EVENT_TYPE_AOD;
				break;
			case FTS_STRING_EVENT_SPAY:
			case FTS_STRING_EVENT_SPAY1:
			case FTS_STRING_EVENT_SPAY2:
				input_info(true, &info->client->dev, "%s: SPAY[%X]\n", __func__, string);
				input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 1);
				info->scrub_id = SPECIAL_EVENT_TYPE_SPAY;
				break;
			default:
				input_info(true, &info->client->dev, "%s: no event:%X\n", __func__, string);
				break;
			}

			input_sync(info->input_dev);
			input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);

			break;
#endif

		default:
			fts_debug_msg_event_handler(info,
						  &data[EventNum *
							FTS_EVENT_SIZE]);
			continue;
		}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		if (EventID == EVENTID_ENTER_POINTER)
			input_info(true, &info->client->dev,
				"[P] tID:%d x:%d y:%d w:%d h:%d z:%d s:%d p:%d tc:%d tm:%d\n",
				TouchID, x, y, bw, bh, z, sumsize, palm, info->touch_count, info->touch_mode);

#ifdef FTS_SUPPORT_HOVER
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			input_dbg(true, &info->client->dev,
				"[HP] tID:%d x:%d y:%d z:%d\n",
				TouchID, x, y, z);
#endif
#else
		if (EventID == EVENTID_ENTER_POINTER)
			input_info(true, &info->client->dev,
				"[P] tID:%d tc:%d tm:%d\n",
				TouchID, info->touch_count, info->touch_mode);
#ifdef FTS_SUPPORT_HOVER
		else if (EventID == EVENTID_HOVER_ENTER_POINTER)
			input_dbg(true, &info->client->dev,
				"[HP] tID:%d\n", TouchID);
#endif
#endif
		else if (EventID == EVENTID_LEAVE_POINTER) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &info->client->dev,
				"[R] tID:%d mc: %d tc:%d  cc:0x%2X lx: %d ly: %d Ver[%02X%04X%01X%01X%01X]\n",
				TouchID, info->finger[TouchID].mcount, info->touch_count,info->cal_count,
				info->finger[TouchID].lx, info->finger[TouchID].ly,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
#else
			input_info(true, &info->client->dev,
				"[R] tID:%d mc: %d tc:%d cc:0x%2X Ver[%02X%04X%01X%01X%01X]\n",
				TouchID, info->finger[TouchID].mcount, info->touch_count,info->cal_count,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
#endif
			info->finger[TouchID].mcount = 0;
		}
#ifdef FTS_SUPPORT_HOVER
		else if (EventID == EVENTID_HOVER_LEAVE_POINTER) {
			input_dbg(true, &info->client->dev,
				"[HR] tID:%d Ver[%02X%04X%01X%01X]\n",
				TouchID,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled);
			info->finger[TouchID].mcount = 0;
		}
#endif
		else if (EventID == EVENTID_MOTION_POINTER && info->fts_power_state != FTS_POWER_STATE_LOWPOWER)
			info->finger[TouchID].mcount++;

		if (((EventID == EVENTID_ENTER_POINTER) ||
			(EventID == EVENTID_MOTION_POINTER) ||
			(EventID == EVENTID_LEAVE_POINTER)) && info->fts_power_state != FTS_POWER_STATE_LOWPOWER)
			info->finger[TouchID].state = EventID;
	}

	input_sync(info->input_dev);

	return LastLeftEvent;
}

#ifdef FTS_SUPPORT_TA_MODE
static void fts_ta_cb(struct fts_callbacks *cb, int ta_status)
{
	struct fts_ts_info *info =
	    container_of(cb, struct fts_ts_info, callbacks);

	if (ta_status == 0x01 || ta_status == 0x03) {
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
		info->TA_Pluged = true;
		input_info(true, &info->client->dev,
			 "%s: device_control : CHARGER CONNECTED, ta_status : %x\n",
			 __func__, ta_status);
	} else {
		fts_command(info, FTS_CMD_CHARGER_UNPLUGGED);
		info->TA_Pluged = false;
		input_info(true, &info->client->dev,
			 "%s: device_control : CHARGER DISCONNECTED, ta_status : %x\n",
			 __func__, ta_status);
	}
}
#endif

/**
 * fts_interrupt_handler()
 *
 * Called by the kernel when an interrupt occurs (when the sensor
 * asserts the attention irq).
 *
 * This function is the ISR thread and handles the acquisition
 * and the reporting of finger data when the presence of fingers
 * is detected.
 */
static irqreturn_t fts_interrupt_handler(int irq, void *handle)
{
	struct fts_ts_info *info = handle;
	unsigned char regAdd[4] = {0xB6, 0x00, 0x23, READ_ALL_EVENT};
	unsigned short evtcount = 0;

#if defined(CONFIG_SECURE_TOUCH)
	if (fts_filter_interrupt(info) == IRQ_HANDLED) {
		int ret;

		ret = wait_for_completion_interruptible_timeout((&info->st_interrupt),
					msecs_to_jiffies(10 * MSEC_PER_SEC));
		dev_err(&info->client->dev, "%s: SECURETOUCH_IRQ_HANDLED, ret:%d",
					__func__, ret);

		return IRQ_HANDLED;
	}
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	if ((info->board->support_sidegesture) &&
		(info->fts_power_state == FTS_POWER_STATE_LOWPOWER)) {
		pm_wakeup_event(info->input_dev->dev.parent, 1000);
	}
#endif
	evtcount = 0;
	fts_read_reg(info, &regAdd[0], 3, (unsigned char *)&evtcount, 2);

	evtcount = evtcount >> 8;
	evtcount = evtcount / 2;

	if (evtcount > FTS_FIFO_MAX)
		evtcount = FTS_FIFO_MAX;

	if (evtcount > 0) {
		memset(info->data, 0x0, FTS_EVENT_SIZE * evtcount);
		fts_read_reg(info, &regAdd[3], 1, (unsigned char *)info->data,
				  FTS_EVENT_SIZE * evtcount);
		fts_event_handler_type_b(info, info->data, evtcount);
	}
#ifdef FTS_SUPPORT_SIDE_GESTURE
	if ((info->board->support_sidegesture) &&
		(info->fts_power_state == FTS_POWER_STATE_LOWPOWER))
		pm_relax(info->input_dev->dev.parent);
#endif

	return IRQ_HANDLED;
}

int fts_irq_enable(struct fts_ts_info *info,
		bool enable)
{
	int retval = 0;

	if (enable) {
		if (info->irq_enabled)
			return retval;

		retval = request_threaded_irq(info->irq, NULL,
				fts_interrupt_handler, info->board->irq_type,
				FTS_TS_DRV_NAME, info);
		if (retval < 0) {
			input_info(true, &info->client->dev,
					"%s: Failed to create irq thread %d\n",
					__func__, retval);
			return retval;
		}

		info->irq_enabled = true;
	} else {
		if (info->irq_enabled) {
			disable_irq(info->irq);
			free_irq(info->irq, info);
			info->irq_enabled = false;
		}
	}

	return retval;
}

#ifdef CONFIG_OF
#ifdef FTS_SUPPORT_TA_MODE
struct fts_callbacks *fts_charger_callbacks;
void tsp_charger_infom(bool en)
{

	pr_err("[TSP]%s: ta:%d\n",	__func__, en);

	if (fts_charger_callbacks && fts_charger_callbacks->inform_charger)
		fts_charger_callbacks->inform_charger(fts_charger_callbacks, en);
}
static void fts_tsp_register_callback(void *cb)
{
	fts_charger_callbacks = cb;
}
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
static int fts_led_power_ctrl(void *data, bool on)
{
	struct fts_ts_info *info = (struct fts_ts_info *)data;
	const struct fts_i2c_platform_data *pdata = info->board;
	struct device *dev = &info->client->dev;
	struct regulator *regulator_tk_led = NULL;
	int retval = 0;

	if (info->tsk_led_enabled == on)
		return retval;

	regulator_tk_led = regulator_get(NULL, pdata->regulator_tk_led);
	if (IS_ERR_OR_NULL(regulator_tk_led)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_tk_led);
		goto out;
	}

	input_info(true, dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		retval = regulator_enable(regulator_tk_led);
		if (retval) {
			input_err(true, dev, "%s: Failed to enable led%d\n", __func__, retval);
			goto out;
		}
	} else {
		if (regulator_is_enabled(regulator_tk_led))
			regulator_disable(regulator_tk_led);
	}

	info->tsk_led_enabled = on;
out:
	regulator_put(regulator_tk_led);

	return retval;
}
#endif

static int fts_power_ctrl(void *data, bool on)
{
	struct fts_ts_info *info = (struct fts_ts_info *)data;
	const struct fts_i2c_platform_data *pdata = info->board;
	struct device *dev = &info->client->dev;
	struct regulator *regulator_dvdd = NULL;
	struct regulator *regulator_avdd = NULL;
	int retval = 0;

	if (info->tsp_enabled == on)
		return retval;

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR_OR_NULL(regulator_dvdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_dvdd);
		goto out;
	}

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR_OR_NULL(regulator_avdd)) {
		input_err(true, dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		goto out;
	}

	input_info(true, dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		retval = regulator_enable(regulator_avdd);
		if (retval) {
			input_err(true, dev, "%s: Failed to enable avdd: %d\n", __func__, retval);
			goto out;
		}

		fts_delay(1);

		retval = regulator_enable(regulator_dvdd);
		if (retval) {
			input_err(true, dev, "%s: Failed to enable vdd: %d\n", __func__, retval);
			goto out;
		}

		/* Use TSP RESET */
		if (gpio_is_valid(pdata->tsp_reset)) {
			input_info(true, dev, "%s: Use TSP RESET.\n", __func__);
			gpio_direction_output(pdata->tsp_reset, 1);
		}

		retval = pinctrl_select_state(pdata->pinctrl, pdata->pins_default);
		if (retval < 0)
			input_err(true, dev, "%s: Failed to configure tsp_attn pin\n", __func__);

		fts_delay(5);
	} else {
		if (regulator_is_enabled(regulator_dvdd))
			regulator_disable(regulator_dvdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		/* Use TSP RESET */
		if (gpio_is_valid(pdata->tsp_reset)) {
			input_info(true, dev, "%s: Use TSP RESET.\n", __func__);
			gpio_direction_output(pdata->tsp_reset, 0);
		}

		retval = pinctrl_select_state(pdata->pinctrl, pdata->pins_sleep);
		if (retval < 0)
			input_err(true, dev, "%s: Failed to configure tsp_attn pin\n", __func__);
	}

	info->tsp_enabled = on;
out:
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return retval;
}

static int fts_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct fts_i2c_platform_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	u32 coords[2], lines[2];
	int retval = 0;

	pdata->tsp_id = of_get_named_gpio(np, "stm,tsp-id_gpio", 0);
	if (gpio_is_valid(pdata->tsp_id))
		input_info(true, dev, "TSP_ID : %d\n", gpio_get_value(pdata->tsp_id));
	else
		input_err(true, dev, "Failed to get tsp-id gpio\n");

	if (gpio_is_valid(pdata->tsp_id))
		gpio_request(pdata->tsp_id, "TSP_ID");

	pdata->tsp_reset = of_get_named_gpio(np, "stm,rst_gpio", 0);
	if (gpio_is_valid(pdata->tsp_reset))
		input_info(true, dev, "TSP_ID : %d\n", gpio_get_value(pdata->tsp_reset));
	else
		input_err(true, dev, "Failed to get tsp_reset gpio\n");

	if (gpio_is_valid(pdata->tsp_reset))
		gpio_request(pdata->tsp_reset, "TSP_RESET");

	pdata->gpio = of_get_named_gpio(np, "stm,irq_gpio", 0);
	if (gpio_is_valid(pdata->gpio)) {
		retval = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "stm,tsp_int");
		if (retval) {
			input_err(true, dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
			return -EINVAL;
		}
	} else {
		input_err(true, dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}
	client->irq = gpio_to_irq(pdata->gpio);

	if (of_property_read_u32(np, "stm,irq_type", &pdata->irq_type)) {
		input_err(true, dev, "Failed to get irq_type property\n");
		return -EINVAL;
	}

	if (of_property_read_u32(np, "stm,grip_area", &pdata->grip_area))
		input_err(true, dev, "Failed to get grip_area property\n");

	if (of_property_read_u32_array(np, "stm,max_coords", coords, 2)) {
		input_err(true, dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pdata->max_x = coords[0];
	pdata->max_y = coords[1];

	if (of_property_read_u32_array(np, "stm,num_lines", lines, 2)) {
		input_dbg(true, dev, "skipped to get num_lines property\n");
	} else {
		pdata->SenseChannelLength = lines[0];
		pdata->ForceChannelLength = lines[1];
		input_info(true, dev, "num_of[rx,tx]: [%d,%d]\n",
			pdata->SenseChannelLength, pdata->ForceChannelLength);
	}

	if (of_property_read_string(np, "stm,regulator_dvdd", &pdata->regulator_dvdd)) {
		input_err(true, dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}

	if (of_property_read_string(np, "stm,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}
	pdata->power = fts_power_ctrl;

	/* Optional parmeters(those values are not mandatory)
	 * do not return error value even if fail to get the value
	 */
	if (gpio_is_valid(pdata->tsp_id))
		of_property_read_string_index(np, "stm,firmware_name", gpio_get_value(pdata->tsp_id), &pdata->firmware_name);
	else
		of_property_read_string_index(np, "stm,firmware_name", 0, &pdata->firmware_name);

	if (of_property_read_string_index(np, "stm,project_name", 0, &pdata->project_name))
		input_dbg(true, dev, "skipped to get project_name property\n");
	if (of_property_read_string_index(np, "stm,project_name", 1, &pdata->model_name))
		input_dbg(true, dev, "skipped to get model_name property\n");

	of_property_read_u32(np, "stm,support_gesture", &pdata->support_sidegesture);

	of_property_read_u32(np, "stm,bringup", &pdata->bringup);

	pdata->max_width = 28;
	pdata->support_hover = false;
	pdata->support_mshover = false;
#ifdef FTS_SUPPORT_TA_MODE
	pdata->register_cb = fts_tsp_register_callback;
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (of_property_read_u32(np, "stm,num_touchkey", &pdata->num_touchkey))
		input_dbg(true, dev, "skipped to get num_touchkey property\n");
	else {
		pdata->support_mskey = true;
		pdata->touchkey = fts_touchkeys;

		if (of_property_read_string(np, "stm,regulator_tk_led", &pdata->regulator_tk_led))
			input_dbg(true, dev, "skipped to get regulator_tk_led name property\n");
		else
			pdata->led_power = fts_led_power_ctrl;
	}
#endif

	if (of_property_read_u32(np, "stm,device_num", &pdata->device_num))
		input_err(true, dev, "Failed to get device_num property\n");

#ifdef PAT_CONTROL
	if (of_property_read_u32(np, "stm,pat_function", &pdata->pat_function) < 0){
		pdata->pat_function = PAT_CONTROL_NONE;
		input_dbg(true, dev, "Failed to get pat_function property\n");
	}

	if (of_property_read_u32(np, "stm,afe_base", &pdata->afe_base) < 0){
		pdata->afe_base = 0;
		input_dbg(true, dev, "Failed to get afe_base property\n");
	}
#endif

/*
 * TODO
 *	pdata->panel_revision = (lcdtype & 0xF000) >> 12;
 */
	input_info(true, dev,
		"irq :%d, irq_type: 0x%04x, max[x,y]: [%d,%d], grip_area :%d, project/model_name: %s/%s, pat_function(%d), panel_revision: %d, gesture: %d, device_num: %d\n",
		pdata->gpio, pdata->irq_type, pdata->max_x, pdata->max_y, pdata->grip_area,
		pdata->project_name, pdata->model_name, pdata->pat_function, pdata->panel_revision,
		pdata->support_sidegesture, pdata->device_num);

	return retval;
}
#endif

static int fts_setup_drv_data(struct i2c_client *client)
{
	int retval = 0;
	struct fts_i2c_platform_data *pdata;
	struct fts_ts_info *info;

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct fts_i2c_platform_data), GFP_KERNEL);

		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		retval = fts_parse_dt(client);
		if (retval) {
			input_err(true, &client->dev, "Failed to parse dt\n");
			return retval;
		}
	} else {
		pdata = client->dev.platform_data;
	}

	if (!pdata) {
		input_err(true, &client->dev, "No platform data found\n");
			return -EINVAL;
	}
	if (!pdata->power) {
		input_err(true, &client->dev, "No power contorl found\n");
			return -EINVAL;
	}

	pdata->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR(pdata->pinctrl)) {
		input_err(true, &client->dev, "could not get pinctrl\n");
		return PTR_ERR(pdata->pinctrl);
	}

	pdata->pins_default = pinctrl_lookup_state(pdata->pinctrl, "on_state");
	if (IS_ERR(pdata->pins_default))
		input_err(true, &client->dev, "could not get default pinstate\n");

	pdata->pins_sleep = pinctrl_lookup_state(pdata->pinctrl, "off_state");
	if (IS_ERR(pdata->pins_sleep))
		input_err(true, &client->dev, "could not get sleep pinstate\n");

	info = kzalloc(sizeof(struct fts_ts_info), GFP_KERNEL);
	if (!info) {
		input_err(true, &client->dev,
				"%s: Failed to alloc mem for info\n",
				__func__);
		return -ENOMEM;
	}

	info->client = client;
	info->board = pdata;
	info->irq = client->irq;
	info->irq_type = info->board->irq_type;
	info->irq_enabled = false;
	info->touch_stopped = false;
	info->panel_revision = info->board->panel_revision;
	info->stop_device = fts_stop_device;
	info->start_device = fts_start_device;
	info->fts_command = fts_command;
	info->fts_enable_feature = fts_enable_feature;
	info->fts_read_reg = fts_read_reg;
	info->fts_write_reg = fts_write_reg;
	info->fts_systemreset = fts_systemreset;
	info->fts_get_version_info = fts_get_version_info;
	info->fts_get_sysinfo_data = fts_get_sysinfo_data;
	info->fts_wait_for_ready = fts_wait_for_ready;
#ifdef FTS_SUPPORT_STRINGLIB
	info->fts_read_from_string = fts_read_from_string;
	info->fts_write_to_string = fts_write_to_string;
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
	info->fts_get_noise_param_address = fts_get_noise_param_address;
#endif

#ifdef USE_OPEN_DWORK
	INIT_DELAYED_WORK(&info->open_work, fts_open_work);
#endif
	info->delay_time = 300;
	INIT_DELAYED_WORK(&info->reset_work, fts_reset_work);

	INIT_DELAYED_WORK(&info->debug_work, dump_tsp_rawdata);
	p_debug_work = &info->debug_work;

	if (info->board->support_hover)
		input_info(true, &info->client->dev, "FTS Support Hover Event\n");
	else
		input_info(true, &info->client->dev, "FTS Not support Hover Event\n");

	i2c_set_clientdata(client, info);

	if (pdata->get_ddi_type) {
		info->ddi_type = pdata->get_ddi_type();
		input_info(true, &client->dev, "%s: DDI Type is %s[%d]\n",
			__func__, info->ddi_type ? "MAGNA" : "SDC", info->ddi_type);
	}

	return retval;
}

static int fts_probe(struct i2c_client *client, const struct i2c_device_id *idp)
{
	int retval;
	struct fts_ts_info *info = NULL;
	static char fts_ts_phys[64] = { 0 };
	int i = 0;

	input_info(true, &client->dev, "FTS Driver [70%s]\n",
	       FTS_TS_DRV_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "FTS err = EIO!\n");
		return -EIO;
	}
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		input_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}
#endif
	/* Build up driver data */
	retval = fts_setup_drv_data(client);
	if (retval < 0) {
		input_err(true, &client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}

	info = (struct fts_ts_info *)i2c_get_clientdata(client);
	if (!info) {
		input_err(true, &client->dev, "%s: Failed to get driver data\n", __func__);
		retval = -ENODEV;
		goto err_get_drv_data;
	}

	info->probe_done = false;

	if (info->board->power)
		info->board->power(info, true);

	info->dev = &info->client->dev;
	info->input_dev = input_allocate_device();
	if (!info->input_dev) {
		input_info(true, &info->client->dev, "FTS err = ENOMEM!\n");
		retval = -ENOMEM;
		goto err_input_allocate_device;
	}

	info->input_dev->dev.parent = &client->dev;
	if (info->board->device_num == 0)
		info->input_dev->name = "sec_touchscreen";
	else if (info->board->device_num == 2)
		info->input_dev->name = "sec_touchscreen2";
	else
		info->input_dev->name = "sec_touchscreen";

	snprintf(fts_ts_phys, sizeof(fts_ts_phys), "%s/input1",
		 info->input_dev->name);
	info->input_dev->phys = fts_ts_phys;
	info->input_dev->id.bustype = BUS_I2C;

#ifdef USE_OPEN_CLOSE
	info->input_dev->open = fts_input_open;
	info->input_dev->close = fts_input_close;
#endif

	mutex_init(&info->device_mutex);
	mutex_init(&info->i2c_mutex);

	info->tsp_enabled = false;

	retval = fts_init(info);

	info->reinit_done = true;

	if (retval) {
		input_err(true, &info->client->dev, "FTS fts_init fail!\n");
		goto err_fts_init;
	}

#ifdef CONFIG_GLOVE_TOUCH
	input_set_capability(info->input_dev, EV_SW, SW_GLOVE);
#endif
	set_bit(EV_SYN, info->input_dev->evbit);
	set_bit(EV_KEY, info->input_dev->evbit);
	set_bit(EV_ABS, info->input_dev->evbit);
#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, info->input_dev->propbit);
#endif
	set_bit(BTN_TOUCH, info->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, info->input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, info->input_dev->keybit);
	set_bit(KEY_HOMEPAGE, info->input_dev->keybit);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		for (i = 0 ; i < info->board->num_touchkey ; i++)
			set_bit(info->board->touchkey[i].keycode, info->input_dev->keybit);

		set_bit(EV_LED, info->input_dev->evbit);
		set_bit(LED_MISC, info->input_dev->ledbit);
	}
#endif
#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->board->support_sidegesture) {
		set_bit(KEY_SIDE_GESTURE, info->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_RIGHT, info->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_LEFT, info->input_dev->keybit);
	}
#endif

	input_mt_init_slots(info->input_dev, FINGER_MAX, INPUT_MT_DIRECT);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_X,
				0, info->board->max_x, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_POSITION_Y,
				0, info->board->max_y, 0, 0);
#ifdef FTS_SUPPORT_PRESSURE_SENSOR
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
			     0, 10000, 0, 0);
#elif defined(CONFIG_SEC_FACTORY)
	input_set_abs_params(info->input_dev, ABS_MT_PRESSURE,
				 0, 0xFF, 0, 0);
#endif

	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MAJOR,
				 0, 255, 0, 0);
	input_set_abs_params(info->input_dev, ABS_MT_TOUCH_MINOR,
				 0, 255, 0, 0);
#ifdef FTS_SUPPORT_SEC_SWIPE
	input_set_abs_params(info->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
#endif
#if defined(FTS_SUPPORT_SIDE_GESTURE)
	if (info->board->support_sidegesture)
		input_set_abs_params(info->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_abs_params(info->input_dev, ABS_MT_DISTANCE,
				 0, 255, 0, 0);

	input_set_drvdata(info->input_dev, info);
	i2c_set_clientdata(client, info);

	retval = input_register_device(info->input_dev);
	if (retval) {
		input_err(true, &info->client->dev, "FTS input_register_device fail!\n");
		goto err_register_input;
	}

	for (i = 0; i < FINGER_MAX; i++) {
		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}

	info->tsp_enabled = true;

	retval = fts_irq_enable(info, true);
	if (retval < 0) {
		input_info(true, &info->client->dev,
						"%s: Failed to enable attention interrupt\n",
						__func__);
		goto err_enable_irq;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	trustedui_set_tsp_irq(info->irq);
	input_info(true, &client->dev, "%s[%d] called!\n",
		__func__, info->irq);
#endif

#ifdef FTS_SUPPORT_TA_MODE
	info->register_cb = info->board->register_cb;

	info->callbacks.inform_charger = fts_ta_cb;
	if (info->register_cb)
		info->register_cb(&info->callbacks);
#endif

#ifdef SEC_TSP_FACTORY_TEST
	retval = sec_cmd_init(&info->sec, ft_commands,
			ARRAY_SIZE(ft_commands), SEC_CLASS_DEVT_TSP);
	if (retval < 0) {
		input_err(true, &info->client->dev,
			"%s: Failed to sec_cmd_init\n", __func__);
		retval = -ENODEV;
		goto err_sec_cmd; // jh32.park
	}

	retval = sysfs_create_group(&info->sec.fac_dev->kobj,
				 &sec_touch_factory_attr_group);
	if (retval < 0) {
		input_err(true, &info->client->dev, "FTS Failed to create sysfs group\n");
		goto err_sysfs;
	}

	retval = sysfs_create_link(&info->sec.fac_dev->kobj,
				&info->input_dev->dev.kobj, "input");

	if (retval < 0) {
		input_err(true, &info->client->dev,
				"%s: Failed to create link\n", __func__);
		goto err_sysfs;
	}

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
#ifdef CONFIG_SEC_SYSFS
		info->fac_dev_tk = sec_device_create(info, "sec_touchkey");
#else
		info->fac_dev_tk = device_create(sec_class, NULL, 11, info, "sec_touchkey");
#endif
		if (IS_ERR(info->fac_dev_tk))
			input_err(true, &info->client->dev, "Failed to create device for the touchkey sysfs\n");
		else {
			dev_set_drvdata(info->fac_dev_tk, info);

			retval = sysfs_create_group(&info->fac_dev_tk->kobj,
						 &sec_touchkey_factory_attr_group);
			if (retval < 0)
				input_err(true, &info->client->dev, "FTS Failed to create sysfs group\n");
			else {
				retval = sysfs_create_link(&info->fac_dev_tk->kobj,
							&info->input_dev->dev.kobj, "input");

				if (retval < 0)
					input_err(true, &info->client->dev,
							"%s: Failed to create link\n", __func__);
			}
		}
	}
#endif
#endif

#if defined(CONFIG_SECURE_TOUCH)
	for (i = 0; i < (int)ARRAY_SIZE(attrs); i++) {
		retval = sysfs_create_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
		if (retval < 0) {
			dev_err(&info->client->dev,
				"%s: Failed to create sysfs attributes\n",
				__func__);
		}
	}

	fts_secure_touch_init(info);
#endif

	device_init_wakeup(&client->dev, true);
	info->probe_done = true;

	return 0;

#ifdef SEC_TSP_FACTORY_TEST
err_sysfs:
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);
err_sec_cmd:
#endif
	if (info->irq_enabled)
		fts_irq_enable(info, false);
err_enable_irq:
	input_unregister_device(info->input_dev);
	info->input_dev = NULL;

err_register_input:
	if (info->input_dev)
		input_free_device(info->input_dev);

err_fts_init:
	mutex_destroy(&info->device_mutex);
	mutex_destroy(&info->i2c_mutex);
err_input_allocate_device:
	info->board->power(info, false);
	kfree(info);
err_get_drv_data:
err_setup_drv_data:
	return retval;
}

static int fts_remove(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);
#if defined(CONFIG_SECURE_TOUCH)
	int i = 0;
#endif

	input_info(true, &info->client->dev, "FTS removed\n");

	fts_interrupt_set(info, INT_DISABLE);
	fts_command(info, FLUSHBUFFER);

	fts_irq_enable(info, false);

#if defined(CONFIG_SECURE_TOUCH)
	for (i = 0; i < (int)ARRAY_SIZE(attrs); i++) {
		sysfs_remove_file(&info->input_dev->dev.kobj,
				&attrs[i].attr);
	}
#endif

	input_mt_destroy_slots(info->input_dev);

#ifdef SEC_TSP_FACTORY_TEST
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey) {
		sysfs_remove_link(&info->fac_dev_tk->kobj, "input");
		sysfs_remove_group(&info->fac_dev_tk->kobj,
				   &sec_touchkey_factory_attr_group);
#ifdef CONFIG_SEC_SYSFS
		sec_device_destroy(info->fac_dev_tk->devt);
#else
		device_destroy(sec_class, 11);
#endif

	}
#endif
	sysfs_remove_link(&info->sec.fac_dev->kobj, "input");
	sysfs_remove_group(&info->sec.fac_dev->kobj,
			   &sec_touch_factory_attr_group);
	sec_cmd_exit(&info->sec, SEC_CLASS_DEVT_TSP);

	kfree(info->cx_data);
	kfree(info->pFrame);
#endif

	input_unregister_device(info->input_dev);
	info->input_dev = NULL;

	info->board->power(info, false);

	kfree(info);

	return 0;
}

#ifdef USE_OPEN_CLOSE
#ifdef USE_OPEN_DWORK
static void fts_open_work(struct work_struct *work)
{
	int retval;
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						open_work.work);

	input_info(true, &info->client->dev, "%s\n", __func__);

	retval = fts_start_device(info);
	if (retval < 0)
		input_err(true, &info->client->dev,
			"%s: Failed to start device\n", __func__);
}
#endif
static int fts_input_open(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);
	int retval;
	unsigned char regAdd[2] = {0xC2, 0x10};
	int rc;

	if (!info->probe_done) {
		input_dbg(false, &info->client->dev, "%s not probe\n", __func__);
		goto out;
	}

	input_dbg(false, &info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	schedule_delayed_work(&info->open_work,
			      msecs_to_jiffies(TOUCH_OPEN_DWORK_TIME));
#else
	retval = fts_start_device(info);
	if (retval < 0) {
		input_err(true, &info->client->dev,
			"%s: Failed to start device\n", __func__);
		goto out;
	}
#endif


#ifdef FTS_SUPPORT_HOVER
	input_info(true, &info->client->dev, "FTS cmd after wakeup : h%d\n", info->retry_hover_enable_after_wakeup);

	if (info->retry_hover_enable_after_wakeup == 1) {
		unsigned char regAdd[4] = {0xB0, 0x01, 0x29, 0x41};

		fts_write_reg(info, &regAdd[0], 4);
		fts_command(info, FTS_CMD_HOVER_ON);
		info->hover_enabled = true;
	}
#endif

	regAdd[1] = 0x10;

	if (info->wirelesscharger_mode == 0)
		regAdd[0] = 0xC2;
	else
		regAdd[0] = 0xC1;

	input_info(true, &info->client->dev, "%s: Set W-Charger Status CMD[%2X]\n", __func__, regAdd[0]);
	rc = fts_write_reg(info, regAdd, 2);
out:
	return 0;
}

static void fts_input_close(struct input_dev *dev)
{
	struct fts_ts_info *info = input_get_drvdata(dev);

	if (!info->probe_done) {
		input_dbg(false, &info->client->dev, "%s not probe\n", __func__);
		return;
	}

	input_dbg(false, &info->client->dev, "%s\n", __func__);

#ifdef USE_OPEN_DWORK
	cancel_delayed_work(&info->open_work);
#endif

	fts_stop_device(info);
#ifdef FTS_SUPPORT_HOVER
	info->retry_hover_enable_after_wakeup = 0;
#endif
}
#endif

#ifdef CONFIG_SEC_FACTORY
#include <linux/uaccess.h>
#define LCD_LDI_FILE_PATH	"/sys/class/lcd/panel/window_type"
static int fts_get_panel_revision(struct fts_ts_info *info)
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *window_type;
	unsigned char lcdtype[4] = {0,};

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	window_type = filp_open(LCD_LDI_FILE_PATH, O_RDONLY, 0666);
	if (IS_ERR(window_type)) {
		iRet = PTR_ERR(window_type);
		if (iRet != -ENOENT)
			input_err(true, &info->client->dev, "%s: window_type file open fail\n", __func__);
		set_fs(old_fs);
		goto exit;
	}

	iRet = window_type->f_op->read(window_type, (u8 *)lcdtype, sizeof(u8) * 4, &window_type->f_pos);
	if (iRet != (sizeof(u8) * 4)) {
		input_err(true, &info->client->dev, "%s: Can't read the lcd ldi data\n", __func__);
		iRet = -EIO;
	}

	/* The variable of lcdtype has ASCII values(40 81 45) at 0x08 OCTA,
	  * so if someone need a TSP panel revision then to read third parameter.
	  */
	info->panel_revision = lcdtype[3] & 0x0F;
	input_info(true, &info->client->dev,
		"%s: update panel_revision 0x%02X\n", __func__, info->panel_revision);

	filp_close(window_type, current->files);
	set_fs(old_fs);

exit:
	return iRet;
}

static void fts_reinit_fac(struct fts_ts_info *info)
{
	info->touch_count = 0;

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);
	fts_delay(50);

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_get_noise_param_address(info);
#endif

	if (info->flip_enable)
		fts_set_cover_type(info, true);
#ifdef CONFIG_GLOVE_TOUCH
	/* enable glove touch when flip cover is closed */
	if (info->fast_mshover_enabled)
		fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
	else if (info->mshover_enabled)
		fts_command(info, FTS_CMD_MSHOVER_ON);
#endif
	fts_command(info, FORCECALIBRATION);

	fts_interrupt_set(info, INT_ENABLE);

	input_info(true, &info->client->dev, "FTS Re-Initialised\n");
}
#endif

static void fts_reinit(struct fts_ts_info *info)
{
	unsigned char retry = 3;
	int rc = 0;

	if (!info->lowpower_mode) {
		fts_wait_for_ready(info);
		fts_read_chip_id(info);
	}

	do {
		fts_systemreset(info);

		rc = fts_wait_for_ready(info);
		if (rc != FTS_NOT_ERROR) {
			if (info->board->power)
				info->board->power(info, false);
			fts_delay(20);

			if (info->board->power)
				info->board->power(info, true);
			fts_delay(5);
		} else {
			break;
		}
	} while (retry--);

	if (retry == 0) {
		input_info(true, &info->client->dev, "%s: Failed to system reset\n", __func__);
		return;
	}

#ifdef CONFIG_SEC_FACTORY
	/* Read firmware version from IC when every power up IC.
	 * During Factory process touch panel can be changed manually.
	 */
	{
		unsigned short orig_fw_main_version_of_ic = info->fw_main_version_of_ic;
		int rc;

		fts_get_panel_revision(info);
		fts_get_version_info(info);

		rc = fts_get_channel_info(info);
		if (rc >= 0) {
			input_info(true, &info->client->dev, "FTS Sense(%02d) Force(%02d)\n",
				   info->SenseChannelLength, info->ForceChannelLength);
		} else {
			input_info(true, &info->client->dev, "FTS read failed rc = %d\n", rc);
			input_info(true, &info->client->dev, "FTS Initialise Failed\n");
			return;
		}

		info->pFrame =
			kzalloc(info->SenseChannelLength * info->ForceChannelLength * 2,
				GFP_KERNEL);
		if (info->pFrame == NULL) {
			input_info(true, &info->client->dev, "FTS pFrame kzalloc Failed\n");
			return;
		}

		info->cx_data = kzalloc(info->SenseChannelLength * info->ForceChannelLength, GFP_KERNEL);
		if (!info->cx_data)
			input_err(true, &info->client->dev, "%s: cx_data kzalloc Failed\n", __func__);

		if (info->fw_main_version_of_ic != orig_fw_main_version_of_ic) {
			/* v pat: disable magic cal, this routine maybe need to change as 1 for factory */
			fts_fw_init(info,0);
			fts_reinit_fac(info);
			return;
		}
	}
#endif

#ifdef FTS_SUPPORT_NOISE_PARAM
	fts_set_noise_param(info);
#endif

#if defined(FTS_SUPPORT_STRINGLIB) && defined(CONFIG_SEC_FACTORY)
	fts_disable_string(info);
#endif

	fts_command(info, FLUSHBUFFER);
	fts_delay(10);
	fts_command(info, SENSEON);
#ifdef FTS_SUPPORT_PRESSURE_SENSOR
	fts_command(info, FTS_CMD_PRESSURE_SENSE_ON);
	fts_delay(50);
#endif

#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->support_mskey)
		info->fts_command(info, FTS_CMD_KEY_SENSE_ON);
#endif

	if (info->flip_enable)
		fts_set_cover_type(info, true);
	else if (info->fast_mshover_enabled)
		fts_command(info, FTS_CMD_SET_FAST_GLOVE_MODE);
	else if (info->mshover_enabled)
		fts_command(info, FTS_CMD_MSHOVER_ON);

#ifdef FTS_SUPPORT_TA_MODE
	if (info->TA_Pluged)
		fts_command(info, FTS_CMD_CHARGER_PLUGGED);
#endif

	info->touch_count = 0;

	fts_interrupt_set(info, INT_ENABLE);
}

void fts_release_all_finger(struct fts_ts_info *info)
{
	int i;

	for (i = 0; i < FINGER_MAX; i++) {
		input_mt_slot(info->input_dev, i);
#if defined(CONFIG_SEC_FACTORY) || defined(FTS_SUPPORT_PRESSURE_SENSOR)
		input_report_abs(info->input_dev, ABS_MT_PRESSURE, 0);
#endif
		input_mt_report_slot_state(info->input_dev, MT_TOOL_FINGER, 0);

		if ((info->finger[i].state == EVENTID_ENTER_POINTER) ||
			(info->finger[i].state == EVENTID_MOTION_POINTER)) {
			info->touch_count--;
			if (info->touch_count < 0)
				info->touch_count = 0;

			input_info(true, &info->client->dev,
				"[RA] tID:%d mc: %d tc:%d Ver[%02X%04X%01X%01X%01X]\n",
				i, info->finger[i].mcount, info->touch_count,
				info->panel_revision, info->fw_main_version_of_ic,
				info->flip_enable, info->mshover_enabled, info->mainscr_disable);
		}

		info->finger[i].state = EVENTID_LEAVE_POINTER;
		info->finger[i].mcount = 0;
	}

	input_report_key(info->input_dev, BTN_TOUCH, 0);
	input_report_key(info->input_dev, BTN_TOOL_FINGER, 0);

#ifdef CONFIG_GLOVE_TOUCH
	input_report_switch(info->input_dev, SW_GLOVE, false);
	info->touch_mode = FTS_TM_NORMAL;
#endif
	input_report_key(info->input_dev, KEY_BLACK_UI_GESTURE, 0);
	input_report_key(info->input_dev, KEY_HOMEPAGE, 0);

#ifdef FTS_SUPPORT_SIDE_GESTURE
	if (info->board->support_sidegesture) {
		input_report_key(info->input_dev, KEY_SIDE_GESTURE, 0);
		input_report_key(info->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);
		input_report_key(info->input_dev, KEY_SIDE_GESTURE_LEFT, 0);
	}
#endif

	input_sync(info->input_dev);
}

#if 0/*def CONFIG_TRUSTONIC_TRUSTED_UI*/
void trustedui_mode_on(void)
{
	input_info(true, &tui_tsp_info->client->dev, "%s, release all finger..", __func__);
	fts_release_all_finger(tui_tsp_info);
}
#endif

static void dump_tsp_rawdata(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
							debug_work.work);
	int i;

	if (info->rawdata_read_lock == true)
		input_err(true, &info->client->dev, "%s, ## checking.. ignored.\n", __func__);

	info->rawdata_read_lock = true;
	input_info(true, &info->client->dev, "%s, ## run CX data ##, %d\n", __func__, __LINE__);
	run_cx_data_read((void *)info);

	for (i = 0; i < 5; i++) {
		input_info(true, &info->client->dev, "%s, ## run Raw Cap data ##, %d\n", __func__, __LINE__);
		run_rawcap_read((void *)info);

		input_info(true, &info->client->dev, "%s, ## run Delta ##, %d\n", __func__, __LINE__);
		run_delta_read((void *)info);
		fts_delay(50);
	}
	input_info(true, &info->client->dev, "%s, ## Done ##, %d\n", __func__, __LINE__);

	info->rawdata_read_lock = false;
}

void tsp_dump(void)
{
#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge)
		return;
#endif
	if (!p_debug_work)
		return;

	pr_err("FTS %s: start\n", __func__);
	schedule_delayed_work(p_debug_work, msecs_to_jiffies(100));
}

static void fts_reset_work(struct work_struct *work)
{
	struct fts_ts_info *info = container_of(work, struct fts_ts_info,
						reset_work.work);
	bool temp_lpm;

	temp_lpm = info->lowpower_mode;
	/* Reset-routine must go to power off state  */
	info->lowpower_mode = 0;

	input_info(true, &info->client->dev, "%s, Call Power-Off to recover IC, lpm:%d\n", __func__, temp_lpm);
	fts_stop_device(info);

	msleep(100);	/* Delay to discharge the IC from ESD or On-state.*/
	if (fts_start_device(info) < 0)
		input_err(true, &info->client->dev, "%s: Failed to start device\n", __func__);

	info->lowpower_mode = temp_lpm;
}


static int fts_stop_device(struct fts_ts_info *info)
{
	input_info(true, &info->client->dev, "%s\n", __func__);

#if defined(CONFIG_SECURE_TOUCH)
	fts_secure_touch_stop(info, 1);
#endif
	mutex_lock(&info->device_mutex);

	if (info->touch_stopped) {
		input_err(true, &info->client->dev, "%s already power off\n", __func__);
		goto out;
	}

	if (info->lowpower_mode) {
#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture) {
			input_info(true, &info->client->dev, "%s mainscreen disable flag:%d, clear\n", __func__, info->mainscr_disable);
			set_mainscreen_disable_cmd((void *)info, 0);
		}
#endif
		input_info(true, &info->client->dev, "%s lowpower flag:%d\n", __func__, info->lowpower_flag);

		info->fts_power_state = FTS_POWER_STATE_LOWPOWER;

		fts_command(info, FLUSHBUFFER);

#ifdef FTS_SUPPORT_SIDE_GESTURE
		if (info->board->support_sidegesture) {
			fts_enable_feature(info, FTS_FEATURE_SIDE_GUSTURE, true);
			fts_delay(20);
		}
#endif

		if (device_may_wakeup(&info->client->dev))
			enable_irq_wake(info->irq);

		fts_command(info, FLUSHBUFFER);

		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
		fts_get_noise_param(info);
#endif

		fts_enable_feature(info, FTS_FEATURE_LPM_FUNCTION, true);

		/* 2 finger gesture!*/
		if (info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP) {
			input_info(true, &info->client->dev, "%s lpgw_mode on!\n", __func__);
			/* Full scan in LP mode */
#ifdef FTS_SUPPORT_STRINGLIB
			if (!info->fts_mode)
#endif
			{
				fts_enable_feature(info, FTS_FEATURE_LPM_FUNCTION, true);
				fts_delay(5);
			}
			/* Two fingers gesture on */
			fts_enable_feature(info, FTS_FEATURE_GESTURE_WAKEUP, true);
			fts_delay(5);
		}

		fts_command(info, FTS_CMD_LOWPOWER_MODE);

	} else {
		fts_interrupt_set(info, INT_DISABLE);
		disable_irq(info->irq);

		fts_command(info, FLUSHBUFFER);
		fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
		fts_release_all_key(info);
#endif
#ifdef FTS_SUPPORT_NOISE_PARAM
		fts_get_noise_param(info);
#endif
		info->touch_stopped = true;
		info->hover_enabled = false;
		info->hover_ready = false;

		if (info->board->power)
			info->board->power(info, false);

		info->fts_power_state = FTS_POWER_STATE_POWERDOWN;
	}
 out:
	mutex_unlock(&info->device_mutex);
	return 0;
}

static int fts_start_device(struct fts_ts_info *info)
{
	input_info(true, &info->client->dev, "%s %s\n",
			__func__, info->lowpower_mode ? "exit low power mode TP" : "");

#if defined(CONFIG_SECURE_TOUCH)
	fts_secure_touch_stop(info, 1);
#endif
	mutex_lock(&info->device_mutex);

	if (info->fts_power_state == FTS_POWER_STATE_ACTIVE) {
		input_err(true, &info->client->dev, "%s already power on\n", __func__);
		goto out;
	}

	fts_release_all_finger(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
	fts_release_all_key(info);
#endif

	if (info->lowpower_mode) {
		/* low power mode command is sent after LCD OFF. turn on touch power @ LCD ON */
		if (info->touch_stopped)
			goto tsp_power_on;

		disable_irq(info->irq);

		info->reinit_done = false;
		fts_reinit(info);
		info->reinit_done = true;

		enable_irq(info->irq);

		if (device_may_wakeup(&info->client->dev))
			disable_irq_wake(info->irq);
	} else {
tsp_power_on:
		if (info->board->power)
			info->board->power(info, true);
		info->touch_stopped = false;
		info->reinit_done = false;

		fts_reinit(info);
		info->reinit_done = true;

		enable_irq(info->irq);
	}

#ifdef FTS_SUPPORT_STRINGLIB
	if (info->fts_mode) {
		unsigned short addr = FTS_CMD_STRING_ACCESS;
		int ret;

		ret = info->fts_write_to_string(info, &addr, &info->fts_mode, sizeof(info->fts_mode));
		if (ret < 0)
			input_err(true, &info->client->dev, "%s: failed. ret: %d\n", __func__, ret);
	}
#endif

 out:
	mutex_unlock(&info->device_mutex);

	if (info->lowpower_flag & FTS_LOWP_FLAG_GESTURE_WAKEUP) {
		input_info(true, &info->client->dev, "%s : device start & lpgw_mode default off!\n", __func__);
		info->lowpower_flag = info->lowpower_flag & ~(FTS_LOWP_FLAG_GESTURE_WAKEUP);
		info->lowpower_mode = check_lowpower_mode(info);
	}

	info->fts_power_state = FTS_POWER_STATE_ACTIVE;

	return 0;
}

static void fts_shutdown(struct i2c_client *client)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &info->client->dev, "FTS %s called!\n", __func__);

	if (info->lowpower_mode) {
		info->lowpower_mode = 0;
		input_info(true, &info->client->dev, "FTS lowpower_mode off!\n");
	}

	fts_stop_device(info);
#ifdef FTS_SUPPORT_TOUCH_KEY
	if (info->board->led_power)
		info->board->led_power(info, false);
#endif
}

#ifdef CONFIG_PM
static int fts_pm_suspend(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	input_err(true, &info->client->dev, "%s\n", __func__);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		fts_stop_device(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}

static int fts_pm_resume(struct device *dev)
{
	struct fts_ts_info *info = dev_get_drvdata(dev);

	input_err(true, &info->client->dev, "%s\n", __func__);

	mutex_lock(&info->input_dev->mutex);

	if (info->input_dev->users)
		fts_start_device(info);

	mutex_unlock(&info->input_dev->mutex);

	return 0;
}
#endif

#if (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
static int fts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct fts_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &info->client->dev, "%s\n", __func__);

	fts_stop_device(info);

	return 0;
}

static int fts_resume(struct i2c_client *client)
{

	struct fts_ts_info *info = i2c_get_clientdata(client);

	input_info(true, &info->client->dev, "%s\n", __func__);

	fts_start_device(info);

	return 0;
}
#endif

static const struct i2c_device_id fts_device_id[] = {
	{FTS_TS_DRV_NAME, 0},
	{}
};

#ifdef CONFIG_PM
static const struct dev_pm_ops fts_dev_pm_ops = {
	.suspend = fts_pm_suspend,
	.resume = fts_pm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id fts_match_table[] = {
	{.compatible = "stm,fts_touch",},
	{},
};
#else
#define fts_match_table NULL
#endif

static struct i2c_driver fts_i2c_driver = {
	.driver = {
		   .name = FTS_TS_DRV_NAME,
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = fts_match_table,
#endif
#ifdef CONFIG_PM
		   .pm = &fts_dev_pm_ops,
#endif
		   },
	.probe = fts_probe,
	.remove = fts_remove,
	.shutdown = fts_shutdown,
#if (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
	.suspend = fts_suspend,
	.resume = fts_resume,
#endif
	.id_table = fts_device_id,
};

static int __init fts_driver_init(void)
{
	return i2c_add_driver(&fts_i2c_driver);
}

static void __exit fts_driver_exit(void)
{
	i2c_del_driver(&fts_i2c_driver);
}

MODULE_DESCRIPTION("STMicroelectronics MultiTouch IC Driver");
MODULE_AUTHOR("STMicroelectronics, Inc.");
MODULE_LICENSE("GPL v2");

module_init(fts_driver_init);
module_exit(fts_driver_exit);
