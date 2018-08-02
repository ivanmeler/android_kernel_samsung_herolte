/*
 * maxim_dsm_power.c -- Module for Rdc calibration
 *
 * Copyright 2015 Maxim Integrated Products
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/init.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/moduleparam.h>
#include <linux/slab.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <sound/maxim_dsm.h>
#include <sound/maxim_dsm_cal.h>
#include <sound/maxim_dsm_power.h>

#define DEBUG_MAXIM_DSM_POWER
#ifdef DEBUG_MAXIM_DSM_POWER
#define dbg_maxdsm(format, args...)	\
pr_info("[MAXIM_DSM_POWER] %s: " format "\n", __func__, ## args)
#else
#define dbg_maxdsm(format, args...)
#endif /* DEBUG_MAXIM_DSM_POWER */

struct maxim_dsm_power *g_mdp;

static void maxdsm_power_work(struct work_struct *work)
{
	struct maxim_dsm_power *mdp;
	unsigned int power = 0;
	unsigned long diff;

	mdp = container_of(work, struct maxim_dsm_power, work.work);

	mutex_lock(&mdp->mutex);

#ifdef CONFIG_SND_SOC_MAXIM_DSM
	power = maxdsm_get_power_measurement();
#endif /* CONFIG_SND_SOC_MAXIM_DSM */
	mdp->values.avg += power;
	mdp->values.count++;

	diff = jiffies - mdp->info.previous_jiffies;
	mdp->info.remaining -= jiffies_to_msecs(diff);

	dbg_maxdsm("power=0x%08x remaining=%d duration=%d",
			power,
			mdp->info.remaining,
			mdp->info.duration);

	if (mdp->info.remaining > 0
			&& mdp->values.status) {
		mdp->info.previous_jiffies = jiffies;
		queue_delayed_work(mdp->wq,
				&mdp->work,
				msecs_to_jiffies(mdp->info.interval));
	} else {
		mdp->values.count > 0 ?
			do_div(mdp->values.avg, mdp->values.count) : 0;
		mdp->values.power = mdp->values.avg;
		dbg_maxdsm("power=0x%08x", mdp->values.power);
		g_mdp->values.status = 0;
	}

	mutex_unlock(&mdp->mutex);
}

static int maxdsm_power_check(
		struct maxim_dsm_power *mdp, int action)
{
	int ret = 0;

	if (delayed_work_pending(&mdp->work))
		cancel_delayed_work(&mdp->work);

	if (action) {
		mdp->info.remaining = mdp->info.duration;
		mdp->values.count = mdp->values.avg = 0;
		mdp->info.previous_jiffies = jiffies;
		queue_delayed_work(mdp->wq,
				&mdp->work,
				1);
	}

	return ret;
}
static ssize_t maxdsm_power_duration_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", g_mdp->info.duration);
}

static ssize_t maxdsm_power_duration_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (kstrtou32(buf, 0, &g_mdp->info.duration))
		dev_err(dev,
			"%s: Failed converting from str to u32.\n", __func__);
	return size;
}
static DEVICE_ATTR(duration, S_IRUGO | S_IWUSR | S_IWGRP,
		maxdsm_power_duration_show, maxdsm_power_duration_store);

static ssize_t maxdsm_power_value_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%x", g_mdp->values.power);
}
static DEVICE_ATTR(value, S_IRUGO | S_IWUSR | S_IWGRP,
		maxdsm_power_value_show, NULL);

static ssize_t maxdsm_power_interval_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%d", g_mdp->info.interval);
}

static ssize_t maxdsm_power_interval_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (kstrtou32(buf, 0, &g_mdp->info.interval))
		dev_err(dev,
			"%s: Failed converting from str to u32.\n", __func__);
	return size;
}
static DEVICE_ATTR(interval, S_IRUGO | S_IWUSR | S_IWGRP,
		maxdsm_power_interval_show, maxdsm_power_interval_store);

static ssize_t maxdsm_power_status_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return sprintf(buf, "%s\n",
			g_mdp->values.status ? "Enabled" : "Disabled");
}

static ssize_t maxdsm_power_status_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int status = 0;

	if (!kstrtou32(buf, 0, &status)) {
		status = status > 0 ? 1 : 0;
		if (status == g_mdp->values.status) {
			dbg_maxdsm("Already run. It will be ignored.");
		} else {
			mutex_lock(&g_mdp->mutex);
			g_mdp->values.status = status;
			mutex_unlock(&g_mdp->mutex);
			if (maxdsm_power_check(g_mdp, status)) {
				pr_err("%s: The codec was connected?\n",
						__func__);
				mutex_lock(&g_mdp->mutex);
				g_mdp->values.status = 0;
				mutex_unlock(&g_mdp->mutex);
			}
		}
	}

	return size;
}
static DEVICE_ATTR(status, S_IRUGO | S_IWUSR | S_IWGRP,
	maxdsm_power_status_show, maxdsm_power_status_store);

static struct attribute *maxdsm_power_attr[] = {
	&dev_attr_duration.attr,
	&dev_attr_value.attr,
	&dev_attr_interval.attr,
	&dev_attr_status.attr,
	NULL,
};

static struct attribute_group maxdsm_power_attr_grp = {
	.attrs = maxdsm_power_attr,
};

static int __init maxdsm_power_init(void)
{
	struct class *class = NULL;
	struct maxim_dsm_power *mdp;
	int ret = 0;

	g_mdp = kzalloc(sizeof(struct maxim_dsm_power), GFP_KERNEL);
	if (g_mdp == NULL)
		return -ENOMEM;
	mdp = g_mdp;

	mdp->wq = create_singlethread_workqueue(MAXDSM_POWER_WQ_NAME);
	if (mdp->wq == NULL) {
		kfree(g_mdp);
		return -ENOMEM;
	}

	INIT_DELAYED_WORK(&g_mdp->work, maxdsm_power_work);
	mutex_init(&g_mdp->mutex);

	mdp->info.duration = 2000; /* 2 secs */
	mdp->info.remaining = mdp->info.duration;
	mdp->info.interval = 100;
	mdp->values.power = 0xFFFFFFFF;
	mdp->platform_type = 0xFFFFFFFF;

#ifdef CONFIG_SND_SOC_MAXIM_DSM_CAL
	class = maxdsm_cal_get_class();
#else
	if (!class)
		class = class_create(THIS_MODULE,
				MAXDSM_POWER_DSM_NAME);
#endif /* CONFIG_SND_SOC_MAXIM_DSM_CAL */
	mdp->class = class;
	if (mdp->class) {
		mdp->dev = device_create(mdp->class, NULL, 1, NULL,
				MAXDSM_POWER_CLASS_NAME);
		if (!IS_ERR(mdp->dev)) {
			if (sysfs_create_group(&mdp->dev->kobj,
						&maxdsm_power_attr_grp))
				dbg_maxdsm(
						"Failed to create sysfs group. ret=%d",
						ret);
		}
	}
	dbg_maxdsm("class=%p %p", class, mdp->class);

	dbg_maxdsm("Completed initialization");

	return ret;
}
module_init(maxdsm_power_init);

static void __exit maxdsm_power_exit(void)
{
	kfree(g_mdp);
};
module_exit(maxdsm_power_exit);

MODULE_DESCRIPTION("For power measurement of DSM");
MODULE_AUTHOR("Kyounghun Jeon<hun.jeon@maximintegrated.com");
MODULE_LICENSE("GPL");
