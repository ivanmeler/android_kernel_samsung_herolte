/*
 * cirrus-amp-calibration.c -- CS35L33 calibration driver
 *
 * Copyright 2015 Cirrus Logic, Inc.
 *
 * Author: Nikesh Oswal <Nikesh.Oswal@cirrus.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/regmap.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/syscalls.h>
#include <linux/file.h>
#include <linux/fcntl.h>
#include <linux/power_supply.h>

#include "cirrus-amp-cal.h"

#define STRING_BUFFER 12

#define STPILOTPARAMSIN_IENABLE 0x2A03E4
#define SPKPROT_MODEL_STADAPTPARAMSIN_FXPEAKTHRCONST 0x2A0370
#define STOUTPUTPARAMS_FXRE 0x2A0470

#define STTSPARAMSIN_FXTREF 0x2A031A
#define STTSPARAMSIN_FXREREFINV 0x2A031C

#define U24_MIN (-8388608)
#define U24_MAX 8388607

struct cirrus_amp_calib_ration {
	struct class *calib_class;
	struct device *dev;
	struct delayed_work calib_work;
	struct mutex lock;
	struct regmap *regmap;

	int running;
	unsigned int duration;

	unsigned int fx_peak;
	int inv_dc_resistance;
	int temperature;

	/* Range for rept and redc in ohms*/
	unsigned int imp_min;
	unsigned int imp_max;

	/* Range for temperature in degrees celsius */
	unsigned int temp_min;
	unsigned int temp_max;
};

struct cirrus_amp_calib_ration *calib;

static int u24_saturate(int value);
static int cirrus_amp_calib_read_file(char *filename, int *value);
static int cirrus_amp_calib_write_file(char *filename, int value);
static int cirrus_amp_calib_read_irdc(int *value);
static int cirrus_amp_calib_read_temp(int *value);
static int cirrus_amp_calib_get_power_temp(void);

/***** External Interface *****/

int cirrus_amp_calib_set_regmap(struct regmap *regmap)
{
	if (!calib)
		return -ENOMEM;

	mutex_lock(&calib->lock);
	calib->regmap = regmap;
	mutex_unlock(&calib->lock);

	return 0;
}
EXPORT_SYMBOL_GPL(cirrus_amp_calib_set_regmap);

int cirrus_amp_calib_apply(void)
{
	int irdc = 0, temp = 0;
	int ret;

	if (!calib)
		return -ENOMEM;

	if (!calib->regmap)
		return -ENXIO;

	ret = cirrus_amp_calib_read_irdc(&irdc);
	if (ret < 0)
		return ret;

	ret = cirrus_amp_calib_read_temp(&temp);
	if (ret < 0)
		return ret;

	dev_info(calib->dev,
		 "Writing calibration: 1/rDC: %d, Temp: %d\n",
		 irdc, temp);

	ret = regmap_write(calib->regmap, STTSPARAMSIN_FXREREFINV, irdc);
	if (ret)
		return ret;

	ret = regmap_write(calib->regmap, STTSPARAMSIN_FXTREF, temp);
	if (ret)
		return ret;

	return 0;
}
EXPORT_SYMBOL_GPL(cirrus_amp_calib_apply);

/***** Calibration Logic *****/

static int cirrus_amp_calib_start(void)
{
	int ret;
	unsigned int val;

	ret = regmap_read(calib->regmap, SPKPROT_MODEL_STADAPTPARAMSIN_FXPEAKTHRCONST,
			&(calib->fx_peak));
	if (ret)
		return ret;

	ret = regmap_read(calib->regmap, STPILOTPARAMSIN_IENABLE, &val);
	if (ret)
		return ret;

	/* Set fx_peak to 0 */
	ret = regmap_write(calib->regmap, SPKPROT_MODEL_STADAPTPARAMSIN_FXPEAKTHRCONST, 0x0);
	if (ret)
		return ret;

	if (val != 0x3) {
		dev_err(calib->dev, "Pilot Tone not enabled by preconfig: %d\n", val);
		/* Enable pilot tone */
		ret = regmap_write(calib->regmap, STPILOTPARAMSIN_IENABLE, 0x3);
		if (ret)
			return ret;
	}

	return 0;
}

static int cirrus_amp_calib_end(void)
{
	int ret;

	/* restore fx_peak */
	ret = regmap_write(calib->regmap, SPKPROT_MODEL_STADAPTPARAMSIN_FXPEAKTHRCONST,
			calib->fx_peak);
	if (ret)
		return ret;

	return 0;
}

static int cirrus_amp_calib_store(void)
{
	int ret;

	ret = cirrus_amp_calib_write_file(FILEPATH_CRUS_TEMP_CAL,
					  calib->temperature);
	if (ret < 0)
		return ret;

	ret = cirrus_amp_calib_write_file(FILEPATH_CRUS_RDC_CAL,
				      calib->inv_dc_resistance);
	if (ret < 0)
		return ret;

	return 0;
}

static void cirrus_amp_calib_work(struct work_struct *work)
{
	u32 raw_rept;
	int rept, temp;
	int ret;

	/* For holding the integer parts in the
	 * comparisons against imp/temp min/max
	 */
	int _rept, _temp;

	mutex_lock(&calib->lock);

	if (!calib->regmap) {
		ret = -ENXIO;
		goto error;
	}

	ret = regmap_read(calib->regmap, STOUTPUTPARAMS_FXRE, &raw_rept);
	if (ret) {
		dev_err(calib->dev, "Failed to read rePT: %d\n", ret);
		goto error;
	}

	rept = sign_extend32(raw_rept, 23);

	/* rept is in 6.17 so make sure we get a sane comparison
	 * with the integer limits for acceptable impedances
	 */
	_rept = rept >> 17;
	if ((_rept) < calib->imp_min ||
	    (_rept) > calib->imp_max) {
		dev_err(calib->dev,
			"rept: 0x%08x or redc: 0x%08x out of range %d<x<%d\n",
			_rept, _rept, calib->imp_min, calib->imp_max);
		goto error;
	}

	/*
	 * rept is in S6.17 and 1/rept should be in S0.23
	 * Calculate 1/rept in S0.23, losing as little precision as possible
	 * rept = (2^17 * 2^23) / ((2^17 * rept) / 2^17);
	 */
	if (!rept) {
		dev_err(calib->dev, "rept too small\n");
		ret = -EDOM;
		goto error;
	}
	rept = u24_saturate(div64_s64(131072ll * 8388608ll, rept));

	calib->inv_dc_resistance = rept;
	temp = cirrus_amp_calib_get_power_temp();
	/* Convert from 10ths of a degree into S8.15 degrees */
	calib->temperature = u24_saturate((temp << 15) / 10);

	_temp = calib->temperature >> 15;
	if ((_temp) < calib->temp_min ||
	    (_temp) > calib->temp_max) {
		dev_err(calib->dev, "temp %d out of range %d<x<%d\n",
			_temp, calib->temp_min, calib->temp_max);
		goto error;
	}

	ret = cirrus_amp_calib_end();
	if (ret) {
		dev_err(calib->dev, "Failed to end calibration: %d\n", ret);
		goto error_end;
	}
	ret = cirrus_amp_calib_store();
	if (ret)
		goto error_end;

	calib->running = 0;

	mutex_unlock(&calib->lock);

	return;

error:
	cirrus_amp_calib_end();
error_end:
	calib->running = ret;
	mutex_unlock(&calib->lock);
}

/***** Helper Functions *****/

static int u24_saturate(int value)
{
	if (value < U24_MIN)
		value = U24_MIN;

	if (value > U24_MAX)
		value = U24_MAX;

	return value;
}

static int cirrus_amp_calib_read_file(char *filename, int *value)
{
	struct file *calib_filp;
	mm_segment_t old_fs = get_fs();
	char str[STRING_BUFFER] = {0};
	int ret;

	set_fs(KERNEL_DS);

	calib_filp = filp_open(filename, O_RDONLY, 0660);
	if (IS_ERR(calib_filp)) {
		ret = PTR_ERR(calib_filp);
		dev_err(calib->dev, "Failed to open calibration file %s: %d\n",
			filename, ret);
		goto err_open;
	}

	ret = calib_filp->f_op->read(calib_filp, str, sizeof(str),
				     &calib_filp->f_pos);
	if (ret != sizeof(str)) {
		dev_err(calib->dev, "Failed to read calibration file %s\n",
			filename);
		ret = -EIO;
		goto err_read;
	}

	ret = 0;

	if (kstrtoint(str, 0, value)) {
		dev_err(calib->dev, "Failed to parse calibration.\n");
		ret = -EINVAL;
	}

err_read:
	filp_close(calib_filp, current->files);
err_open:
	set_fs(old_fs);

	return ret;
}

static int cirrus_amp_calib_write_file(char *filename, int value)
{
	struct file *calib_filp;
	mm_segment_t old_fs = get_fs();
	char str[STRING_BUFFER] = {0};
	int ret;

	set_fs(KERNEL_DS);

	calib_filp = filp_open(filename, O_CREAT | O_TRUNC | O_WRONLY, 0660);
	if (IS_ERR(calib_filp)) {
		ret = PTR_ERR(calib_filp);
		dev_err(calib->dev, "Failed to open calibration file %s: %d\n",
			filename, ret);
		goto err_open;
	}

	snprintf(str, sizeof(str), "%d", value);

	ret = calib_filp->f_op->write(calib_filp, str, sizeof(str),
				      &calib_filp->f_pos);
	if (ret != sizeof(str)) {
		dev_err(calib->dev, "Failed to write calibration file %s\n",
			filename);
		ret = -EIO;
		goto err_write;
	}

	ret = 0;

err_write:
	filp_close(calib_filp, current->files);
err_open:
	set_fs(old_fs);

	return ret;
}

static int cirrus_amp_calib_read_irdc(int *value)
{
	if (calib->inv_dc_resistance == INT_MIN) {
		int ret = cirrus_amp_calib_read_file(FILEPATH_CRUS_RDC_CAL,
						 &calib->inv_dc_resistance);
		if (ret < 0)
			return ret;
	}

	*value = calib->inv_dc_resistance;
	return 0;
}

static int cirrus_amp_calib_read_temp(int *value)
{
	if (calib->temperature == INT_MIN) {
		int ret = cirrus_amp_calib_read_file(FILEPATH_CRUS_TEMP_CAL,
						 &calib->temperature);
		if (ret < 0)
			return ret;
	}

	*value = calib->temperature;
	return 0;
}

static int cirrus_amp_calib_get_power_temp(void)
{
	union power_supply_propval value = {0};
	struct power_supply *psy;

	psy = power_supply_get_by_name("battery");
	if (!psy) {
		dev_warn(calib->dev, "Failed to get battery, assuming 23C\n");
		return 23 * 10;
	}

	psy->get_property(psy, POWER_SUPPLY_PROP_TEMP, &value);

	return value.intval;
}

/***** SYSFS Interfaces *****/

static ssize_t cirrus_amp_calib_status_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	int ret;

	mutex_lock(&calib->lock);

	if (calib->running < 0)
		ret = sprintf(buf, "Error: %d\n", calib->running);
	else if (calib->running)
		ret = sprintf(buf, "Running\n");
	else
		ret = sprintf(buf, "Idle\n");

	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_status_store(struct device *dev,
					 struct device_attribute *attr,
					 const char *buf, size_t size)
{
	int status = 0;

	if (kstrtos32(buf, 0, &status)) {
		dev_err(dev, "Failed to parse status\n");
		return -EINVAL;
	}

	status = !!status;

	cancel_delayed_work_sync(&calib->calib_work);

	mutex_lock(&calib->lock);

	if (!calib->regmap) {
		dev_err(dev, "Regmap not available\n");
		calib->running = -ENXIO;
		goto error;
	}

	if (status != calib->running) {
		calib->running = status;

		if (status) {
			int delay = msecs_to_jiffies(calib->duration);
			int ret;

			ret = cirrus_amp_calib_start();
			if (ret) {
				dev_err(dev,
					"Failed to start calibration: %d\n",
					ret);
				calib->running = ret;
				goto error;
			} else {
				queue_delayed_work(system_unbound_wq,
						   &calib->calib_work, delay);
			}
		}
	}

	mutex_unlock(&calib->lock);

	return size;

error:
	mutex_unlock(&calib->lock);
	return calib->running;
}

static ssize_t cirrus_amp_calib_irdc_show(struct device *dev,
				     struct device_attribute *attr, char *buf)
{
	int ret, value = 0;

	mutex_lock(&calib->lock);

	ret = cirrus_amp_calib_read_irdc(&value);
	if (!ret)
		ret = sprintf(buf, "%d", value);
	else
		calib->running = ret;

	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_irdc_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	int ret, dc;

	if (kstrtos32(buf, 0, &ret)) {
		dev_err(dev, "Failed parsing 1/rDC\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);

	/* Given that we can store the irdc value which will get applied
	 * at calibration, make sure the inverse still corresponds to an
	 * impedance in the specified range
	 */
	dc = (1 << 23) / ret;

	if (dc < calib->imp_min ||
	    dc > calib->imp_max) {
		dev_err(dev,
			"dc equivalent value out %d of range %d<x<%d\n",
			dc, calib->imp_min, calib->imp_max);

		mutex_unlock(&calib->lock);
		return -EDOM;
	}

	calib->inv_dc_resistance = ret;

	ret = cirrus_amp_calib_write_file(FILEPATH_CRUS_RDC_CAL, ret);
	if (ret < 0) {
		calib->running = ret;
		mutex_unlock(&calib->lock);
		return ret;
	}

	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_temp_show(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int ret, value = 0;

	mutex_lock(&calib->lock);

	ret = cirrus_amp_calib_read_temp(&value);
	if (!ret)
		ret = sprintf(buf, "%d", value);
	else
		calib->running = ret;

	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_temp_store(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t size)
{
	int ret, temp;

	if (kstrtos32(buf, 0, &ret)) {
		dev_err(dev, "Failed parsing temperature\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);

	temp = ret >> 15;
	if (temp < calib->temp_min ||
	    temp > calib->temp_max) {
		dev_err(dev,
			"temperature %d is out of range %d<x%d\n",
			temp, calib->temp_min, calib->temp_max);
			mutex_unlock(&calib->lock);
			return -EDOM;
	}

	calib->temperature = ret;

	ret = cirrus_amp_calib_write_file(FILEPATH_CRUS_TEMP_CAL, ret);
	if (ret < 0) {
		calib->running = ret;
		mutex_unlock(&calib->lock);
		return ret;
	}

	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_duration_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret;

	mutex_lock(&calib->lock);
	ret = sprintf(buf, "%u", calib->duration);
	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_duration_store(struct device *dev,
					   struct device_attribute *attr,
					   const char *buf, size_t size)
{
	int duration;

	if (kstrtou32(buf, 0, &duration)) {
		dev_err(dev, "Failed parsing duration\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);
	calib->duration = duration;
	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_imp_min_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret;

	mutex_lock(&calib->lock);
	ret = sprintf(buf, "%u\n", calib->imp_min);
	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_imp_min_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	int imp_min;

	if (kstrtou32(buf, 0, &imp_min)) {
		dev_err(dev, "Failed parsing imp_min\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);
	calib->imp_min = imp_min;
	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_imp_max_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret;

	mutex_lock(&calib->lock);
	ret = sprintf(buf, "%u\n", calib->imp_max);
	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_imp_max_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	int imp_max;

	if (kstrtou32(buf, 0, &imp_max)) {
		dev_err(dev, "Failed parsing imp_max\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);
	calib->imp_max = imp_max;
	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_temp_min_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret;

	mutex_lock(&calib->lock);
	ret = sprintf(buf, "%u\n", calib->temp_min);
	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_temp_min_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	int temp_min;

	if (kstrtou32(buf, 0, &temp_min)) {
		dev_err(dev, "Failed parsing temp_min\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);
	calib->temp_min = temp_min;
	mutex_unlock(&calib->lock);

	return size;
}

static ssize_t cirrus_amp_calib_temp_max_show(struct device *dev,
					  struct device_attribute *attr,
					  char *buf)
{
	int ret;

	mutex_lock(&calib->lock);
	ret = sprintf(buf, "%u\n", calib->temp_max);
	mutex_unlock(&calib->lock);

	return ret;
}

static ssize_t cirrus_amp_calib_temp_max_store(struct device *dev,
					      struct device_attribute *attr,
					      const char *buf, size_t size)
{
	int temp_max;

	if (kstrtou32(buf, 0, &temp_max)) {
		dev_err(dev, "Failed parsing temp_max\n");
		return -EINVAL;
	}

	mutex_lock(&calib->lock);
	calib->temp_max = temp_max;
	mutex_unlock(&calib->lock);

	return size;
}

static DEVICE_ATTR(status, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_status_show, cirrus_amp_calib_status_store);
static DEVICE_ATTR(duration, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_duration_show,
		   cirrus_amp_calib_duration_store);
static DEVICE_ATTR(irdc, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_irdc_show, cirrus_amp_calib_irdc_store);
static DEVICE_ATTR(temp, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_temp_show, cirrus_amp_calib_temp_store);
static DEVICE_ATTR(imp_min, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_imp_min_show,
		   cirrus_amp_calib_imp_min_store);
static DEVICE_ATTR(imp_max, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_imp_max_show,
		   cirrus_amp_calib_imp_max_store);
static DEVICE_ATTR(temp_min, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_temp_min_show,
		   cirrus_amp_calib_temp_min_store);
static DEVICE_ATTR(temp_max, S_IRUGO | S_IWUSR | S_IWGRP,
		   cirrus_amp_calib_temp_max_show,
		   cirrus_amp_calib_temp_max_store);

static struct attribute *cirrus_amp_calib_attr[] = {
	&dev_attr_duration.attr,
	&dev_attr_irdc.attr,
	&dev_attr_temp.attr,
	&dev_attr_status.attr,
	&dev_attr_imp_min.attr,
	&dev_attr_imp_max.attr,
	&dev_attr_temp_min.attr,
	&dev_attr_temp_max.attr,
	NULL,
};

static struct attribute_group cirrus_amp_calib_attr_grp = {
	.attrs = cirrus_amp_calib_attr,
};

static int __init cirrus_amp_calib_init(void)
{
	int ret;

	calib = kzalloc(sizeof(struct cirrus_amp_calib_ration), GFP_KERNEL);
	if (calib == NULL)
		return -ENOMEM;

	INIT_DELAYED_WORK(&calib->calib_work, cirrus_amp_calib_work);
	mutex_init(&calib->lock);

	calib->duration = 2000; /* 2 secs */
	calib->inv_dc_resistance = INT_MIN;
	calib->temperature = INT_MIN;

	calib->imp_min = 4;
	calib->imp_max = 10;

	calib->temp_min = 15;
	calib->temp_max = 50;

	calib->calib_class = class_create(THIS_MODULE, CALIB_CLASS_NAME);
	if (IS_ERR(calib->calib_class)) {
		ret = PTR_ERR(calib->calib_class);
		goto err_alloc;
	}

	calib->dev = device_create(calib->calib_class, NULL, 1, NULL,
				   CALIB_DIR_NAME);
	if (IS_ERR(calib->dev)) {
		ret = PTR_ERR(calib->dev);
		goto err_device;
	}

	ret = sysfs_create_group(&calib->dev->kobj, &cirrus_amp_calib_attr_grp);
	if (ret)
		goto err_device;

	return 0;

err_device:
	class_destroy(calib->calib_class);
err_alloc:
	kfree(calib);
	calib = NULL;

	return ret;
}
module_init(cirrus_amp_calib_init);

static void __exit cirrus_amp_calib_exit(void)
{
	kfree(calib);
}
module_exit(cirrus_amp_calib_exit);

MODULE_DESCRIPTION("CS35L33 Calibration driver");
MODULE_AUTHOR("Nikesh Oswal, Cirrus Logic Inc, <Nikesh.Oswal@cirrus.com>");
MODULE_LICENSE("GPL");
