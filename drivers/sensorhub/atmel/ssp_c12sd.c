/*
 * drivers/sensorhub/ssp_c12sd.c
 * Copyright (c) 2012 SAMSUNG ELECTRONICS
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston,
 * MA  02110-1301, USA.
 */

#include <linux/module.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>
#include <linux/delay.h>
#include <linux/types.h>
#include <linux/input.h>
#include <linux/platform_device.h>
#include <linux/errno.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/gpio_event.h>
#include <linux/device.h>
#include <linux/slab.h>
#include <linux/earlysuspend.h>
#include <linux/sensor/guva_c12sd.h>
#include "ssp.h"

#define VENDOR_NAME	"GENICOM"
#define CHIP_NAME	"GUVA-C12SD"

#ifdef CONFIG_HAS_EARLYSUSPEND
/* early suspend */
static void ssp_early_suspend(struct early_suspend *handler);
static void ssp_late_resume(struct early_suspend *handler);
#endif

struct uv_info {
	struct uv_platform_data *pdata;
	struct workqueue_struct *uv_wq;
	struct work_struct work_uv;
	struct device *uv_dev;
	struct hrtimer uv_timer;
	struct mutex power_lock;
	struct mutex read_lock;
	struct input_dev *uv_input_dev;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif
	ktime_t uv_poll_delay;
	int uv_raw_data;
	bool onoff;
};

/* sysfs for input device */
static ssize_t uv_poll_delay_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uv_info *uv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%lld\n",
		ktime_to_ns(uv->uv_poll_delay));
}

static ssize_t uv_poll_delay_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct uv_info *uv = dev_get_drvdata(dev);
	int64_t new_delay;
	int err;

	err = kstrtoll(buf, 10, &new_delay);
	if (err < 0)
		return err;

	mutex_lock(&uv->power_lock);
	if (uv->onoff) {
		hrtimer_cancel(&uv->uv_timer);
		cancel_work_sync(&uv->work_uv);
	}

	if (new_delay != ktime_to_ns(uv->uv_poll_delay)) {
		uv->uv_poll_delay = ns_to_ktime(new_delay);
		pr_info("%s, poll_delay = %lld\n", __func__, new_delay);
	}

	if (uv->onoff)
		hrtimer_start(&uv->uv_timer, uv->uv_poll_delay,
			HRTIMER_MODE_REL);
	mutex_unlock(&uv->power_lock);

	return size;
}

static ssize_t uv_enable_store(struct device *dev,
	struct device_attribute *attr,
	const char *buf, size_t size)
{
	struct uv_info *uv = dev_get_drvdata(dev);
	bool new_value;

	if (sysfs_streq(buf, "1")) {
		new_value = true;
	} else if (sysfs_streq(buf, "0")) {
		new_value = false;
	} else {
		pr_err("%s: invalid value %d\n", __func__, *buf);
		return -EINVAL;
	}
	pr_info("%s, old = %d, new = %d\n", __func__, uv->onoff, new_value);
	mutex_lock(&uv->power_lock);
	if (new_value && !uv->onoff) { /* Enable */
		uv->onoff = new_value;
		if (uv->pdata->power_on)
			uv->pdata->power_on(uv->onoff);
		hrtimer_start(&uv->uv_timer, uv->uv_poll_delay,
			HRTIMER_MODE_REL);
	} else if (!new_value && uv->onoff) { /* Disable */
		hrtimer_cancel(&uv->uv_timer);
		uv->onoff = new_value;
		if (uv->pdata->power_on)
			uv->pdata->power_on(uv->onoff);
	} else
		pr_err("%s, new_enable = %d\n", __func__, new_value);
	mutex_unlock(&uv->power_lock);

	return size;
}

static ssize_t uv_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uv_info *uv = dev_get_drvdata(dev);
	return snprintf(buf, PAGE_SIZE, "%d\n", uv->onoff);
}

static DEVICE_ATTR(poll_delay, S_IRUGO | S_IWUSR | S_IWGRP,
		   uv_poll_delay_show, uv_poll_delay_store);
static DEVICE_ATTR(enable, S_IRUGO | S_IWUSR | S_IWGRP,
		   uv_enable_show, uv_enable_store);

static struct attribute *uv_sysfs_attrs[] = {
	&dev_attr_enable.attr,
	&dev_attr_poll_delay.attr,
	NULL
};

static struct attribute_group uv_attribute_group = {
	.attrs = uv_sysfs_attrs,
};

/* sysfs for uv sensor class */
static ssize_t get_vendor_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", VENDOR_NAME);
}

static ssize_t get_chip_name(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", CHIP_NAME);
}

static ssize_t get_adc_value(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uv_info *uv = dev_get_drvdata(dev);
	int adc = 0;
	 /* If power is not turned on, it won't work. */
	if (!uv->onoff) {
		mutex_lock(&uv->read_lock);
		if (uv->pdata->power_on)
			uv->pdata->power_on(true);
		mdelay(20);
		adc = uv->pdata->get_adc_value();
		if (uv->pdata->power_on)
			uv->pdata->power_on(false);
		mutex_unlock(&uv->read_lock);
	} else
		adc = uv->uv_raw_data;
	pr_info("%s: uv_adc = 0x%x, (%dmV)\n", __func__, adc, adc / 1000);

	return snprintf(buf, PAGE_SIZE, "%d\n", adc);
}

static ssize_t uv_power_on(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uv_info *uv = dev_get_drvdata(dev);

	if (!uv->onoff && uv->pdata->power_on)
		uv->pdata->power_on(true);
	pr_info("%s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static ssize_t uv_power_off(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct uv_info *uv = dev_get_drvdata(dev);

	if (!uv->onoff && uv->pdata->power_on)
		uv->pdata->power_on(false);
	pr_info("%s\n", __func__);

	return sprintf(buf, "%d\n", 1);
}

static DEVICE_ATTR(vendor, S_IRUGO, get_vendor_name, NULL);
static DEVICE_ATTR(name, S_IRUGO, get_chip_name, NULL);
static DEVICE_ATTR(raw_data, S_IRUGO, get_adc_value, NULL);
static DEVICE_ATTR(power_on, S_IRUGO, uv_power_on, NULL);
static DEVICE_ATTR(power_off, S_IRUGO, uv_power_off, NULL);

static struct device_attribute *uv_sensor_attrs[] = {
	&dev_attr_vendor,
	&dev_attr_name,
	&dev_attr_raw_data,
	&dev_attr_power_on,
	&dev_attr_power_off,
	NULL
};

/* timer function for uv sensor polling */
static enum hrtimer_restart uv_timer_func(struct hrtimer *timer)
{
	struct uv_info *uv
	    = container_of(timer, struct uv_info, uv_timer);
	queue_work(uv->uv_wq, &uv->work_uv);
	hrtimer_forward_now(&uv->uv_timer, uv->uv_poll_delay);
	return HRTIMER_RESTART;
}

static void work_func_uv(struct work_struct *work)
{
	struct uv_info *uv
	    = container_of(work, struct uv_info, work_uv);

	mutex_lock(&uv->read_lock);
	uv->uv_raw_data = uv->pdata->get_adc_value();
	mutex_unlock(&uv->read_lock);

	input_report_rel(uv->uv_input_dev, REL_MISC, uv->uv_raw_data+1);
	input_sync(uv->uv_input_dev);
}

static int uv_probe(struct platform_device *pdev)
{
	struct uv_info *uv;
	struct uv_platform_data *pdata = pdev->dev.platform_data;
	int ret = 0;

	pr_info("%s: is started\n", __func__);
	if (!pdata) {
		pr_err("%s: pdata is NULL\n", __func__);
		return -ENODEV;
	}

	if (!pdata->get_adc_value || !pdata->power_on) {
		pr_err("%s: need to check pdata\n", __func__);
		return -ENODEV;
	}

	/* allocate memory */
	uv = kzalloc(sizeof(struct uv_info), GFP_KERNEL);
	if (uv == NULL) {
		pr_err("%s: Failed to allocate memory\n", __func__);
		return -ENOMEM;
	}

	uv->pdata = pdata;
	if (pdata->power_on) {
		uv->pdata->power_on = pdata->power_on;
		uv->pdata->power_on(false);
	}
	uv->onoff = false;

	if (pdata->get_adc_value)
		uv->pdata->get_adc_value = pdata->get_adc_value;

	mutex_init(&uv->power_lock);
	mutex_init(&uv->read_lock);

	/* allocate uv input device */
	uv->uv_input_dev = input_allocate_device();
	if (!uv->uv_input_dev) {
		pr_err("%s: could not allocate input device\n",
			__func__);
		goto err_input_allocate_device_uv;
	}

	input_set_drvdata(uv->uv_input_dev, uv);
	uv->uv_input_dev->name = "uv_sensor";
	input_set_capability(uv->uv_input_dev, EV_REL, REL_MISC);

	ret = input_register_device(uv->uv_input_dev);
	if (ret < 0) {
		pr_err("%s: could not register input device\n",
			__func__);
		input_free_device(uv->uv_input_dev);
		goto err_input_register_device_uv;
	}

	ret = sysfs_create_group(&uv->uv_input_dev->dev.kobj,
				&uv_attribute_group);
	if (ret) {
		pr_err("%s: could not create sysfs group\n",
			__func__);
		goto err_sysfs_create_group_uv;
	}

	/* timer init */
	hrtimer_init(&uv->uv_timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	uv->uv_poll_delay = ns_to_ktime(200 * NSEC_PER_MSEC);
	uv->uv_timer.function = uv_timer_func;

	/* workqueue init */
	uv->uv_wq = create_singlethread_workqueue("uv_wq");
	if (!uv->uv_wq) {
		ret = -ENOMEM;
		pr_err("%s: could not create uv workqueue\n",
			__func__);
		goto err_create_uv_workqueue;
	}
	INIT_WORK(&uv->work_uv, work_func_uv);

	/* sysfs for uv sensor */
	ret = sensors_register(uv->uv_dev, uv, uv_sensor_attrs,
		"uv_sensor");
	if (ret) {
		pr_err("%s: could not register uv device(%d)\n",
			__func__, ret);
		goto err_sensor_register_failed;
	}

	ret = sensors_create_symlink(uv->uv_input_dev);
	if (ret < 0) {
		pr_err("%s, sensors_create_symlinks failed!(%d)\n",
			__func__, ret);
		goto err_uv_input__sysfs_create_link;
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	uv->early_suspend.suspend = ssp_early_suspend;
	uv->early_suspend.resume = ssp_late_resume;
	register_early_suspend(&uv->early_suspend);
#endif

	platform_set_drvdata(pdev, uv);

	pr_info("%s, success\n", __func__);
	return 0;
err_uv_input__sysfs_create_link:
	sensors_unregister(uv->uv_dev, uv_sensor_attrs);
err_sensor_register_failed:
	destroy_workqueue(uv->uv_wq);
err_create_uv_workqueue:
	sysfs_remove_group(&uv->uv_input_dev->dev.kobj,
			   &uv_attribute_group);
err_sysfs_create_group_uv:
	input_unregister_device(uv->uv_input_dev);
err_input_register_device_uv:
err_input_allocate_device_uv:
	mutex_destroy(&uv->read_lock);
	mutex_destroy(&uv->power_lock);
	kfree(uv);
	return ret;
}

static int uv_remove(struct platform_device *pdev)
{
	struct uv_info *uv = dev_get_drvdata(&pdev->dev);

	pr_info("%s+\n", __func__);
	if (uv->onoff) {
		hrtimer_cancel(&uv->uv_timer);
		cancel_work_sync(&uv->work_uv);
	}
	destroy_workqueue(uv->uv_wq);

#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&uv->early_suspend);
#endif
	sensors_unregister(uv->uv_dev, uv_sensor_attrs);
	sysfs_remove_group(&uv->uv_input_dev->dev.kobj,
			   &uv_attribute_group);
	input_unregister_device(uv->uv_input_dev);

	if (uv->onoff && uv->pdata->power_on)
		uv->pdata->power_on(false);
	mutex_destroy(&uv->read_lock);
	mutex_destroy(&uv->power_lock);
	kfree(uv);
	pr_info("%s-\n", __func__);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
/* early suspend */
static void ssp_early_suspend(struct early_suspend *handler)
{
	struct uv_info *uv;
	uv = container_of(handler, struct uv_info, early_suspend);

	if (uv->onoff) {
		hrtimer_cancel(&uv->uv_timer);
		cancel_work_sync(&uv->work_uv);
		if (uv->pdata->power_on)
			uv->pdata->power_on(false);
	}
	pr_err("%s, enabled = %d\n", __func__, uv->onoff);
}

static void ssp_late_resume(struct early_suspend *handler)
{
	struct uv_info *uv;
	uv = container_of(handler, struct uv_info, early_suspend);

	if (uv->onoff) {
		if (uv->pdata->power_on)
			uv->pdata->power_on(true);
		hrtimer_start(&uv->uv_timer,
			uv->uv_poll_delay, HRTIMER_MODE_REL);
	}
	pr_err("%s, enabled = %d\n", __func__, uv->onoff);
}
#else
static int uv_suspend(struct platform_device *pdev, pm_message_t state)
{
	struct uv_info *uv = dev_get_drvdata(&pdev->dev);

	if (uv->onoff) {
		hrtimer_cancel(&uv->uv_timer);
		cancel_work_sync(&uv->work_uv);
		if (uv->pdata->power_on)
			uv->pdata->power_on(false);
	}
	pr_err("%s, enabled = %d\n", __func__, uv->onoff);
	return 0;
}

static int uv_resume(struct platform_device *pdev)
{
	struct uv_info *uv = dev_get_drvdata(&pdev->dev);

	if (uv->onoff) {
		if (uv->pdata->power_on)
			uv->pdata->power_on(true);
		hrtimer_start(&uv->uv_timer,
			uv->uv_poll_delay, HRTIMER_MODE_REL);
	}
	pr_err("%s, enabled = %d\n", __func__, uv->onoff);
	return 0;
}
#endif

static void uv_shutdown(struct platform_device *pdev)
{
	struct uv_info *uv = dev_get_drvdata(&pdev->dev);

	pr_info("%s+\n", __func__);
	if (uv->onoff) {
		hrtimer_cancel(&uv->uv_timer);
		cancel_work_sync(&uv->work_uv);
	}
	destroy_workqueue(uv->uv_wq);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&uv->early_suspend);
#endif
	sensors_unregister(uv->uv_dev, uv_sensor_attrs);
	sysfs_remove_group(&uv->uv_input_dev->dev.kobj,
			   &uv_attribute_group);
	input_unregister_device(uv->uv_input_dev);

	if (uv->onoff && uv->pdata->power_on)
		uv->pdata->power_on(false);
	mutex_destroy(&uv->power_lock);
	kfree(uv);
	pr_info("%s-\n", __func__);
}

static struct platform_driver uv_driver = {
	.probe      = uv_probe,
	.remove     = uv_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.suspend    = uv_suspend,
	.resume     = uv_resume,
#endif
	.shutdown   = uv_shutdown,
	.driver = {
		.name   = "uv_sensor",
		.owner  = THIS_MODULE,
	},
};

static int __init uv_init(void)
{
	return platform_driver_register(&uv_driver);
}

static void __exit uv_exit(void)
{
	platform_driver_unregister(&uv_driver);
}

module_init(uv_init);
module_exit(uv_exit);

MODULE_DESCRIPTION("UV sensor device driver");
MODULE_AUTHOR("OHeon Kwon <koh82.kwon@samsung.com>");
MODULE_LICENSE("GPL");
