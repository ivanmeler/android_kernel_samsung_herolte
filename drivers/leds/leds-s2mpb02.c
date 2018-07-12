/*
 * LED driver for Samsung S2MPB02
 *
 * Copyright (C) 2014 Samsung Electronics
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 *  This driver is based on leds-max77804.c
 */

#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/workqueue.h>
#include <linux/spinlock.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/mfd/s2mpb02.h>
#include <linux/mfd/s2mpb02-private.h>
#include <linux/leds-s2mpb02.h>
#include <linux/ctype.h>

extern struct class *camera_class; /*sys/class/camera*/
struct device *camera_flash_dev;
struct s2mpb02_led_data *global_led_datas[S2MPB02_LED_MAX];

struct s2mpb02_led_data {
	struct led_classdev led;
	struct s2mpb02_dev *s2mpb02;
	struct s2mpb02_led *data;
	struct i2c_client *i2c;
	struct work_struct work;
	struct mutex lock;
	spinlock_t value_lock;
	int brightness;
	int test_brightness;
};

static u8 leds_mask[S2MPB02_LED_MAX] = {
	S2MPB02_FLASH_MASK,
	S2MPB02_TORCH_MASK,
	S2MPB02_TORCH_MASK,
};

static u8 leds_shift[S2MPB02_LED_MAX] = {
	4,
	0,
	0,
};

u32 original_brightness[S2MPB02_LED_MAX];

static int s2mpb02_set_bits(struct i2c_client *client, const u8 reg,
			     const u8 mask, const u8 inval)
{
	int ret;
	u8 value;

	ret = s2mpb02_read_reg(client, reg, &value);
	if (unlikely(ret < 0))
		return ret;

	value = (value & ~mask) | (inval & mask);

	ret = s2mpb02_write_reg(client, reg, value);

	return ret;
}

static int s2mpb02_led_get_en_value(struct s2mpb02_led_data *led_data, int on)
{
	if (on) {
		if (led_data->data->id == S2MPB02_FLASH_LED_1) {
			return ((S2MPB02_FLED_ENABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_FLASH_MODE << S2MPB02_FLED_MODE_SHIFT));
				/* Turn on FLASH by I2C */
		} else if (led_data->data->id == S2MPB02_TORCH_LED_2) {
			return S2MPB02_FLED2_TORCH_ON;
				/* Turn on TORCH by I2C */
		} else {
			return ((S2MPB02_FLED_ENABLE << S2MPB02_FLED_ENABLE_SHIFT) |
				(S2MPB02_FLED_TORCH_MODE << S2MPB02_FLED_MODE_SHIFT));
				/* Turn on TORCH by I2C */
		}
	} else {
		return (S2MPB02_FLED_DISABLE << S2MPB02_FLED_ENABLE_SHIFT);
				/* controlled by GPIO */
	}
}

static void s2mpb02_led_set(struct led_classdev *led_cdev,
						enum led_brightness value)
{
#if 0 /* disable LED control by other sysfs */
	unsigned long flags;
	struct s2mpb02_led_data *led_data
		= container_of(led_cdev, struct s2mpb02_led_data, led);

	pr_debug("[LED] %s\n", __func__);

	spin_lock_irqsave(&led_data->value_lock, flags);
	led_data->data->brightness = min((int)value, S2MPB02_FLASH_TORCH_CURRENT_MAX);
	spin_unlock_irqrestore(&led_data->value_lock, flags);

	schedule_work(&led_data->work);
#endif
}

static void led_set(struct s2mpb02_led_data *led_data)
{
	int ret;
	struct s2mpb02_led *data = led_data->data;
	int id = data->id;
	int value;
	u8 reg;
	u8 mask;

	if (led_data->data->brightness == LED_OFF) {
		value = s2mpb02_led_get_en_value(led_data, 0);
		if (id == S2MPB02_TORCH_LED_1) {
			reg = S2MPB02_REG_FLED_CTRL1;
			mask = S2MPB02_FLED_ENABLE_MODE_MASK;
		} else {
			reg = S2MPB02_REG_FLED_CTRL2;
			mask = S2MPB02_FLED2_ENABLE_MODE_MASK;
		}
		ret = s2mpb02_set_bits(led_data->i2c, reg, mask, value);
		if (unlikely(ret))
			goto error_set_bits;

		if (id == S2MPB02_TORCH_LED_1) {
			reg = S2MPB02_REG_FLED_CUR1;
		} else {
			reg = S2MPB02_REG_FLED_CUR2;
		}
		/* set current */
		ret = s2mpb02_set_bits(led_data->i2c, reg, leds_mask[id],
					data->brightness << leds_shift[id]);
		if (unlikely(ret))
			goto error_set_bits;
	} else {
		if (id == S2MPB02_TORCH_LED_1) {
			reg = S2MPB02_REG_FLED_CUR1;
		} else {
			reg = S2MPB02_REG_FLED_CUR2;
		}
		/* set current */
		ret = s2mpb02_set_bits(led_data->i2c, reg, leds_mask[id],
					data->brightness << leds_shift[id]);
		if (unlikely(ret))
			goto error_set_bits;

		/* Turn on LED by I2C */
		value = s2mpb02_led_get_en_value(led_data, 1);
		if (id == S2MPB02_TORCH_LED_1) {
			reg = S2MPB02_REG_FLED_CTRL1;
			mask = S2MPB02_FLED_ENABLE_MODE_MASK;
		} else {
			reg = S2MPB02_REG_FLED_CTRL2;
			mask = S2MPB02_FLED2_ENABLE_MODE_MASK;
		}
		ret = s2mpb02_set_bits(led_data->i2c, reg, mask, value);
		if (unlikely(ret))
			goto error_set_bits;
	}

	return;

error_set_bits:
	pr_err("%s: can't set led level %d\n", __func__, ret);

	return;
}

static void s2mpb02_led_work(struct work_struct *work)
{
	struct s2mpb02_led_data *led_data
		= container_of(work, struct s2mpb02_led_data, work);

	pr_debug("[LED] %s\n", __func__);

	mutex_lock(&led_data->lock);
	led_set(led_data);
	mutex_unlock(&led_data->lock);
}

static int s2mpb02_led_setup(struct s2mpb02_led_data *led_data)
{
	int ret = 0;
	struct s2mpb02_led *data = led_data->data;
	int id, value;

	if (data == NULL) {
		pr_err("%s : data is null\n",__func__);
		return -1;
	}

	id = data->id;

	/* set Low Voltage operating mode disable */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL1,
				S2MPB02_FLED_CTRL1_LV_DISABLE, S2MPB02_FLED_CTRL1_LV_EN_MASK);

	/* set current & timeout */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CUR1,
				  data->brightness << leds_shift[id], leds_mask[id]);
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_TIME1,
				  data->timeout << leds_shift[id], leds_mask[id]);

	value = s2mpb02_led_get_en_value(led_data, 0);
	ret = s2mpb02_update_reg(led_data->i2c,
				S2MPB02_REG_FLED_CTRL1, value, S2MPB02_FLED_ENABLE_MODE_MASK);

#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
	/* set current & timeout */
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_CUR2,
				  data->brightness << leds_shift[id], leds_mask[id]);
	ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_TIME2,
				  0x00 , leds_mask[id]);

	value = s2mpb02_led_get_en_value(led_data, 0);
	ret = s2mpb02_update_reg(led_data->i2c,
				S2MPB02_REG_FLED_CTRL2, value, S2MPB02_FLED2_ENABLE_MODE_MASK);
#endif

	if (data->irda_off) {
		ret = s2mpb02_update_reg(led_data->i2c,
				S2MPB02_REG_FLED_CTRL2, 0x04, S2MPB02_TORCH_MASK);
	}

#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
	/* Remote-mode off, Power-LED Mode on, GPIO Polarity */
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL2, 0x38);

	/* set current - 1050mA */
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_CUR2, 0xAF);

	/* set 0x18, 0x19, 0x1A, 0x1B */
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_IRON1, 0x19);
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_IRON2, 0x0B);
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_IRD1, 0x00);
	ret |= s2mpb02_write_reg(led_data->i2c, S2MPB02_REG_FLED_IRD2, 0x2C);
#endif

	return ret;
}

void s2mpb02_led_get_status(struct led_classdev *led_cdev, bool status, bool onoff)
{
	int ret = 0;
	u8 value[6] = {0, };
	struct s2mpb02_led_data *led_data
		= container_of(led_cdev, struct s2mpb02_led_data, led);

	ret = s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL1, &value[0]); //Fled_ctrl1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL2, &value[1]); //Fled_ctrl2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CUR1, &value[2]); //Fled_cur1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_TIME1, &value[3]); //Fled_time1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CUR2, &value[4]); //Fled_cur2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_TIME2, &value[5]); //Fled_time2
	if (unlikely(ret < 0)) {
		printk("%s : error to get dt node\n", __func__);
	}

	printk("%s[%d, %d] : Fled_ctrl1 = 0x%12x, Fled_ctrl2 = 0x%13x, Fled_cur1 = 0x%14x, "
		"Fled_time1 = 0x%15x, Fled_cur2 = 0x%16x, Fled_time2 = 0x%17x\n",
		__func__, status, onoff, value[0], value[1], value[2], value[3], value[4], value[5]);
}

#ifdef CONFIG_TORCH_CURRENT_CHANGE_SUPPORT
int s2mpb02_set_torch_current(bool torch_mode)
{
	struct s2mpb02_led_data *led_data = global_led_datas[S2MPB02_TORCH_LED_1];
	struct s2mpb02_led *data = led_data->data;
	int ret = 0;

	pr_info("%s: torch_mode %d\n", __func__, torch_mode);
	mutex_lock(&led_data->lock);

	data->brightness = torch_mode ? S2MPB02_TORCH_OUT_I_60MA : original_brightness[data->id];

	/* set current */
	ret = s2mpb02_set_bits(led_data->i2c, S2MPB02_REG_FLED_CUR1,
			  leds_mask[data->id], data->brightness << leds_shift[data->id]);
	if (unlikely(ret)) {
		pr_err("%s: failed to set FLED_CUR1, %d\n", __func__, ret);
	}

	mutex_unlock(&led_data->lock);
	return ret;
}
#endif

ssize_t s2mpb02_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int value = 0;

	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if(global_led_datas[S2MPB02_TORCH_LED_1] == NULL) {
		pr_err("<%s> global_led_datas[S2MPB02_TORCH_LED_1] is NULL\n", __func__);
		return -1;
	}

	mutex_lock(&global_led_datas[S2MPB02_TORCH_LED_1]->lock);

	if (value == 0) {
		/* Turn off Torch */
		global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness = LED_OFF;
		led_set(global_led_datas[S2MPB02_TORCH_LED_1]);
	} else if (value == 1) {
		/* Turn on Torch */
		global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness
			= global_led_datas[S2MPB02_TORCH_LED_1]->data->torch_current_value;
		led_set(global_led_datas[S2MPB02_TORCH_LED_1]);
	} else if (value == 100) {
		/* Factory mode Turn on Torch */
		global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness
			= global_led_datas[S2MPB02_TORCH_LED_1]->data->factory_torch_current_value;
		led_set(global_led_datas[S2MPB02_TORCH_LED_1]);
	} else if (1001 <= value && value <= 1010) {
		/* Turn on Torch Step 20mA ~ 200mA */
		global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness = value - 1000;
		led_set(global_led_datas[S2MPB02_TORCH_LED_1]);
	} else {
		pr_info("[LED]%s , Invalid value:%d\n", __func__, value);
	}

	pr_info("[LED]%s , value:%d, current:%d\n",
			__func__, value, global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness);

	if (value <= 0) {
		s2mpb02_set_bits(global_led_datas[S2MPB02_TORCH_LED_1]->i2c, S2MPB02_REG_FLED_CUR1,
				leds_mask[global_led_datas[S2MPB02_TORCH_LED_1]->data->id],
				original_brightness[S2MPB02_TORCH_LED_1] << leds_shift[global_led_datas[S2MPB02_TORCH_LED_1]->data->id]);
		global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness = original_brightness[S2MPB02_TORCH_LED_1];
	}

	mutex_unlock(&global_led_datas[S2MPB02_TORCH_LED_1]->lock);
	return count;
}

#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
ssize_t s2mpb02_store_2nd(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int value = 0;

	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if(global_led_datas[S2MPB02_TORCH_LED_2] == NULL) {
		pr_err("<%s> global_led_datas[S2MPB02_TORCH_LED_2] is NULL\n", __func__);
		return -1;
	}

	mutex_lock(&global_led_datas[S2MPB02_TORCH_LED_2]->lock);

	if (value == 0) {
		/* Turn off Torch */
		global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness = LED_OFF;
		led_set(global_led_datas[S2MPB02_TORCH_LED_2]);
	} else if (value == 1) {
		/* Turn on Torch */
		global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness
			= global_led_datas[S2MPB02_TORCH_LED_2]->data->torch_current_value;
		led_set(global_led_datas[S2MPB02_TORCH_LED_2]);
	} else if (value == 100) {
		/* Factory mode Turn on Torch */
		global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness
			= global_led_datas[S2MPB02_TORCH_LED_2]->data->factory_torch_current_value;
		led_set(global_led_datas[S2MPB02_TORCH_LED_2]);
	} else if (1001 <= value && value <= 1010) {
		/* Turn on Torch Step 20mA ~ 200mA */
		global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness = value - 1000;
		led_set(global_led_datas[S2MPB02_TORCH_LED_2]);
	} else {
		pr_info("[LED]%s , Invalid value:%d\n", __func__, value);
	}

	pr_info("[LED2]%s , value:%d, current:%d\n",
			__func__, value, global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness);

	if (value <= 0) {
		s2mpb02_set_bits(global_led_datas[S2MPB02_TORCH_LED_2]->i2c, S2MPB02_REG_FLED_CUR2,
				leds_mask[global_led_datas[S2MPB02_TORCH_LED_2]->data->id],
				original_brightness[S2MPB02_TORCH_LED_2] << leds_shift[global_led_datas[S2MPB02_TORCH_LED_2]->data->id]);
		global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness = original_brightness[S2MPB02_TORCH_LED_2];
	}

	mutex_unlock(&global_led_datas[S2MPB02_TORCH_LED_2]->lock);
	return count;
}
#endif

ssize_t s2mpb02_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	if(global_led_datas[S2MPB02_TORCH_LED_1] == NULL) {
		pr_err("<%s> global_led_datas[S2MPB02_TORCH_LED_1] is NULL\n", __func__);
		return -1;
	}

	pr_info("[LED] %s , MAX STEP TORCH_LED:%d\n", __func__, S2MPB02_TORCH_OUT_I_MAX - 1);
	return sprintf(buf, "%d\n", S2MPB02_TORCH_OUT_I_MAX - 1);
}

#ifdef CONFIG_LEDS_IRIS_IRLED_CERTIFICATE_SUPPORT
ssize_t s2mpb02_irled_torch_store(struct device *dev,
			struct device_attribute *attr, const char *buf,
			size_t count)
{
	int value = 0;
	u8 ledvalue = 0;
	int ret = 0;

	if ((buf == NULL) || kstrtouint(buf, 10, &value)) {
		return -1;
	}

	if(global_led_datas[0] == NULL) {
		pr_err("<%s> global_led_datas[S2MPB02_TORCH_LED_1] is NULL\n", __func__);
		return -1;
	}

	pr_info("%s , value:%d\n", __func__, value);
	if (value == 1) {
		pr_info("%s IRLED TORCH ON\n", __func__);
		/* FLED_TIME2.IR_MAX_TIMER_EN Disable */
		ret |= s2mpb02_update_reg(global_led_datas[0]->i2c,
				S2MPB02_REG_FLED_TIME2,
				S2MPB02_FLED_TIME2_IRMAX_TIMER_DISABLE,
				S2MPB02_FLED_TIME2_IRMAX_TIMER_EN_MASK);

		/* Remote-mode off, TORCH MODE */
		ret |= s2mpb02_update_reg(global_led_datas[0]->i2c,
				S2MPB02_REG_FLED_CTRL2,
				S2MPB02_FLED_CTRL2_TORCH_ON,
				S2MPB02_FLED_CTRL2_TORCH_MASK);
		ret |= s2mpb02_update_reg(global_led_datas[0]->i2c,
				S2MPB02_REG_FLED_CUR2, 0x0E, S2MPB02_FLED_CUR2_TORCH_CUR2_MASK);
	} else if (value == 0) {
		pr_info("%s IRLED TORCH OFF\n", __func__);
		/* Remote-mode off, Power-LED Mode on, GPIO Polarity */
		ret |= s2mpb02_update_reg(global_led_datas[0]->i2c,
				S2MPB02_REG_FLED_CTRL2,
				S2MPB02_FLED_CTRL2_FLASH_ON,
				S2MPB02_FLED_CTRL2_TORCH_MASK);
	} else {
		pr_err("%s value(%d) is invalid!!\n", __func__, value);
		return -1;
	}
	ret |= s2mpb02_read_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CTRL2, &ledvalue);
	pr_info("[LED]%s , ledvalue:%x\n", __func__, ledvalue);

	return count;
}

ssize_t s2mpb02_irled_torch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct s2mpb02_led_data *led_data;
	int ret = 0;
	u8 value[10] = {0, };

	if(global_led_datas[0] == NULL) {
		pr_err("<%s> global_led_datas[S2MPB02_TORCH_LED_1] is NULL\n", __func__);
		return -1;
	}
        led_data = global_led_datas[0];

	ret = s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL1, &value[0]); //Fled_ctrl1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CTRL2, &value[1]); //Fled_ctrl2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CUR1, &value[2]); //Fled_cur1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_TIME1, &value[3]); //Fled_time1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_CUR2, &value[4]); //Fled_cur2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_TIME2, &value[5]); //Fled_time2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_IRON1, &value[6]); //Fled_iron1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_IRON2, &value[7]); //Fled_iron2
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_IRD1, &value[8]); //Fled_ird1
	ret |= s2mpb02_read_reg(led_data->i2c, S2MPB02_REG_FLED_IRD2, &value[9]); //Fled_ird2
	if (unlikely(ret < 0)) {
		printk("%s : error to get dt node\n", __func__);
	}

	pr_info("%s : Fled_ctrl1 = 0x%12x, Fled_ctrl2 = 0x%13x, Fled_cur1 = 0x%14x, "
		"Fled_time1 = 0x%15x, Fled_cur2 = 0x%16x, Fled_time2 = 0x%17x, "
		"Fled_iron1 = 0x%x, Fled_iron2 = 0x%x, Fled_ird1 = 0x%x, Fled_ird2 = 0x%x\n",
		__func__, value[0], value[1], value[2], value[3], value[4], value[5],
		value[6], value[7], value[8], value[9]);

	return sprintf(buf, "0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x,0x%x\n",
			value[0], value[1], value[2], value[3], value[4], value[5],
			value[6], value[7], value[8], value[9]);
}

static DEVICE_ATTR(irled_torch, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_irled_torch_show, s2mpb02_irled_torch_store);
#endif

static DEVICE_ATTR(rear_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_show, s2mpb02_store);

static DEVICE_ATTR(rear_torch_flash, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_show, s2mpb02_store);

#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
static DEVICE_ATTR(rear_flash2, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_show, s2mpb02_store_2nd);
static DEVICE_ATTR(rear_torch_flash2, S_IRUSR|S_IWUSR|S_IRGRP|S_IWGRP|S_IROTH,
	s2mpb02_show, s2mpb02_store_2nd);
#endif

#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
int s2mpb02_ir_led_current(uint32_t current_value)
{
	int ret = 0;
	unsigned int value = current_value - 1;
	unsigned char data = 0;

	pr_info("[%s] led current value : %u \n", __func__, value);

	data = ((value & 0x0F) << 4) | 0x0F;

	ret = s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CUR2, data);
	if (ret < 0)
		pr_err("[%s] i2c write error", __func__);

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_current);

int s2mpb02_ir_led_pulse_width(uint32_t width)
{
	unsigned int value = width;
	unsigned char iron1 = 0;
	unsigned char iron2 = 0;
	int ret = 0;

	pr_info("[%s] led pulse_width value : %u\n", __func__, value);

	iron1 = (value >> 2) & 0xFF;
	iron2 = (value & 0x03) << 6;
    
	pr_info("[%s] IRON1(0x%02x), IRON2(0x%02x)\n", __func__, iron1, iron2);
    
	/* set 0x18, 0x19 */
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON1, iron1);
	ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON2,
		S2MPB02_FLED2_IRON2_MASK, iron2);
	if (ret < 0)
		pr_err("[%s] i2c write error", __func__);

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_pulse_width);

int s2mpb02_ir_led_pulse_delay(uint32_t delay)
{
	unsigned int value = delay;
	unsigned char ird1 = 0;
	unsigned char ird2 = 0;
	int ret = 0;

	pr_info("[%s] led pulse_delay value : %u\n", __func__, value);

	ird1 = (value >> 2) & 0xFF;
	ird2 = ((value & 0x03) << 6) | 0x2C; /* value 0x2C means RSVD[5:0] Reserved */
    
	pr_info("[%s] IRD1(0x%02x), IRD2(0x%02x)\n", __func__, ird1, ird2);
    
	/* set 0x1A, 0x1B */
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD1, ird1);
	ret |= s2mpb02_write_reg(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRD2, ird2);
	if (ret < 0)
		pr_err("[%s] i2c write error", __func__);

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_pulse_delay);

int s2mpb02_ir_led_max_time(uint32_t max_time)
{
	int ret = 0;

	pr_info("[%s] led max_time value : %u\n", __func__, max_time);

	/* set IRLED max timer interrupt clear and enabled */
	ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_CTRL2,
		S2MPB02_FLED2_MAX_TIME_CLEAR_MASK, 0x00);
	ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2,
		S2MPB02_FLED2_MAX_TIME_EN_MASK, 0x00);
	if (max_time > 0) {
		ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_TIME2,
			S2MPB02_FLED2_MAX_TIME_EN_MASK, 0x01);

		ret |= s2mpb02_set_bits(global_led_datas[0]->i2c, S2MPB02_REG_FLED_IRON2, 
			S2MPB02_FLED2_MAX_TIME_MASK, (u8) max_time - 1);
	}

	return ret;
}
EXPORT_SYMBOL(s2mpb02_ir_led_max_time);
#endif

#if defined(CONFIG_OF)
static int of_s2mpb02_torch_dt(struct s2mpb02_dev *iodev,
					struct s2mpb02_led_platform_data *pdata)
{
	struct device_node *led_np, *np, *c_np;
	int ret;
	u32 temp;
	u32 irda_off = 0;
	const char *temp_str;
	int index;

	led_np = iodev->dev->of_node;
	if (!led_np) {
		pr_info("<%s> could not find led sub-node\n", __func__);
		return -ENODEV;
	}

	np = of_find_node_by_name(led_np, "torch");
	if (!np) {
		pr_info("<%s> could not find led sub-node\n",
								__func__);
		return -EINVAL;
	}

	pdata->num_leds = of_get_child_count(np);

	for_each_child_of_node(np, c_np) {
		ret = of_property_read_u32(c_np, "id", &temp);
		if (ret) {
			pr_info("%s failed to get a id\n", __func__);
		}
		index = temp;
		pdata->leds[index].id = temp;

		ret = of_property_read_string(c_np, "ledname", &temp_str);
		if (ret) {
			pr_info("%s failed to get a ledname\n", __func__);
		}
		pdata->leds[index].name = temp_str;
		ret = of_property_read_u32(c_np, "brightness", &temp);
		if (ret) {
			pr_info("%s failed to get a brightness\n", __func__);
		}
		if(temp > S2MPB02_FLASH_TORCH_CURRENT_MAX) {
			pr_info("%s out of range : brightness\n", __func__);
		}
		pdata->leds[index].brightness = temp;
		original_brightness[index] = temp;
		ret = of_property_read_u32(c_np, "timeout", &temp);
		if (ret) {
			pr_info("%s failed to get a timeout\n", __func__);
		}
		if(temp > S2MPB02_TIMEOUT_MAX) {
			pr_info("%s out of range : timeout\n", __func__);
		}
		pdata->leds[index].timeout = temp;

		pdata->leds[index].use_torch_current_value = of_property_read_bool(c_np, "use_torch_current_value");
		if (pdata->leds[index].use_torch_current_value) {
			ret = of_property_read_u32(c_np, "torch_current_value", &temp);
			if (ret) {
				pr_info("%s failed to get a torch_current_value\n", __func__);
			}
			pdata->leds[index].torch_current_value = temp;

			ret = of_property_read_u32(c_np, "factory_torch_current_value", &temp);
			if (ret) {
				pr_info("%s failed to get a factory_torch_current_value\n", __func__);
			}
			pdata->leds[index].factory_torch_current_value = temp;
		} else {
			pr_info("%s: torch current value dt parsing skipped index(%d)\n", __func__, index);
			if (index != S2MPB02_FLASH_LED_1) {
				pdata->leds[index].torch_current_value = S2MPB02_TORCH_OUT_I_60MA;
				pdata->leds[index].factory_torch_current_value = S2MPB02_TORCH_OUT_I_240MA;
			}
		}

		if(index == 1) {
			ret = of_property_read_u32(c_np, "irda_off", &irda_off);
			if (ret) {
				pr_info("%s failed to get a irda_off\n", __func__);
			}
			pdata->leds[index].irda_off = irda_off;
		}
	}
	of_node_put(led_np);

	return 0;
}
#endif /* CONFIG_OF */

static int s2mpb02_led_probe(struct platform_device *pdev)
{
	int ret = 0;
	int i;
	struct s2mpb02_dev *s2mpb02 = dev_get_drvdata(pdev->dev.parent);
#ifndef CONFIG_OF
	struct s2mpb02_platform_data *s2mpb02_pdata
		= dev_get_platdata(s2mpb02->dev);
#endif
	struct s2mpb02_led_platform_data *pdata;
	struct s2mpb02_led_data *led_data;
	struct s2mpb02_led *data;
	struct s2mpb02_led_data **led_datas;

#ifdef CONFIG_OF
	pdata = kzalloc(sizeof(struct s2mpb02_led_platform_data), GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return -ENOMEM;
	}
	ret = of_s2mpb02_torch_dt(s2mpb02, pdata);
	if (ret < 0) {
		pr_err("s2mpb02-torch : %s not found torch dt! ret[%d]\n",
				 __func__, ret);
		kfree(pdata);
		return -1;
	}
#else
	pdata = s2mpb02_pdata->led_data;
	if (pdata == NULL) {
		pr_err("[LED] no platform data for this led is found\n");
		return -EFAULT;
	}
#endif
	led_datas = kzalloc(sizeof(struct s2mpb02_led_data *)
			    * S2MPB02_LED_MAX, GFP_KERNEL);
	if (unlikely(!led_datas)) {
		pr_err("[LED] memory allocation error %s", __func__);
		kfree(pdata);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, led_datas);

	pr_info("[LED] %s %d leds\n", __func__, pdata->num_leds);

	for (i = 0; i != pdata->num_leds; ++i) {
		pr_info("%s led%d setup ...\n", __func__, i);

		data = kzalloc(sizeof(struct s2mpb02_led), GFP_KERNEL);
		if (unlikely(!data)) {
			pr_err("[LED] memory allocation error %s\n", __func__);
			ret = -ENOMEM;
			continue;
		}

		memcpy(data, &(pdata->leds[i]), sizeof(struct s2mpb02_led));

		led_data = kzalloc(sizeof(struct s2mpb02_led_data),
				   GFP_KERNEL);

		global_led_datas[i] = led_data;
		led_datas[i] = led_data;
		if (unlikely(!led_data)) {
			pr_err("[LED] memory allocation error %s\n", __func__);
			ret = -ENOMEM;
			kfree(data);
			continue;
		}

		led_data->s2mpb02 = s2mpb02;
		led_data->i2c = s2mpb02->i2c;
		led_data->data = data;
		led_data->led.name = data->name;
		led_data->led.brightness_set = s2mpb02_led_set;
		led_data->led.brightness = LED_OFF;
		led_data->brightness = data->brightness;
		led_data->led.flags = 0;
		led_data->led.max_brightness = S2MPB02_FLASH_TORCH_CURRENT_MAX;

		mutex_init(&led_data->lock);
		spin_lock_init(&led_data->value_lock);
		INIT_WORK(&led_data->work, s2mpb02_led_work);

		ret = led_classdev_register(&pdev->dev, &led_data->led);
		if (unlikely(ret)) {
			pr_err("unable to register LED\n");
			cancel_work_sync(&led_data->work);
			mutex_destroy(&led_data->lock);
			kfree(data);
			kfree(led_data);
			led_datas[i] = NULL;
			global_led_datas[i] = NULL;
			ret = -EFAULT;
			continue;
		}

		ret = s2mpb02_led_setup(led_data);
		/* To prevent faint LED light, enable active discharge */
		ret |= s2mpb02_update_reg(led_data->i2c, S2MPB02_REG_FLED_IRD2,
			0x02, 0x02);
		if (unlikely(ret)) {
			pr_err("unable to register LED\n");
			cancel_work_sync(&led_data->work);
			mutex_destroy(&led_data->lock);
			led_classdev_unregister(&led_data->led);
			kfree(data);
			kfree(led_data);
			led_datas[i] = NULL;
			global_led_datas[i] = NULL;
			ret = -EFAULT;
		}
	}

#ifdef CONFIG_OF
	kfree(pdata);
#endif

	camera_flash_dev = device_create(camera_class, NULL, 3, NULL, "flash");
	if (IS_ERR(camera_flash_dev)) {
		pr_err("<%s> Failed to create device(flash)!\n", __func__);
	} else {
		if(device_create_file(camera_flash_dev, &dev_attr_rear_flash) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__ ,dev_attr_rear_flash.attr.name);
		}

		if (device_create_file(camera_flash_dev,
					 &dev_attr_rear_torch_flash) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__ , dev_attr_rear_torch_flash.attr.name);
		}

#ifdef CONFIG_LEDS_IRIS_IRLED_CERTIFICATE_SUPPORT
		if (device_create_file(camera_flash_dev,
					 &dev_attr_irled_torch) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__ , dev_attr_irled_torch.attr.name);
		}
#endif

#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
		if (device_create_file(camera_flash_dev,
					 &dev_attr_rear_torch_flash2) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__ , dev_attr_rear_torch_flash2.attr.name);
		}

		if(device_create_file(camera_flash_dev, &dev_attr_rear_flash2) < 0) {
			pr_err("<%s> failed to create device file, %s\n",
				__func__ ,dev_attr_rear_flash2.attr.name);
		}
#endif
	}

	pr_err("<%s> end\n", __func__);

	return ret;
}

static int s2mpb02_led_remove(struct platform_device *pdev)
{
	struct s2mpb02_led_data **led_datas = platform_get_drvdata(pdev);
	int i;

	for (i = 0; i != S2MPB02_LED_MAX; ++i) {
		if (led_datas[i] == NULL)
			continue;

		cancel_work_sync(&led_datas[i]->work);
		mutex_destroy(&led_datas[i]->lock);
		led_classdev_unregister(&led_datas[i]->led);
		kfree(led_datas[i]->data);
		kfree(led_datas[i]);
		led_datas[i] = NULL;
		global_led_datas[i] = NULL;
	}
	kfree(led_datas);

	if(camera_flash_dev) {
		device_remove_file(camera_flash_dev, &dev_attr_rear_flash);
		device_remove_file(camera_flash_dev, &dev_attr_rear_torch_flash);
#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
		device_remove_file(camera_flash_dev, &dev_attr_rear_flash2);
		device_remove_file(camera_flash_dev, &dev_attr_rear_torch_flash2);
#endif
#ifdef CONFIG_LEDS_IRIS_IRLED_CERTIFICATE_SUPPORT
		device_remove_file(camera_flash_dev, &dev_attr_irled_torch);
#endif
	}

	if (camera_class && camera_flash_dev) {
		device_destroy(camera_class, camera_flash_dev->devt);
	}

	return 0;
}

static void s2mpb02_led_shutdown(struct device *dev)
{
	global_led_datas[S2MPB02_TORCH_LED_1]->data->brightness = LED_OFF;
	led_set(global_led_datas[S2MPB02_TORCH_LED_1]);
#ifdef CONFIG_LEDS_S2MPB02_MULTI_TORCH_REAR2
	global_led_datas[S2MPB02_TORCH_LED_2]->data->brightness = LED_OFF;
	led_set(global_led_datas[S2MPB02_TORCH_LED_2]);
#endif
}

static struct platform_driver s2mpb02_led_driver = {
	.probe		= s2mpb02_led_probe,
	.remove		= s2mpb02_led_remove,
	.driver		= {
	.name		= "s2mpb02-led",
	.owner		= THIS_MODULE,
	.shutdown	= s2mpb02_led_shutdown,
	},
};

static int __init s2mpb02_led_init(void)
{
	return platform_driver_register(&s2mpb02_led_driver);
}
module_init(s2mpb02_led_init);

static void __exit s2mpb02_led_exit(void)
{
	platform_driver_unregister(&s2mpb02_led_driver);
}
module_exit(s2mpb02_led_exit);

MODULE_DESCRIPTION("S2MPB02 LED driver");
MODULE_LICENSE("GPL");

