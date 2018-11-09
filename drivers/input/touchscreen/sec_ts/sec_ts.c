/* drivers/input/touchscreen/sec_ts.c
 *
 * Copyright (C) 2011 Samsung Electronics Co., Ltd.
 * http://www.samsungsemi.com/
 *
 * Core file for Samsung TSC driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/interrupt.h>
#include <linux/io.h>
#include <linux/platform_device.h>
#include <linux/firmware.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/delay.h>
#include <linux/sec_sysfs.h>
#include <linux/irq.h>
#include <linux/of_gpio.h>
#include <linux/time.h>
#include <linux/vmalloc.h>

#ifdef CONFIG_WAKE_GESTURES
#include <linux/kernel.h>
#include <linux/wake_gestures.h>
static bool is_suspended;
bool scr_suspended(void)
{
	return is_suspended;
}
#endif

#include "../../../i2c/busses/i2c-exynos5.h"
#include "sec_ts.h"

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
#include <linux/t-base-tui.h>
struct sec_ts_data *tui_tsp_info;
extern int tui_force_close(uint32_t arg);
#ifdef CONFIG_EPEN_WACOM_W9018
extern void epen_disable_mode(int mode);
#endif
#endif

#ifdef CONFIG_OF
#ifdef USE_OPEN_CLOSE
#undef CONFIG_HAS_EARLYSUSPEND
#undef CONFIG_PM
#endif
#endif

struct sec_ts_data *ts_dup;
static struct device *sec_ts_dev;
EXPORT_SYMBOL(sec_ts_dev);

struct sec_ts_fw_file {
	u8* data;
	u32 pos;
	size_t size;
};

struct sec_ts_event_status {
	u8 tchsta:3;
	u8 ttype:3;
	u8 eid:2;
	u8 sid;
	u8 buff2;
	u8 buff3;
	u8 buff4;
	u8 buff5;
	u8 buff6;
	u8 buff7;
} __attribute__ ((packed));

struct sec_ts_gesture_status {
	u8 stype:6;
	u8 eid:2;
	u8 gesture;
	u8 y_4_2:3;
	u8 x:5;
	u8 h_4:1;
	u8 w:5;
	u8 y_1_0:2;
	u8 reserved:4;
	u8 h_3_0:4;
} __attribute__ ((packed));

struct sec_ts_exp_fn {
	int (*func_init)(void *device_data);
	void (*func_remove)(void);
};

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev);
static void sec_ts_input_close(struct input_dev *dev);
#endif

static void sec_ts_reset_work(struct work_struct *work);
static void sec_ts_read_nv_work(struct work_struct *work);

static int sec_ts_stop_device(struct sec_ts_data *ts);
static int sec_ts_start_device(struct sec_ts_data *ts);

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_ts_early_suspend(struct early_suspend *h);
static void sec_ts_late_resume(struct early_suspend *h);
#endif

u8 lv1cmd;
u8* read_lv1_buff;
static int lv1_readsize;
static int lv1_readremain;
static int lv1_readoffset;

static ssize_t sec_ts_reg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static ssize_t sec_ts_regreadsize_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);
static inline ssize_t sec_ts_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count);
static ssize_t sec_ts_enter_recovery_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size);

static ssize_t sec_ts_regread_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static ssize_t sec_ts_gesture_status_show(struct device *dev,
	struct device_attribute *attr, char *buf);
static inline ssize_t sec_ts_show_error(struct device *dev,
	struct device_attribute *attr, char *buf);


static DEVICE_ATTR(sec_ts_reg, 0660, NULL, sec_ts_reg_store);
static DEVICE_ATTR(sec_ts_regreadsize, 0660, NULL, sec_ts_regreadsize_store);
static DEVICE_ATTR(sec_ts_enter_recovery, 0660, NULL, sec_ts_enter_recovery_store);
static DEVICE_ATTR(sec_ts_regread, 0660, sec_ts_regread_show, NULL);
static DEVICE_ATTR(sec_ts_gesture_status, 0660, sec_ts_gesture_status_show, NULL);

static struct attribute *cmd_attributes[] = {
	&dev_attr_sec_ts_reg.attr,
	&dev_attr_sec_ts_regreadsize.attr,
	&dev_attr_sec_ts_enter_recovery.attr,
	&dev_attr_sec_ts_regread.attr,
	&dev_attr_sec_ts_gesture_status.attr,
	NULL,
};

static struct attribute_group cmd_attr_group = {
	.attrs = cmd_attributes,
};

static inline ssize_t sec_ts_show_error(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	input_err(true, &ts->client->dev, "sec_ts :%s read only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

static inline ssize_t sec_ts_store_error(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	input_err(true, &ts->client->dev, "sec_ts :%s write only function, %s\n", __func__, attr->attr.name );
	return -EPERM;
}

int sec_ts_i2c_write(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[I2C_WRITE_BUFFER_SIZE + 1];
	int ret;
	unsigned char retry;
#ifdef POR_AFTER_I2C_RETRY
	int retry_cnt = 0;
#endif
	struct i2c_msg msg;

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &ts->client->dev,
			"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	if (len > I2C_WRITE_BUFFER_SIZE) {
		input_err(true, &ts->client->dev,
				"sec_ts_i2c_write len is larger than buffer size\n");
		return -1;
	}

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF \n", __func__);
		goto err;
	}

	buf[0] = reg;
	memcpy(buf+1, data, len);

	msg.addr = ts->client->addr;
	msg.flags = 0;
	msg.len = len + 1;
	msg.buf = buf;

#ifdef POR_AFTER_I2C_RETRY
 retry_fail:
#endif
	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1) {
			break;
		}

		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF ret = %d\n", __func__, ret);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}
		if (retry > 0)
			sec_ts_delay(10);

#ifdef USE_HW_PARAMETER
		if (retry > 1)
			ts->comm_err_count++;
#endif
	}
	mutex_unlock(&ts->i2c_mutex);
	if (retry == 10) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
#ifdef POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work,
					msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));

		if (!retry_cnt++)
			goto retry_fail;
#endif
		ret = -EIO;
	}

	if (ret == 1)
		return 0;
err:
	return -EIO;
}

int sec_ts_i2c_read(struct sec_ts_data * ts, u8 reg, u8 * data, int len)
{
	u8 buf[4];
	int ret;
	unsigned char retry;
#ifdef POR_AFTER_I2C_RETRY
	int retry_cnt = 0;
#endif
	struct i2c_msg msg[2];

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if (TRUSTEDUI_MODE_INPUT_SECURED & trustedui_get_current_mode()) {
		input_err(true, &ts->client->dev,
				"%s TSP no accessible from Linux, TUI is enabled!\n", __func__);
		return -EIO;
	}
#endif

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF \n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->i2c_mutex);

	buf[0] = reg;
	msg[0].addr = ts->client->addr;
	msg[0].flags = 0;
	msg[0].len = 1;
	msg[0].buf = buf;

#ifdef POR_AFTER_I2C_RETRY
 retry_fail_write:
#endif

	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, msg, 1)) == 1) {
			break;
		}

		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF ret = %d\n", __func__, ret);
			goto err;
		}
		if (retry > 0)
			sec_ts_delay(10);

#ifdef USE_HW_PARAMETER
		if (retry > 1)
			ts->comm_err_count++;
#endif
	}

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
#ifdef POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work,
					msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));

		if (!retry_cnt++)
			goto retry_fail_write;
#endif
	}

	if (ret != 1)
		goto err;

	udelay(100);

	msg[0].addr = ts->client->addr;
	msg[0].flags = I2C_M_RD;
	msg[0].len = len;
	msg[0].buf = data;

#ifdef POR_AFTER_I2C_RETRY
	retry_cnt = 0;
#endif

	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, msg, 1)) == 1) {
			break;
		}
	
		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF ret = %d\n", __func__, ret);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}
		if (retry > 0)
			sec_ts_delay(10);

#ifdef USE_HW_PARAMETER
		if (retry > 1)
			ts->comm_err_count++;
#endif
	}

	if (retry == SEC_TS_I2C_RETRY_CNT) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
#ifdef POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work,
			msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));

		if (!retry_cnt++)
			goto retry_fail_write;
#endif
		ret = -EIO;
	}

	mutex_unlock(&ts->i2c_mutex);
	return ret;
err:
	mutex_unlock(&ts->i2c_mutex);
	return -EIO;
}

#if defined(CONFIG_SEC_DEBUG_TSP_LOG)
struct delayed_work * p_ghost_check;
extern void sec_ts_run_rawdata_all(struct sec_ts_data *ts);
static void sec_ts_check_rawdata(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data, ghost_check.work);

	if (ts->tsp_dump_lock == 1) {
		input_err(true, &ts->client->dev, "%s, ignored ## already checking..\n", __func__);
		return;
	}
	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s, ignored ## IC is power off\n", __func__);
		return;
	}

	ts->tsp_dump_lock = 1;
	input_err(true, &ts->client->dev, "%s, start ##\n", __func__);
	sec_ts_run_rawdata_all((void *)ts);
	msleep(100);

	input_err(true, &ts->client->dev, "%s, done ##\n", __func__);
	ts->tsp_dump_lock = 0;

}

void tsp_dump_sec(void)
{
	printk(KERN_ERR "%s sec_ts %s: start \n", SECLOG, __func__);

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		printk(KERN_ERR "%s sec_ts %s : Do not load driver due to : lpm %d\n",
				SECLOG, __func__, lpcharge);
		return;
	}
#endif

	if (p_ghost_check == NULL) {
		printk(KERN_ERR "%s sec_ts %s, ignored ## tsp probe fail!!\n", SECLOG, __func__);
		return;
	}
	schedule_delayed_work(p_ghost_check, msecs_to_jiffies(100));
}
#else
void tsp_dump_sec(void)
{
	printk(KERN_ERR "%s sec_ts %s: not support\n", SECLOG, __func__);
}
#endif

static int sec_ts_i2c_read_bulk(struct sec_ts_data * ts, u8 * data, int len)
{
	int ret;
	unsigned char retry;
	struct i2c_msg msg;
#ifdef POR_AFTER_I2C_RETRY
	int retry_cnt = 0;
#endif

	msg.addr = ts->client->addr;
	msg.flags = I2C_M_RD;
	msg.len = len;
	msg.buf = data;

	mutex_lock(&ts->i2c_mutex);

#ifdef POR_AFTER_I2C_RETRY
 retry_fail:
#endif
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_transfer(ts->client->adapter, &msg, 1)) == 1) {
			break;
		}
		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF ret = %d\n", __func__, ret);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}

#ifdef USE_HW_PARAMETER
		if (retry > 1)
			ts->comm_err_count++;
#endif
	}

	mutex_unlock(&ts->i2c_mutex);

	if (retry == 10) {
		input_err(true, &ts->client->dev, "%s: I2C read over retry limit\n", __func__);
#ifdef POR_AFTER_I2C_RETRY
		schedule_delayed_work(&ts->reset_work,
					msecs_to_jiffies(TOUCH_RESET_DWORK_TIME));

		if (!retry_cnt++)
			goto retry_fail;
#endif
		ret = -EIO;
	}

	if (ret == 1)
		return 0;
err:
	return -EIO;
}

void sec_ts_delay(unsigned int ms)
{
	if (ms < 20)
		usleep_range(ms * 1000, ms * 1000);
	else
		msleep(ms);
}

int sec_ts_wait_for_ready(struct sec_ts_data *ts, unsigned int ack)
{
	int rc = -1;
	int retry = 0;
	u8 tBuff[SEC_TS_Event_Buff_Size];

	while (sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, tBuff, SEC_TS_Event_Buff_Size) > 0) {
		if (tBuff[0] == TYPE_STATUS_EVENT_ACK) {
			if (tBuff[1] == ack) {
				rc = 0;
				break;
			}
		}

		if (retry++ > SEC_TS_WAIT_RETRY_CNT) {
			input_err(true, &ts->client->dev, "%s: Time Over\n", __func__);
			break;
		}
		sec_ts_delay(20);
	}

	input_info(true, &ts->client->dev,
		"%s: %02X, %02X, %02X, %02X, %02X, %02X, %02X, %02X [%d]\n",
		__func__, tBuff[0], tBuff[1], tBuff[2], tBuff[3],
		tBuff[4], tBuff[5], tBuff[6], tBuff[7], retry);

	return rc;
}

int sec_ts_read_calibration_report(struct sec_ts_data *ts)
{
	int ret;
	u8 buf[5] = { 0 };

	buf[0] = SEC_TS_READ_CALIBRATION_REPORT;

	ret = sec_ts_i2c_read(ts, buf[0], &buf[1], 4);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read, %d\n", __func__, ret);
		return ret;
	}

	input_info(true, &ts->client->dev, "%s: count:%d, pass count:%d, fail count:%d, status:0x%X\n",
				__func__, buf[1], buf[2], buf[3], buf[4]);

	return buf[4];
}

#define MAX_EVENT_COUNT 128
static void sec_ts_read_event(struct sec_ts_data *ts)
{
	int ret;
	int is_event_remain;
	int t_id;
	int event_id;
	int read_event_count;
	u8 read_event_buff[SEC_TS_Event_Buff_Size];
	struct sec_ts_event_coordinate * p_event_coord;
	struct sec_ts_event_status * p_event_status;
	struct sec_ts_coordinate coordinate;
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
	u8 report_pressure;
#endif

	is_event_remain = 0;
	read_event_count = 0;
	ret = t_id = event_id = 0;

	memset(&coordinate, 0x00, sizeof(struct sec_ts_coordinate));

	/* repeat READ_ONE_EVENT until buffer is empty(No event) */
	do {
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_ONE_EVENT, read_event_buff, SEC_TS_Event_Buff_Size);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read one event failed\n", __func__);
			return ;
		}

		if (read_event_count > MAX_EVENT_COUNT) {
			input_err(true, &ts->client->dev, "%s : event buffer overflow\n", __func__);

			/* write clear event stack command when read_event_count > MAX_EVENT_COUNT */
			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
			if (ret < 0)
				input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

			return ;
		}

		event_id = read_event_buff[0] >> 6;
		switch (event_id) {
		case SEC_TS_Status_Event:
			if ((read_event_buff[0] == TYPE_STATUS_EVENT_ACK) &&
				(read_event_buff[1] == SEC_TS_ACK_BOOT_COMPLETE)) {
				complete_all(&ts->init_done);

				if (read_event_buff[2] == 0x20) { /* watchdog reset flag */
					sec_ts_release_all_finger(ts);
					ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
					if (ret < 0)
						input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);
				}
			}

			/* event queue full-> all finger release */
			if ((read_event_buff[0] == TYPE_STATUS_EVENT_ERR) &&
				(read_event_buff[1] == SEC_TS_ACK_EVENT_QUEUE_FULL)) {
				input_err(true, &ts->client->dev, "%s: IC Event Queue is full\n", __func__);
				sec_ts_release_all_finger(ts);
			}

			if (read_event_buff[0] > 0)
				input_info(true, &ts->client->dev, "%s: STATUS %x %x %x %x %x %x %x %x\n", __func__,
					read_event_buff[0], read_event_buff[1], read_event_buff[2],
					read_event_buff[3], read_event_buff[4], read_event_buff[5],
					read_event_buff[6], read_event_buff[7]);

			if ((read_event_buff[0] == TYPE_STATUS_EVENT_ERR) &&
				(read_event_buff[1] == SEC_TS_ERR_ESD)) {
				input_err(true, &ts->client->dev, "%s: ESD detected. run reset\n", __func__);
				schedule_work(&ts->reset_work.work);
			}

			if (read_event_buff[0] == TYPE_STATUS_EVENT_ACK &&
					read_event_buff[1] == SEC_TS_ACK_EVENT_WATER_WET_MODE) {
				ts->wet_mode = read_event_buff[2];
				input_info(true, &ts->client->dev, "%s: water wet mode %d\n",
						__func__, ts->wet_mode);
#ifdef USE_HW_PARAMETER
				if (ts->wet_mode)
					ts->wet_count++;
#endif
			}

			if (read_event_buff[0] == TYPE_STATUS_EVENT_ACK &&
					read_event_buff[1] == SEC_TS_ACK_EVENT_WATER_DIVE_MODE) {
				ts->dive_mode = read_event_buff[2];
				input_info(true, &ts->client->dev, "%s: water dive mode %d\n",
						__func__, ts->dive_mode);
#ifdef USE_HW_PARAMETER
			if (ts->dive_mode)
				ts->dive_count++;
#endif
			}

			if ((ts->use_sponge) && (read_event_buff[0] == TYPE_STATUS_EVENT_ACK) &&
				(read_event_buff[1] == TYPE_STATUS_CODE_SPONGE)) {
				u8 sponge[3] = { 0 };

				ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_READ_PARAM, sponge, 2);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to read sponge command\n", __func__);

				memset(sponge, 0x00, 3);

				ret = sec_ts_i2c_read_bulk(ts, sponge, 3);
				if (ret < 0)
					input_err(true, &ts->client->dev, "%s: fail to read sponge data\n", __func__);

				input_info(true, &ts->client->dev, "%s: Sponge, %x, %x, %x\n",
							__func__, sponge[0], sponge[1], sponge[2]);

				/* will be fixed to data structure */

				if (sponge[1] & SEC_TS_MODE_SPONGE_AOD) {
					u8 data[5] = {0x0A, 0x00, 0x00, 0x00};

					ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 2);
					if (ret < 0)
						input_err(true, &ts->client->dev,
								"%s: Failed to write aod read offset\n", __func__);

					ret = sec_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_READ_PARAM, data, 5);
					if (ret < 0)
						input_err(true, &ts->client->dev,
								"%s: Failed to read touch coord\n", __func__);

					if (data[4] & SEC_TS_AOD_GESTURE_DOUBLETAB)
						ts->scrub_id = SPONGE_EVENT_TYPE_AOD_DOUBLETAB;
					else if (data[4] & SEC_TS_AOD_GESTURE_LONGPRESS)
						ts->scrub_id = SPONGE_EVENT_TYPE_AOD_LONGPRESS;
					else if (data[4] & SEC_TS_AOD_GESTURE_PRESS)
						ts->scrub_id = SPONGE_EVENT_TYPE_AOD_PRESS;

					ts->scrub_x = (data[1] & 0xFF) << 8 | (data[0] & 0xFF);
					ts->scrub_y = (data[3] & 0xFF) << 8 | (data[2] & 0xFF);

#ifdef CONFIG_SAMSUNG_PRODUCT_SHIP
					input_info(true, &ts->client->dev, "%s: aod: %d\n",
							__func__, ts->scrub_id);
#else
					input_info(true, &ts->client->dev, "%s: aod: %d, %d, %d\n",
							__func__, ts->scrub_id, ts->scrub_x, ts->scrub_y);
#endif
				} else if(sponge[1] & SEC_TS_MODE_SPONGE_SPAY) {
					ts->scrub_id = SPONGE_EVENT_TYPE_SPAY;
				}

				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);
			}

			/* sar mode */
			if ((read_event_buff[0] == TYPE_STATUS_EVENT_ACK) && (read_event_buff[1] == 0x28)) {
				if (read_event_buff[2])
					ts->scrub_id = 101; /*sar on*/
				else
					ts->scrub_id = 100; /*sar off*/

				input_err(true, &ts->client->dev, "%s: SAR detected %d\n", __func__, read_event_buff[2]);
				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);
				input_sync(ts->input_dev);
				input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);
			}
			coordinate.action = SEC_TS_Coordinate_Action_None;
			is_event_remain = 0;
			break;

		case SEC_TS_Coordinate_Event:
			if (ts->lowpower_status) {
				is_event_remain = 0;
				input_info(true, &ts->client->dev, "%s: skip touch on lpm\n", __func__);
				break;
			}

			p_event_coord = (struct sec_ts_event_coordinate *)read_event_buff;

			t_id = (p_event_coord->tid - 1);

			if (t_id < MAX_SUPPORT_TOUCH_COUNT) {
				coordinate.id = t_id;
				coordinate.action = p_event_coord->tchsta;
				coordinate.x = (p_event_coord->x_11_4 << 4) | (p_event_coord->x_3_0);
				coordinate.y = (p_event_coord->y_11_4 << 4) | (p_event_coord->y_3_0);
				coordinate.touch_width = p_event_coord->z;
				coordinate.ttype = p_event_coord->ttype & 0x7;
				coordinate.major = p_event_coord->major;
				coordinate.minor = p_event_coord->minor;
				coordinate.mcount = ts->coord[t_id].mcount;
				coordinate.palm = (coordinate.ttype == SEC_TS_TOUCHTYPE_PALM) ? 1 : 0;

				if ((t_id == SEC_TS_EVENTID_HOVER) && coordinate.ttype == SEC_TS_TOUCHTYPE_PROXIMITY) {
					if ((coordinate.action == SEC_TS_Coordinate_Action_Release)) {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);
						input_dbg(true, &ts->client->dev,
							"%s: Hover - Release - tid=%d, touch_count=%d\n",
							__func__, t_id, ts->touch_count);
					} else {
						input_mt_slot(ts->input_dev, 0);
						input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, true);

						input_report_key(ts->input_dev, BTN_TOUCH, false);
						input_report_key(ts->input_dev, BTN_TOOL_FINGER, true);

						input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
						input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
						input_report_abs(ts->input_dev, ABS_MT_DISTANCE, coordinate.touch_width);

						if (coordinate.action == SEC_TS_Coordinate_Action_Press)
							input_dbg(true, &ts->client->dev,
								"%s: Hover - Press - tid=%d, touch_count=%d\n",
								__func__, t_id, ts->touch_count);
						else if (coordinate.action == SEC_TS_Coordinate_Action_Move)
							input_dbg(true, &ts->client->dev,
								"%s: Hover - Move - tid=%d, touch_count=%d\n",
								__func__, t_id, ts->touch_count);
					}
				}
				else if (coordinate.ttype == SEC_TS_TOUCHTYPE_NORMAL
						|| coordinate.ttype == SEC_TS_TOUCHTYPE_PALM
						|| coordinate.ttype == SEC_TS_TOUCHTYPE_GLOVE) {
						if (coordinate.action == SEC_TS_Coordinate_Action_Release) {
							coordinate.touch_width = 0;
							/*coordinate.action = SEC_TS_Coordinate_Action_None;*/

							input_mt_slot(ts->input_dev, t_id);
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
							input_report_abs(ts->input_dev, ABS_MT_PRESSURE, 0);
#endif
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 0);

							if (ts->touch_count > 0 )
								ts->touch_count--;
							if (ts->touch_count == 0) {
								input_report_key(ts->input_dev, BTN_TOUCH, 0);
								input_report_key(ts->input_dev, BTN_TOOL_FINGER, 0);
#ifdef USE_HW_PARAMETER
								ts->check_multi = 0;
#endif

							}
						} else if (coordinate.action == SEC_TS_Coordinate_Action_Press) {
							ts->touch_count++;

							input_mt_slot(ts->input_dev, t_id);
							input_mt_report_slot_state(ts->input_dev,
													MT_TOOL_FINGER,
													1 + (coordinate.palm << 1));
							input_report_key(ts->input_dev, BTN_TOUCH, 1);
							input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

							input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
							input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
							input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate.major);
							input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate.minor);
#ifdef SEC_TS_SUPPORT_SEC_SWIPE
							input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate.palm);
#endif
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
							report_pressure = coordinate.touch_width;
							if(report_pressure <= 0)
								report_pressure = 1;
#if defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
							if(!(ts->velocity_enable||ts->brush_enable))
								report_pressure = 255;
#endif
							input_report_abs(ts->input_dev, ABS_MT_PRESSURE, report_pressure);
#endif
#ifdef USE_HW_PARAMETER
							if ((ts->touch_count > 2) && (ts->check_multi == 0)) {
								ts->check_multi = 1;
								ts->multi_count++;
							}
#endif
						} else if (coordinate.action == SEC_TS_Coordinate_Action_Move) {
							if ((coordinate.ttype == SEC_TS_TOUCHTYPE_GLOVE) && !ts->touchkey_glove_mode_status) {
								ts->touchkey_glove_mode_status = true;
								input_report_switch(ts->input_dev, SW_GLOVE, 1);
							}

							input_mt_slot(ts->input_dev, t_id);
							input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, 1);
							input_report_key(ts->input_dev, BTN_TOUCH, 1);
							input_report_key(ts->input_dev, BTN_TOOL_FINGER, 1);

#ifdef CONFIG_WAKE_GESTURES
							if (is_suspended)
								coordinate.x += 5000;
#endif

							input_report_abs(ts->input_dev, ABS_MT_POSITION_X, coordinate.x);
							input_report_abs(ts->input_dev, ABS_MT_POSITION_Y, coordinate.y);
							input_report_abs(ts->input_dev, ABS_MT_TOUCH_MAJOR, coordinate.major);
							input_report_abs(ts->input_dev, ABS_MT_TOUCH_MINOR, coordinate.minor);
#ifdef SEC_TS_SUPPORT_SEC_SWIPE
							input_report_abs(ts->input_dev, ABS_MT_PALM, coordinate.palm);
#endif
#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
							report_pressure = coordinate.touch_width;
							if(report_pressure <= 0)
								report_pressure = 1;
#if defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
							if(!(ts->velocity_enable||ts->brush_enable))
								report_pressure = 255;
#endif
							input_report_abs(ts->input_dev, ABS_MT_PRESSURE, report_pressure);
#endif

							coordinate.mcount++;
						}

						memcpy(&ts->coord[t_id], &coordinate, sizeof(struct sec_ts_coordinate));
					}
				} else {
				input_err(true, &ts->client->dev, "%s: tid(%d) is  out of range\n", __func__, t_id);
			}

			is_event_remain = 1;
			break;

		case SEC_TS_Gesture_Event:
			p_event_status = (struct sec_ts_event_status *)read_event_buff;

			if((p_event_status->eid == 0x02) && (p_event_status->tchsta == 0x01)) {
				struct sec_ts_gesture_status *p_gesture_status = (struct sec_ts_gesture_status *)read_event_buff;
				if (p_gesture_status->gesture == SEC_TS_GESTURE_CODE_SIDE_GESTURE) {
					input_info(true, &ts->client->dev, "%s: Side gestrue : %x\n",
							__func__, p_gesture_status->gesture);

					if ((read_event_buff[2] >= 13) && (read_event_buff[4] >= 13))
						input_report_key(ts->input_dev, KEY_SIDE_GESTURE_RIGHT, 1);
					else if ((read_event_buff[2] <= 1) && (read_event_buff[4] <= 1))
						input_report_key(ts->input_dev, KEY_SIDE_GESTURE_LEFT, 1);					
					else
						input_info(true, &ts->client->dev, "%s: Side gestrue ERROR %x %x %x %x %x %x %x %x\n", __func__,
							read_event_buff[0], read_event_buff[1], read_event_buff[2],
							read_event_buff[3], read_event_buff[4], read_event_buff[5],
							read_event_buff[6], read_event_buff[7]);

					input_sync(ts->input_dev);
					input_report_key(ts->input_dev, KEY_SIDE_GESTURE_RIGHT, 0);					
					input_report_key(ts->input_dev, KEY_SIDE_GESTURE_LEFT, 0);										
				} else {
					input_info(true, &ts->client->dev, "%s: %s\n", __func__,
								p_gesture_status->gesture== SEC_TS_GESTURE_CODE_AOD ? "AOD" :
								p_gesture_status->gesture == SEC_TS_GESTURE_CODE_SPAY ? "SPAY" : "OTHER");

					/* will be fixed after merge String Liabrary : SPAY or Double Tab */
					if (p_gesture_status->gesture == SEC_TS_GESTURE_CODE_SPAY)
						ts->scrub_id = 0x04;
					else if (p_gesture_status->gesture== SEC_TS_GESTURE_CODE_AOD)
						ts->scrub_id = 0x08;

					input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 1);
					input_sync(ts->input_dev);
					input_report_key(ts->input_dev, KEY_BLACK_UI_GESTURE, 0);
				}
			}
			input_info(true, &ts->client->dev, "%s: GESTURE  %x %x %x %x %x %x\n", __func__,
				read_event_buff[0], read_event_buff[1], read_event_buff[2], read_event_buff[3], read_event_buff[4], read_event_buff[5]);

			is_event_remain = 1;
			break;

		default:
			input_err(true, &ts->client->dev, "%s: unknown event  %x %x %x %x %x %x\n", __func__,
				read_event_buff[0], read_event_buff[1], read_event_buff[2], read_event_buff[3], read_event_buff[4], read_event_buff[5]);

			is_event_remain = 1;
			break;

		}

#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
		if (coordinate.action == SEC_TS_Coordinate_Action_Press)
			input_info(true, &ts->client->dev,
				"%s: [P] tID:%d, x:%d, y:%d, z:%d, major:%d, minor:%d, tc:%d palm:%d\n",
				__func__, t_id, coordinate.x, coordinate.y, coordinate.touch_width,
				coordinate.major, coordinate.minor, ts->touch_count, coordinate.palm);
#else
		if (coordinate.action == SEC_TS_Coordinate_Action_Press)
			input_info(true, &ts->client->dev,
				"%s: [P] tID:%d, z:%d, major:%d, minor:%d, palm:%d, tc:%d\n",
				__func__, t_id, coordinate.touch_width, coordinate.major, coordinate.minor,
				coordinate.palm, ts->touch_count);
#endif
		else if (coordinate.action == SEC_TS_Coordinate_Action_Release) {
#if !defined(CONFIG_SAMSUNG_PRODUCT_SHIP)
			input_info(true, &ts->client->dev,
				"%s: [R] tID:%d mc:%d tc:%d lx:%d ly:%d cal:0x%x(%X|%X), [SE%02X%02X%02X] P%02X\n",
				__func__, t_id, ts->coord[t_id].mcount, ts->touch_count,
				ts->coord[t_id].x, ts->coord[t_id].y, ts->cal_status, ts->nv, ts->cal_count,
				ts->plat_data->panel_revision, ts->plat_data->img_version_of_ic[2],
				ts->plat_data->img_version_of_ic[3], ts->cal_pos);
#else
			input_info(true, &ts->client->dev,
				"%s: [R] tID:%d mc:%d tc:%d cal:0x%x(%X|%X) [SE%02X%02X%02X] P%02X\n",
				__func__, t_id, ts->coord[t_id].mcount, ts->touch_count, ts->cal_status,
				ts->nv, ts->cal_count, ts->plat_data->panel_revision, ts->plat_data->img_version_of_ic[2],
				ts->plat_data->img_version_of_ic[3], ts->cal_pos);
#endif
			ts->coord[t_id].mcount = 0;
		}
	} while(is_event_remain);
	input_sync(ts->input_dev);
}

static irqreturn_t sec_ts_irq_thread(int irq, void *ptr)
{
	struct sec_ts_data * ts;

	ts = (struct sec_ts_data *)ptr;

	if (ts->lowpower_mode)
		pm_wakeup_event(ts->input_dev->dev.parent, 1000);

	sec_ts_read_event(ts);

	return IRQ_HANDLED;
}

int get_tsp_status(void)
{
	return 0;
}
EXPORT_SYMBOL(get_tsp_status);

#define NOISE_MODE_CMD 0x77
void sec_ts_set_charger(bool enable)
{
	return;
/*
	int ret;
	u8 noise_mode_on[] = {0x01};
	u8 noise_mode_off[] = {0x00};
	if (enable) {
		input_info(true, &ts->client->dev, "sec_ts_set_charger : charger CONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, NOISE_MODE_CMD, noise_mode_on, sizeof(noise_mode_on));
		if (ret < 0)
			input_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_ON\n");
	} else {
		input_info(true, &ts->client->dev, "sec_ts_set_charger : charger DISCONNECTED!!\n");
		ret = sec_ts_i2c_write(ts, NOISE_MODE_CMD, noise_mode_off, sizeof(noise_mode_off));
		if (ret < 0)
			input_err(true, &ts->client->dev, "sec_ts_set_charger: fail to write NOISE_OFF\n");
	}
*/
}
EXPORT_SYMBOL(sec_ts_set_charger);

int sec_ts_glove_mode_enables(struct sec_ts_data *ts, int mode)
{
	int ret;

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: fail to enable glove status, POWER_STATUS=OFF \n",__func__);
		goto glove_enable_err;
	}

	if (mode)
		ts->touch_functions = (ts->touch_functions|SEC_TS_BIT_SETFUNC_GLOVE|SEC_TS_BIT_SETFUNC_MUTUAL);
	else 
		ts->touch_functions = ((ts->touch_functions&(~SEC_TS_BIT_SETFUNC_GLOVE))|SEC_TS_BIT_SETFUNC_MUTUAL);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto glove_enable_err;
	}

	input_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(mode)?"glove enable":"glove disable", ts->touch_functions);

	return 0;

glove_enable_err:
	(mode)? (ts->touch_functions = SEC_TS_BIT_SETFUNC_GLOVE)|SEC_TS_BIT_SETFUNC_MUTUAL:
	(ts->touch_functions = (ts->touch_functions&(~SEC_TS_BIT_SETFUNC_GLOVE))|SEC_TS_BIT_SETFUNC_MUTUAL);
		input_err(true, &ts->client->dev, "%s: %s, status =%x\n",
			__func__, (mode) ? "glove enable" : "glove disable", ts->touch_functions);
	return -EIO;
	}
EXPORT_SYMBOL(sec_ts_glove_mode_enables);

int sec_ts_hover_enables(struct sec_ts_data *ts, int enables)
{
	int ret;

	if (enables)
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_HOVER | SEC_TS_BIT_SETFUNC_MUTUAL);
	else
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_HOVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);

	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto hover_enable_err;
	}

	input_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enables)?"hover enable":"hover disable", ts->touch_functions);
	return 0;
hover_enable_err:
	(enables)? (ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_HOVER)|SEC_TS_BIT_SETFUNC_MUTUAL):
	(ts->touch_functions = (ts->touch_functions&(~SEC_TS_BIT_SETFUNC_HOVER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	input_err(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,(enables)?"hover enable":"hover disable", ts->touch_functions);
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_hover_enables);

int sec_ts_set_cover_type(struct sec_ts_data *ts, bool enable)
{
	int ret;

	switch (ts->cover_type) {
	case SEC_TS_VIEW_WIRELESS:
	case SEC_TS_VIEW_COVER:
	case SEC_TS_VIEW_WALLET:
	case SEC_TS_LED_COVER:
	case SEC_TS_MONTBLANC_COVER:
	case SEC_TS_CLEAR_FLIP_COVER :
	case SEC_TS_QWERTY_KEYBOARD_EUR :
	case SEC_TS_QWERTY_KEYBOARD_KOR :
		ts->cover_cmd = (u8)ts->cover_type;
		break;
	case SEC_TS_CHARGER_COVER:
	case SEC_TS_COVER_NOTHING1:
	case SEC_TS_COVER_NOTHING2:
	case SEC_TS_FLIP_WALLET:
	default:
		ts->cover_cmd = 0;
		input_err(true, &ts->client->dev, "%s: not chage touch state, %d\n",
		__func__, ts->cover_type);
		break;
	}

	if (enable)
		ts->touch_functions = (ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER | SEC_TS_BIT_SETFUNC_MUTUAL);
	else
		ts->touch_functions = ((ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER)) | SEC_TS_BIT_SETFUNC_MUTUAL);

	if (ts->power_status == SEC_TS_STATE_POWER_OFF || (ts->cover_cmd == 0 && enable))
		goto cover_enable_err;

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send covertype command: %d", __func__, ts->cover_cmd);
		goto cover_enable_err;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command", __func__);
		goto cover_enable_err;
	}

	input_info(true, &ts->client->dev, "%s: %s, status =%x\n", __func__,
		(enable) ? "clearcover enable" : "clearcover disable",
		ts->touch_functions);

	return 0;

cover_enable_err:
	input_err(true, &ts->client->dev, "%s error: %s, status =%x, cover_cmd = %d\n", __func__,
	(enable) ? "cover enable" : "cover disable", ts->touch_functions, ts->cover_cmd);
	return -EIO;
}
EXPORT_SYMBOL(sec_ts_set_cover_type);

#ifdef TWO_LEVEL_GRIP_CONCEPT
void sec_ts_set_grip_type(struct sec_ts_data *ts, u8 set_type)
{
	u8 mode = G_NONE;

	if(!(ts->plat_data->grip_concept & 0x6))	// 0x2 : grace, 0x4 : dual mode for hero2
		return;

	input_info(true, &ts->client->dev, "%s: re-init grip(%d), edh:%d, edg:%d, lan:%d\n", __func__,\
		set_type, ts->grip_edgehandler_direction, ts->grip_edge_range, ts->grip_landscape_mode);

	// edge handler
	if(ts->grip_edgehandler_direction != 0)	// default 0
		mode = mode | G_SET_EDGE_HANDLER;

	if(set_type == GRIP_ALL_DATA){
		//edge
		if(ts->grip_edge_range != 60)		// default 60
			mode = mode | G_SET_EDGE_ZONE;

		// dead zone
		if(ts->grip_landscape_mode == 1)	// default 0 mode, 32
			mode = mode | G_SET_LANDSCAPE_MODE;
		else
			mode = mode | G_SET_NORMAL_MODE;
	}

	if(mode)
		set_grip_data_to_ic(ts, mode);

}
#endif


int sec_ts_i2c_write_burst(struct sec_ts_data *ts, u8 *data, int len)
{
	int ret;
	int retry;

	mutex_lock(&ts->i2c_mutex);
	for (retry = 0; retry < SEC_TS_I2C_RETRY_CNT; retry++) {
		if ((ret = i2c_master_send(ts->client, data, len)) == len) {
			break;
		}
		if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
			input_err(true, &ts->client->dev,
				"%s: fail to POWER_STATUS=OFF ret = %d\n", __func__, ret);
			mutex_unlock(&ts->i2c_mutex);
			goto err;
		}
		if (retry > 0)
			sec_ts_delay(10);
	}
	mutex_unlock(&ts->i2c_mutex);
	if (retry == 10) {
		input_err(true, &ts->client->dev, "%s: I2C write over retry limit\n", __func__);
		ret = -EIO;
	}

	if (ret == len)
		return 0;
err:
	return -EIO;
}

/* for debugging--------------------------------------------------------------------------------------*/
static ssize_t sec_ts_reg_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int length;
	int remain;
	int offset;
	int ret;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
		return -EIO;
	}

	mutex_lock(&ts->device_mutex);
	disable_irq(ts->client->irq);
	if (size > 0) {
		remain = size;
		offset = 0;
		do {
			if (remain >= ts->i2c_burstmax)
				length = ts->i2c_burstmax;
			else
				length = remain;
			ret = sec_ts_i2c_write_burst(ts, (u8 *) & buf[offset], length);
			if (ret < 0) {
				input_err(true, &ts->client->dev,
						"%s: i2c write %x command, remain = %d\n", __func__,
						buf[offset], remain);
				goto i2c_err;
			}

			remain -= length;
			offset += length;
		} while (remain > 0);
	}

 i2c_err:
	enable_irq(ts->client->irq);
	input_info(true, &ts->client->dev, "%s: 0x%x, 0x%x, size %d\n", __func__,
		buf[0], buf[1], (int)size);
	mutex_unlock(&ts->device_mutex);

	return size;
}

static ssize_t sec_ts_regread_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	int ret;
	int length;
	int remain;
	int offset;

	if (ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_info(true, &ts->client->dev, "%s: Power off state\n", __func__);
		return -EIO;
	}

	disable_irq(ts->client->irq);

	mutex_lock(&ts->device_mutex);

	if ((lv1_readsize <= 0) || (lv1_readsize > PAGE_SIZE)) {
		input_err(true, &ts->client->dev, "%s: invalid lv1_readsize = %d\n",
						__func__, lv1_readsize);
		lv1_readsize = 0;
		goto malloc_err;
	}

	read_lv1_buff = (u8 *)kzalloc(sizeof(u8)*lv1_readsize, GFP_KERNEL);
	if (!read_lv1_buff) {
		input_err(true, &ts->client->dev, "%s kzalloc failed\n", __func__);
		goto malloc_err;
	}

	remain = lv1_readsize;
	offset = 0;
	do
	{
		if(remain >= ts->i2c_burstmax)
			length = ts->i2c_burstmax;
		else
			length = remain;

		if( offset == 0 )
			ret = sec_ts_i2c_read(ts, lv1cmd, &read_lv1_buff[offset], length);
		else
			ret = sec_ts_i2c_read_bulk(ts, &read_lv1_buff[offset], length);

		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: i2c read %x command, remain =%d\n", __func__, lv1cmd, remain);
			goto i2c_err;
		}

		remain -= length;
		offset += length;
	} while(remain > 0);

	input_info(true, &ts->client->dev, "%s: lv1_readsize = %d \n", __func__, lv1_readsize);
	memcpy(buf, read_lv1_buff + lv1_readoffset, lv1_readsize);

i2c_err:
	kfree(read_lv1_buff);
malloc_err:
	mutex_unlock(&ts->device_mutex);
	lv1_readremain = 0;
	enable_irq(ts->client->irq);

	return lv1_readsize;
}

static ssize_t sec_ts_gesture_status_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->device_mutex);
	memcpy(buf,ts->gesture_status,sizeof(ts->gesture_status));
	input_info(true, &ts->client->dev, "%s: GESTURE STATUS %x %x %x %x %x %x\n", __func__,
		ts->gesture_status[0], ts->gesture_status[1], ts->gesture_status[2],
		ts->gesture_status[3], ts->gesture_status[4], ts->gesture_status[5]);
	mutex_unlock(&ts->device_mutex);

	return sizeof(ts->gesture_status);
}

static ssize_t sec_ts_regreadsize_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	mutex_lock(&ts->device_mutex);

	lv1cmd = buf[0];
	lv1_readsize = ((unsigned int)buf[4] << 24) |
			((unsigned int)buf[3]<<16) |((unsigned int) buf[2]<<8) |((unsigned int)buf[1]<<0);
	lv1_readoffset = 0;
	lv1_readremain = 0;

	mutex_unlock(&ts->device_mutex);

	return size;
}

static ssize_t sec_ts_enter_recovery_store(struct device *dev, struct device_attribute *attr, const char *buf, size_t size)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);
	struct sec_ts_plat_data *pdata = dev->platform_data;
	int ret;
	u8 on = (u8)buf[0];

	if(on == 1) {
		disable_irq(ts->client->irq);
		gpio_free(pdata->gpio);

		input_info(true, &ts->client->dev, "%s: gpio free\n", __func__);
		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_OUT_INIT_LOW, "sec,tsp_int");
			input_info(true, &ts->client->dev, "%s: gpio request one\n", __func__);
			if (ret) {
				input_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			input_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(100);
		pdata->power(ts, true);
	} else {
		gpio_free(pdata->gpio);

		if (gpio_is_valid(pdata->gpio)) {
			ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
			if (ret) {
				input_err(true, &ts->client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
				return -EINVAL;
			}
		} else {
			input_err(true, &ts->client->dev, "Failed to get irq gpio\n");
			return -EINVAL;
		}

		pdata->power(ts, false);
		sec_ts_delay(500);
		pdata->power(ts, true);
		sec_ts_delay(500);

		/* AFE Calibration */
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CALIBRATION_AMBIENT, NULL, 0);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "%s: fail to write AFE_CAL\n",__func__);
		}
		sec_ts_delay(1000);
		enable_irq(ts->client->irq);
	}

	return size;
}

#ifdef SEC_TS_SUPPORT_TA_MODE
static void sec_ts_charger_config(struct sec_ts_data * ts, int status)
{
	int ret;

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: fail to enalbe charger status, POWER_STATUS=OFF \n",__func__);
		goto charger_config_err;
	}

	if (status == 0x01 || status == 0x03)
		ts->touch_functions = ts->touch_functions|SEC_TS_BIT_SETFUNC_CHARGER|SEC_TS_BIT_SETFUNC_MUTUAL;
	else
		ts->touch_functions = ((ts->touch_functions&(~SEC_TS_BIT_SETFUNC_CHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: Failed to send command\n", __func__);
		goto charger_config_err;
	}

	input_err(true, &ts->client->dev, "%s: charger inform : read status = %x\n", __func__, ts->touch_functions);
	return;

charger_config_err:
	if(status == 0x01 || status == 0x03)
		ts->touch_functions = ts->touch_functions|SEC_TS_BIT_SETFUNC_CHARGER|SEC_TS_BIT_SETFUNC_MUTUAL;
	else
		ts->touch_functions = ((ts->touch_functions&(~SEC_TS_BIT_SETFUNC_CHARGER))|SEC_TS_BIT_SETFUNC_MUTUAL);
	input_err(true, &ts->client->dev, "%s: charger inform : touch function status = %x\n", __func__, ts->touch_functions);
}

static void sec_ts_ta_cb(struct sec_ts_callbacks *cb, int status)
{
	struct sec_ts_data *ts =
		container_of(cb, struct sec_ts_data, callbacks);
	input_err(true, &ts->client->dev, "[TSP] %s: status : %x\n", __func__, status);

	ts->ta_status = status;
	/* if do not completed driver loading, ta_cb will not run. */
	/*if (!rmi4_data->init_done.done) {
		input_err(true, &rmi4_data->i2c_client->dev,
			"%s: until driver loading done.\n", __func__);
		return;
	}

	if (rmi4_data->touch_stopped || rmi4_data->doing_reflash) {
		input_err(true, &rmi4_data->i2c_client->dev,
			"%s: device is in suspend state or reflash.\n",
			__func__);
		return;
	}*/

	sec_ts_charger_config(ts,status);
}
#endif
static void sec_ts_raw_device_init(struct sec_ts_data *ts)
{
	int ret;

	sec_ts_dev = sec_device_create(ts, "sec_ts");

	ret = IS_ERR(sec_ts_dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: fail - device_create\n", __func__);
		return;
	}

	ret = sysfs_create_group(&sec_ts_dev->kobj, &cmd_attr_group);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail - sysfs_create_group\n", __func__);
		goto err_sysfs;
	}
#if 0
	ret = sysfs_create_link(&sec_ts_dev->kobj,
				&ts->input_dev->dev.kobj, "input");

	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: fail - sysfs_create_link\n", __func__);
		goto err_sysfs;
	}
#endif
	return;

err_sysfs:
	input_err(true, &ts->client->dev, "%s: fail\n",__func__);
	return;
}

/* for debugging--------------------------------------------------------------------------------------*/
static int sec_ts_power(void *data, bool on)
{
	struct sec_ts_data *ts = (struct sec_ts_data *)data;
	const struct sec_ts_plat_data *pdata = ts->plat_data;
	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;
	static bool enabled;
	int ret = 0;

	if (enabled == on)
		return ret;

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}

	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, &ts->client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}

	input_info(true, &ts->client->dev, "%s: %s\n", __func__, on ? "on" : "off");

	if (on) {
		ret = regulator_enable(regulator_dvdd);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to enable vdd: %d\n", __func__, ret);
			return ret;
		}
		ret = regulator_enable(regulator_avdd);
		if (ret) {
			input_err(true, &ts->client->dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
			return ret;
		}

		ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_default);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to configure tsp_attn pin\n", __func__);

		sec_ts_delay(5);
	} else {
		if (regulator_is_enabled(regulator_dvdd))
			regulator_disable(regulator_dvdd);
		if (regulator_is_enabled(regulator_avdd))
			regulator_disable(regulator_avdd);

		ret = pinctrl_select_state(pdata->pinctrl, pdata->pins_sleep);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to configure tsp_attn pin\n", __func__);
	}

	enabled = on;
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);

	return ret;
}

static int sec_ts_parse_dt(struct i2c_client *client)
{
	struct device *dev = &client->dev;
	struct sec_ts_plat_data *pdata = dev->platform_data;
	struct device_node *np = dev->of_node;
	struct regulator *regulator_dvdd;
	struct regulator *regulator_avdd;

	u32 coords[2], lines[2];
	int ret = 0;

	pdata->gpio_det = of_get_named_gpio(np, "sec,ds_det", 0);
	if (gpio_is_valid(pdata->gpio_det)) {
		ret = gpio_get_value(pdata->gpio_det);
		if (ret) {
			input_err(true, &client->dev, "ds_det: %d\n", ret);
			return -ENODEV;
		}
	}

	if (of_property_read_string(np, "sec,regulator_dvdd", &pdata->regulator_dvdd)) {
		input_err(true, &client->dev, "Failed to get regulator_dvdd name property\n");
		return -EINVAL;
	}
	if (of_property_read_string(np, "sec,regulator_avdd", &pdata->regulator_avdd)) {
		input_err(true, &client->dev, "Failed to get regulator_avdd name property\n");
		return -EINVAL;
	}
	pdata->power = sec_ts_power;

	regulator_dvdd = regulator_get(NULL, pdata->regulator_dvdd);
	if (IS_ERR(regulator_dvdd)) {
		input_err(true, &client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_dvdd);
		return PTR_ERR(regulator_dvdd);
	}
	regulator_avdd = regulator_get(NULL, pdata->regulator_avdd);
	if (IS_ERR(regulator_avdd)) {
		input_err(true, &client->dev, "%s: Failed to get %s regulator.\n",
			 __func__, pdata->regulator_avdd);
		return PTR_ERR(regulator_avdd);
	}
	ret = regulator_enable(regulator_dvdd);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to enable dvdd: %d\n", __func__, ret);
		return ret;
	}
	ret = regulator_enable(regulator_avdd);
	if (ret) {
		input_err(true, &client->dev, "%s: Failed to enable avdd: %d\n", __func__, ret);
		return ret;
	}

	pdata->tspid = of_get_named_gpio(np, "sec,tspid_gpio", 0);
	if(gpio_is_valid(pdata->tspid)){
		if (gpio_get_value(pdata->tspid)) {
			input_err(true, &client->dev, "%s :TSP_ID1 : %d  Use LSI IC\n",__func__,gpio_get_value(pdata->tspid));
			ret = false;
		}
		else{
			input_err(true, &client->dev, "%s :TSP_ID1 : %d  Skip to use STM IC\n",__func__,gpio_get_value(pdata->tspid));
			ret = true; //return -ENODEV;
		}
	}
	else{
		if((lcdtype & LCD_ID2_MODEL_MASK) != MODEL_HERO) {
			input_err(true, &client->dev, "%s : Do not support this panel %d\n",
				 __func__, lcdtype);
			ret = true; //return -ENODEV;
		}
		else ret = false;
	}

	if (regulator_is_enabled(regulator_dvdd)) regulator_disable(regulator_dvdd);
	if (regulator_is_enabled(regulator_avdd)) regulator_disable(regulator_avdd);
	regulator_put(regulator_dvdd);
	regulator_put(regulator_avdd);
	if(ret){
		sec_ts_delay(100);
		return -ENODEV;
	}
	else sec_ts_delay(20); 

	pdata->gpio = of_get_named_gpio(np, "sec,irq_gpio", 0);
	if (gpio_is_valid(pdata->gpio)) {
		ret = gpio_request_one(pdata->gpio, GPIOF_DIR_IN, "sec,tsp_int");
		if (ret) {
			input_err(true, &client->dev, "Unable to request tsp_int [%d]\n", pdata->gpio);
			return -EINVAL;
		}
	} else {
		input_err(true, &client->dev, "Failed to get irq gpio\n");
		return -EINVAL;
	}
	client->irq = gpio_to_irq(pdata->gpio);

	if (of_property_read_u32(np, "sec,irq_type", &pdata->irq_type)) {
		input_err(true, &client->dev, "Failed to get irq_type property\n");
		return -EINVAL;
	}

	if (of_property_read_u32_array(np, "sec,max_coords", coords, 2)) {
		input_err(true, &client->dev, "Failed to get max_coords property\n");
		return -EINVAL;
	}
	pdata->max_x = coords[0];
	pdata->max_y = coords[1];

	if (of_property_read_u32(np, "sec,grip_area", &pdata->grip_area))
		input_err(true, dev, "Failed to get grip_area property\n");

	if (of_property_read_u32(np, "sec,clear_calnv", &pdata->clear_calnv))
		input_err(true, dev, "Failed to get clear_calnv property\n");

	if (of_property_read_u32_array(np, "sec,num_lines", lines, 2))
		input_info(true, &client->dev, "skipped to get num_lines property\n");
	else {
		pdata->num_rx = lines[0];
		pdata->num_tx = lines[1];
		input_info(true, &client->dev, "num_of[rx,tx]: [%d,%d]\n",
			pdata->num_rx, pdata->num_tx);
	}

	of_property_read_string(np, "sec,firmware_name", &pdata->firmware_name);
	/* of_property_read_string(np, "sec,parameter_name", &pdata->parameter_name); */

	if (of_property_read_string_index(np, "sec,project_name", 0, &pdata->project_name))
		input_info(true, &client->dev, "skipped to get project_name property\n");
	if (of_property_read_string_index(np, "sec,project_name", 1, &pdata->model_name))
		input_info(true, &client->dev, "skipped to get model_name property\n");

	if (of_property_read_u32(np, "sec,mis_cal_check", &pdata->mis_cal_check) < 0)
		pdata->mis_cal_check = 0;

#if ANDROID_VERSION >= 70000  /* N OS */
	if (of_property_read_u32(np, "sec,grip_concept", &pdata->grip_concept) < 0)
		pdata->grip_concept = 1;	// default 1(set_tunning_data) for Hero.
#else
	pdata->grip_concept = 1;
#endif

	pdata->panel_revision = (lcdtype & 0xF000) >> 12;
	pdata->i2c_burstmax = SEC_TS_FW_MAX_BURSTSIZE;
	input_info(true, &client->dev, "irq :%d, irq_type: 0x%04x, max[x,y]: [%d,%d], grip_area :%d project/model_name: %s/%s, panel_revision: %d, lcdtype :0x%06X, clear_calnv(%d) mis_cal:%d, grip_cc:%d\n",
			pdata->gpio, pdata->irq_type, pdata->max_x, pdata->max_y, pdata->grip_area, pdata->project_name,
			pdata->model_name, pdata->panel_revision, lcdtype,pdata->clear_calnv, pdata->mis_cal_check, pdata->grip_concept);

	return ret;
}

static int sec_ts_setup_drv_data(struct i2c_client *client)
{
	int ret = 0;
	struct sec_ts_data *ts;
	struct sec_ts_plat_data *pdata;

	/* parse dt */
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct sec_ts_plat_data), GFP_KERNEL);

		if (!pdata) {
			input_err(true, &client->dev, "Failed to allocate platform data\n");
			return -ENOMEM;
		}

		client->dev.platform_data = pdata;
		ret = sec_ts_parse_dt(client);
		if (ret) {
			input_err(true, &client->dev, "Failed to parse dt\n");
			return ret;
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

	ts = kzalloc(sizeof(struct sec_ts_data), GFP_KERNEL);
	if (!ts) {
		input_err(true, &client->dev, "%s: Failed to alloc mem for info\n", __func__);
		return -ENOMEM;
	}

	ts->client = client;
	ts->plat_data = pdata;
	ts->crc_addr = 0x0001FE00;
	ts->fw_addr = 0x00002000;
	ts->para_addr = 0x18000;
	ts->sec_ts_i2c_read = sec_ts_i2c_read;
	ts->sec_ts_i2c_write = sec_ts_i2c_write;
	ts->sec_ts_i2c_read_bulk = sec_ts_i2c_read_bulk;
	ts->sec_ts_i2c_write_burst = sec_ts_i2c_write_burst;
	ts->i2c_burstmax = pdata->i2c_burstmax;

	INIT_DELAYED_WORK(&ts->reset_work, sec_ts_reset_work);
	INIT_DELAYED_WORK(&ts->read_nv_work, sec_ts_read_nv_work);
	i2c_set_clientdata(client, ts);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = ts;
#endif

	return ret;
}
#ifdef SEC_TS_SUPPORT_SPONGELIB
static int sec_ts_check_custom_library(struct sec_ts_data *ts)
{
	u8 data[10] = {0 };
	int ret;

	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_SPONGE_GET_INFO, &data[0], 10);
	
	input_info(true, &ts->client->dev,
				"%s: (%d) %c%c%c%c, || %02X, %02X, %02X, %02X, || %02X, %02X\n",
				__func__, ret, data[0], data[1], data[2], data[3], data[4],
				data[5], data[6], data[7], data[8], data[9]);

	/* compare model name with device tree */
	ret = strncmp(data, ts->plat_data->model_name, 4);
	if (ret == 0)
		ts->use_sponge = true;
	else
		ts->use_sponge = false;

	input_info(true, &ts->client->dev, "%s: use %s\n",
				__func__, ts->use_sponge ? "SPONGE" : "VENDOR");

	return ret;
}
#endif

#define T_BUFF_SIZE 5
static int sec_ts_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct sec_ts_data *ts;
	struct exynos5_i2c *i2c_master = (struct exynos5_i2c *)client->adapter->algo_data;
	u8 tBuff[T_BUFF_SIZE];
#ifndef CONFIG_FW_UPDATE_ON_PROBE
	const struct firmware *fw_entry;
	char fw_path[SEC_TS_MAX_FW_PATH];
#endif
	static char sec_ts_phys[64] = { 0 };
	int ret = 0;
	bool force_update = false;
	unsigned char data[5] = { 0 };

	input_info(true, &client->dev, "SEC_TS Driver [%s]\n",
			SEC_TS_DRV_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		input_err(true, &client->dev, "%s : EIO err!\n", __func__);
		return -EIO;
	}

#ifdef CONFIG_BATTERY_SAMSUNG
	if (lpcharge == 1) {
		input_err(true, &client->dev, "%s : Do not load driver due to : lpm %d\n",
			 __func__, lpcharge);
		return -ENODEV;
	}
#endif

	ret = sec_ts_setup_drv_data(client);
	if (ret < 0) {
		input_err(true, &client->dev, "%s: Failed to set up driver data\n", __func__);
		goto err_setup_drv_data;
	}

	ts = (struct sec_ts_data *)i2c_get_clientdata(client);
	if (!ts) {
		input_err(true, &client->dev, "%s: Failed to get driver data\n", __func__);
		ret = -ENODEV;
		goto err_get_drv_data;
	}

#ifdef TWO_LEVEL_GRIP_CONCEPT
	if(ts->plat_data->grip_concept & 0x4){	// 0x2 : grace, 0x4 : dual mode for hero2
		ts->gripreg_edge_handler = SEC_TS_CMD_EDGE_HANDLER_FOR_DUAL;
		ts->gripreg_edge_area = SEC_TS_CMD_EDGE_AREA_FOR_DUAL;
		ts->gripreg_dead_zone = SEC_TS_CMD_DEAD_ZONE_FOR_DUAL;
		ts->gripreg_landscape_mode = SEC_TS_CMD_LANDSCAPE_MODE_FOR_DUAL;
	}else{
		ts->gripreg_edge_handler = SEC_TS_CMD_EDGE_HANDLER;
		ts->gripreg_edge_area = SEC_TS_CMD_EDGE_AREA;
		ts->gripreg_dead_zone = SEC_TS_CMD_DEAD_ZONE;
		ts->gripreg_landscape_mode = SEC_TS_CMD_LANDSCAPE_MODE;
	}
#endif

	ts->input_dev = input_allocate_device();
	if (!ts->input_dev) {
		input_err(true, &ts->client->dev, "%s: allocate device err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_device;
	}

	i2c_master->stop_after_trans = 1;

	ts->input_dev->name = "sec_touchscreen";
	snprintf(sec_ts_phys, sizeof(sec_ts_phys), "%s/input1",
		ts->input_dev->name);
	ts->input_dev->phys = sec_ts_phys;
	ts->input_dev->id.bustype = BUS_I2C;
	ts->input_dev->dev.parent = &client->dev;
	ts->touch_count = 0;
#ifdef USE_OPEN_CLOSE
	ts->input_dev->open = sec_ts_input_open;
	ts->input_dev->close = sec_ts_input_close;
#endif

	mutex_init(&ts->lock);
	mutex_init(&ts->device_mutex);
	mutex_init(&ts->i2c_mutex);

	init_completion(&ts->init_done);

	/* Enable Power */
	ts->plat_data->power(ts, true);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	sec_ts_delay(60);
	sec_ts_wait_for_ready(ts, SEC_TS_ACK_BOOT_COMPLETE);
	ts->external_factory = false;

#ifndef CONFIG_FW_UPDATE_ON_PROBE
	input_info(true, &ts->client->dev, "%s: fw update on probe disabled!\n", __func__);
	snprintf(fw_path, SEC_TS_MAX_FW_PATH, "%s", SEC_TS_DEFAULT_FW_NAME);
	if (request_firmware(&fw_entry, fw_path, &ts->client->dev) != 0)
		input_err(true, &ts->client->dev, "%s: firmware is not available\n", __func__);
	else
		sec_ts_check_firmware_version(ts, fw_entry->data);

	release_firmware(fw_entry);
#endif

	ret = sec_ts_i2c_read(ts, SEC_TS_READ_BOOT_STATUS, &data[0], 1);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "%s: failed to read sub id(%d)\n", __func__, ret);
	} else {
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_TS_STATUS, &data[1], 4);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: failed to touch status(%d)\n", __func__, ret);
	}

	input_info(true, &ts->client->dev, 
		"%s: TOUCH STATUS : %02X || %02X, %02X, %02X, %02X\n", __func__,
		data[0], data[1], data[2], data[3], data[4]); 

	if (((data[0] == SEC_TS_STATUS_APP_MODE) && (data[2] == TOUCH_SYSTEM_MODE_FLASH)) || (ret < 0))
		force_update = true;
	else
		force_update = false;

#ifdef CONFIG_FW_UPDATE_ON_PROBE
	ret = sec_ts_firmware_update_on_probe(ts, force_update);
	if (ret < 0)
		goto err_init;
#endif

	if ((ts->tx_count == 0) || (ts->rx_count == 0)) {

		/* Read Raw Channel Info */
		ret = sec_ts_i2c_read(ts, SEC_TS_READ_SUB_ID, tBuff, 5);
		if (ret < 0) {
			input_err(true, &ts->client->dev, "sec_ts_probe: fail to read raw channel info\n");
			goto err_init;
		} else {
			ts->tx_count = tBuff[3];
			ts->rx_count = tBuff[4];
			input_info(true, &ts->client->dev, "sec_ts_probe: S6SSEC_TS Tx : %d, Rx : %d\n", ts->tx_count, ts->rx_count);
		}
	}

	/* Sense_on */
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0) {
		input_err(true, &ts->client->dev, "sec_ts_probe: fail to write Sense_on\n");
		goto err_init;
	}

	ts->pFrame = kzalloc(ts->tx_count * ts->rx_count * 2, GFP_KERNEL);
	if (!ts->pFrame) {
		input_err(true, &ts->client->dev, "%s: allocate pFrame err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_frame;
	}
	ts->sFrame = kzalloc((ts->tx_count + ts->rx_count) * 2, GFP_KERNEL);
	if (!ts->sFrame) {
		input_err(true, &ts->client->dev, "%s: allocate sFrame err!\n", __func__);
		ret = -ENOMEM;
		goto err_allocate_sframe;
	}

#ifdef CONFIG_TOUCHSCREN_SEC_TS_GLOVEMODE
	input_set_capability(ts->input_dev, EV_SW, SW_GLOVE);
#endif
	set_bit(EV_SYN, ts->input_dev->evbit);
	set_bit(EV_KEY, ts->input_dev->evbit);
	set_bit(EV_ABS, ts->input_dev->evbit);
	set_bit(BTN_TOUCH, ts->input_dev->keybit);
	set_bit(BTN_TOOL_FINGER, ts->input_dev->keybit);
	set_bit(KEY_BLACK_UI_GESTURE, ts->input_dev->keybit);

#ifdef SEC_TS_SUPPORT_TOUCH_KEY
	if (ts->plat_data->support_mskey) {
		for (i = 0 ; i < ts->plat_data->num_touchkey ; i++)
			set_bit(ts->plat_data->touchkey[i].keycode, ts->input_dev->keybit);

		set_bit(EV_LED, ts->input_dev->evbit);
		set_bit(LED_MISC, ts->input_dev->ledbit);
	}
#endif
		set_bit(KEY_SIDE_GESTURE, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_RIGHT, ts->input_dev->keybit);
		set_bit(KEY_SIDE_GESTURE_LEFT, ts->input_dev->keybit);

#ifdef INPUT_PROP_DIRECT
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);
#endif

	ts->input_dev->evbit[0] = BIT_MASK(EV_ABS) | BIT_MASK(EV_KEY);
	set_bit(INPUT_PROP_DIRECT, ts->input_dev->propbit);

	input_mt_init_slots(ts->input_dev, MAX_SUPPORT_TOUCH_COUNT, INPUT_MT_DIRECT);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_X, 0, ts->plat_data->max_x, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_POSITION_Y, 0, ts->plat_data->max_y, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MAJOR, 0, 255, 0, 0);
	input_set_abs_params(ts->input_dev, ABS_MT_TOUCH_MINOR, 0, 255, 0, 0);
#ifdef SEC_TS_SUPPORT_SEC_SWIPE
	input_set_abs_params(ts->input_dev, ABS_MT_PALM, 0, 1, 0, 0);
#endif
#if defined(SEC_TS_SUPPORT_GRIP_EVENT)
		input_set_abs_params(ts->input_dev, ABS_MT_GRIP, 0, 1, 0, 0);
#endif
	input_set_abs_params(ts->input_dev, ABS_MT_DISTANCE, 0, 255, 0, 0);

#if defined(CONFIG_SEC_FACTORY) || defined(CONFIG_TOUCHSCREEN_SUPPORT_MULTIMEDIA)
	input_set_abs_params(ts->input_dev, ABS_MT_PRESSURE, 0, 255, 0, 0);
#endif

	input_set_drvdata(ts->input_dev, ts);
	i2c_set_clientdata(client, ts);

	ret = input_register_device(ts->input_dev);
	if (ret) {
		input_err(true, &ts->client->dev, "%s: Unable to register %s input device\n", __func__, ts->input_dev->name);
		goto err_input_register_device;
	}

	input_info(true, &ts->client->dev, "sec_ts_probe request_irq = %d\n" , client->irq);

	ret = request_threaded_irq(client->irq, NULL, sec_ts_irq_thread,
		ts->plat_data->irq_type, SEC_TS_I2C_NAME, ts);

	if (ret < 0) {
		input_err(true, &ts->client->dev, "sec_ts_probe: Unable to request threaded irq\n");
		goto err_irq;
	}

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	trustedui_set_tsp_irq(client->irq);
	input_info(true, &client->dev, "%s[%d] called!\n",
		__func__, client->irq);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
	ts->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	ts->early_suspend.suspend = sec_ts_early_suspend;
	ts->early_suspend.resume = sec_ts_late_resume;
	register_early_suspend(&ts->early_suspend);
#endif

#ifdef SEC_TS_SUPPORT_TA_MODE
	ts->callbacks.inform_charger = sec_ts_ta_cb;
	if(ts->plat_data->register_cb)
		ts->plat_data->register_cb(&ts->callbacks);
#endif

	sec_ts_raw_device_init(ts);
	sec_ts_fn_init(ts);

#ifdef CONFIG_SEC_DEBUG_TSP_LOG
	INIT_DELAYED_WORK(&ts->ghost_check, sec_ts_check_rawdata);
	p_ghost_check = &ts->ghost_check;
#endif

#ifdef SEC_TS_SUPPORT_SPONGELIB
	sec_ts_check_custom_library(ts);
#endif
	schedule_delayed_work(&ts->read_nv_work, msecs_to_jiffies(5000));

	ts_dup = ts;

	ts->probe_done = true;
	device_init_wakeup(&client->dev, true);

	return 0;

err_irq:
	input_unregister_device(ts->input_dev);
	ts->input_dev = NULL;
err_input_register_device:
	if (ts->input_dev)
		input_free_device(ts->input_dev);

	kfree(ts->sFrame);
err_allocate_sframe:
	kfree(ts->pFrame);
err_allocate_frame:
err_init:
	ts->plat_data->power(ts, false);
err_allocate_device:
err_get_drv_data:
	kfree(ts);
err_setup_drv_data:
#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = NULL;
#endif
	ts_dup = NULL;
	return ret;
}

void sec_ts_release_all_finger(struct sec_ts_data *ts)
{
	int i;

	for (i=0; i < MAX_SUPPORT_TOUCH_COUNT; i++) {
		input_mt_slot(ts->input_dev, i);
		input_mt_report_slot_state(ts->input_dev, MT_TOOL_FINGER, false);

		if ((ts->coord[i].action == SEC_TS_Coordinate_Action_Press) ||
			(ts->coord[i].action == SEC_TS_Coordinate_Action_Move)) {
			ts->touch_count--;
			if (ts->touch_count < 0)
				ts->touch_count = 0;

			ts->coord[i].action = SEC_TS_Coordinate_Action_Release;

			input_info(true, &ts->client->dev,
				"%s: [RA] tID:%d mc:%d tc:%d cal:0x%x(%X|%X) [SE%02X%02X%02X]\n",
				__func__, i, ts->coord[i].mcount, ts->touch_count, ts->cal_status,
				ts->nv, ts->cal_count, ts->plat_data->panel_revision, ts->plat_data->img_version_of_ic[2],
				ts->plat_data->img_version_of_ic[3]);
		}

		ts->coord[i].mcount = 0;
	}

	input_report_key(ts->input_dev, BTN_TOUCH, false);
	input_report_key(ts->input_dev, BTN_TOOL_FINGER, false);
	input_report_switch(ts->input_dev, SW_GLOVE, false);
	ts->touchkey_glove_mode_status = false;
	ts->touch_count = 0;
#ifdef USE_HW_PARAMETER
	ts->check_multi = 0;
#endif

	input_report_switch(ts->input_dev, KEY_SIDE_GESTURE_LEFT, false);
	input_report_switch(ts->input_dev, KEY_SIDE_GESTURE_RIGHT, false);

	input_sync(ts->input_dev);
}

#if 0 //def CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	return;
}
#endif

static int sec_ts_set_lowpowermode(struct sec_ts_data *ts, u8 mode)
{
	int ret = -1,retrycnt = 0;
	u8 lowpower_data;
	char para = 0;

	input_err(true, &ts->client->dev, "%s: %s\n", __func__,
			mode == TO_LOWPOWER_MODE ? "ENTER" : "EXIT");

	disable_irq(ts->client->irq);

	if (mode) {
		if (ts->use_sponge) {
			u8 data[3] = { 0 };

			data[2] = ((ts->lowpower_flag) & 0xF);

			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_WRITE_PARAM, &data[0], 3);
			if (ret < 0)
				input_err(true, &ts->client->dev, "%s: Failed to write mode\n", __func__);

			ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_NOTIFY_PACKET, NULL, 0);
			if (ret < 0)
				input_err(true, &ts->client->dev, "%s: Failed to write mode\n", __func__);
		}

		lowpower_data = ((ts->lowpower_flag >> 4) & 0xF);

		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_GESTURE_MODE, &lowpower_data, 1);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to write mode\n", __func__);

		if (ts->lowpower_flag & SEC_TS_MODE_SPONGE_AOD) {
			u8 aod_rect[10] = {0x02, 0};

			/* Clear AOD_RECT */
			input_info(true, &ts->client->dev, "%s: clear aod_rect\n", __func__);
			ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_WRITE_PARAM, &aod_rect[0], 10);
			if (ret < 0)
				input_err(true, &ts->client->dev, "%s: Failed to write offset\n", __func__);

			ret = ts->sec_ts_i2c_write(ts, SEC_TS_CMD_SPONGE_NOTIFY_PACKET, NULL, 0);
			if (ret < 0)
				input_err(true, &ts->client->dev, "%s: Failed to send notify\n", __func__);
		}

		input_info(true, &ts->client->dev, "%s set lowpower flag:%d lowpower_data:%d\n", __func__,
			ts->lowpower_flag, lowpower_data);
	}

	ts->lowpower_status = mode;

retry_pmode:
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_POWER_MODE, &mode, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: failed\n", __func__);
	sec_ts_delay(50);

	/* read data */
	ret = ts->sec_ts_i2c_read(ts, SEC_TS_CMD_SET_POWER_MODE, &para, 1);
	if (ret < 0) input_err(true, &ts->client->dev, "%s: read power mode failed!\n", __func__);
	else input_err(true, &ts->client->dev, "%s: power mode - write(%d) read(%d)\n", __func__,mode,para);
	if(mode != para){
		retrycnt++;
		if(retrycnt<5) goto retry_pmode;
	}

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_CLEAR_EVENT_STACK, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: i2c write clear event failed\n", __func__);

	sec_ts_release_all_finger(ts);

	enable_irq(ts->client->irq);

	return ret;
}

static void sec_ts_reset_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
						reset_work.work);

	if (!ts->probe_done)
		return;

	input_info(true, &ts->client->dev, "%s start\n", __func__);

	sec_ts_stop_device(ts);
	sec_ts_delay(30);
	sec_ts_start_device(ts);

	input_info(true, &ts->client->dev, "%s done\n", __func__);
}

static void sec_ts_read_nv_work(struct work_struct *work)
{
	struct sec_ts_data *ts = container_of(work, struct sec_ts_data,
						read_nv_work.work);
#ifndef CONFIG_SEC_FACTORY
	struct sec_ts_test_mode mode;	
#endif

	ts->nv = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_FAC_RESULT);
	ts->cal_count = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_COUNT);
	ts->cal_pos = get_tsp_nvm_data(ts, SEC_TS_NVM_OFFSET_CAL_POS);

	input_info(true, &ts->client->dev, "%s: fac_nv:%02X, cal_nv:%02X cal_pos:%02X\n", __func__, ts->nv, ts->cal_count, ts->cal_pos);
#ifdef USE_HW_PARAMETER
#ifndef CONFIG_SEC_FACTORY
	/* run self-test */
	disable_irq(ts->client->irq);
	execute_selftest(ts);
	enable_irq(ts->client->irq);

	input_raw_info(true, &ts->client->dev, "%s: ITO %02X %02X %02X %02X\n",
		__func__, ts->ito_test[0], ts->ito_test[1]
		, ts->ito_test[2], ts->ito_test[3]);

	/* run ambient read */
	memset(&mode, 0x00, sizeof(struct sec_ts_test_mode));
	mode.type = 3;//TYPE_AMBIENT_DATA;
	mode.allnode = TEST_MODE_ALL_NODE;

	sec_ts_read_raw_data(ts, &mode);
#endif
#endif

}

#ifdef USE_OPEN_CLOSE
static int sec_ts_input_open(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);
	int ret;

#ifdef CONFIG_WAKE_GESTURES
	is_suspended = false;
#endif

	input_info(true, &ts->client->dev, "%s, wet:%d, dive:%d\n", __func__, ts->wet_mode, ts->dive_mode);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
		input_err(true, &ts->client->dev, "%s TUI cancel event call!\n", __func__);
		msleep(100);
		tui_force_close(1);
		msleep(200);
		if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
			input_err(true, &ts->client->dev, "%s TUI flag force clear!\n",	__func__);
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif

	if (ts->lowpower_status) {
#ifdef USE_OPEN_POWER_RESET
		sec_ts_stop_device(ts);
		sec_ts_delay(30);
		sec_ts_start_device(ts);
		ts->lowpower_status = TO_TOUCH_MODE;
#else
#ifdef CONFIG_WAKE_GESTURES
		if (s2w_switch || dt2w_switch)
			disable_irq_wake(ts->client->irq);
		else
#endif
		sec_ts_set_lowpowermode(ts, TO_TOUCH_MODE);
#endif
		if (device_may_wakeup(&ts->client->dev))
			disable_irq_wake(ts->client->irq);
	} else {
		ret = sec_ts_start_device(ts);
		if (ret < 0)
			input_err(true, &ts->client->dev, "%s: Failed to start device\n", __func__);
	}

#ifdef TWO_LEVEL_GRIP_CONCEPT
	sec_ts_set_grip_type(ts, ONLY_EDGE_HANDLER);	// because edge and dead zone will recover soon
#endif

#ifdef CONFIG_WAKE_GESTURES
	if (dt2w_switch_changed) {
		dt2w_switch = dt2w_switch_temp;
		dt2w_switch_changed = false;
	}
	if (s2w_switch_changed) {
		s2w_switch = s2w_switch_temp;
		s2w_switch_changed = false;
	}
#endif

	return 0;
}

static void sec_ts_input_close(struct input_dev *dev)
{
	struct sec_ts_data *ts = input_get_drvdata(dev);

#ifdef CONFIG_WAKE_GESTURES
	is_suspended = true;
#endif

	input_info(true, &ts->client->dev, "%s, wet:%d, dive:%d\n", __func__, ts->wet_mode, ts->dive_mode);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
		input_err(true, &ts->client->dev, "%s TUI cancel event call!\n", __func__);
		msleep(100);
		tui_force_close(1);
		msleep(200);
		if(TRUSTEDUI_MODE_TUI_SESSION & trustedui_get_current_mode()){	
			input_err(true, &ts->client->dev, "%s TUI flag force clear!\n",	__func__);
			trustedui_clear_mask(TRUSTEDUI_MODE_VIDEO_SECURED|TRUSTEDUI_MODE_INPUT_SECURED);
			trustedui_set_mode(TRUSTEDUI_MODE_OFF);
		}
	}
#endif

	cancel_delayed_work(&ts->reset_work);

#ifdef CONFIG_WAKE_GESTURES
	if (s2w_switch || dt2w_switch)
		enable_irq_wake(ts->client->irq);
	else
#endif
	if (ts->lowpower_mode) {
		sec_ts_set_lowpowermode(ts, TO_LOWPOWER_MODE);

		if (device_may_wakeup(&ts->client->dev))
			enable_irq_wake(ts->client->irq);
	} else
		sec_ts_stop_device(ts);
}
#endif

static int sec_ts_remove(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	input_info(true, &ts->client->dev, "%s\n", __func__);
#ifdef USE_HW_PARAMETER
	cancel_delayed_work(&ts->read_nv_work);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ts->early_suspend);
#endif

	free_irq(client->irq, ts);

	input_mt_destroy_slots(ts->input_dev);
	input_unregister_device(ts->input_dev);

	ts->input_dev = NULL;
	ts_dup = NULL;

	ts->plat_data->power(ts, false);

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
	tui_tsp_info = NULL;
#endif
	kfree(ts);
	return 0;
}

static void sec_ts_shutdown(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_stop_device(ts);
}

static int sec_ts_stop_device(struct sec_ts_data *ts)
{
	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if(ts->power_status == SEC_TS_STATE_POWER_OFF) {
		input_err(true, &ts->client->dev, "%s: already power off\n", __func__);
		goto out;
	}
	ts->power_status = SEC_TS_STATE_POWER_OFF;

	disable_irq(ts->client->irq);
	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, false);

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(false);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

static int sec_ts_start_device(struct sec_ts_data *ts)
{
	int ret;
	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->device_mutex);

	if (ts->power_status == SEC_TS_STATE_POWER_ON) {
		input_err(true, &ts->client->dev, "%s: already power on\n", __func__);
		goto out;
	}

	sec_ts_release_all_finger(ts);

	ts->plat_data->power(ts, true);
	ts->power_status = SEC_TS_STATE_POWER_ON;
	enable_irq(ts->client->irq);

	reinit_completion(&ts->init_done);
	ret = wait_for_completion_interruptible_timeout(&ts->init_done, msecs_to_jiffies(100));
	if (ret < 0) {
		input_err(true, &ts->client->dev,
			"%s: timeout while waiting for device to init (%d)\n", __func__, ret);
	}

	if (ts->plat_data->enable_sync)
		ts->plat_data->enable_sync(true);

#ifdef SEC_TS_SUPPORT_TA_MODE
	if (ts->ta_status)
		sec_ts_charger_config(ts, ts->ta_status);
#endif

	if (ts->flip_enable) {
		ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_COVERTYPE, &ts->cover_cmd, 1);
		ts->touch_functions = ts->touch_functions | SEC_TS_BIT_SETFUNC_COVER;
		input_info(true, &ts->client->dev,
				"%s: cover cmd write type:%d, ret:%d", __func__, ts->cover_cmd, ret);
	} else {
		ts->touch_functions = (ts->touch_functions & (~SEC_TS_BIT_SETFUNC_COVER));
		input_info(true, &ts->client->dev,
				"%s: cover open, not send cmd", __func__);
	}

	ts->touch_functions = ts->touch_functions|SEC_TS_BIT_SETFUNC_MUTUAL;
	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SET_TOUCHFUNCTION, &ts->touch_functions, 1);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: Failed to send glove_mode command", __func__);

	ret = sec_ts_i2c_write(ts, SEC_TS_CMD_SENSE_ON, NULL, 0);
	if (ret < 0)
		input_err(true, &ts->client->dev, "%s: fail to write Sense_on\n", __func__);

out:
	mutex_unlock(&ts->device_mutex);
	return 0;
}

#if (!defined(CONFIG_HAS_EARLYSUSPEND)) && (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
static int sec_ts_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_stop_device(ts);

	return 0;
}

static int sec_ts_resume(struct i2c_client *client)
{
	struct sec_ts_data *ts = i2c_get_clientdata(client);

	sec_ts_start_device(ts);

	return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void sec_ts_early_suspend(struct early_suspend *h)
{
	struct sec_ts_data *ts;
	ts = container_of(h, struct sec_ts_data, early_suspend);

	sec_ts_suspend(ts->client, PMSG_SUSPEND);
}

static void sec_ts_late_resume(struct early_suspend *h)
{
	struct sec_ts_data *ts;
	ts = container_of(h, struct sec_ts_data, early_suspend);

	sec_ts_resume(ts->client);
}
#endif

#ifdef CONFIG_PM
static int sec_ts_pm_suspend(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->input_dev->mutex);

	if (ts->input_dev->users)
		sec_ts_stop_device(ts);

	mutex_unlock(&ts->input_dev->mutex);

	return 0;
}

static int sec_ts_pm_resume(struct device *dev)
{
	struct sec_ts_data *ts = dev_get_drvdata(dev);

	input_info(true, &ts->client->dev, "%s\n", __func__);

	mutex_lock(&ts->input_dev->mutex);

	if (ts->input_dev->users)
		sec_ts_start_device(ts);

	mutex_unlock(&ts->input_dev->mutex);

	return 0;
}
#endif

#ifdef CONFIG_TRUSTONIC_TRUSTED_UI
void trustedui_mode_on(void){
	if (!tui_tsp_info)
		return;

	sec_ts_release_all_finger(tui_tsp_info);
#ifdef CONFIG_EPEN_WACOM_W9018
	epen_disable_mode(1);

	/* Release prohibited touch by wacom / grace concept */
	set_spen_mode(0);
#endif
}

void trustedui_mode_off(void){
	if (!tui_tsp_info)
		return;

#ifdef CONFIG_EPEN_WACOM_W9018
	epen_disable_mode(0);
#endif
}
#endif

static const struct i2c_device_id sec_ts_id[] = {
	{ SEC_TS_I2C_NAME, 0 },
	{ },
};

#ifdef CONFIG_PM
static const struct dev_pm_ops sec_ts_dev_pm_ops = {
	.suspend = sec_ts_pm_suspend,
	.resume = sec_ts_pm_resume,
};
#endif

#ifdef CONFIG_OF
static struct of_device_id sec_ts_match_table[] = {
	{ .compatible = "sec,sec_ts",},
	{ },
};
#else
#define sec_ts_match_table NULL
#endif

static struct i2c_driver sec_ts_driver = {
	.probe		= sec_ts_probe,
	.remove		= sec_ts_remove,
	.shutdown	= sec_ts_shutdown,
#if (!defined(CONFIG_HAS_EARLYSUSPEND)) && (!defined(CONFIG_PM)) && !defined(USE_OPEN_CLOSE)
	.suspend	= sec_ts_suspend,
	.resume		= sec_ts_resume,
#endif
	.id_table	= sec_ts_id,
	.driver = {
		.owner	= THIS_MODULE,
		.name	= SEC_TS_I2C_NAME,
#ifdef CONFIG_OF
		.of_match_table = sec_ts_match_table,
#endif
#ifdef CONFIG_PM
		.pm = &sec_ts_dev_pm_ops,
#endif
	},
};

static int __init sec_ts_init(void)
{
	return i2c_add_driver(&sec_ts_driver);
}

static void __exit sec_ts_exit(void)
{
	i2c_del_driver(&sec_ts_driver);
}

MODULE_AUTHOR("Hyobae, Ahn<hyobae.ahn@samsung.com>");
MODULE_DESCRIPTION("Samsung Electronics TouchScreen driver");
MODULE_LICENSE("GPL");

module_init(sec_ts_init);
module_exit(sec_ts_exit);
