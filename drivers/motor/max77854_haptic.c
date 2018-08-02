/*
 * haptic motor driver for max77854_haptic.c
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#define pr_fmt(fmt) "[VIB] " fmt
#define DEBUG	1

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/pwm.h>
#include <linux/platform_device.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/slab.h>
#include <linux/clk.h>
#include <linux/i2c.h>
#include <linux/regulator/consumer.h>
#include <linux/mfd/max77854.h>
#include <linux/mfd/max77854-private.h>
#include <linux/sec_sysfs.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/kthread.h>
#if defined(CONFIG_MOTOR_DRV_SENSOR)
#include "max77854_haptic.h"
#include "../sensorhub/brcm/ssp_motor.h"
#endif

#define MAX_INTENSITY			10000
#define PACKET_MAX_SIZE			1000

#define HAPTIC_ENGINE_FREQ_MIN		1200
#define HAPTIC_ENGINE_FREQ_MAX		3500

#define MOTOR_LRA			(1<<7)
#define MOTOR_EN			(1<<6)
#define EXT_PWM				(0<<5)
#define DIVIDER_128			(1<<1)
#define MRDBTMER_MASK			(0x7)
#define MREN				(1 << 3)
#define BIASEN				(1 << 7)

#define VIB_BUFSIZE			100

#if defined(CONFIG_MOTOR_DRV_SENSOR)
int (*sensorCallback)(int);
#endif

struct vib_packet {
	int time;
	int intensity;
	int freq;
	int overdrive;
};

enum {
	VIB_PACKET_TIME = 0,
	VIB_PACKET_INTENSITY,
	VIB_PACKET_FREQUENCY,
	VIB_PACKET_OVERDRIVE,
	VIB_PACKET_MAX,
};

struct max77854_haptic_drvdata {
	struct max77854_dev *max77854;
	struct i2c_client *i2c;
	struct device *dev;
	struct max77854_haptic_pdata *pdata;
	struct pwm_device *pwm;
	struct regulator *regulator;
	struct timed_output_dev tout_dev;
	struct hrtimer timer;
	struct kthread_worker kworker;
	struct kthread_work kwork;
	struct mutex mutex;
	u32 intensity;
	u32 timeout;
	int duty;

	bool f_packet_en;
	int packet_size;
	int packet_cnt;
	bool packet_running;
	struct vib_packet test_pac[PACKET_MAX_SIZE];
};

#if defined(CONFIG_MOTOR_DRV_SENSOR)
int setMotorCallback(int (*callback)(int state))
{
	if (!callback)
		sensorCallback = callback;
	else
		pr_debug("sensor callback is Null !%s\n", __func__);
	return 0;
}
#endif

static int max77854_haptic_set_frequency(struct max77854_haptic_drvdata *drvdata, int num)
{
	struct max77854_haptic_pdata *pdata = drvdata->pdata;

	if (num >= 0 && num < pdata->multi_frequency) {
		pdata->period = pdata->multi_freq_period[num];
		pdata->duty = pdata->multi_freq_duty[num];
		pdata->freq_num = num;
	} else if (num >= HAPTIC_ENGINE_FREQ_MIN && num <= HAPTIC_ENGINE_FREQ_MAX) {
		pdata->period = pdata->multi_freq_divider / num;
		pdata->duty = pdata->period * 95 / 100;
		drvdata->intensity = MAX_INTENSITY;
		drvdata->duty = pdata->duty;
		pdata->freq_num = num;
	} else {
		pr_err("%s out of range %d\n", __func__, num);
		return -EINVAL;
	}

	return 0;
}

static int max77854_haptic_set_intensity(struct max77854_haptic_drvdata *drvdata, int intensity)
{
	int duty = drvdata->pdata->period >> 1;

	if (intensity < -(MAX_INTENSITY) || MAX_INTENSITY < intensity) {
		pr_err("%s out of range %d\n", __func__, intensity);
		return -EINVAL;
	}

	if (MAX_INTENSITY == intensity)
		duty = drvdata->pdata->duty;
	else if (intensity == -(MAX_INTENSITY))
		duty = drvdata->pdata->period - drvdata->pdata->duty;
	else if (0 != intensity)
		duty += (drvdata->pdata->duty - duty) * intensity / MAX_INTENSITY;

	drvdata->intensity = intensity;
	drvdata->duty = duty;

	return 0;
}

static int max77854_haptic_set_pwm(struct max77854_haptic_drvdata *drvdata, bool en)
{
	struct max77854_haptic_pdata *pdata = drvdata->pdata;
	int ret = 0;

	if (en && drvdata->intensity > 0) {
		ret = pwm_config(drvdata->pwm, drvdata->duty, pdata->period);
		if (ret < 0) {
			pr_err("faild to config pwm %d\n", ret);
			return ret;
		}

		ret = pwm_enable(drvdata->pwm);
		if (ret < 0)
			pr_err("faild to enable pwm %d\n", ret);
	} else {
		ret = pwm_config(drvdata->pwm, pdata->period >> 1, pdata->period);
		if (ret < 0)
			pr_err("faild to config pwm %d\n", ret);

		pwm_disable(drvdata->pwm);
	}

	return ret;
}

static int motor_vdd_en(struct max77854_haptic_drvdata *drvdata, bool en)
{
	int ret = 0;

	if (drvdata->pdata->gpio)
		ret = gpio_direction_output(drvdata->pdata->gpio, en);
	else {
		if (en)
			ret = regulator_enable(drvdata->regulator);
		else
			ret = regulator_disable(drvdata->regulator);

		if (ret < 0)
			pr_err("failed to %sable regulator %d\n",
				en ? "en" : "dis", ret);
	}

	return ret;
}

static void max77854_haptic_init_reg(struct max77854_haptic_drvdata *drvdata)
{
	int ret;

	motor_vdd_en(drvdata, true);

	usleep_range(6000, 6000);

	ret = max77854_update_reg(drvdata->i2c,
			MAX77854_PMIC_REG_MAINCTRL1, 0xff, BIASEN);
	if (ret)
		pr_err("i2c REG_BIASEN update error %d\n", ret);

	ret = max77854_update_reg(drvdata->i2c,
		MAX77854_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);
	if (ret)
		pr_err("i2c MOTOR_EN update error %d\n", ret);

	ret = max77854_update_reg(drvdata->i2c,
		MAX77854_PMIC_REG_MCONFIG, 0xff, MOTOR_LRA);
	if (ret)
		pr_err("i2c MOTOR_LPA update error %d\n", ret);

	ret = max77854_update_reg(drvdata->i2c,
		MAX77854_PMIC_REG_MCONFIG, 0xff, DIVIDER_128);
	if (ret)
		pr_err("i2c DIVIDER_128 update error %d\n", ret);
}

static void max77854_haptic_i2c(struct max77854_haptic_drvdata *drvdata,
	bool en)
{

	if (en)
		max77854_update_reg(drvdata->i2c,
			MAX77854_PMIC_REG_MCONFIG, 0xff, MOTOR_EN);
	else
		max77854_update_reg(drvdata->i2c,
			MAX77854_PMIC_REG_MCONFIG, 0x0, MOTOR_EN);
}

static void max77854_haptic_engine_run_packet(struct max77854_haptic_drvdata *drvdata,
		struct vib_packet haptic_packet)
{
	int frequency = haptic_packet.freq;
	int intensity = haptic_packet.intensity;
	int time = haptic_packet.time;

	if (!drvdata->f_packet_en) {
		pr_err("haptic packet is empty\n");
		return;
	}

	max77854_haptic_set_frequency(drvdata, frequency);
	max77854_haptic_set_intensity(drvdata, intensity);
	max77854_haptic_set_pwm(drvdata, true);

	if (intensity == 0) {
		if (drvdata->packet_running) {
			pr_info("[haptic engine] motor stop\n");
			max77854_haptic_i2c(drvdata, false);
		}
		drvdata->packet_running = false;
	} else {
		if (!drvdata->packet_running) {
			pr_info("[haptic engine] motor run\n");
			max77854_haptic_i2c(drvdata, true);
		}
		drvdata->packet_running = true;
	}

	pr_info("%s [haptic_engine] freq:%d, intensity:%d, time:%d\n", __func__, frequency, intensity, time);
}

static ssize_t store_duty(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max77854_haptic_drvdata *drvdata
		= dev_get_drvdata(dev);

	int ret;
	u16 duty;

	ret = kstrtou16(buf, 0, &duty);
	if (ret != 0) {
		dev_err(dev, "fail to get duty.\n");
		return count;
	}
	drvdata->pdata->duty = duty;

	return count;
}

static ssize_t store_period(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct max77854_haptic_drvdata *drvdata
		= dev_get_drvdata(dev);

	int ret;
	u16 period;

	ret = kstrtou16(buf, 0, &period);
	if (ret != 0) {
		dev_err(dev, "fail to get period.\n");
		return count;
	}
	drvdata->pdata->period = period;

	return count;
}

static ssize_t show_duty_period(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct max77854_haptic_drvdata *drvdata = dev_get_drvdata(dev);

	return snprintf(buf, VIB_BUFSIZE, "duty: %u, period: %u\n",
			drvdata->pdata->duty,
			drvdata->pdata->period);
}

/* below nodes is SAMSUNG specific nodes */
static DEVICE_ATTR(set_duty, 0220, NULL, store_duty);
static DEVICE_ATTR(set_period, 0220, NULL, store_period);
static DEVICE_ATTR(show_duty_period, 0440, show_duty_period, NULL);

static struct attribute *sec_motor_attributes[] = {
	&dev_attr_set_duty.attr,
	&dev_attr_set_period.attr,
	&dev_attr_show_duty_period.attr,
	NULL,
};

static struct attribute_group sec_motor_attr_group = {
	.attrs = sec_motor_attributes,
};

static ssize_t intensity_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);
	int intensity = 0, ret = 0;

	ret = kstrtoint(buf, 0, &intensity);
	if (ret) {
		pr_err("fail to get intensity\n");
		return -EINVAL;
	}

	ret = max77854_haptic_set_intensity(drvdata, intensity);
	if (ret) {
		pr_err("cannot change intensity\n");
		return ret;
	}

	ret = max77854_haptic_set_pwm(drvdata, true);
	if (ret) {
		pr_err("cannot enable pwm\n");
	}

	return count;
}

static ssize_t intensity_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);

	return snprintf(buf, VIB_BUFSIZE, "intensity: %u\n", drvdata->intensity);
}

static DEVICE_ATTR(intensity, 0660, intensity_show, intensity_store);

static ssize_t frequency_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);
	int num, ret;

	ret = kstrtoint(buf, 0, &num);
	if (ret) {
		pr_err("fail to get frequency\n");
		return -EINVAL;
	}

	ret = max77854_haptic_set_frequency(drvdata, num);
	if (ret) {
		pr_err("cannot change frequency\n");
		return ret;
	}

	return count;
}

static ssize_t frequency_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);
	struct max77854_haptic_pdata *pdata = drvdata->pdata;

	return snprintf(buf, VIB_BUFSIZE, "frequency: %d\n", pdata->freq_num);
}

static DEVICE_ATTR(multi_freq, 0660, frequency_show, frequency_store);

static ssize_t haptic_engine_store(struct device *dev,
	struct device_attribute *devattr, const char *buf, size_t count)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);
	int i = 0, _data = 0, tmp = 0;

	if (sscanf(buf, "%6d", &_data) == 1) {
		if (_data > PACKET_MAX_SIZE * VIB_PACKET_MAX)
			pr_info("%s, [%d] packet size over\n", __func__, _data);
		else {
			drvdata->packet_size = _data / VIB_PACKET_MAX;
			drvdata->packet_cnt = 0;
			drvdata->f_packet_en = true;

			buf = strstr(buf, " ");

			for (i = 0; i < drvdata->packet_size; i++) {
				for (tmp = 0; tmp < VIB_PACKET_MAX; tmp++) {
					if (sscanf(buf++, "%6d", &_data) == 1) {
						switch (tmp) {
						case VIB_PACKET_TIME:
							drvdata->test_pac[i].time = _data;
							break;
						case VIB_PACKET_INTENSITY:
							drvdata->test_pac[i].intensity = _data;
							break;
						case VIB_PACKET_FREQUENCY:
							drvdata->test_pac[i].freq = _data;
							break;
						case VIB_PACKET_OVERDRIVE:
							drvdata->test_pac[i].overdrive = _data;
							break;
						}
						buf = strstr(buf, " ");
					} else {
						pr_info("%s packet data error\n", __func__);
						drvdata->f_packet_en = false;
						return count;
					}
				}
			}
		}
	}

	return count;
}

static ssize_t haptic_engine_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct timed_output_dev *tdev = dev_get_drvdata(dev);
	struct max77854_haptic_drvdata *drvdata
		= container_of(tdev, struct max77854_haptic_drvdata, tout_dev);
	int i = 0;
	size_t size = 0;

	for (i = 0; i < drvdata->packet_size && drvdata->f_packet_en &&
			((4 * VIB_BUFSIZE + size) < PAGE_SIZE); i++) {
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", drvdata->test_pac[i].time);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", drvdata->test_pac[i].intensity);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", drvdata->test_pac[i].freq);
		size += snprintf(&buf[size], VIB_BUFSIZE, "%u,", drvdata->test_pac[i].overdrive);
	}

	return size;
}

static DEVICE_ATTR(haptic_engine, 0660, haptic_engine_show, haptic_engine_store);

static int haptic_get_time(struct timed_output_dev *tout_dev)
{
	struct max77854_haptic_drvdata *drvdata
		= container_of(tout_dev, struct max77854_haptic_drvdata, tout_dev);
	struct hrtimer *timer = &drvdata->timer;

	if (hrtimer_active(timer)) {
		ktime_t remain = hrtimer_get_remaining(timer);
		struct timeval t = ktime_to_timeval(remain);

		return t.tv_sec * 1000 + t.tv_usec / 1000;
	}
	return 0;
}

static void haptic_enable(struct timed_output_dev *tout_dev, int value)
{
	struct max77854_haptic_drvdata *drvdata
		= container_of(tout_dev, struct max77854_haptic_drvdata, tout_dev);
	struct hrtimer *timer = &drvdata->timer;
	struct max77854_haptic_pdata *pdata = drvdata->pdata;
	int ret = 0;

	flush_kthread_worker(&drvdata->kworker);
	ret = hrtimer_cancel(timer);

	value = min_t(int, value, (int)pdata->max_timeout);
	drvdata->timeout = value;
	if (pdata->multi_frequency)
		pr_debug("%d %u %ums\n", pdata->freq_num, drvdata->duty, value);
	else
		pr_debug("%u %ums\n", drvdata->duty, value);
	if (value > 0) {
		mutex_lock(&drvdata->mutex);

#if defined(CONFIG_MOTOR_DRV_SENSOR)
		if (sensorCallback != NULL)
			sensorCallback(true);
		else {
			pr_info("%s sensorCallback is null.start\n", __func__);
			sensorCallback = getMotorCallback();
			sensorCallback(true);
		}
#endif
		if (drvdata->f_packet_en) {
			drvdata->packet_running = false;
			drvdata->timeout = drvdata->test_pac[0].time;
			max77854_haptic_engine_run_packet(drvdata, drvdata->test_pac[0]);
		} else {
			ret = max77854_haptic_set_pwm(drvdata, true);
			if (ret)
				pr_err("%s: error to enable pwm %d\n", __func__, ret);
			max77854_haptic_i2c(drvdata, true);
		}

		mutex_unlock(&drvdata->mutex);
		hrtimer_start(timer, ns_to_ktime((u64)drvdata->timeout * NSEC_PER_MSEC),
			HRTIMER_MODE_REL);
	} else if (value == 0) {
		mutex_lock(&drvdata->mutex);

		drvdata->f_packet_en = false;
		drvdata->packet_cnt = 0;
		drvdata->packet_size = 0;

		max77854_haptic_i2c(drvdata, false);
		max77854_haptic_set_pwm(drvdata, false);

#if defined(CONFIG_MOTOR_DRV_SENSOR)
		if (sensorCallback != NULL)
			sensorCallback(false);
		else {
			pr_debug("%s sensorCallback is null.start\n", __func__);
			sensorCallback = getMotorCallback();
			sensorCallback(true);
		}
#endif
		pr_debug("off\n");

		mutex_unlock(&drvdata->mutex);
	}
}

static enum hrtimer_restart haptic_timer_func(struct hrtimer *timer)
{
	struct max77854_haptic_drvdata *drvdata
		= container_of(timer, struct max77854_haptic_drvdata, timer);

	queue_kthread_work(&drvdata->kworker, &drvdata->kwork);
	return HRTIMER_NORESTART;
}

static void haptic_work(struct kthread_work *work)
{
	struct max77854_haptic_drvdata *drvdata
		= container_of(work, struct max77854_haptic_drvdata, kwork);
	struct hrtimer *timer = &drvdata->timer;

	mutex_lock(&drvdata->mutex);

	if (drvdata->f_packet_en) {
		drvdata->packet_cnt++;
		if (drvdata->packet_cnt < drvdata->packet_size) {
			max77854_haptic_engine_run_packet(drvdata, drvdata->test_pac[drvdata->packet_cnt]);
			hrtimer_start(timer, ns_to_ktime((u64)drvdata->test_pac[drvdata->packet_cnt].time * NSEC_PER_MSEC), HRTIMER_MODE_REL);

			goto unlock_without_vib_off;
		} else {
			drvdata->f_packet_en = false;
			drvdata->packet_cnt = 0;
			drvdata->packet_size = 0;
		}
	}

	max77854_haptic_i2c(drvdata, false);
	max77854_haptic_set_pwm(drvdata, false);

#if defined(CONFIG_MOTOR_DRV_SENSOR)
	if (sensorCallback != NULL)
		sensorCallback(false);
	else {
		pr_debug("%s sensorCallback is null.start\n", __func__);
		sensorCallback = getMotorCallback();
		sensorCallback(true);
	}
#endif
	pr_debug("off\n");

unlock_without_vib_off:
	mutex_unlock(&drvdata->mutex);
}

#if defined(CONFIG_OF)
static struct max77854_haptic_pdata *of_max77854_haptic_dt(struct device *dev)
{
	struct device_node *np_root = dev->parent->of_node;
	struct device_node *np;
	struct max77854_haptic_pdata *pdata;
	u32 temp;
	const char *temp_str;
	int ret, i;

	pdata = kzalloc(sizeof(struct max77854_haptic_pdata),
		GFP_KERNEL);
	if (!pdata) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		return NULL;
	}

	np = of_find_node_by_name(np_root,
			"haptic");
	if (np == NULL) {
		pr_err("%s : error to get dt node\n", __func__);
		goto err_parsing_dt;
	}

	ret = of_property_read_u32(np,
			"haptic,model", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : error to get dt node max_timeout\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->model = (u16)temp;

	ret = of_property_read_u32(np,
			"haptic,max_timeout", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : error to get dt node max_timeout\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->max_timeout = (u16)temp;

	ret = of_property_read_u32(np, "haptic,multi_frequency", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : error to get dt node multi_frequency\n", __func__);
		pdata->multi_frequency = 0;
	} else
		pdata->multi_frequency = (int)temp;

	if (pdata->multi_frequency) {
		ret = of_property_read_u32(np, "haptic,multi_frequency_divider", &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node multi_freq_divider\n", __func__);
			goto err_parsing_dt;
		} else {
			pdata->multi_freq_divider = (int)temp;
			pr_debug("multi_frequency_divider = %d\n", pdata->multi_freq_divider);
		}

		pdata->multi_freq_duty
			= devm_kzalloc(dev, sizeof(u32)*pdata->multi_frequency, GFP_KERNEL);
		if (!pdata->multi_freq_duty) {
			pr_err("%s: failed to allocate duty data\n", __func__);
			goto err_parsing_dt;
		}
		pdata->multi_freq_period
			= devm_kzalloc(dev, sizeof(u32)*pdata->multi_frequency, GFP_KERNEL);
		if (!pdata->multi_freq_period) {
			pr_err("%s: failed to allocate period data\n", __func__);
			goto err_parsing_dt;
		}

		ret = of_property_read_u32_array(np, "haptic,duty", pdata->multi_freq_duty,
				pdata->multi_frequency);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node duty\n", __func__);
			goto err_parsing_dt;
		}

		ret = of_property_read_u32_array(np, "haptic,period", pdata->multi_freq_period,
				pdata->multi_frequency);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node period\n", __func__);
			goto err_parsing_dt;
		}

		pdata->duty = pdata->multi_freq_duty[0];
		pdata->period = pdata->multi_freq_period[0];
		pdata->freq_num = 0;
	} else {
		ret = of_property_read_u32(np,
				"haptic,duty", &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node duty\n", __func__);
			goto err_parsing_dt;
		} else
			pdata->duty = (u16)temp;

		ret = of_property_read_u32(np,
				"haptic,period", &temp);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node period\n", __func__);
			goto err_parsing_dt;
		} else
			pdata->period = (u16)temp;
	}

	ret = of_property_read_u32(np,
			"haptic,pwm_id", &temp);
	if (IS_ERR_VALUE(ret)) {
		pr_err("%s : error to get dt node pwm_id\n", __func__);
		goto err_parsing_dt;
	} else
		pdata->pwm_id = (u16)temp;

	if (2 == pdata->model) {
		np = of_find_compatible_node(NULL, NULL, "vib,ldo_en");
		if (np == NULL) {
			pr_debug("%s: error to get motorldo dt node\n", __func__);
			goto err_parsing_dt;
		}
		pdata->gpio = of_get_named_gpio(np, "ldo_en", 0);
		if (!gpio_is_valid(pdata->gpio))
			pr_err("[VIB]failed to get ldo_en gpio\n");
	} else {
		pdata->gpio = 0;
		ret = of_property_read_string(np,
				"haptic,regulator_name", &temp_str);
		if (IS_ERR_VALUE(ret)) {
			pr_err("%s : error to get dt node regulator_name\n", __func__);
			goto err_parsing_dt;
		} else
			pdata->regulator_name = (char *)temp_str;

		pr_debug("regulator_name = %s\n", pdata->regulator_name);
	}

	/* debugging */
	pr_debug("model = %d\n", pdata->model);
	pr_debug("max_timeout = %d\n", pdata->max_timeout);
	if (pdata->multi_frequency) {
		pr_debug("multi frequency = %d\n", pdata->multi_frequency);
		for (i = 0; i < pdata->multi_frequency; i++) {
			pr_debug("duty[%d] = %d\n", i, pdata->multi_freq_duty[i]);
			pr_debug("period[%d] = %d\n", i, pdata->multi_freq_period[i]);
		}
	} else {
		pr_debug("duty = %d\n", pdata->duty);
		pr_debug("period = %d\n", pdata->period);
	}
	pr_debug("pwm_id = %d\n", pdata->pwm_id);

	return pdata;

err_parsing_dt:
	kfree(pdata);
	return NULL;
}
#endif

static int max77854_haptic_probe(struct platform_device *pdev)
{
	int error = 0;
	struct max77854_dev *max77854 = dev_get_drvdata(pdev->dev.parent);
	struct max77854_platform_data *max77854_pdata
		= dev_get_platdata(max77854->dev);
	struct max77854_haptic_pdata *pdata
		= max77854_pdata->haptic_data;
	struct max77854_haptic_drvdata *drvdata;
	struct task_struct *kworker_task;

#if defined(CONFIG_OF)
	if (pdata == NULL) {
		pdata = of_max77854_haptic_dt(&pdev->dev);
		if (!pdata) {
			pr_err("max77854-haptic : %s not found haptic dt!\n",
					__func__);
			return -1;
		}
	}
#else
	if (pdata == NULL) {
		pr_err("%s: no pdata\n", __func__);
		return -ENODEV;
	}
#endif /* CONFIG_OF */

	drvdata = kzalloc(sizeof(struct max77854_haptic_drvdata), GFP_KERNEL);
	if (!drvdata) {
		kfree(pdata);
		return -ENOMEM;
	}
	platform_set_drvdata(pdev, drvdata);
	drvdata->max77854 = max77854;
	drvdata->i2c = max77854->i2c;
	drvdata->pdata = pdata;
	drvdata->intensity = 8000;
	drvdata->duty = pdata->duty;

	init_kthread_worker(&drvdata->kworker);
	mutex_init(&drvdata->mutex);
	kworker_task = kthread_run(kthread_worker_fn,
		   &drvdata->kworker, "max77854_haptic");
	if (IS_ERR(kworker_task)) {
		pr_err("Failed to create message pump task\n");
		error = -ENOMEM;
		goto err_kthread;
	}

	init_kthread_work(&drvdata->kwork, haptic_work);

	drvdata->pwm = pwm_request(pdata->pwm_id, "vibrator");
	if (IS_ERR(drvdata->pwm)) {
		error = -EFAULT;
		pr_err("Failed to request pwm, err num: %d\n", error);
		goto err_pwm_request;
	} else
		pwm_config(drvdata->pwm, pdata->period >> 1, pdata->period);

	if (!pdata->gpio) {
		drvdata->regulator	= regulator_get(NULL, pdata->regulator_name);

		if (IS_ERR(drvdata->regulator)) {
			error = -EFAULT;
			pr_err("Failed to get vmoter regulator, err num: %d\n", error);
			goto err_regulator_get;
		}
	}

	max77854_haptic_init_reg(drvdata);

	/* hrtimer init */
	hrtimer_init(&drvdata->timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	drvdata->timer.function = haptic_timer_func;

	/* timed_output_dev init*/
	drvdata->tout_dev.name = "vibrator";
	drvdata->tout_dev.get_time = haptic_get_time;
	drvdata->tout_dev.enable = haptic_enable;

	drvdata->dev = sec_device_create(drvdata, "motor");
	if (IS_ERR(drvdata->dev)) {
		error = -ENODEV;
		pr_err("Failed to create device %d\n", error);
		goto exit_sec_devices;
	}
	error = sysfs_create_group(&drvdata->dev->kobj, &sec_motor_attr_group);
	if (error) {
		error = -ENODEV;
		pr_err("Failed to create sysfs %d\n", error);
		goto exit_sysfs;
	}

	error = timed_output_dev_register(&drvdata->tout_dev);
	if (error < 0) {
		error = -EFAULT;
		pr_err("Failed to register timed_output : %d\n", error);
		goto err_timed_output_register;
	}

	error = sysfs_create_file(&drvdata->tout_dev.dev->kobj,
				&dev_attr_intensity.attr);
	if (error < 0) {
		pr_err("Failed to register intensity sysfs : %d\n", error);
		goto err_timed_output_register;
	}

	if (pdata->multi_frequency) {
		error = sysfs_create_file(&drvdata->tout_dev.dev->kobj,
					&dev_attr_multi_freq.attr);
		if (error < 0) {
			pr_err("Failed to register multi_freq sysfs : %d\n", error);
			goto err_timed_output_register;
		}

		error = sysfs_create_file(&drvdata->tout_dev.dev->kobj,
					&dev_attr_haptic_engine.attr);
		if (error < 0) {
			pr_err("Failed to register haptic_engine sysfs : %d\n", error);
			goto err_timed_output_register;
		}
	}

	return error;

err_timed_output_register:
	sysfs_remove_group(&drvdata->dev->kobj, &sec_motor_attr_group);
exit_sysfs:
	sec_device_destroy(drvdata->dev->devt);
exit_sec_devices:
	regulator_put(drvdata->regulator);
err_regulator_get:
	pwm_free(drvdata->pwm);
err_kthread:
err_pwm_request:
	kfree(drvdata);
	kfree(pdata);
	return error;
}

static int max77854_haptic_remove(struct platform_device *pdev)
{
	struct max77854_haptic_drvdata *drvdata
		= platform_get_drvdata(pdev);

	timed_output_dev_unregister(&drvdata->tout_dev);
	sysfs_remove_group(&drvdata->dev->kobj, &sec_motor_attr_group);
	sec_device_destroy(drvdata->dev->devt);
	regulator_put(drvdata->regulator);
	pwm_free(drvdata->pwm);
	max77854_haptic_i2c(drvdata, false);
	kfree(drvdata);

	return 0;
}

static int max77854_haptic_suspend(struct platform_device *pdev,
		pm_message_t state)
{
	struct max77854_haptic_drvdata *drvdata
		= platform_get_drvdata(pdev);

	motor_vdd_en(drvdata, false);
	flush_kthread_worker(&drvdata->kworker);
	hrtimer_cancel(&drvdata->timer);
	return 0;
}
static int max77854_haptic_resume(struct platform_device *pdev)
{
	struct max77854_haptic_drvdata *drvdata
		= platform_get_drvdata(pdev);

	max77854_haptic_init_reg(drvdata);
	return 0;
}

static struct platform_driver max77854_haptic_driver = {
	.probe		= max77854_haptic_probe,
	.remove		= max77854_haptic_remove,
	.suspend		= max77854_haptic_suspend,
	.resume		= max77854_haptic_resume,
	.driver = {
		.name	= "max77854-haptic",
		.owner	= THIS_MODULE,
	},
};

static int __init max77854_haptic_init(void)
{
	return platform_driver_register(&max77854_haptic_driver);
}
module_init(max77854_haptic_init);

static void __exit max77854_haptic_exit(void)
{
	platform_driver_unregister(&max77854_haptic_driver);
}
module_exit(max77854_haptic_exit);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("max77854 haptic driver");
