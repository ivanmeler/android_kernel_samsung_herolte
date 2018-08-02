/*
 * linux/drivers/exynos/soc/samsung/exynos-hotplug_governor.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kthread.h>
#include <linux/slab.h>
#include <linux/pm_qos.h>
#include <linux/sched.h>

#include <soc/samsung/exynos-cpu_hotplug.h>

#define CREATE_TRACE_POINTS
#include <trace/events/hotplug_governor.h>

#define DEFAULT_DUAL_CHANGE_MS (15)		/* 15 ms */
#define DEFAULT_BOOT_ENABLE_MS (0)		/* boot delay is not applied */
#define RETRY_BOOT_ENABLE_MS (100)		/* 100 ms */

enum hpgov_event {
	HPGOV_SLACK_TIMER_EXPIRED = 1,	/* slack timer expired */
	HPGOV_BIG_MODE_UPDATED = 2,	/* dual/quad mode updated */
};

struct hpgov_attrib {
	struct kobj_attribute	enabled;
	struct kobj_attribute	dual_change_ms;
#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
	struct kobj_attribute	to_quad_ratio;
	struct kobj_attribute	to_dual_ratio;
	struct kobj_attribute	lit_mult_ratio;
#endif

	struct attribute_group	attrib_group;
};

struct hpgov_data {
	enum hpgov_event event;
	int req_cpu_max;
	int req_cpu_min;
};

struct {
	int				enabled;
	int				cur_cpu_max;
	int				cur_cpu_min;
	long				use_fast_hp;

	int				dual_change_ms;
	struct hpgov_attrib		attrib;
	struct mutex			attrib_lock;
	struct task_struct		*task;
	struct task_struct		*hptask;
	struct hrtimer			slack_timer;
	ktime_t				slack_start_time;
	struct irq_work			update_irq_work;
	struct irq_work			start_slack_timer_irq_work;
	struct hpgov_data		data;
	int				hp_state;
	wait_queue_head_t		wait_q;
	wait_queue_head_t		wait_hpq;

	int				boost_cnt;

#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
	int				*to_quad_ratio;
	int				*to_dual_ratio;
	int				*lit_mult_ratio;
#endif
} exynos_hpgov;

static struct pm_qos_request hpgov_max_pm_qos;
static struct pm_qos_request hpgov_min_pm_qos;

static DEFINE_SPINLOCK(hpgov_lock);
static DEFINE_SPINLOCK(hpgov_boost_cnt_lock);

enum {
	HP_STATE_WAITING = 0,		/* waiting for cpumask update */
	HP_STATE_SCHEDULED = 1,		/* hotplugging is scheduled */
	HP_STATE_IN_PROGRESS = 2,	/* in the process of hotplugging */
};

enum {
	BIG_DUAL_MODE,
	BIG_QUAD_MODE,
};

static int start_slack_timer(void)
{
	int ret;

	if (!exynos_hpgov.enabled)
		return 0;

	hrtimer_cancel(&exynos_hpgov.slack_timer);
	ret = hrtimer_start(&exynos_hpgov.slack_timer,
		ktime_add(exynos_hpgov.slack_start_time, ktime_set(0,
			exynos_hpgov.dual_change_ms * NSEC_PER_MSEC)),
			HRTIMER_MODE_PINNED);
	if (ret)
		pr_err("Failed to register slack timer %d\n", ret);

	return ret;
}

static inline int slack_timer_is_queued(void)
{
	return hrtimer_is_queued(&exynos_hpgov.slack_timer);
}

static enum hrtimer_restart exynos_hpgov_slack_timer(struct hrtimer *timer)
{
	unsigned long flags;

	if (exynos_hpgov.boost_cnt)
		goto out;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_SLACK_TIMER_EXPIRED;
	exynos_hpgov.data.req_cpu_min = 6;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	wake_up(&exynos_hpgov.wait_q);
out:
	return HRTIMER_NORESTART;
}

static void exynos_hpgov_big_mode_update(int big_mode)
{
	unsigned long flags;

	if (!exynos_hpgov.enabled)
		return;

	spin_lock_irqsave(&hpgov_lock, flags);
	exynos_hpgov.data.event = HPGOV_BIG_MODE_UPDATED;
	if (big_mode == BIG_QUAD_MODE)
		exynos_hpgov.data.req_cpu_min = 8;
	else
		exynos_hpgov.data.req_cpu_min = 6;
	spin_unlock_irqrestore(&hpgov_lock, flags);

	irq_work_queue(&exynos_hpgov.update_irq_work);
}

static void exynos_hpgov_irq_work(struct irq_work *irq_work)
{
	wake_up(&exynos_hpgov.wait_q);
}

static void exynos_hpgov_slack_timer_start(void)
{
	if (!exynos_hpgov.enabled)
		return;

	irq_work_queue(&exynos_hpgov.start_slack_timer_irq_work);
}

static void slack_timer_irq_work(struct irq_work *irq_work)
{
	start_slack_timer();
}

void inc_boost_req_count(void)
{
	unsigned long flags;
	int boost_cnt;

	spin_lock_irqsave(&hpgov_boost_cnt_lock, flags);
	boost_cnt = ++exynos_hpgov.boost_cnt;
	spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);

	if (boost_cnt == 1)
		exynos_hpgov_big_mode_update(BIG_QUAD_MODE);
}

void dec_boost_req_count(bool delayed_boost)
{
	unsigned long flags;
	int boost_cnt;

	spin_lock_irqsave(&hpgov_boost_cnt_lock, flags);
	if (delayed_boost)
		exynos_hpgov.slack_start_time = ktime_get();

	boost_cnt = --exynos_hpgov.boost_cnt;

	if (boost_cnt == 0) {
		if (delayed_boost || ktime_before(ktime_get(),
		ktime_add_ms(exynos_hpgov.slack_start_time,
			exynos_hpgov.dual_change_ms - 1)))
			exynos_hpgov_slack_timer_start();
		else
			exynos_hpgov_big_mode_update(BIG_DUAL_MODE);
	}
	spin_unlock_irqrestore(&hpgov_boost_cnt_lock, flags);
}

static int exynos_hpgov_update_governor(enum hpgov_event event, int req_cpu_max, int req_cpu_min)
{
	int ret = 0;
	unsigned long flags;

	spin_lock_irqsave(&hpgov_lock, flags);

	switch(event) {
	case HPGOV_BIG_MODE_UPDATED:
	case HPGOV_SLACK_TIMER_EXPIRED:
		exynos_hpgov.use_fast_hp = 1;

	default:
		break;
	}

	if (req_cpu_max == exynos_hpgov.cur_cpu_max)
		req_cpu_max = 0;

	if (req_cpu_min == exynos_hpgov.cur_cpu_min)
		req_cpu_min = 0;

	if (!req_cpu_max && !req_cpu_min) {
		spin_unlock_irqrestore(&hpgov_lock, flags);
		return ret;
	}

	trace_exynos_hpgov_governor_update(event, req_cpu_max, req_cpu_min);
	if (req_cpu_max)
		exynos_hpgov.cur_cpu_max = req_cpu_max;
	if (req_cpu_min)
		exynos_hpgov.cur_cpu_min = req_cpu_min;

	spin_unlock_irqrestore(&hpgov_lock, flags);

	exynos_hpgov.hp_state = HP_STATE_SCHEDULED;
	wake_up(&exynos_hpgov.wait_hpq);

	return ret;
}

static int exynos_hpgov_do_update_governor(void *data)
{
	struct hpgov_data *pdata = (struct hpgov_data *)data;
	unsigned long flags;
	enum hpgov_event event;
	int req_cpu_max;
	int req_cpu_min;

	while (1) {
		wait_event(exynos_hpgov.wait_q, pdata->event || kthread_should_stop());
		if (kthread_should_stop())
			break;

		spin_lock_irqsave(&hpgov_lock, flags);
		event = pdata->event;
		req_cpu_max = pdata->req_cpu_max;
		req_cpu_min = pdata->req_cpu_min;
		pdata->event = 0;
		pdata->req_cpu_max = 0;
		pdata->req_cpu_min = 0;
		spin_unlock_irqrestore(&hpgov_lock, flags);

		exynos_hpgov_update_governor(event, req_cpu_max, req_cpu_min);
	}
	return 0;
}

static int exynos_hpgov_do_hotplug(void *data)
{
	int *event = (int *)data;
	unsigned long flags;

	int cpu_max;
	int cpu_min;
	long use_fast_hp;
	int last_max = 0;
	int last_min = 0;

	while (1) {
		wait_event(exynos_hpgov.wait_hpq, *event || kthread_should_stop());
		if (kthread_should_stop())
			break;

restart:
		exynos_hpgov.hp_state = HP_STATE_IN_PROGRESS;

		spin_lock_irqsave(&hpgov_lock, flags);
		cpu_max = exynos_hpgov.cur_cpu_max;
		cpu_min = exynos_hpgov.cur_cpu_min;
		use_fast_hp = exynos_hpgov.use_fast_hp;
		spin_unlock_irqrestore(&hpgov_lock, flags);

		if (cpu_max != last_max) {
			pm_qos_update_request_param(&hpgov_max_pm_qos,
						cpu_max, (void *)use_fast_hp);
			last_max = cpu_max;
		}

		if (cpu_min != last_min) {
			pm_qos_update_request_param(&hpgov_min_pm_qos,
						cpu_min, (void *)use_fast_hp);
			last_min = cpu_min;
		}

		exynos_hpgov.hp_state = HP_STATE_WAITING;
		if (last_max != exynos_hpgov.cur_cpu_max ||
			last_min != exynos_hpgov.cur_cpu_min)
			goto restart;
	}

	return 0;
}

static int exynos_hpgov_set_enabled(int enable)
{
	int ret = 0;
	static int last_enable = 0;

	enable = (enable > 0) ? 1 : 0;
	if (last_enable == enable)
		return ret;

	last_enable = enable;

	if (enable) {
		exynos_hpgov.task = kthread_create(exynos_hpgov_do_update_governor,
					      &exynos_hpgov.data, "exynos_hpgov");
		if (IS_ERR(exynos_hpgov.task))
			return -EFAULT;

		set_user_nice(exynos_hpgov.task, MIN_NICE);
#ifdef CONFIG_SCHED_HMP
		set_cpus_allowed(exynos_hpgov.task, hmp_fast_cpu_mask);
#else
		kthread_bind(exynos_hpgov.task, 0);
#endif
		wake_up_process(exynos_hpgov.task);

		exynos_hpgov.hptask = kthread_create(exynos_hpgov_do_hotplug,
						&exynos_hpgov.hp_state, "exynos_hp");
		if (IS_ERR(exynos_hpgov.hptask)) {
			kthread_stop(exynos_hpgov.task);
			return -EFAULT;
		}

		set_user_nice(exynos_hpgov.hptask, MIN_NICE);
#ifdef CONFIG_SCHED_HMP
		set_cpus_allowed(exynos_hpgov.hptask, hmp_fast_cpu_mask);
#else
		kthread_bind(exynos_hpgov.hptask, 0);
#endif
		wake_up_process(exynos_hpgov.hptask);

		exynos_hpgov.enabled = 1;
		smp_wmb();
		start_slack_timer();
	} else {
		kthread_stop(exynos_hpgov.hptask);
		kthread_stop(exynos_hpgov.task);

		exynos_hpgov.enabled = 0;
		smp_wmb();

		pm_qos_update_request(&hpgov_max_pm_qos, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
		pm_qos_update_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
		exynos_hpgov.cur_cpu_max = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
		exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	}

	return ret;
}

int hpgov_default_level(void)
{
	/* FIXME: need to use information from cpufreq */
	return 2;
}

static int exynos_hpgov_set_dual_change_ms(int val)
{
	if (!(val >= 0))
		return -EINVAL;

	exynos_hpgov.dual_change_ms = val;
	return 0;
}

#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
/* Currently max value of to_quad_ratio is 100. */
/* TODO: Change max value for ratio to 1024 (permille) */
static int exynos_hpgov_set_to_quad_ratio(int val)
{
	if ((val > 100) || (val < 0))
		return -EINVAL;

	*exynos_hpgov.to_quad_ratio = val;
	hp_sysload_param_calc();

	/* XXX: Does not required hp_event_update_rq_load()? */

	return 0;
}

/* Currently max value of to_dual_ratio is 100. */
/* TODO: Change max value for ratio to 1024 (permille) */
static int exynos_hpgov_set_to_dual_ratio(int val)
{
	if ((val > 100) || (val < 0))
		return -EINVAL;

	*exynos_hpgov.to_dual_ratio = val;
	hp_sysload_param_calc();

	/* XXX: Does not required hp_event_update_rq_load()? */

	return 0;
}

/* Currently max value of lit_mult_ratio is 200. */
/* TODO: Change max value for ratio to 2048 (permille) */
static int exynos_hpgov_set_lit_mult_ratio(int val)
{
	if ((val > 200) || (val < 0))
		return -EINVAL;

	*exynos_hpgov.lit_mult_ratio = val;
	hp_sysload_param_calc();

	/* XXX: Does not required hp_event_update_rq_load()? */

	return 0;
}
#endif

#define HPGOV_PARAM(_name, _param) \
static ssize_t exynos_hpgov_attr_##_name##_show(struct kobject *kobj, \
			struct kobj_attribute *attr, char *buf) \
{ \
	return snprintf(buf, PAGE_SIZE, "%d\n", _param); \
} \
static ssize_t exynos_hpgov_attr_##_name##_store(struct kobject *kobj, \
		struct kobj_attribute *attr, const char *buf, size_t count) \
{ \
	int ret = 0; \
	int val; \
	int old_val; \
	mutex_lock(&exynos_hpgov.attrib_lock); \
	ret = kstrtoint(buf, 10, &val); \
	if (ret) { \
		pr_err("Invalid input %s for %s %d\n", \
				buf, __stringify(_name), ret);\
		return 0; \
	} \
	old_val = _param; \
	ret = exynos_hpgov_set_##_name(val); \
	if (ret) { \
		pr_err("Error %d returned when setting param %s to %d\n",\
				ret, __stringify(_name), val); \
		_param = old_val; \
	} \
	mutex_unlock(&exynos_hpgov.attrib_lock); \
	return count; \
}

#define HPGOV_RW_ATTRIB(i, _name) \
	exynos_hpgov.attrib._name.attr.name = __stringify(_name); \
	exynos_hpgov.attrib._name.attr.mode = S_IRUGO | S_IWUSR; \
	exynos_hpgov.attrib._name.show = exynos_hpgov_attr_##_name##_show; \
	exynos_hpgov.attrib._name.store = exynos_hpgov_attr_##_name##_store; \
	exynos_hpgov.attrib.attrib_group.attrs[i] = &exynos_hpgov.attrib._name.attr;

HPGOV_PARAM(enabled, exynos_hpgov.enabled);
HPGOV_PARAM(dual_change_ms, exynos_hpgov.dual_change_ms);
#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
HPGOV_PARAM(to_quad_ratio, *exynos_hpgov.to_quad_ratio);
HPGOV_PARAM(to_dual_ratio, *exynos_hpgov.to_dual_ratio);
HPGOV_PARAM(lit_mult_ratio, *exynos_hpgov.lit_mult_ratio);
#endif

static void hpgov_boot_enable(struct work_struct *work);
static DECLARE_DELAYED_WORK(hpgov_boot_work, hpgov_boot_enable);
static void hpgov_boot_enable(struct work_struct *work)
{
	if (exynos_hpgov_set_enabled(1))
		schedule_delayed_work_on(0, &hpgov_boot_work, msecs_to_jiffies(RETRY_BOOT_ENABLE_MS));
}

static int __init exynos_hpgov_init(void)
{
	int ret = 0;
	const int attr_count = 5;
	int i_attr = attr_count;

	hrtimer_init(&exynos_hpgov.slack_timer, CLOCK_MONOTONIC,
			HRTIMER_MODE_PINNED);

	exynos_hpgov.slack_timer.function = exynos_hpgov_slack_timer;

	mutex_init(&exynos_hpgov.attrib_lock);
	init_waitqueue_head(&exynos_hpgov.wait_q);
	init_waitqueue_head(&exynos_hpgov.wait_hpq);
	init_irq_work(&exynos_hpgov.update_irq_work, exynos_hpgov_irq_work);
	init_irq_work(&exynos_hpgov.start_slack_timer_irq_work, slack_timer_irq_work);

	exynos_hpgov.attrib.attrib_group.attrs =
		kzalloc(attr_count * sizeof(struct attribute *), GFP_KERNEL);
	if (!exynos_hpgov.attrib.attrib_group.attrs) {
		ret = -ENOMEM;
		goto done;
	}

	HPGOV_RW_ATTRIB(attr_count - (i_attr--), enabled);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), dual_change_ms);
#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), to_quad_ratio);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), to_dual_ratio);
	HPGOV_RW_ATTRIB(attr_count - (i_attr--), lit_mult_ratio);
#endif

#if defined(CONFIG_HP_EVENT_HMP_SYSTEM_LOAD)
	exynos_hpgov.to_quad_ratio = &hp_sysload_to_quad_ratio;
	exynos_hpgov.to_dual_ratio = &hp_sysload_to_dual_ratio;
	exynos_hpgov.lit_mult_ratio = &hp_little_multiplier_ratio;
#endif

	exynos_hpgov.attrib.attrib_group.name = "governor";
	ret = sysfs_create_group(exynos_cpu_hotplug_kobj(), &exynos_hpgov.attrib.attrib_group);
	if (ret)
		pr_err("Unable to create sysfs objects :%d\n", ret);

	exynos_hpgov.dual_change_ms = DEFAULT_DUAL_CHANGE_MS;
	exynos_hpgov.cur_cpu_max = PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE;
	exynos_hpgov.cur_cpu_min = PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE;

	pm_qos_add_request(&hpgov_max_pm_qos, PM_QOS_CPU_ONLINE_MAX, PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);
	pm_qos_add_request(&hpgov_min_pm_qos, PM_QOS_CPU_ONLINE_MIN, PM_QOS_CPU_ONLINE_MIN_DEFAULT_VALUE);

	schedule_delayed_work_on(0, &hpgov_boot_work, msecs_to_jiffies(DEFAULT_BOOT_ENABLE_MS));

done:
	return ret;
}
late_initcall(exynos_hpgov_init);
