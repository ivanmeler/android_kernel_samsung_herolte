/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/cpu.h>
#include <linux/fb.h>
#include <linux/pm_qos.h>
#include <linux/suspend.h>
#include <linux/gpio_keys.h>

#if defined(CONFIG_SENSORS_FP_LOCKSCREEN_MODE)
extern bool fp_lockscreen_mode;

struct pm_qos_request suspend_min_cpu_hotplug_request;

static void set_cpu_min_on_suspend(bool enable)
{
	pr_debug("%s: %sable\n", __func__, enable?"en":"dis");

	if (enable)
		pm_qos_update_request(&suspend_min_cpu_hotplug_request, 5);
	else
		pm_qos_update_request(&suspend_min_cpu_hotplug_request, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	pr_debug("%s: end\n", __func__);
}

static int sec_hotplug_suspend_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		if (fp_lockscreen_mode)
			set_cpu_min_on_suspend(true);
		break;

	case PM_POST_SUSPEND:
		if (!wakeup_by_key())
			set_cpu_min_on_suspend(false);
		break;
	}

	return NOTIFY_OK;
}

static struct notifier_block sec_hotplug_suspend_nb = {
	.notifier_call = sec_hotplug_suspend_pm_notifier,
	.priority = 1,		/* should be called before cpu driver notifier */
};

static int sec_hotplug_fb_notifier(struct notifier_block *nb,
					unsigned long val, void *data)
{
	struct fb_event *evdata = data;
	struct fb_info *info = evdata->info;
	unsigned int blank;
	int ret = NOTIFY_OK;

	if (val != FB_EVENT_BLANK && val != FB_R_EARLY_EVENT_BLANK)
		return 0;

	/*
	 * If FBNODE is not zero, it is not primary display(LCD)
	 * and don't need to process these scheduling.
	 */
	if (info->node)
		return ret;

	blank = *(int *)evdata->data;

	switch (blank) {
	case FB_BLANK_POWERDOWN:
		/* nothing */
		break;
	case FB_BLANK_UNBLANK:
		/* LCD is on */
		set_cpu_min_on_suspend(false);
		break;
	default:
		break;
	}

	return ret;
}

static struct notifier_block sec_hotplug_fb_nb = {
	.notifier_call = sec_hotplug_fb_notifier,
};
#endif

static void __init sec_hotplug_pm_qos_init(void)
{
#if defined(CONFIG_SENSORS_FP_LOCKSCREEN_MODE)
	/* Add PM QoS for min on suspend */
	pm_qos_add_request(&suspend_min_cpu_hotplug_request,
		PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);
#endif
}

static int __init sec_hotplug_init(void)
{
	/* Initialize pm_qos request and handler */
	sec_hotplug_pm_qos_init();

#if defined(CONFIG_SENSORS_FP_LOCKSCREEN_MODE)
	/* Register FB notifier */
	fb_register_client(&sec_hotplug_fb_nb);

	/* register pm notifier for min on suspend */
	register_pm_notifier(&sec_hotplug_suspend_nb);
#endif

	return 0;
}
device_initcall(sec_hotplug_init);
