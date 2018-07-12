/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/device.h>
#include <linux/kobject.h>
#include <linux/cpuidle.h>
#include <linux/slab.h>
#include <linux/cpuidle_profiler.h>

#include <asm/page.h>
#include <asm/cputype.h>
#include <asm/smp_plat.h>
#include <asm/topology.h>

#include <soc/samsung/exynos-powermode.h>

static bool profile_started;

/*
 * "profile_info" contains profiling data for per cpu idle state which
 * declared in cpuidle driver.
 */
static DEFINE_PER_CPU(struct cpuidle_profile_info, profile_info);

/*
 * "cpd_info" contains profiling data for CPD(Cluster Power Down) which
 * is subordinate to C2 state idle. Each cluster has one element in
 * cpd_info[].
 */
static struct cpuidle_profile_info cpd_info[MAX_CLUSTER];

/*
 * "lpm_info" contains profiling data for LPM(Low Power Mode). LPM
 * comprises many system power mode such AFTR, ALPA.
 */
static struct cpuidle_profile_info lpm_info;

/*
 * "idle_ip_pending" contains which blocks to enter system power mode.
 * It is used by only SICD/SICD_CPD and ALPA.
 */
static int idle_ip_pending[NUM_SYS_POWERDOWN][NUM_IDLE_IP][IDLE_IP_REG_SIZE];

/*
 * "idle_ip_list" contains IP name in IDLE_IP
 */
char *idle_ip_list[NUM_IDLE_IP][IDLE_IP_REG_SIZE];

/************************************************************************
 *                              Profiling                               *
 ************************************************************************/
/*
 * If cpu does not enter idle state, cur_state has -EINVAL. By this,
 * profiler can be aware of cpu state.
 */
#define state_entered(state)	(((int)state < (int)0) ? 0 : 1)

static void enter_idle_state(struct cpuidle_profile_info *info,
					int state, ktime_t now)
{
	if (state_entered(info->cur_state))
		return;

	info->cur_state = state;
	info->last_entry_time = now;

	info->usage[state].entry_count++;
}

static void exit_idle_state(struct cpuidle_profile_info *info,
					int state, ktime_t now,
					int earlywakeup)
{
	s64 diff;

	if (!state_entered(info->cur_state))
		return;

	info->cur_state = -EINVAL;

	if (earlywakeup) {
		/*
		 * If cpu cannot enter power mode, residency time
		 * should not be updated.
		 */
		info->usage[state].early_wakeup_count++;
		return;
	}

	diff = ktime_to_us(ktime_sub(now, info->last_entry_time));
	info->usage[state].time += diff;
}

/*
 * C2 subordinate state such as CPD and SICD can be entered by many cpus.
 * The variables which contains these idle states need to keep
 * synchronization.
 */
static DEFINE_SPINLOCK(substate_lock);

void __cpuidle_profile_start(int cpu, int state, int substate)
{
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);
	ktime_t now = ktime_get();

	/*
	 * Start to profile idle state. profile_info is per-CPU variable,
	 * it does not need to synchronization.
	 */
	enter_idle_state(info, state, now);

	/* Start to profile subordinate idle state. */
	if (substate) {
		spin_lock(&substate_lock);

		/*
		 * SICD is a system power mode and also C2 subordinate
		 * state becuase it is entered by C2 entry cpu. On this
		 * count, in case of SICD or SICD_CPD, profiler updates
		 * lpm_info although idle state is C2.
		 */
		if (state == PROFILE_C2) {
			switch (substate) {
			case C2_CPD:
				info = &cpd_info[to_cluster(cpu)];
				enter_idle_state(info, 0, now);
				break;
			case C2_SICD:
				info = &lpm_info;
				enter_idle_state(info, LPM_SICD, now);
				break;
			case C2_SICD_CPD:
				info = &cpd_info[to_cluster(cpu)];
				enter_idle_state(info, 0, now);

				info = &lpm_info;
				enter_idle_state(info, LPM_SICD_CPD, now);
				break;
			case C2_SICD_AUD:
				info = &lpm_info;
				enter_idle_state(info, LPM_SICD_AUD, now);
				break;
			}
		} else if (state == PROFILE_LPM)
			enter_idle_state(&lpm_info, substate, now);

		spin_unlock(&substate_lock);
	}
}

void cpuidle_profile_start(int cpu, int state, int substate)
{
	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	__cpuidle_profile_start(cpu, state, substate);
}

void cpuidle_profile_start_no_substate(int cpu, int state)
{
	cpuidle_profile_start(cpu, state, 0);
}

void __cpuidle_profile_finish(int cpu, int earlywakeup)
{
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);
	int state = info->cur_state;
	ktime_t now = ktime_get();

	exit_idle_state(info, state, now, earlywakeup);

	spin_lock(&substate_lock);

	/*
	 * Subordinate state can be wakeup by many cpus. We cannot predict
	 * which cpu wakeup from idle state, profiler always try to update
	 * residency time of subordinate state. To avoid duplicate updating,
	 * exit_idle_state() checks validation.
	 */
	if (has_sub_state(state)) {
		info = &cpd_info[to_cluster(cpu)];
		exit_idle_state(info, info->cur_state, now, earlywakeup);

		info = &lpm_info;
		exit_idle_state(info, info->cur_state, now, earlywakeup);
	}

	spin_unlock(&substate_lock);
}

void cpuidle_profile_finish(int cpu, int earlywakeup)
{
	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	__cpuidle_profile_finish(cpu, earlywakeup);
}

void cpuidle_profile_finish_no_earlywakeup(int cpu)
{
	cpuidle_profile_finish(cpu, 0);
}

/*
 * Before system enters system power mode, it checks idle-ip status. Its
 * status is conveyed to cpuidle_profile_collect_idle_ip().
 */
void cpuidle_profile_collect_idle_ip(int mode, int index,
						unsigned int idle_ip)
{
	int i;

	/*
	 * Return if profile is not started
	 */
	if (!profile_started)
		return;

	for (i = 0; i < IDLE_IP_REG_SIZE; i++) {
		/*
		 * If bit of idle_ip has 1, IP corresponding to its bit
		 * is not idle.
		 */
		if (idle_ip & (1 << i))
			idle_ip_pending[mode][index][i]++;
	}
}

/************************************************************************
 *                              Show result                             *
 ************************************************************************/
static ktime_t profile_start_time;
static ktime_t profile_finish_time;
static s64 profile_time;

static int calculate_percent(s64 residency)
{
	if (!residency)
		return 0;

	residency *= 100;
	do_div(residency, profile_time);

	return residency;
}

static unsigned long long sum_idle_time(int cpu)
{
	int i;
	unsigned long long idle_time = 0;
	struct cpuidle_profile_info *info = &per_cpu(profile_info, cpu);

	for (i = 0; i < info->state_count; i++)
		idle_time += info->usage[i].time;

	return idle_time;
}

static int total_idle_ratio(int cpu)
{
	return calculate_percent(sum_idle_time(cpu));
}

static void show_result(void)
{
	int i, idle_ip, bit, cpu;
	struct cpuidle_profile_info *info;
	int state_count;

	pr_info("#############################################################\n");
	pr_info("Profiling Time : %lluus\n", profile_time);

	pr_info("\n");

	pr_info("[total idle ratio]\n");
	pr_info("#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		pr_info("cpu%d %10lluus   %3u%%\n", cpu,
			sum_idle_time(cpu), total_idle_ratio(cpu));

	pr_info("\n");

	/*
	 * All profile_info has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(profile_info, 0).state_count;

	for (i = 0; i < state_count; i++) {
		pr_info("[state%d]\n", i);
		pr_info("#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(profile_info, cpu);
			pr_info("cpu%d   %5u   %5u   %10lluus   %3u%%\n", cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				info->usage[i].time,
				calculate_percent(info->usage[i].time));
		}

		pr_info("\n");
	}

	pr_info("[CPD] - Cluster Power Down\n");
	pr_info("#cluster     #entry   #early      #time     #ratio\n");
	for_each_cluster(i) {
		pr_info("cl_%s   %5u   %5u   %10lluus    %3u%%\n",
			i == to_cluster(0) ? "boot   " : "nonboot",
			cpd_info[i].usage->entry_count,
			cpd_info[i].usage->early_wakeup_count,
			cpd_info[i].usage->time,
			calculate_percent(cpd_info[i].usage->time));
	}

	pr_info("\n");

	pr_info("[LPM] - Low Power Mode\n");
	pr_info("#mode        #entry   #early      #time     #ratio\n");
	for_each_syspower_mode(i) {
		pr_info("%-9s    %5u   %5u   %10lluus    %3u%%\n",
			get_sys_powerdown_str(i),
			lpm_info.usage[i].entry_count,
			lpm_info.usage[i].early_wakeup_count,
			lpm_info.usage[i].time,
			calculate_percent(lpm_info.usage[i].time));
	}

	pr_info("\n");

	pr_info("[LPM blockers]\n");
	for_each_syspower_mode(i) {
		for_each_idle_ip(idle_ip) {
			for (bit = 0; bit < IDLE_IP_REG_SIZE; bit++) {
				if (idle_ip_pending[i][idle_ip][bit])
					pr_info("%s block by IDLE_IP%d[%d](%s, count = %d)\n",
						get_sys_powerdown_str(i),
						idle_ip, bit, idle_ip_list[idle_ip][bit],
						idle_ip_pending[i][idle_ip][bit]);
			}
		}
	}

	pr_info("\n");
	pr_info("#############################################################\n");
}

/************************************************************************
 *                            Profile control                           *
 ************************************************************************/
static void clear_time(ktime_t *time)
{
	time->tv64 = 0;
}

static void clear_profile_info(struct cpuidle_profile_info *info)
{
	memset(info->usage, 0,
		sizeof(struct cpuidle_profile_state_usage) * info->state_count);

	clear_time(&info->last_entry_time);
	info->cur_state = -EINVAL;
}

static void reset_profile_record(void)
{
	int i;

	clear_time(&profile_start_time);
	clear_time(&profile_finish_time);

	for_each_possible_cpu(i)
		clear_profile_info(&per_cpu(profile_info, i));

	for_each_cluster(i)
		clear_profile_info(&cpd_info[i]);

	clear_profile_info(&lpm_info);

	memset(idle_ip_pending, 0,
		NUM_SYS_POWERDOWN * NUM_IDLE_IP * IDLE_IP_REG_SIZE * sizeof(int));
}

static void call_cpu_start_profile(void *p) {};
static void call_cpu_finish_profile(void *p) {};

static void cpuidle_profile_main_start(void)
{
	if (profile_started) {
		pr_err("cpuidle profile is ongoing\n");
		return;
	}

	reset_profile_record();
	profile_start_time = ktime_get();

	profile_started = 1;

	/* Wakeup all cpus and clear own profile data to start profile */
	preempt_disable();
	smp_call_function(call_cpu_start_profile, NULL, 1);
	preempt_enable();

	pr_info("cpuidle profile start\n");
}

static void cpuidle_profile_main_finish(void)
{
	if (!profile_started) {
		pr_err("CPUIDLE profile does not start yet\n");
		return;
	}

	pr_info("cpuidle profile finish\n");

	/* Wakeup all cpus to update own profile data to finish profile */
	preempt_disable();
	smp_call_function(call_cpu_finish_profile, NULL, 1);
	preempt_enable();

	profile_started = 0;

	profile_finish_time = ktime_get();
	profile_time = ktime_to_us(ktime_sub(profile_finish_time,
						profile_start_time));

	show_result();
}

/*********************************************************************
 *                          Sysfs interface                          *
 *********************************************************************/
static ssize_t show_sysfs_result(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;
	int i, cpu, idle_ip, bit;
	struct cpuidle_profile_info *info;
	int state_count;

	if (profile_started) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
		return ret;
	}

	if (profile_time == 0) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profiler has not started yet\n");
		return ret;
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"Profiling Time : %lluus\n", profile_time);

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[total idle ratio]\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cpu      #time    #ratio\n");
	for_each_possible_cpu(cpu)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cpu%d %10lluus   %3u%%\n",
			cpu, sum_idle_time(cpu), total_idle_ratio(cpu));

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	/*
	 * All profile_info has same state_count. As a representative,
	 * cpu0's is used.
	 */
	state_count = per_cpu(profile_info, 0).state_count;

	for (i = 0; i < state_count; i++) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"[state%d]\n", i);
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"#cpu   #entry   #early      #time    #ratio\n");
		for_each_possible_cpu(cpu) {
			info = &per_cpu(profile_info, cpu);
			ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"cpu%d   %5u   %5u   %10lluus   %3u%%\n",
				cpu,
				info->usage[i].entry_count,
				info->usage[i].early_wakeup_count,
				info->usage[i].time,
				calculate_percent(info->usage[i].time));
		}

		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"\n");
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[CPD] - Cluster Power Down\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#cluster     #entry   #early      #time     #ratio\n");
	for_each_cluster(i) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"cl_%s   %5u   %5u   %10lluus    %3u%%\n",
			i == to_cluster(0) ? "boot   " : "nonboot",
			cpd_info[i].usage->entry_count,
			cpd_info[i].usage->early_wakeup_count,
			cpd_info[i].usage->time,
			calculate_percent(cpd_info[i].usage->time));
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[LPM] - Low Power Mode\n");
	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#mode        #entry   #early      #time     #ratio\n");
	for_each_syspower_mode(i) {
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
			"%-9s    %5u   %5u   %10lluus    %3u%%\n",
			get_sys_powerdown_str(i),
			lpm_info.usage[i].entry_count,
			lpm_info.usage[i].early_wakeup_count,
			lpm_info.usage[i].time,
			calculate_percent(lpm_info.usage[i].time));
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"\n");

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"[LPM blockers]\n");
	for_each_syspower_mode(i) {
		for_each_idle_ip(idle_ip) {
			for (bit = 0; bit < IDLE_IP_REG_SIZE; bit++) {
				if (idle_ip_pending[i][idle_ip][bit])
					ret += snprintf(buf + ret, PAGE_SIZE -ret,
						"%s block by IDLE_IP%d[%d](%s, count = %d)\n",
						get_sys_powerdown_str(i),
						idle_ip, bit, idle_ip_list[idle_ip][bit],
						idle_ip_pending[i][idle_ip][bit]);
			}
		}
	}

	ret += snprintf(buf + ret, PAGE_SIZE - ret,
		"#############################################################\n");

	return ret;
}

static ssize_t show_cpuidle_profile(struct kobject *kobj,
				     struct kobj_attribute *attr,
				     char *buf)
{
	int ret = 0;

	if (profile_started)
		ret += snprintf(buf + ret, PAGE_SIZE - ret,
				"CPUIDLE profile is ongoing\n");
	else
		ret = show_sysfs_result(kobj, attr, buf);


	return ret;
}

static ssize_t store_cpuidle_profile(struct kobject *kobj,
				      struct kobj_attribute *attr,
				      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%1d", &input))
		return -EINVAL;

	if (!!input)
		cpuidle_profile_main_start();
	else
		cpuidle_profile_main_finish();

	return count;
}

static struct kobj_attribute cpuidle_profile_attr =
	__ATTR(profile, 0644, show_cpuidle_profile, store_cpuidle_profile);

static struct attribute *cpuidle_profile_attrs[] = {
	&cpuidle_profile_attr.attr,
	NULL,
};

static const struct attribute_group cpuidle_profile_group = {
	.attrs = cpuidle_profile_attrs,
};


/*********************************************************************
 *                   Initialize cpuidle profiler                     *
 *********************************************************************/
static void __init cpuidle_profile_info_init(struct cpuidle_profile_info *info,
						int state_count)
{
	int size = sizeof(struct cpuidle_profile_state_usage) * state_count;

	info->state_count = state_count;
	info->usage = kmalloc(size, GFP_KERNEL);
	if (!info->usage) {
		pr_err("%s:%d: Memory allocation failed\n", __func__, __LINE__);
		return;
	}
}

void __init cpuidle_profile_register(struct cpuidle_driver *drv)
{
	int idle_state_count = drv->state_count;
	int i;

	/* Initialize each cpuidle state information */
	for_each_possible_cpu(i)
		cpuidle_profile_info_init(&per_cpu(profile_info, i),
						idle_state_count);

	/* Initiailize CPD(Cluster Power Down) information */
	for_each_cluster(i)
		cpuidle_profile_info_init(&cpd_info[i], 1);

	/* Initiailize LPM(Low Power Mode) information */
	cpuidle_profile_info_init(&lpm_info, NUM_SYS_POWERDOWN);
}

static int __init cpuidle_profile_init(void)
{
	struct class *class;
	struct device *dev;

	class = class_create(THIS_MODULE, "cpuidle");
	dev = device_create(class, NULL, 0, NULL, "cpuidle_profiler");

	if (sysfs_create_group(&dev->kobj, &cpuidle_profile_group)) {
		pr_err("CPUIDLE Profiler : error to create sysfs\n");
		return -EINVAL;
	}

	exynos_get_idle_ip_list(idle_ip_list);

	return 0;
}
late_initcall(cpuidle_profile_init);
