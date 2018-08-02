/*
 * Copyright (C) 2013 Samsung Electronics. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
//#include <linux/sensor/sensors_core.h>
#include <linux/power_supply.h>
#include "sx9310_reg.h"
#if defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#define VENDOR_NAME              "SEMTECH"
#define MODEL_NAME               "SX9310"
#define MODULE_NAME              "grip_sensor"

#define I2C_M_WR                 0 /* for i2c Write */
#define I2c_M_RD                 1 /* for i2c Read */

#define IDLE                     0
#define ACTIVE                   1

#define SX9310_MODE_SLEEP        0
#define SX9310_MODE_NORMAL       1

#define MAIN_SENSOR              1
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
#define CH2_SENSOR               2
#endif
#define DIFF_READ_NUM            10
#define GRIP_LOG_TIME            30 /* sec */
#define CSX_STATUS_REG           SX9310_TCHCMPSTAT_TCHSTAT0_FLAG
#define BODY_STATUS_REG		SX9310_BODYCMPSTAT_FLAG
#define RAW_DATA_BLOCK_SIZE      (SX9310_REGOFFSETLSB - SX9310_REGUSEMSB + 1)

/* CS0, CS1, CS2, CS3 */
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
#define ENABLE_CSX               ((1 << MAIN_SENSOR)| (1 << CH2_SENSOR))
#else
#define ENABLE_CSX               (1 << MAIN_SENSOR)
#endif
#define IRQ_PROCESS_CONDITION   (SX9310_IRQSTAT_TOUCH_FLAG \
				| SX9310_IRQSTAT_RELEASE_FLAG)

#define NONE_ENABLE		-1
#define IDLE_STATE		0
#define TOUCH_STATE		1
#define BODY_STATE		2

#define HALLIC1_PATH		"/sys/class/sec/sec_key/certify_hall_detect"
#define HALLIC2_PATH		"/sys/class/sec/sec_key/hall_detect"

struct sx9310_p {
	struct i2c_client *client;
	struct input_dev *input;
	struct device *factory_device;
	struct delayed_work init_work;
	struct delayed_work irq_work;
	struct delayed_work debug_work;
	struct wake_lock grip_wake_lock;
	struct mutex read_mutex;
#if defined(CONFIG_MUIC_NOTIFIER)
	struct notifier_block cpuidle_muic_nb;
#endif
	bool skip_data;
	bool check_usb;
	u8 normal_th;
	u8 normal_th_buf;
	int irq;
	int gpio_nirq;
	int state;
	int debug_count;
	int diff_avg;
	int diff_cnt;
	int init_done;
	s32 capmain;
	s16 useful;
	s16 avg;
	s16 diff;
	u16 offset;
	u16 freq;

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	u8 normal_th_ch2;
	u8 normal_th_buf_ch2;
	int diff_avg_ch2;
	int diff_cnt_ch2;
	s32 capmain_ch2;
	s16 useful_ch2;
	s16 avg_ch2;
	s16 diff_ch2;
	u16 offset_ch2;
	u16 freq_ch2;
#endif

	int ch1_state;
	int ch2_state;

	atomic_t enable;

	unsigned char hall_ic1[6];
	unsigned char hall_ic2[6];
};

static int check_hallic_state(char *file_path, unsigned char hall_ic_status[])
{
	int iRet = 0;
	mm_segment_t old_fs;
	struct file *filep;
	u8 hall_sysfs[5];

	old_fs = get_fs();
	set_fs(KERNEL_DS);

	filep = filp_open(file_path, O_RDONLY, 0666);
	if (IS_ERR(filep)) {
		iRet = PTR_ERR(filep);
		set_fs(old_fs);
		goto exit;
	}

	iRet = filep->f_op->read(filep, hall_sysfs,
		sizeof(hall_sysfs), &filep->f_pos);

	if (iRet != sizeof(hall_sysfs)) {
		iRet = -EIO;
	} else {
		strncpy(hall_ic_status, hall_sysfs, sizeof(hall_sysfs));
	}

	filp_close(filep, current->files);
	set_fs(old_fs);

	exit:
	return iRet;
}

static int sx9310_get_nirq_state(struct sx9310_p *data)
{
	return gpio_get_value_cansleep(data->gpio_nirq);
}

static int sx9310_i2c_write(struct sx9310_p *data, u8 reg_addr, u8 buf)
{
	int ret;
	struct i2c_msg msg;
	unsigned char w_buf[2];

	w_buf[0] = reg_addr;
	w_buf[1] = buf;

	msg.addr = data->client->addr;
	msg.flags = I2C_M_WR;
	msg.len = 2;
	msg.buf = (char *)w_buf;

	ret = i2c_transfer(data->client->adapter, &msg, 1);
	if (ret < 0)
		pr_err("[SX9310]: %s - i2c write error %d\n",
			__func__, ret);

	return ret;
}

static int sx9310_i2c_read(struct sx9310_p *data, u8 reg_addr, u8 *buf)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = 1;
	msg[1].buf = buf;

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0)
		pr_err("[SX9310]: %s - i2c read error %d\n",
			__func__, ret);

	return ret;
}

static int sx9310_i2c_read_block(struct sx9310_p *data, u8 reg_addr,
	u8 *buf, u8 buf_size)
{
	int ret;
	struct i2c_msg msg[2];

	msg[0].addr = data->client->addr;
	msg[0].flags = I2C_M_WR;
	msg[0].len = 1;
	msg[0].buf = &reg_addr;

	msg[1].addr = data->client->addr;
	msg[1].flags = I2C_M_RD;
	msg[1].len = buf_size;
	msg[1].buf = buf;

	ret = i2c_transfer(data->client->adapter, msg, 2);
	if (ret < 0)
		pr_err("[SX9310]: %s - i2c read error %d\n",
			__func__, ret);

	return ret;
}

static u8 sx9310_read_irqstate(struct sx9310_p *data)
{
	u8 val = 0;

	if (sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val) >= 0)
		return val;

	return 0;
}

static void sx9310_initialize_register(struct sx9310_p *data)
{
	u8 val = 0;
	int idx;

	for (idx = 0; idx < (sizeof(setup_reg) >> 1); idx++) {
		sx9310_i2c_write(data, setup_reg[idx].reg, setup_reg[idx].val);
		pr_info("[SX9310]: %s - Write Reg: 0x%x Value: 0x%x\n",
			__func__, setup_reg[idx].reg, setup_reg[idx].val);

		sx9310_i2c_read(data, setup_reg[idx].reg, &val);
		pr_info("[SX9310]: %s - Read Reg: 0x%x Value: 0x%x\n\n",
			__func__, setup_reg[idx].reg, val);
	}
	data->init_done = ON;
}

static void sx9310_initialize_chip(struct sx9310_p *data)
{
	int cnt = 0;

	while ((sx9310_get_nirq_state(data) == 0) && (cnt++ < 10)) {
		sx9310_read_irqstate(data);
		msleep(20);
	}

	if (cnt >= 10)
		pr_err("[SX9310]: %s - s/w reset fail(%d)\n", __func__, cnt);

	sx9310_initialize_register(data);
}

static int sx9310_set_offset_calibration(struct sx9310_p *data)
{
	int ret = 0;

	ret = sx9310_i2c_write(data, SX9310_IRQSTAT_REG, 0xFF);

	return ret;
}

static void send_event(struct sx9310_p *data, u8 state)
{
	data->normal_th = data->normal_th_buf;

	if (state == ACTIVE) {
		data->state = ACTIVE;
#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th_ch2);
#endif
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		pr_info("[SX9310]: %s - button touched\n", __func__);
	} else {
		data->state = IDLE;
#if (MAIN_SENSOR == 1)
		sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th_ch2);
#endif
#else
		sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
		pr_info("[SX9310]: %s - button released\n", __func__);
	}

	if (data->skip_data == true)
		return;

	if (state == ACTIVE)
		input_report_rel(data->input, REL_MISC, 1);
	else
		input_report_rel(data->input, REL_MISC, 2);

	input_sync(data->input);
}

static void sx9310_display_data_reg(struct sx9310_p *data)
{
	u8 val, reg;

	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	for (reg = SX9310_REGUSEMSB; reg <= SX9310_REGOFFSETLSB; reg++) {
		sx9310_i2c_read(data, reg, &val);
		pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
			__func__, reg, val);
	}

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, CH2_SENSOR);
	for (reg = SX9310_REGUSEMSB; reg <= SX9310_REGOFFSETLSB; reg++) {
		sx9310_i2c_read(data, reg, &val);
		pr_info("[SX9310]: %s 2ch - Register(0x%2x) data(0x%2x)\n",
			__func__, reg, val);
	}
#endif
}

static void sx9310_get_data(struct sx9310_p *data)
{
	u8 ms_byte = 0;
	u8 is_byte = 0;
	u8 ls_byte = 0;
	u8 buf[RAW_DATA_BLOCK_SIZE];
#if (MAIN_SENSOR == 0)
	s32 gain = 1 << ((setup_reg[5].val >> 2) & 0x03);
#else
	s32 gain = 1 << (setup_reg[5].val & 0x03);
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	s32 gain2= 1 << ((setup_reg[5].val >> 2) & 0x03);
#endif
#endif

	mutex_lock(&data->read_mutex);
	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, MAIN_SENSOR);
	sx9310_i2c_read_block(data, SX9310_REGUSEMSB,
		&buf[0], RAW_DATA_BLOCK_SIZE);

	data->useful = (s16)((s32)buf[0] << 8) | ((s32)buf[1]);
	data->avg = (s16)((s32)buf[2] << 8) | ((s32)buf[3]);
	data->offset = ((u16)buf[6] << 8) | ((u16)buf[7]);
	data->diff = (data->useful - data->avg) >> 4;

	ms_byte = (u8)(data->offset >> 13);
	is_byte = (u8)((data->offset & 0x1fc0) >> 6);
	ls_byte = (u8)(data->offset & 0x3f);

	data->capmain = (((s32)ms_byte * 234000) + ((s32)is_byte * 9000) +
		((s32)ls_byte * 450)) + (((s32)data->useful * 50000) /
		(gain * 65536));

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	sx9310_i2c_write(data, SX9310_REGSENSORSELECT, CH2_SENSOR);
	sx9310_i2c_read_block(data, SX9310_REGUSEMSB,
		&buf[0], RAW_DATA_BLOCK_SIZE);

	data->useful_ch2 = (s16)((s32)buf[0] << 8) | ((s32)buf[1]);
	data->avg_ch2 = (s16)((s32)buf[2] << 8) | ((s32)buf[3]);
	data->offset_ch2 = ((u16)buf[6] << 8) | ((u16)buf[7]);
	data->diff_ch2 = (data->useful_ch2 - data->avg_ch2) >> 4;

	ms_byte = (u8)(data->offset_ch2 >> 13);
	is_byte = (u8)((data->offset_ch2 & 0x1fc0) >> 6);
	ls_byte = (u8)(data->offset_ch2 & 0x3f);

	data->capmain_ch2 = (((s32)ms_byte * 234000) + ((s32)is_byte * 9000) +
		((s32)ls_byte * 450)) + (((s32)data->useful_ch2 * 50000) /
		(gain2 * 65536));
#endif
	mutex_unlock(&data->read_mutex);

	pr_info("[SX9310]: %s - Capmain: %d, Useful: %d, avg: %d, diff: %d, Offset: %u\n",
		__func__, data->capmain, data->useful, data->avg,
		data->diff, data->offset);
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	pr_info("[SX9310]: %s - Capmain[2ch]: %d, Useful: %d, avg: %d, diff: %d, Offset: %u\n",
		__func__, data->capmain_ch2, data->useful_ch2, data->avg_ch2,
		data->diff_ch2, data->offset_ch2);
#endif
}

static int sx9310_set_mode(struct sx9310_p *data, unsigned char mode)
{
	int ret = -EINVAL;

	if (mode == SX9310_MODE_SLEEP) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[2].val);
	} else if (mode == SX9310_MODE_NORMAL) {
		ret = sx9310_i2c_write(data, SX9310_CPS_CTRL0_REG,
			setup_reg[2].val | ENABLE_CSX);
		msleep(20);

		sx9310_set_offset_calibration(data);
		msleep(400);
	}

	pr_info("[SX9310]: %s - change the mode : %u\n", __func__, mode);
	return ret;
}

static void sx9310_ch_interrupt_read(struct sx9310_p *data, u8 status)
{
	if (status & (CSX_STATUS_REG << MAIN_SENSOR)) {
		if (status & (BODY_STATUS_REG << (MAIN_SENSOR+1))) {
			data->ch1_state = BODY_STATE;
		} else {
			data->ch1_state = TOUCH_STATE;
		}
	} else {
		data->ch1_state = IDLE_STATE;
	}

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	if (status & (CSX_STATUS_REG << CH2_SENSOR))
		data->ch2_state = TOUCH_STATE;
	else
		data->ch2_state = IDLE_STATE;
#endif
	pr_info("[SX9310]: %s - ch1:%d, ch2:%d\n",
		__func__, data->ch1_state, data->ch2_state);
}

static void sx9310_set_enable(struct sx9310_p *data, int enable)
{
	u8 status = 0;

	pr_info("[SX9310]: %s\n", __func__);

	if (enable == ON) {
		sx9310_i2c_read(data, SX9310_STAT0_REG, &status);
		pr_info("[SX9310]: %s - enable(status : 0x%x)\n", __func__, status);

		sx9310_ch_interrupt_read(data, status);
		
		data->diff_avg = 0;
		data->diff_cnt = 0;
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
		data->diff_avg_ch2 = 0;
		data->diff_cnt_ch2 = 0;
#endif
		sx9310_get_data(data);

		if (data->skip_data == true) {
			input_report_rel(data->input, REL_MISC, 2);
			input_sync(data->input);
		} else if (status & (CSX_STATUS_REG << MAIN_SENSOR)) {
			send_event(data, ACTIVE);
		} else {
			send_event(data, IDLE);
		}

		msleep(20);
		/* make sure no interrupts are pending since enabling irq
		 * will only work on next falling edge */
		sx9310_read_irqstate(data);

		/* enable interrupt */
		sx9310_i2c_write(data, SX9310_IRQ_ENABLE_REG, setup_reg[0].val);

		enable_irq(data->irq);
		enable_irq_wake(data->irq);
	} else {
		pr_info("[SX9310]: %s - disable\n", __func__);

		/* disable interrupt */
 		sx9310_i2c_write(data, SX9310_IRQ_ENABLE_REG, 0x00);

		disable_irq(data->irq);
		disable_irq_wake(data->irq);
	}
}

static void sx9310_set_debug_work(struct sx9310_p *data, u8 enable,
		unsigned int time_ms)
{
	if (enable == ON) {
		data->debug_count = 0;
		schedule_delayed_work(&data->debug_work,
			msecs_to_jiffies(time_ms));
	} else {
		cancel_delayed_work_sync(&data->debug_work);
	}
}

static ssize_t sx9310_get_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_i2c_read(data, SX9310_IRQSTAT_REG, &val);

	return snprintf(buf, PAGE_SIZE, "%d\n", val);
}

static ssize_t sx9310_set_offset_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	if (val)
		sx9310_set_offset_calibration(data);

	return count;
}

static ssize_t sx9310_register_write_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int regist = 0, val = 0;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (sscanf(buf, "%d,%d", &regist, &val) != 2) {
		pr_err("[SX9310]: %s - The number of data are wrong\n",
			__func__);
		return -EINVAL;
	}

	sx9310_i2c_write(data, (unsigned char)regist, (unsigned char)val);
	pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, regist, val);

	return count;
}

static ssize_t sx9310_register_read_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned char val[30], i;
	struct sx9310_p *data = dev_get_drvdata(dev);

	for (i = 0; i < 19; i++) {
		sx9310_i2c_read(data, i + 0x10, &val[i]);
		pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, i + 0x10, val[i]);
	}

	for (i = 26; i < 29; i++) {
		sx9310_i2c_read(data, i + 0x10, &val[i]);
		pr_info("[SX9310]: %s - Register(0x%2x) data(0x%2x)\n",
		__func__, i + 0x10, val[i]);
	}

	return snprintf(buf, PAGE_SIZE,
		"0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
		val[0], val[1], val[2], val[3], val[4],
		val[5], val[6], val[7], val[8], val[9]);
}

static ssize_t sx9310_read_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_display_data_reg(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9310_sw_reset_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	pr_info("[SX9310]: %s\n", __func__);
	sx9310_set_offset_calibration(data);
	msleep(400);
	sx9310_get_data(data);

	return snprintf(buf, PAGE_SIZE, "%d\n", 0);
}

static ssize_t sx9310_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return count;
	}

	data->freq = (u16)val;
	val = ((val << 3) | (setup_reg[6].val & 0x07)) & 0xff;
	sx9310_i2c_write(data, SX9310_CPS_CTRL4_REG, (u8)val);

	pr_info("[SX9310]: %s - Freq : 0x%x\n", __func__, data->freq);

	return count;
}

static ssize_t sx9310_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	pr_info("[SX9310]: %s - Freq : 0x%x\n", __func__, data->freq);

	return snprintf(buf, PAGE_SIZE, "%u\n", data->freq);
}

static ssize_t sx9310_vendor_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t sx9310_name_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", MODEL_NAME);
}

static ssize_t sx9310_touch_mode_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "1\n");
}

static ssize_t sx9310_raw_data_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int sum;
	struct sx9310_p *data = dev_get_drvdata(dev);

	sx9310_get_data(data);
	if (data->diff_cnt == 0)
		sum = data->diff;
	else
		sum += data->diff;

	if (++data->diff_cnt >= DIFF_READ_NUM) {
		data->diff_avg = sum / DIFF_READ_NUM;
		data->diff_cnt = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u,%d,%d\n", data->capmain,
		data->useful, data->offset, data->diff, data->avg);
}

static ssize_t sx9310_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	/* It's for init touch */
	return snprintf(buf, PAGE_SIZE, "0\n");
}

static ssize_t sx9310_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sx9310_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[32] = {2, 4, 6, 8, 12, 16, 20, 24, 28, 32,
				40, 48, 56, 64, 72, 80, 88, 96, 112, 128,
				144, 160, 192, 224, 256, 320, 384, 512, 640,
				768, 1024, 1536};

	thresh_temp = (data->normal_th >> 3) & 0x1f;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[12].val >> 4) & 0x3;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
}

static ssize_t sx9310_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	data->normal_th &= 0x07;
	data->normal_th |= val;

	pr_info("[SX9310]: %s - normal threshold %lu\n", __func__, val);
	data->normal_th_buf = data->normal_th = (u8)(val);

	return count;
}

static ssize_t sx9310_onoff_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%u\n", !data->skip_data);
}

static ssize_t sx9310_onoff_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	u8 val;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &val);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (val == 0) {
		data->skip_data = true;
		if (atomic_read(&data->enable) == ON) {
			data->state = IDLE;
			input_report_rel(data->input, REL_MISC, 2);
			input_sync(data->input);
		}
	} else {
		data->skip_data = false;
	}

	pr_info("[SX9310]: %s -%u\n", __func__, val);
	return count;
}

static ssize_t sx9310_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "2,0,0\n");
}

static ssize_t sx9310_calibration_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	return count;
}

static ssize_t sx9310_gain_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
#if (MAIN_SENSOR == 0)
	u8 gain = (setup_reg[5].val >> 2) & 0x03;
#else
	u8 gain = setup_reg[5].val & 0x03;
#endif

	switch (gain) {
	case 0x00:
		ret = snprintf(buf, PAGE_SIZE, "x1\n");
		break;
	case 0x01:
		ret = snprintf(buf, PAGE_SIZE, "x2\n");
		break;
	case 0x02:
		ret = snprintf(buf, PAGE_SIZE, "x4\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "x8\n");
		break;
	}

	return ret;
}

static ssize_t sx9310_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	switch ((setup_reg[7].val >> 6) & 0x03) {
	case 0x00:
		ret = snprintf(buf, PAGE_SIZE, "Large\n");
		break;
	case 0x01:
		ret = snprintf(buf, PAGE_SIZE, "Medium\n");
		break;
	case 0x02:
		ret = snprintf(buf, PAGE_SIZE, "Medium Small\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "Small\n");
		break;
	}

	return ret;
}

static ssize_t sx9310_diff_avg_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->diff_avg);
}

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
static ssize_t sx9310_ch2_normal_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[32] = {2, 4, 6, 8, 12, 16, 20, 24, 28, 32,
				40, 48, 56, 64, 72, 80, 88, 96, 112, 128,
				144, 160, 192, 224, 256, 320, 384, 512, 640,
				768, 1024, 1536};

	thresh_temp = (data->normal_th_ch2 >> 3) & 0x1f;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[12].val >> 4) & 0x3;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
}

static ssize_t sx9310_ch2_normal_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	/* It's for normal touch */
	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	pr_info("[SX9310]: %s - normal threshold %lu\n", __func__, val);
	data->normal_th_buf_ch2 = data->normal_th_ch2 = (u8)(val);

	return count;
}

static ssize_t sx9310_raw_data_ch2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int sum;
	struct sx9310_p *data = dev_get_drvdata(dev);

//	sx9310_get_data(data);
	if (data->diff_cnt_ch2 == 0)
		sum = data->diff_ch2;
	else
		sum += data->diff_ch2;

	if (++data->diff_cnt_ch2 >= DIFF_READ_NUM) {
		data->diff_avg_ch2 = sum / DIFF_READ_NUM;
		data->diff_cnt_ch2 = 0;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d,%u,%d,%d\n", data->capmain_ch2,
		data->useful_ch2, data->offset_ch2, data->diff_ch2, data->avg_ch2);
}

static ssize_t sx9310_gain_ch2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;

	u8 gain = (setup_reg[5].val >> 2) & 0x03;

	switch (gain) {
	case 0x00:
		ret = snprintf(buf, PAGE_SIZE, "x1\n");
		break;
	case 0x01:
		ret = snprintf(buf, PAGE_SIZE, "x2\n");
		break;
	case 0x02:
		ret = snprintf(buf, PAGE_SIZE, "x4\n");
		break;
	default:
		ret = snprintf(buf, PAGE_SIZE, "x8\n");
		break;
	}

	return ret;
}

static ssize_t sx9310_diff_avg_ch2_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", data->diff_avg_ch2);
}
#endif

static ssize_t sx9310_ch_state_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (data->skip_data == true) {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			NONE_ENABLE, NONE_ENABLE);
	} else if (atomic_read(&data->enable) == ON) {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			data->ch1_state, data->ch2_state);
	} else {
		ret = snprintf(buf, PAGE_SIZE, "%d,%d\n",
			NONE_ENABLE, NONE_ENABLE);
	}

	return ret;
}

static ssize_t sx9310_body_threshold_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	u16 thresh_temp = 0, hysteresis = 0;
	u16 thresh_table[8] = {0, 300, 600, 900, 1200, 1500, 1800, 30000};

	thresh_temp = (data->normal_th) & 0x07;
	thresh_temp = thresh_table[thresh_temp];

	/* CTRL10 */
	hysteresis = (setup_reg[12].val >> 4) & 0x3;

	switch (hysteresis) {
	case 0x01: /* 6% */
		hysteresis = thresh_temp >> 4;
		break;
	case 0x02: /* 12% */
		hysteresis = thresh_temp >> 3;
		break;
	case 0x03: /* 25% */
		hysteresis = thresh_temp >> 2;
		break;
	default:
		/* None */
		break;
	}

	return snprintf(buf, PAGE_SIZE, "%d,%d\n", thresh_temp + hysteresis,
			thresh_temp - hysteresis);
}

static ssize_t sx9310_body_threshold_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned long val;
	struct sx9310_p *data = dev_get_drvdata(dev);

	if (kstrtoul(buf, 10, &val)) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	data->normal_th &= 0xf8;
	data->normal_th |= val;

	pr_info("[SX9310]: %s - body threshold %lu\n", __func__, val);
	data->normal_th_buf = data->normal_th = (u8)(val);

	return count;
}

static ssize_t sx9310_grip_flush_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	int ret = 0;
	u8 handle = 0;

	ret = kstrtou8(buf, 10, &handle);
	if (ret < 0) {
		pr_err("%s - kstrtou8 failed.(%d)\n", __func__, ret);
		return ret;
	}

	pr_info("[SX9310]: %s - handle = %d\n", __func__, handle);

	input_report_rel(data->input, REL_MAX, handle);
	input_sync(data->input);
	return size;
}

static DEVICE_ATTR(menual_calibrate, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_get_offset_calibration_show,
		sx9310_set_offset_calibration_store);
static DEVICE_ATTR(register_write, S_IWUSR | S_IWGRP,
		NULL, sx9310_register_write_store);
static DEVICE_ATTR(register_read, S_IRUGO,
		sx9310_register_read_show, NULL);
static DEVICE_ATTR(readback, S_IRUGO, sx9310_read_data_show, NULL);
static DEVICE_ATTR(reset, S_IRUGO, sx9310_sw_reset_show, NULL);
static DEVICE_ATTR(name, S_IRUGO, sx9310_name_show, NULL);
static DEVICE_ATTR(vendor, S_IRUGO, sx9310_vendor_show, NULL);
static DEVICE_ATTR(gain, S_IRUGO, sx9310_gain_show, NULL);
static DEVICE_ATTR(range, S_IRUGO, sx9310_range_show, NULL);
static DEVICE_ATTR(mode, S_IRUGO, sx9310_touch_mode_show, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, sx9310_raw_data_show, NULL);
static DEVICE_ATTR(diff_avg, S_IRUGO, sx9310_diff_avg_show, NULL);
static DEVICE_ATTR(calibration, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_calibration_show, sx9310_calibration_store);
static DEVICE_ATTR(onoff, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_onoff_show, sx9310_onoff_store);
static DEVICE_ATTR(threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_threshold_show, sx9310_threshold_store);
static DEVICE_ATTR(normal_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_normal_threshold_show, sx9310_normal_threshold_store);
static DEVICE_ATTR(freq, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_freq_show, sx9310_freq_store);
static DEVICE_ATTR(ch_state, S_IRUGO, sx9310_ch_state_show, NULL);
static DEVICE_ATTR(body_threshold, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_body_threshold_show, sx9310_body_threshold_store);

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
static DEVICE_ATTR(normal_threshold_ch2, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_ch2_normal_threshold_show, sx9310_ch2_normal_threshold_store);
static DEVICE_ATTR(gain_ch2, S_IRUGO, sx9310_gain_ch2_show, NULL);
static DEVICE_ATTR(raw_data_ch2, S_IRUGO, sx9310_raw_data_ch2_show, NULL);
static DEVICE_ATTR(diff_avg_ch2, S_IRUGO, sx9310_diff_avg_ch2_show, NULL);
#endif

static DEVICE_ATTR(grip_flush, S_IWUSR | S_IWGRP, NULL, sx9310_grip_flush_store);

static struct device_attribute *sensor_attrs[] = {
	&dev_attr_menual_calibrate,
	&dev_attr_register_write,
	&dev_attr_register_read,
	&dev_attr_readback,
	&dev_attr_reset,
	&dev_attr_name,
	&dev_attr_vendor,
	&dev_attr_gain,
	&dev_attr_range,
	&dev_attr_mode,
	&dev_attr_diff_avg,
	&dev_attr_raw_data,
	&dev_attr_threshold,
	&dev_attr_normal_threshold,
	&dev_attr_onoff,
	&dev_attr_calibration,
	&dev_attr_freq,
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	&dev_attr_normal_threshold_ch2,
	&dev_attr_gain_ch2,
	&dev_attr_raw_data_ch2,
	&dev_attr_diff_avg_ch2,
#endif
	&dev_attr_ch_state,
	&dev_attr_body_threshold,
	&dev_attr_grip_flush,
	NULL,
};

static ssize_t sx9310_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);
	int pre_enable = atomic_read(&data->enable);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	pr_info("[SX9310]: %s - new_value = %u old_value = %d\n",
		__func__, enable, pre_enable);

	if (pre_enable == enable)
		return size;

	atomic_set(&data->enable, enable);
	sx9310_set_enable(data, enable);

	return size;
}

static ssize_t sx9310_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	return snprintf(buf, PAGE_SIZE, "%d\n", atomic_read(&data->enable));
}

static ssize_t sx9310_flush_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	u8 enable;
	int ret;
	struct sx9310_p *data = dev_get_drvdata(dev);

	ret = kstrtou8(buf, 2, &enable);
	if (ret) {
		pr_err("[SX9310]: %s - Invalid Argument\n", __func__);
		return ret;
	}

	if (enable == 1) {
		input_report_rel(data->input, REL_MAX, 1);
		input_sync(data->input);
	}

	return size;
}

static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		sx9310_enable_show, sx9310_enable_store);
static DEVICE_ATTR(flush, S_IWUSR | S_IWGRP,
		NULL, sx9310_flush_store);

static struct attribute *sx9310_attributes[] = {
	&dev_attr_enable.attr,
	&dev_attr_flush.attr,
	NULL
};

static struct attribute_group sx9310_attribute_group = {
	.attrs = sx9310_attributes
};

static void sx9310_touch_process(struct sx9310_p *data, u8 flag)
{
	u8 status = 0;

	sx9310_i2c_read(data, SX9310_STAT0_REG, &status);
	pr_info("[SX9310]: %s - (status: 0x%x)\n", __func__, status);
	sx9310_get_data(data);
	sx9310_ch_interrupt_read(data, status);

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	if (data->state == IDLE) {
		if (status & (CSX_STATUS_REG << MAIN_SENSOR)) {
			if (status & (CSX_STATUS_REG << CH2_SENSOR)) {
				if (status & (BODY_STATUS_REG << (MAIN_SENSOR+1)))
					send_event(data, ACTIVE);
				else
					pr_info("[SX9310]: %s - main D, ch2 D, but body no\n",
					__func__);
			} else {
				send_event(data, ACTIVE);
			}
		} else {
			if (status & (CSX_STATUS_REG << CH2_SENSOR))
				pr_info("[SX9310]: %s - only CH2 D, no SAR\n",
					__func__);
		}
	} else {
		if (!(status & (CSX_STATUS_REG << MAIN_SENSOR)))
			send_event(data, IDLE);
		else
			pr_info("[SX9310]: %s - still touched\n",
				__func__);
	}
#else
	if (data->state == IDLE) {
		if (status & (CSX_STATUS_REG << MAIN_SENSOR))
			send_event(data, ACTIVE);
		else
			pr_info("[SX9310]: %s - already released\n",
				__func__);
	} else {
		if (!(status & (CSX_STATUS_REG << MAIN_SENSOR)))
			send_event(data, IDLE);
		else
			pr_info("[SX9310]: %s - still touched\n",
				__func__);
	}
#endif
}

static void sx9310_process_interrupt(struct sx9310_p *data)
{
	u8 flag = 0;

	/* since we are not in an interrupt don't need to disable irq. */
	flag = sx9310_read_irqstate(data);

	if (flag & IRQ_PROCESS_CONDITION)
		sx9310_touch_process(data, flag);
}

static void sx9310_init_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, init_work);

	sx9310_initialize_chip(data);

#if (MAIN_SENSOR == 1)
	sx9310_i2c_write(data, SX9310_CPS_CTRL9_REG, data->normal_th);
#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th_ch2);
#endif
#else
	sx9310_i2c_write(data, SX9310_CPS_CTRL8_REG, data->normal_th);
#endif
	sx9310_set_mode(data, SX9310_MODE_NORMAL);
	/* make sure no interrupts are pending since enabling irq
	 * will only work on next falling edge */
	sx9310_read_irqstate(data);
}

static void sx9310_irq_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, irq_work);

	if (sx9310_get_nirq_state(data) == 0)
		sx9310_process_interrupt(data);
	else
		pr_err("[SX9310]: %s - nirq read high %d\n",
			__func__, sx9310_get_nirq_state(data));
}

static void sx9310_debug_work_func(struct work_struct *work)
{
	struct sx9310_p *data = container_of((struct delayed_work *)work,
		struct sx9310_p, debug_work);

	static int hall_flag = 1;

	if (atomic_read(&data->enable) == ON) {
		if (data->debug_count >= GRIP_LOG_TIME) {
			sx9310_get_data(data);
			data->debug_count = 0;
		} else {
			data->debug_count++;
		}
	}

	check_hallic_state(HALLIC1_PATH, data->hall_ic1);
	check_hallic_state(HALLIC2_PATH, data->hall_ic2);

	/* Hall IC closed : offset cal (once)*/
	if (strcmp(data->hall_ic1, "CLOSE") == 0 &&
		strcmp(data->hall_ic2, "CLOSE") == 0) {
		if (hall_flag) {
			pr_info("[SX9310]: %s - hall IC1&2 is closed\n",
				__func__);
			sx9310_set_offset_calibration(data);
			hall_flag = 0;
		}
	} else
		hall_flag = 1;

	schedule_delayed_work(&data->debug_work, msecs_to_jiffies(1000));
}

static irqreturn_t sx9310_interrupt_thread(int irq, void *pdata)
{
	struct sx9310_p *data = pdata;

	if (sx9310_get_nirq_state(data) == 1) {
		pr_err("[SX9310]: %s - nirq read high\n", __func__);
	} else {
		wake_lock_timeout(&data->grip_wake_lock, 3 * HZ);
		schedule_delayed_work(&data->irq_work, msecs_to_jiffies(100));
	}

	return IRQ_HANDLED;
}

static int sx9310_input_init(struct sx9310_p *data)
{
	int ret = 0;
	struct input_dev *dev = NULL;

	/* Create the input device */
	dev = input_allocate_device();
	if (!dev)
		return -ENOMEM;

	dev->name = MODULE_NAME;
	dev->id.bustype = BUS_I2C;

	input_set_capability(dev, EV_REL, REL_MISC);
	input_set_capability(dev, EV_REL, REL_MAX);
	input_set_drvdata(dev, data);

	ret = input_register_device(dev);
	if (ret < 0) {
		input_free_device(dev);
		return ret;
	}

	ret = sensors_create_symlink(dev);
	if (ret < 0) {
		input_unregister_device(dev);
		return ret;
	}

	ret = sysfs_create_group(&dev->dev.kobj, &sx9310_attribute_group);
	if (ret < 0) {
		sensors_remove_symlink(dev);
		input_unregister_device(dev);
		return ret;
	}

	/* save the input pointer and finish initialization */
	data->input = dev;

	return 0;
}

static int sx9310_setup_pin(struct sx9310_p *data)
{
	int ret;

	ret = gpio_request(data->gpio_nirq, "SX9310_nIRQ");
	if (ret < 0) {
		pr_err("[SX9310]: %s - gpio %d request failed (%d)\n",
			__func__, data->gpio_nirq, ret);
		return ret;
	}

	ret = gpio_direction_input(data->gpio_nirq);
	if (ret < 0) {
		pr_err("[SX9310]: %s - failed to set gpio %d(%d)\n",
			__func__, data->gpio_nirq, ret);
		gpio_free(data->gpio_nirq);
		return ret;
	}

	return 0;
}

static void sx9310_initialize_variable(struct sx9310_p *data)
{
	data->state = IDLE;
	data->skip_data = false;
	data->check_usb = false;
	data->freq = setup_reg[6].val >> 3;
	data->debug_count = 0;
	atomic_set(&data->enable, OFF);
	data->normal_th = (u8)CONFIG_SENSORS_SX9310_NORMAL_TOUCH_THRESHOLD;
	data->normal_th_buf = data->normal_th;
	data->ch1_state = IDLE;
	data->init_done = OFF;

	pr_info("%s normal_th[%u]\n", __func__, data->normal_th);

#ifdef CONFIG_SENSORS_SX9310_USE_2ND_CH
	data->normal_th_ch2 = (u8)CONFIG_SENSORS_SX9310_NORMAL_TOUCH_THRESHOLD_CH2;
	data->normal_th_buf_ch2 = data->normal_th_ch2;
	data->ch2_state = IDLE;

	pr_info("%s normal_th_ch2[%u]\n", __func__, data->normal_th_ch2);
#endif
}

static int sx9310_parse_dt(struct sx9310_p *data, struct device *dev)
{
	struct device_node *node = dev->of_node;
	enum of_gpio_flags flags;

	if (node == NULL)
		return -ENODEV;

	data->gpio_nirq = of_get_named_gpio_flags(node,
		"sx9310-i2c,nirq-gpio", 0, &flags);
	if (data->gpio_nirq < 0) {
		pr_err("[SX9310]: %s - get gpio_nirq error\n", __func__);
		return -ENODEV;
	}

	return 0;
}

#if defined(CONFIG_MUIC_NOTIFIER)
static int sx9310_cpuidle_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	struct sx9310_p *grip_data;
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	grip_data = container_of(nb, struct sx9310_p, cpuidle_muic_nb);

	switch (attached_dev) {
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_PREPARE_MUIC:
	case ATTACHED_DEV_AFC_CHARGER_9V_MUIC:
		if (action == MUIC_NOTIFY_CMD_ATTACH) 
			pr_info("[SX9310]:%s - TA/USB is inserted\n", __func__);
		else
			pr_info("[SX9310]:%s - TA/USB is removed\n", __func__);

		if (grip_data->init_done == ON)
			sx9310_set_offset_calibration(grip_data);
		else
			pr_info("[SX9310]:%s - not initialized\n", __func__);
		break;
	default:
		break;
	}

	pr_info("[SX9310]:%s dev=%d, action=%lu\n",
		__func__, attached_dev, action);

	return NOTIFY_DONE;
}
#endif

static int sx9310_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	int ret = -ENODEV;
	struct sx9310_p *data = NULL;

	pr_info("[SX9310]: %s - Probe Start!\n", __func__);
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("[SX9310]: %s - i2c_check_functionality error\n",
			__func__);
		goto exit;
	}

	/* create memory for main struct */
	data = kzalloc(sizeof(struct sx9310_p), GFP_KERNEL);
	if (data == NULL) {
		pr_err("[SX9310]: %s - kzalloc error\n", __func__);
		ret = -ENOMEM;
		goto exit_kzalloc;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->factory_device = &client->dev;

	ret = sx9310_input_init(data);
	if (ret < 0)
		goto exit_input_init;

	wake_lock_init(&data->grip_wake_lock,
		WAKE_LOCK_SUSPEND, "grip_wake_lock");
	mutex_init(&data->read_mutex);

	ret = sx9310_parse_dt(data, &client->dev);
	if (ret < 0) {
		pr_err("[SX9310]: %s - of_node error\n", __func__);
		ret = -ENODEV;
		goto exit_of_node;
	}

	ret = sx9310_setup_pin(data);
	if (ret) {
		pr_err("[SX9310]: %s - could not setup pin\n", __func__);
		goto exit_setup_pin;
	}

	/* read chip id */
	ret = sx9310_i2c_write(data, SX9310_SOFTRESET_REG, SX9310_SOFTRESET);
	if (ret < 0) {
		pr_err("[SX9310]: %s - reset failed %d\n", __func__, ret);
		goto exit_chip_reset;
	}

	sx9310_initialize_variable(data);
	INIT_DELAYED_WORK(&data->init_work, sx9310_init_work_func);
	INIT_DELAYED_WORK(&data->irq_work, sx9310_irq_work_func);
	INIT_DELAYED_WORK(&data->debug_work, sx9310_debug_work_func);

	data->irq = gpio_to_irq(data->gpio_nirq);
	ret = request_threaded_irq(data->irq, NULL, sx9310_interrupt_thread,
			IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
			"sx9310_irq", data);
	if (ret < 0) {
		pr_err("[SX9310]: %s - failed to set request irq %d(%d)\n",
			__func__, data->irq, ret);
		goto exit_request_threaded_irq;
	}
	disable_irq(data->irq);

	ret = sensors_register(data->factory_device,
		data, sensor_attrs, MODULE_NAME);
	if (ret) {
		pr_err("[SX9310] %s - cound not register sensor(%d).\n",
			__func__, ret);
		goto grip_sensor_register_failed;
	}

	schedule_delayed_work(&data->init_work, msecs_to_jiffies(300));
	sx9310_set_debug_work(data, ON, 20000);

#if defined(CONFIG_MUIC_NOTIFIER)
	muic_notifier_register(&data->cpuidle_muic_nb,
		sx9310_cpuidle_muic_notifier, MUIC_NOTIFY_DEV_CPUIDLE);
#endif
	pr_info("[SX9310]: %s - Probe done!\n", __func__);

	return 0;

grip_sensor_register_failed:
	free_irq(data->irq, data);
exit_request_threaded_irq:
exit_chip_reset:
	gpio_free(data->gpio_nirq);
exit_setup_pin:
exit_of_node:
	mutex_destroy(&data->read_mutex);
	wake_lock_destroy(&data->grip_wake_lock);
	sysfs_remove_group(&data->input->dev.kobj, &sx9310_attribute_group);
	sensors_remove_symlink(data->input);
	input_unregister_device(data->input);
exit_input_init:
	kfree(data);
exit_kzalloc:
exit:
	pr_err("[SX9310]: %s - Probe fail!\n", __func__);
	return ret;
}

static int sx9310_remove(struct i2c_client *client)
{
	struct sx9310_p *data = (struct sx9310_p *)i2c_get_clientdata(client);

	if (atomic_read(&data->enable) == ON)
		sx9310_set_enable(data, OFF);

	sx9310_set_mode(data, SX9310_MODE_SLEEP);

	cancel_delayed_work_sync(&data->init_work);
	cancel_delayed_work_sync(&data->irq_work);
	cancel_delayed_work_sync(&data->debug_work);
	free_irq(data->irq, data);
	gpio_free(data->gpio_nirq);

	wake_lock_destroy(&data->grip_wake_lock);
	sensors_unregister(data->factory_device, sensor_attrs);
	sensors_remove_symlink(data->input);
	sysfs_remove_group(&data->input->dev.kobj, &sx9310_attribute_group);
	input_unregister_device(data->input);
	mutex_destroy(&data->read_mutex);

	kfree(data);

	return 0;
}

static int sx9310_suspend(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);
	int cnt = 0;

	pr_info("[SX9310]: %s\n", __func__);
	/* before go to sleep, make the interrupt pin as high*/
	while ((sx9310_get_nirq_state(data) == 0) && (cnt++ < 3)) {
		sx9310_read_irqstate(data);
		msleep(10);
	}
	if (cnt >= 3)
		pr_err("[SX9310]: %s - s/w reset fail(%d)\n", __func__, cnt);

	sx9310_set_debug_work(data, OFF, 1000);

	return 0;
}

static int sx9310_resume(struct device *dev)
{
	struct sx9310_p *data = dev_get_drvdata(dev);

	pr_info("[SX9310]: %s\n", __func__);
	sx9310_set_debug_work(data, ON, 1000);

	return 0;
}

static void sx9310_shutdown(struct i2c_client *client)
{
	struct sx9310_p *data = i2c_get_clientdata(client);

	pr_info("[SX9310]: %s\n", __func__);
	sx9310_set_debug_work(data, OFF, 1000);
	if (atomic_read(&data->enable) == ON)
		sx9310_set_enable(data, OFF);
	sx9310_set_mode(data, SX9310_MODE_SLEEP);
}

static struct of_device_id sx9310_match_table[] = {
	{ .compatible = "sx9310-i2c",},
	{},
};

static const struct i2c_device_id sx9310_id[] = {
	{ "sx9310_match_table", 0 },
	{ }
};

static const struct dev_pm_ops sx9310_pm_ops = {
	.suspend = sx9310_suspend,
	.resume = sx9310_resume,
};

static struct i2c_driver sx9310_driver = {
	.driver = {
		.name	= MODEL_NAME,
		.owner	= THIS_MODULE,
		.of_match_table = sx9310_match_table,
		.pm = &sx9310_pm_ops
	},
	.probe		= sx9310_probe,
	.remove		= sx9310_remove,
	.shutdown	= sx9310_shutdown,
	.id_table	= sx9310_id,
};

static int __init sx9310_init(void)
{
	return i2c_add_driver(&sx9310_driver);
}

static void __exit sx9310_exit(void)
{
	i2c_del_driver(&sx9310_driver);
}

module_init(sx9310_init);
module_exit(sx9310_exit);

MODULE_DESCRIPTION("Semtech Corp. SX9310 Capacitive Touch Controller Driver");
MODULE_AUTHOR("Samsung Electronics");
MODULE_LICENSE("GPL");
