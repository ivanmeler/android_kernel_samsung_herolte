/*
 *  ARM Intelligent Power Allocation
 *
 *  Copyright (C) 2013 ARM Limited.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/uaccess.h>

#include <linux/cpufreq.h>
#include <linux/cpumask.h>
#include <linux/debugfs.h>
#include <linux/fs.h>
#include <linux/ipa.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/sysfs.h>
#include <linux/workqueue.h>
#include <linux/math64.h>
#include <linux/of.h>

#include <soc/samsung/cpufreq.h>

#include "platform_tables.h"
#include "../cpufreq/cpu_load_metric.h"

#define MHZ_TO_KHZ(freq) ((freq) * 1000)
#define KHZ_TO_MHZ(freq) ((freq) / 1000)

#define NUM_CLUSTERS CL_END

struct mali_utilisation_stats {
	int utilisation;
	int norm_utilisation;
	int freq_for_norm;
};

struct mali_debug_utilisation_stats {
	struct mali_utilisation_stats s;
	u32 time_busy;
	u32 time_idle;
	int time_tick;
};

struct cpu_power_info {
	unsigned int load[4];
	unsigned int freq;
	cluster_type cluster;
	unsigned int temp;
};

#define WEIGHT_SHIFT 8

struct ctlr {
	int k_po;
	int k_pu;
	int mult;
	int k_i;
	int k_d;
	int err_integral;
	int integral_reset_value;
	int err_prev;
	u32 feed_forward;
	int integral_cutoff;
	int integral_reset_threshold;
};

struct ipa_config {
	u32 control_temp;
	u32 temp_threshold;
	u32 enabled;
	u32 tdp;
	u32 boost;
	u32 ros_power;
	u32 little_weight;
	u32 big_weight;
	u32 gpu_weight;
	u32 little_max_power;
	u32 big_max_power;
	u32 gpu_max_power;
	u32 soc_max_power;
	int hotplug_out_threshold;
	int hotplug_in_threshold;
	u32 cores_out;
	u32 enable_ctlr;
	struct ctlr ctlr;
};

#ifdef CONFIG_OF
static struct ipa_config default_config;
#else
static struct ipa_config default_config = {
	.control_temp = 81,
	.temp_threshold = 30,
	.enabled = 1,
	.tdp = 3500,
	.boost = 1,
	.ros_power = 500, /* rest of soc */
	.little_weight = 4 << WEIGHT_SHIFT,
	.big_weight = 1 << WEIGHT_SHIFT,
	.gpu_weight = 1024,
	.little_max_power = 250*4,
	.big_max_power = 1638 * 4,
	.gpu_max_power = 3110,
	.enable_ctlr = 1,
	.hotplug_out_threshold = 10,
	.hotplug_in_threshold = 2,
	.ctlr = {
		.mult = 2,
		.k_i = 1,
		.k_d = 0,
		.feed_forward = 1,
		.integral_reset_value = 0,
		.integral_cutoff = 0,
		.integral_reset_threshold = 10,
	},
};
#endif

static BLOCKING_NOTIFIER_HEAD(thermal_notifier_list);

void gpu_ipa_dvfs_get_utilisation_stats(struct mali_debug_utilisation_stats *stats);
int gpu_ipa_dvfs_max_lock(int clock);
void gpu_ipa_dvfs_max_unlock(void);
int kbase_platform_dvfs_freq_to_power(int freq);
int kbase_platform_dvfs_power_to_freq(int power);
unsigned int get_power_value(struct cpu_power_info *power_info);
int get_ipa_dvfs_max_freq(void);
int get_real_max_freq(cluster_type cluster);

#define ARBITER_ACTIVE_PERIOD_MSEC 	100
#define ARBITER_PASSIVE_PERIOD_MSEC	1000

static int nr_big_coeffs, nr_little_coeffs;

static struct arbiter_data
{
	struct ipa_config config;
	struct ipa_sensor_conf *sensor;
	struct dentry *debugfs_root;
	struct kobject *kobj;

	bool active;
	bool initialised;

	struct mali_debug_utilisation_stats mali_stats;
	int gpu_freq, gpu_load;

	int cpu_freqs[NUM_CLUSTERS][NR_CPUS];
	struct cluster_stats cl_stats[NUM_CLUSTERS];

	int max_sensor_temp;
	int skin_temperature, cp_temperature;

	int gpu_freq_limit, cpu_freq_limits[NUM_CLUSTERS];

	struct delayed_work work;
} arbiter_data = {
	.initialised = false,
};

static void setup_cpusmasks(struct cluster_stats *cl_stats)
{
	if (!zalloc_cpumask_var(&cl_stats[CL_ONE].mask, GFP_KERNEL))
		pr_warn("unable to allocate cpumask");
	if (!zalloc_cpumask_var(&cl_stats[CL_ZERO].mask, GFP_KERNEL))
		pr_warn("unable to allocate cpumask");

	cpumask_setall(cl_stats[CL_ONE].mask);
	if (strlen(CONFIG_HMP_FAST_CPU_MASK))
		cpulist_parse(CONFIG_HMP_FAST_CPU_MASK, cl_stats[CL_ONE].mask);
	else
		pr_warn("IPA: No CONFIG_HMP_FAST_CPU_MASK found.\n");

	cpumask_andnot(cl_stats[CL_ZERO].mask, cpu_present_mask, cl_stats[CL_ONE].mask);
}

#define FRAC_BITS 8
#define int_to_frac(x) ((x) << FRAC_BITS)
#define frac_to_int(x) ((x) >> FRAC_BITS)

static inline s64 mul_frac(s64 x, s64 y)
{
	/* note returned number is still fractional, hence one shift not two*/
	return (x * y) >> FRAC_BITS;
}

static inline int div_frac(int x, int y)
{
	return div_s64((s64)x << FRAC_BITS, y);
}

static void reset_controller(struct ctlr *ctlr_config)
{
	ctlr_config->err_integral = ctlr_config->integral_reset_value;
}

static void init_controller_coeffs(struct ipa_config *config)
{
	config->ctlr.k_po = int_to_frac(config->tdp) / config->temp_threshold;
	config->ctlr.k_pu = int_to_frac(config->ctlr.mult * config->tdp) / config->temp_threshold;
}

static void reset_arbiter_configuration(struct ipa_config *config)
{
	memcpy(config, &default_config, sizeof(*config));

	init_controller_coeffs(config);
	config->ctlr.k_i = int_to_frac(default_config.ctlr.k_i);
	config->ctlr.k_d = int_to_frac(default_config.ctlr.k_d);

	config->ctlr.integral_reset_value =
		int_to_frac(default_config.ctlr.integral_reset_value);
	reset_controller(&config->ctlr);
}

static int queue_arbiter_poll(void)
{
	int cpu, ret = 0;
	unsigned int period = (arbiter_data.active == true) ? ARBITER_ACTIVE_PERIOD_MSEC : ARBITER_PASSIVE_PERIOD_MSEC;

	if (arbiter_data.config.enabled) {
		cpu = cpumask_any(arbiter_data.cl_stats[CL_ZERO].mask);
		ret = queue_delayed_work_on(cpu, system_freezable_wq,
					&arbiter_data.work,
					msecs_to_jiffies(period));
	} else {
		arbiter_data.active = false;
		cancel_delayed_work(&arbiter_data.work);
	}

	return ret;
}

static int get_humidity_sensor_temp(void)
{
	return 0;
}

#ifdef CONFIG_SENSORS_SEC_THERMISTOR
extern int sec_therm_get_ap_temperature(void);
#else
/* Placeholder function, as sensor not present on SMDK */
static int sec_therm_get_ap_temperature(void)
{
	/* Assumption that 80 degrees at SoC = 68 degrees at AP = 55 degrees at skin */
	return (arbiter_data.max_sensor_temp - 12) * 10;
}
#endif

static void arbiter_set_gpu_freq_limit(int freq)
{
	if (arbiter_data.gpu_freq_limit == freq)
		return;

	arbiter_data.gpu_freq_limit = freq;

	gpu_ipa_dvfs_max_lock(freq);

	/*Mali DVFS code will apply changes on next DVFS tick (100ms)*/
}

static int thermal_call_chain(int freq, int idx)
{
	struct thermal_limits limits = {
		.max_freq = freq,
		.cur_freq = arbiter_data.cl_stats[idx].freq,
	};

	cpumask_copy(&limits.cpus, arbiter_data.cl_stats[idx].mask);

	return blocking_notifier_call_chain(&thermal_notifier_list, THERMAL_NEW_MAX_FREQ, &limits);
}

static bool is_cluster1_hotplugged(void)
{
	return cpumask_empty(cpu_coregroup_mask(4));
}

static void arbiter_set_cpu_freq_limit(int freq, int idx)
{
	int i, cpu, max_freq, currT;
	int little_limit_temp = 0, allowed_diff_temp = 10;
	struct ipa_config *config = &arbiter_data.config;

	/* if (arbiter_data.cpu_freq_limits[idx] == freq) */
	/*	return; */

	arbiter_data.cpu_freq_limits[idx] = freq;

	i = 0;
	max_freq = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[idx].mask) {
		if (arbiter_data.cpu_freqs[idx][i] > max_freq)
			max_freq = arbiter_data.cpu_freqs[idx][i];

		i++;
	}

	/* To ensure the minimum performance, */
	/* little's freq is not limited until little_limit_temp. */

	currT = arbiter_data.skin_temperature / 10;
	little_limit_temp = config->control_temp + config->hotplug_out_threshold + allowed_diff_temp;

	if (is_cluster1_hotplugged() && (idx == CL_ZERO) && (currT < little_limit_temp))
		freq = get_real_max_freq(CL_ZERO);

	ipa_set_clamp(cpumask_any(arbiter_data.cl_stats[idx].mask), freq, max_freq);

	thermal_call_chain(freq, idx);
}

static int freq_to_power(int freq, int max, struct coefficients *coeff)
{
	int i = 0;
	while (i < (max - 1)) {
		if (coeff[i].frequency == freq)
			break;
		i++;
	}

	return coeff[i].power;
}

static int power_to_freq(int power, int max, struct coefficients *coeff)
{
	int i = 0;
	while (i < (max - 1)) {
		if (coeff[i].power < power)
			break;
		i++;
	}

	return coeff[i].frequency;
}

static int get_cpu_freq_limit(cluster_type cl, int power_out, int util)
{
	int nr_coeffs;
	struct coefficients *coeffs;

	if (cl == CL_ONE) {
		coeffs = big_cpu_coeffs;
		nr_coeffs = nr_big_coeffs;
	} else {
		coeffs = little_cpu_coeffs;
		nr_coeffs = nr_little_coeffs;
	}

	return MHZ_TO_KHZ(power_to_freq((power_out * 100) / util, nr_coeffs, coeffs));
}

/* Powers in mW
 * TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max
 */
#define PUNCONSTRAINED  (8000)

static void release_power_caps(void)
{
	int cl_idx;
	struct ipa_config *config = &arbiter_data.config;

	/* TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max*/
	for (cl_idx = 0; cl_idx < NUM_CLUSTERS; cl_idx++) {
		int freq = get_cpu_freq_limit(cl_idx, PUNCONSTRAINED, 1);

		arbiter_set_cpu_freq_limit(freq, cl_idx);
	}

	if (config->cores_out) {
		if (ipa_hotplug(false))
			pr_err("%s: failed ipa hotplug in\n", __func__);
		else
			config->cores_out = 0;
	}

	arbiter_data.gpu_freq_limit = 0;
	gpu_ipa_dvfs_max_unlock();
}

struct trace_data {
	int gpu_freq_in;
	int gpu_util;
	int gpu_nutil;
	int big_freq_in;
	int big_util;
	int big_nutil;
	int little_freq_in;
	int little_util;
	int little_nutil;
	int Pgpu_in;
	int Pbig_in;
	int Plittle_in;
	int Pcpu_in;
	int Ptot_in;
	int Pgpu_out;
	int Pbig_out;
	int Plittle_out;
	int Pcpu_out;
	int Ptot_out;
	int t0;
	int t1;
	int t2;
	int t3;
	int t4;
	int skin_temp;
	int cp_temp;
	int currT;
	int deltaT;
	int gpu_freq_out;
	int big_freq_out;
	int little_freq_out;
	int gpu_freq_req;
	int big_0_freq_in;
	int big_1_freq_in;
	int big_2_freq_in;
	int big_3_freq_in;
	int little_0_freq_in;
	int little_1_freq_in;
	int little_2_freq_in;
	int little_3_freq_in;
	int Pgpu_req;
	int Pbig_req;
	int Plittle_req;
	int Pcpu_req;
	int Ptot_req;
	int extra;
	int online_cores;
	int hotplug_cores_out;
};
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
static void print_trace(struct trace_data *td)
{
	trace_printk("gpu_freq_in=%d gpu_util=%d gpu_nutil=%d "
		"big_freq_in=%d big_util=%d big_nutil=%d "
		"little_freq_in=%d little_util=%d little_nutil=%d "
		"Pgpu_in=%d Pbig_in=%d Plittle_in=%d Pcpu_in=%d Ptot_in=%d "
		"Pgpu_out=%d Pbig_out=%d Plittle_out=%d Pcpu_out=%d Ptot_out=%d "
		"t0=%d t1=%d t2=%d t3=%d t4=%d ap_temp=%d cp_temp=%d currT=%d deltaT=%d "
		"gpu_freq_out=%d big_freq_out=%d little_freq_out=%d "
		"gpu_freq_req=%d "
		"big_0_freq_in=%d "
		"big_1_freq_in=%d "
		"big_2_freq_in=%d "
		"big_3_freq_in=%d "
		"little_0_freq_in=%d "
		"little_1_freq_in=%d "
		"little_2_freq_in=%d "
		"little_3_freq_in=%d "
		"Pgpu_req=%d Pbig_req=%d Plittle_req=%d Pcpu_req=%d Ptot_req=%d extra=%d "
		"online_cores=%d hotplug_cores_out=%d\n",
		td->gpu_freq_in, td->gpu_util, td->gpu_nutil,
		td->big_freq_in, td->big_util, td->big_nutil,
		td->little_freq_in, td->little_util, td->little_nutil,
		td->Pgpu_in, td->Pbig_in, td->Plittle_in, td->Pcpu_in, td->Ptot_in,
		td->Pgpu_out, td->Pbig_out, td->Plittle_out, td->Pcpu_out, td->Ptot_out,
		td->t0, td->t1, td->t2, td->t3, td->t4, td->skin_temp, td->cp_temp, td->currT, td->deltaT,
		td->gpu_freq_out, td->big_freq_out, td->little_freq_out,
		td->gpu_freq_req,
		td->big_0_freq_in,
		td->big_1_freq_in,
		td->big_2_freq_in,
		td->big_3_freq_in,
		td->little_0_freq_in,
		td->little_1_freq_in,
		td->little_2_freq_in,
		td->little_3_freq_in,
		td->Pgpu_req, td->Pbig_req, td->Plittle_req, td->Pcpu_req, td->Ptot_req, td->extra,
		td->online_cores, td->hotplug_cores_out);
}

static void print_only_temp_trace(int skin_temp)
{
	struct trace_data trace_data;
	int currT = skin_temp / 10;

	/* Initialize every int to -1 */
	memset(&trace_data, 0xff, sizeof(trace_data));

	trace_data.skin_temp = skin_temp;
	trace_data.cp_temp = get_humidity_sensor_temp();
	trace_data.currT = currT;
	trace_data.deltaT = arbiter_data.config.control_temp - currT;

	print_trace(&trace_data);
}
#else
static void print_trace(struct trace_data *td)
{
}

static void print_only_temp_trace(int skin_temp)
{
}
#endif
static void check_switch_ipa_onoff(int skin_temp)
{
	int currT, threshold_temp;

	currT = skin_temp / 10;
	threshold_temp = arbiter_data.config.control_temp - arbiter_data.config.temp_threshold;

	if (arbiter_data.active && currT < threshold_temp) {
		/* Switch Off */
		release_power_caps();
		arbiter_data.active = false;
		/* The caller should dequeue arbiter_poll() *if* it's queued */
	} else if (!arbiter_data.active && currT >= threshold_temp) {
		reset_controller(&arbiter_data.config.ctlr);
		arbiter_data.active = true;
	}

	if (!arbiter_data.active)
		print_only_temp_trace(skin_temp);
}

#define PLIMIT_SCALAR	(20)
#define PLIMIT_NONE	(5 * PLIMIT_SCALAR)

static int F(int deltaT)
{
	/* All notional values multiplied by PLIMIT_SCALAR to avoid float */
	const int limits[] = {
		/* Limit                    Temp */
		/* =====                    ==== */
		0.75 * PLIMIT_SCALAR,  /* -3 degrees or lower */
		0.8  * PLIMIT_SCALAR,  /* -2 */
		0.9  * PLIMIT_SCALAR,  /* -1 */
		1    * PLIMIT_SCALAR,  /*  0 */
		1.2  * PLIMIT_SCALAR,  /*  1 */
		1.4  * PLIMIT_SCALAR,  /*  2 */
		1.7  * PLIMIT_SCALAR,  /*  3 */
		2    * PLIMIT_SCALAR,  /*  4 */
		2.4  * PLIMIT_SCALAR,  /*  5 */
		2.8  * PLIMIT_SCALAR,  /*  6 */
		3.4  * PLIMIT_SCALAR,  /*  7 */
		4    * PLIMIT_SCALAR,  /*  8 */
		4    * PLIMIT_SCALAR,  /*  9 */
		PLIMIT_NONE,  /* No limit  10 degrees or higher */
	};

	if (deltaT < -3)
		deltaT = -3;
	if (deltaT > 10)
		deltaT = 10;

	return limits[deltaT+3];
}

static int F_ctlr(int curr)
{
	struct ipa_config *config = &arbiter_data.config;
	int setpoint = config->control_temp;
	struct ctlr *ctlr = &config->ctlr;
	s64 p, i, d, out;
	int err_int = setpoint - curr;
	int err = int_to_frac(err_int);

	/*
	 * Only integrate error if its <= integral reset threshold
	 * otherwise reset integral to reset value
	 */
	if (err_int > ctlr->integral_reset_threshold) {
		ctlr->err_integral = ctlr->integral_reset_value;
		i = mul_frac(ctlr->k_i, ctlr->err_integral);
	} else {
		/*
		 * if error less than cut off allow integration but integral
		 * is limited to max power
		 */
		i = mul_frac(ctlr->k_i, ctlr->err_integral);

		if (err_int < config->ctlr.integral_cutoff) {
			s64 tmpi = mul_frac(ctlr->k_i, err);
			tmpi += i;
			if (tmpi <= int_to_frac(config->soc_max_power)) {
				i = tmpi;
				ctlr->err_integral += err;
			}
		}
	}

	if (err_int < 0)
		p = mul_frac(ctlr->k_po, err);
	else
		p = mul_frac(ctlr->k_pu, err);

	/*
	 * dterm does err - err_prev, so with a +ve k_d, a decreasing
	 * error (which is driving closer to the line) results in less
	 * power being applied (slowing down the controller
	 */
	d = mul_frac(ctlr->k_d, err - ctlr->err_prev);
	ctlr->err_prev = err;

	out = p + i + d;
	out = frac_to_int(out);

	if (ctlr->feed_forward)
		out += config->tdp;

	/* output power must not be negative */
	if (out < 0)
		out = 0;

	/* output should not exceed max power */
	if (out > config->soc_max_power)
		out = config->soc_max_power;
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("curr=%d err=%d err_integral=%d p=%d i=%d d=%d out=%d\n",
		     curr, frac_to_int(err), frac_to_int(ctlr->err_integral),
		     (int) frac_to_int(p), (int) frac_to_int(i), (int) frac_to_int(d),
		     (int) out);
#endif
	/* the casts here should be safe */
	return (int)out;
}

static int debugfs_k_set(void *data, u64 val)
{
	*(int *)data = int_to_frac(val);
	return 0;
}
static int debugfs_k_get(void *data, u64 *val)
{
	*val = frac_to_int(*(int *)data);
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(k_fops, debugfs_k_get, debugfs_k_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(k_fops_ro, debugfs_k_get, NULL, "%llu\n");

static int debugfs_u32_get(void *data, u64 *val)
{
	*val = *(u32 *)data;
	return 0;
}

static int debugfs_s32_get(void *data, u64 *val)
{
	*val = *(s32 *)data;
	return 0;
}

static void setup_debugfs_ctlr(struct ipa_config *config, struct dentry *parent)
{
	struct dentry *ctlr_d, *dentry_f;

	ctlr_d = debugfs_create_dir("ctlr", parent);
	if (IS_ERR_OR_NULL(ctlr_d))
		pr_warn("unable to create debugfs directory: ctlr\n");

	dentry_f = debugfs_create_u32("k_po", 0644, ctlr_d, &config->ctlr.k_po);

	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_po\n");

	dentry_f = debugfs_create_u32("k_pu", 0644, ctlr_d, &config->ctlr.k_pu);

	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_pu\n");

	dentry_f = debugfs_create_file("k_i", 0644, ctlr_d, &config->ctlr.k_i,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_i\n");

	dentry_f = debugfs_create_file("k_d", 0644, ctlr_d, &config->ctlr.k_d,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: k_d\n");

	dentry_f = debugfs_create_u32("integral_cutoff", 0644, ctlr_d,
				       &config->ctlr.integral_cutoff);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_cutoff\n");

	dentry_f = debugfs_create_file("err_integral", 0644, ctlr_d,
				       &config->ctlr.err_integral, &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: err_integral\n");

	dentry_f = debugfs_create_file("integral_reset_value", 0644, ctlr_d,
				       &config->ctlr.integral_reset_value,
				       &k_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_reset_value\n");

	dentry_f = debugfs_create_u32("integral_reset_threshold", 0644, ctlr_d,
				       &config->ctlr.integral_reset_threshold);
	if (!dentry_f)
		pr_warn("unable to create debugfs directory: integral_reset_threshold\n");
}

static int debugfs_power_set(void *data, u64 val)
{
	struct ipa_config *config = &arbiter_data.config;
	u32 old = *(u32 *)data;

	*(u32 *)data = val;

	if (config->tdp < config->ros_power) {
		*(u32 *)data = old;
		pr_warn("tdp < rest_of_soc. Reverting to previous value\n");
	}

	return 0;
}

static int debugfs_s32_set(void *data, u64 val)
{
	*(s32 *)data = val;
	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(power_fops, debugfs_u32_get, debugfs_power_set, "%llu\n");
DEFINE_SIMPLE_ATTRIBUTE(signed_fops, debugfs_s32_get, debugfs_s32_set, "%lld\n");

/* Shamelessly ripped from fs/debugfs/file.c */
static ssize_t read_file_bool(struct file *file, char __user *user_buf,
			      size_t count, loff_t *ppos)
{
	char buf[3];
	u32 *val = file->private_data;

	if (*val)
		buf[0] = 'Y';
	else
		buf[0] = 'N';
	buf[1] = '\n';
	buf[2] = 0x00;
	return simple_read_from_buffer(user_buf, count, ppos, buf, 2);
}

/* Shamelessly ripped from fs/debugfs/file.c */
static ssize_t __write_file_bool(struct file *file, const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	char buf[32];
	size_t buf_size;
	bool bv;
	u32 *val = file->private_data;

	buf_size = min(count, (sizeof(buf)-1));
	if (copy_from_user(buf, user_buf, buf_size))
		return -EFAULT;

	if (strtobool(buf, &bv) == 0)
		*val = bv;

	return count;
}

static ssize_t control_temp_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.config.control_temp);
}

static ssize_t control_temp_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	arbiter_data.config.control_temp = val;
	return n;
}

static ssize_t tdp_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.config.tdp);
}

static ssize_t tdp_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	arbiter_data.config.tdp = val;
	return n;
}

static ssize_t enabled_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%c\n", arbiter_data.config.enabled ? 'Y' : 'N');
}

static ssize_t enabled_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
    if (!buf)
		return -EINVAL;

    if ((buf[0]) == 'Y' && arbiter_data.config.enabled == 0) {
		arbiter_data.config.enabled = true;
		queue_arbiter_poll();
		return n;
    } else  if ((buf[0]) == 'N' && arbiter_data.config.enabled != 0) {
		arbiter_data.config.enabled = false;
		release_power_caps();
		return n;
    }

    return -EINVAL;
}

static ssize_t hotplug_out_threshold_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.config.hotplug_out_threshold);
}

static ssize_t hotplug_out_threshold_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtoul(buf, 10, &val))
		return -EINVAL;

	arbiter_data.config.hotplug_out_threshold = val;
	return n;
}

static ssize_t hotplug_in_threshold_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.config.hotplug_in_threshold);
}

static ssize_t hotplug_in_threshold_store(struct kobject *kobj,
				    struct kobj_attribute *attr,
				    const char *buf, size_t n)
{
	unsigned long val;

	if (kstrtol(buf, 10, &val))
		return -EINVAL;

	arbiter_data.config.hotplug_in_threshold = val;
	return n;
}

static ssize_t big_max_freq_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.cpu_freq_limits[CL_ONE]);
}

static ssize_t little_max_freq_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.cpu_freq_limits[CL_ZERO]);
}

static ssize_t gpu_max_freq_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.gpu_freq_limit);
}

static ssize_t cpu_hotplug_out_show(struct kobject *kobj,
				   struct kobj_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", arbiter_data.config.cores_out);
}

static ssize_t debugfs_enabled_set(struct file *file, const char __user *user_buf,
			       size_t count, loff_t *ppos)
{
	u32 enabled = arbiter_data.config.enabled;
	ssize_t ret = __write_file_bool(file, user_buf, count, ppos);

	if (enabled && !arbiter_data.config.enabled)
		release_power_caps();
	else if (!enabled && arbiter_data.config.enabled)
		queue_arbiter_poll();

	return ret;
}

static const struct file_operations enabled_fops = {
	.open = simple_open,
	.read = read_file_bool,
	.write = debugfs_enabled_set,
	.llseek = default_llseek,
};

static struct dentry *setup_debugfs(struct ipa_config *config)
{
	struct dentry *ipa_d, *dentry_f;

	ipa_d = debugfs_create_dir("ipa", NULL);
	if (!ipa_d)
		pr_warn("unable to create debugfs directory: ipa\n");

	dentry_f = debugfs_create_u32("control_temp", 0644, ipa_d,
				      &config->control_temp);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: control_temp\n");

	dentry_f = debugfs_create_u32("temp_threshold", 0644, ipa_d,
				       &config->temp_threshold);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: temp_threshold\n");

	dentry_f = debugfs_create_file("enabled", 0644, ipa_d,
				&config->enabled, &enabled_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: enabled\n");

	dentry_f = debugfs_create_file("tdp", 0644, ipa_d, &config->tdp,
				       &power_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: tdp\n");

	dentry_f = debugfs_create_bool("boost", 0644, ipa_d, &config->boost);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: boost\n");

	dentry_f = debugfs_create_file("rest_of_soc_power", 0644, ipa_d,
				      &config->ros_power, &power_fops);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: rest_of_soc_power\n");

	dentry_f = debugfs_create_u32("little_weight", 0644, ipa_d,
				      &config->little_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: little_weight\n");

	dentry_f = debugfs_create_u32("big_weight", 0644, ipa_d,
				      &config->big_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: big_weight\n");

	dentry_f = debugfs_create_u32("gpu_weight", 0644, ipa_d,
				      &config->gpu_weight);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: gpu_weight\n");

	dentry_f = debugfs_create_u32("little_max_power", 0644, ipa_d,
				      &config->little_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: little_max_power\n");

	dentry_f = debugfs_create_u32("big_max_power", 0644, ipa_d,
				      &config->big_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: big_max_power\n");

	dentry_f = debugfs_create_u32("gpu_max_power", 0644, ipa_d,
				      &config->gpu_max_power);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: gpu_max_power\n");

	dentry_f = debugfs_create_u32("hotplug_out_threshold", 0644, ipa_d,
				      &config->hotplug_out_threshold);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: hotplug_out_threshold\n");

	dentry_f = debugfs_create_file("hotplug_in_threshold", 0644, ipa_d,
				      &config->hotplug_in_threshold, &signed_fops);
	if (!dentry_f)
		pr_warn("unable to create the debugfs file: hotplug_in_threshold\n");

	dentry_f = debugfs_create_bool("enable_ctlr", 0644, ipa_d,
				       &config->enable_ctlr);
	if (!dentry_f)
		pr_warn("unable to create debugfs file: enable_ctlr\n");

	setup_debugfs_ctlr(config, ipa_d);
	return ipa_d;
}

define_ipa_attr(enabled);
define_ipa_attr(control_temp);
define_ipa_attr(tdp);
define_ipa_attr(hotplug_out_threshold);
define_ipa_attr(hotplug_in_threshold);
define_ipa_attr_ro(big_max_freq);
define_ipa_attr_ro(little_max_freq);
define_ipa_attr_ro(gpu_max_freq);
define_ipa_attr_ro(cpu_hotplug_out);

static struct attribute *ipa_attrs[] = {
	&enabled.attr,
	&control_temp.attr,
	&tdp.attr,
	&hotplug_out_threshold.attr,
	&hotplug_in_threshold.attr,
	&big_max_freq.attr,
	&little_max_freq.attr,
	&gpu_max_freq.attr,
	&cpu_hotplug_out.attr,
	NULL
};

struct attribute_group ipa_attr_group = {
	.attrs = ipa_attrs,
};

static void setup_sysfs(struct arbiter_data *arb)
{
	int rc;

	arb->kobj = kobject_create_and_add("ipa", power_kobj);
	if (!arb->kobj) {
		pr_info("Unable to create ipa kobject\n");
		return;
	}

	rc = sysfs_create_group(arb->kobj, &ipa_attr_group);
	if (rc) {
		pr_info("Unable to create ipa group\n");
		return;
	}
}

static void setup_power_tables(void)
{
	struct cpu_power_info t;
	int i;

	t.load[0] = 100; t.load[1] = t.load[2] = t.load[3] = 0;
	t.cluster = CL_ZERO;
	for (i = 0; i < nr_little_coeffs; i++) {
		t.freq = MHZ_TO_KHZ(little_cpu_coeffs[i].frequency);
		little_cpu_coeffs[i].power = get_power_value(&t);
		pr_info("cluster: %d freq: %d power=%d\n", CL_ZERO, t.freq, little_cpu_coeffs[i].power);
	}

	t.cluster = CL_ONE;
	for (i = 0; i < nr_big_coeffs; i++) {
		t.freq = MHZ_TO_KHZ(big_cpu_coeffs[i].frequency);
		big_cpu_coeffs[i].power = get_power_value(&t);
		pr_info("cluster: %d freq: %d power=%d\n", CL_ONE, t.freq, big_cpu_coeffs[i].power);
	}
}

static int setup_cpufreq_tables(int cl_idx)
{
	int i, cnt = 0;
	int policy_cpu = ((cl_idx == CL_ONE) ? 4 : 0);
	struct coefficients *coeff = ((cl_idx == CL_ONE) ? big_cpu_coeffs : little_cpu_coeffs);
	struct cpufreq_frequency_table *table =
			cpufreq_get_info_table(policy_cpu);

	for (i = 0; table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (table[i].frequency == CPUFREQ_ENTRY_INVALID)
			continue;

		coeff[cnt].frequency = KHZ_TO_MHZ(table[i].frequency);
		cnt++;
	}

	return cnt;
}

static int __maybe_unused read_soc_temperature(void)
{
	void *pdata = arbiter_data.sensor->private_data;

	return arbiter_data.sensor->read_soc_temperature(pdata);
}

static int get_cpu_power_req(int cl_idx, struct coefficients *coeffs, int nr_coeffs)
{
	int cpu, i, power;

	power = 0;
	i = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[cl_idx].mask) {
		if (cpu_online(cpu)) {
			int util = arbiter_data.cl_stats[cl_idx].utils[i];
			int freq = KHZ_TO_MHZ(arbiter_data.cpu_freqs[cl_idx][i]);
			power += freq_to_power(freq, nr_coeffs, coeffs) * util;
		}

		i++;
	}

	return power;
}

static void arbiter_calc(int currT)
{
	int Plittle_req, Pbig_req;
	int Pgpu_req, Pcpu_req;
	int Ptot_req;
	int Plittle_in, Pbig_in;
	int Pgpu_in, Pcpu_in;
	int Ptot_in;
	int Prange;
	u32 Pgpu_out, Pcpu_out, Pbig_out, Plittle_out;
	int power_limit_factor = PLIMIT_NONE;
	int deltaT;
	int extra;
	int big_util, little_util;
	int gpu_freq_limit, cpu_freq_limits[NUM_CLUSTERS];
	int cpu, online_cores;
	struct trace_data trace_data;

	struct ipa_config *config = &arbiter_data.config;

	/*
	 * P*req are in mW
	 * In addition, P*in are weighted based on configuration as well
	 * NOTE RISK OF OVERFLOW NOT CHECKING FOR NOW BUT WILL NEED FIXING
	 */
	Pbig_req = (config->big_weight *
		get_cpu_power_req(CL_ONE, big_cpu_coeffs, nr_big_coeffs)) >> WEIGHT_SHIFT;
	Plittle_req = (config->little_weight *
		get_cpu_power_req(CL_ZERO, little_cpu_coeffs, nr_little_coeffs)) >> WEIGHT_SHIFT;

	Pgpu_req = (config->gpu_weight
		   * kbase_platform_dvfs_freq_to_power(arbiter_data.gpu_freq)
		   * arbiter_data.gpu_load) >> WEIGHT_SHIFT;

	Pcpu_req = Plittle_req + Pbig_req;
	Ptot_req = Pcpu_req + Pgpu_req;

	BUG_ON(Pbig_req < 0);
	BUG_ON(Plittle_req < 0);
	BUG_ON(Pgpu_req < 0);

	/*
	 * Calculate the model values to observe power in the previous window
	 */
	Pbig_in = freq_to_power(KHZ_TO_MHZ(arbiter_data.cl_stats[CL_ONE].freq), nr_big_coeffs, big_cpu_coeffs) * arbiter_data.cl_stats[CL_ONE].util;
	Plittle_in = freq_to_power(KHZ_TO_MHZ(arbiter_data.cl_stats[CL_ZERO].freq), nr_little_coeffs, little_cpu_coeffs) * arbiter_data.cl_stats[CL_ZERO].util;
	Pgpu_in = kbase_platform_dvfs_freq_to_power(arbiter_data.mali_stats.s.freq_for_norm) * arbiter_data.gpu_load;

	Pcpu_in = Plittle_in + Pbig_in;
	Ptot_in = Pcpu_in + Pgpu_in;

	deltaT = config->control_temp - currT;

	extra = 0;

	if (config->enable_ctlr) {
		Prange = F_ctlr(currT);

		Prange = max(Prange - (int)config->ros_power, 0);
	} else {
		power_limit_factor = F(deltaT);
		BUG_ON(power_limit_factor < 0);
		if (!config->boost && (power_limit_factor > PLIMIT_SCALAR))
			power_limit_factor = PLIMIT_SCALAR;

		/*
		 * If within the control zone, or boost is disabled
		 * constrain power.
		 *
		 * Prange (mW) = tdp (mW) - ROS (mW)
		 */
		Prange = config->tdp - config->ros_power;

		/*
		 * Prange divided to compensate for PLIMIT_SCALAR
		 */
		Prange = (Prange * power_limit_factor) / PLIMIT_SCALAR;
	}

	BUG_ON(Prange < 0);

	if (!config->enable_ctlr && power_limit_factor == PLIMIT_NONE) {
		/*
		 * If we are outside the control zone, and boost is enabled
		 * run un-constrained.
		 */
		/* TODO get rid of PUNCONSTRAINED and replace with config->aX_power_max*/
		Pbig_out = PUNCONSTRAINED;
		Plittle_out  = PUNCONSTRAINED;
		Pgpu_out = PUNCONSTRAINED;
	} else {
		/*
		 * Pcpu_out (mw) = Prange (mW) * (Pcpu_req / Ptot_req) (10uW) * power_limit_factor
		 * Pgpu_out (mw) = Prange (mW) * (Pgpu_req / Ptot_req) (10uW) * power_limit_factor
		 *
		 * Note: the (10uW) variables get cancelled and result is in mW
		 */
		if (Ptot_req != 0) {
			u64 tmp;
			/*
			 * Divvy-up the available power (Prange) in proportion to
			 * the weighted request.
			 */
			tmp = Pbig_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Pbig_out = (int) tmp;

			tmp = Plittle_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Plittle_out = (int) tmp;

			tmp = Pgpu_req;
			tmp *= Prange;
			tmp = div_u64(tmp, (u32) Ptot_req);
			Pgpu_out = (int) tmp;

			/*
			 * Do we exceed the max for any of the actors?
			 * Reclaim the extra and update the denominator to
			 * exclude that actor
			 */
			if (Plittle_out > config->little_max_power) {
				extra += Plittle_out - config->little_max_power;
				Plittle_out = config->little_max_power;
			}

			if (Pbig_out > config->big_max_power) {
				extra += Pbig_out - config->big_max_power;
				Pbig_out = config->big_max_power;
			}

			if (Pgpu_out > config->gpu_max_power) {
				extra += Pgpu_out - config->gpu_max_power;
				Pgpu_out = config->gpu_max_power;
			}

			/*
			 * Re-divvy the reclaimed extra among actors base on
			 * how far they are from the max
			 */
			if (extra > 0) {
				int distlittle  = config->little_max_power - Plittle_out;
				int distbig = config->big_max_power - Pbig_out;
				int distgpu = config->gpu_max_power - Pgpu_out;
				int capped_extra = distlittle + distbig + distgpu;

				extra = min(extra, capped_extra);
				if (capped_extra > 0) {
					Plittle_out += (distlittle * extra) / capped_extra;
					Pbig_out += (distbig * extra) / capped_extra;
					Pgpu_out += (distgpu * extra) / capped_extra;
				}
			}

		} else {	/* Avoid divide by zero */
			Pbig_out = config->big_max_power;
			Plittle_out  = config->little_max_power;
			Pgpu_out = config->gpu_max_power;
		}
	}

	Plittle_out = min(Plittle_out, config->little_max_power);
	Pbig_out = min(Pbig_out, config->big_max_power);
	Pgpu_out = min(Pgpu_out, config->gpu_max_power);

	Pcpu_out = Pbig_out + Plittle_out;

	big_util = arbiter_data.cl_stats[CL_ONE].util > 0 ? arbiter_data.cl_stats[CL_ONE].util : 1;
	little_util  = arbiter_data.cl_stats[CL_ZERO].util > 0 ? arbiter_data.cl_stats[CL_ZERO].util : 1;
	/*
	 * Output Power per cpu (mW) = Pcpu_out (mw) * ( 100 / util ) - where 0 < util < 400
	 */
	cpu_freq_limits[CL_ONE] = get_cpu_freq_limit(CL_ONE, Pbig_out, big_util);
	cpu_freq_limits[CL_ZERO] = get_cpu_freq_limit(CL_ZERO, Plittle_out, little_util);

	gpu_freq_limit = kbase_platform_dvfs_power_to_freq(Pgpu_out);

#ifdef CONFIG_CPU_THERMAL_IPA_CONTROL
	if (config->enabled) {
		arbiter_set_cpu_freq_limit(cpu_freq_limits[CL_ONE], CL_ONE);
		arbiter_set_cpu_freq_limit(cpu_freq_limits[CL_ZERO], CL_ZERO);
		arbiter_set_gpu_freq_limit(gpu_freq_limit);

		/*
		 * If we have reached critical thresholds then also
		 * hotplug cores as approprpiate
		 */
		if (deltaT <= -config->hotplug_out_threshold && !config->cores_out) {
			if (ipa_hotplug(true))
				pr_err("%s: failed ipa hotplug out\n", __func__);
			else
				config->cores_out = 1;
		}
		if (deltaT >= -config->hotplug_in_threshold && config->cores_out) {
			if (ipa_hotplug(false))
				pr_err("%s: failed ipa hotplug in\n", __func__);
			else
				config->cores_out = 0;
		}
	}
#else
#error "Turn on CONFIG_CPU_THERMAL_IPA_CONTROL to enable IPA control"
#endif

	online_cores = 0;
	for_each_online_cpu(cpu)
		online_cores++;

	trace_data.gpu_freq_in = arbiter_data.mali_stats.s.freq_for_norm;
	trace_data.gpu_util = arbiter_data.mali_stats.s.utilisation;
	trace_data.gpu_nutil = arbiter_data.mali_stats.s.norm_utilisation;
	trace_data.big_freq_in = KHZ_TO_MHZ(arbiter_data.cl_stats[CL_ONE].freq);
	trace_data.big_util = arbiter_data.cl_stats[CL_ONE].util;
	trace_data.big_nutil = (arbiter_data.cl_stats[CL_ONE].util * arbiter_data.cl_stats[CL_ONE].freq) / get_real_max_freq(CL_ONE);
	trace_data.little_freq_in = KHZ_TO_MHZ(arbiter_data.cl_stats[CL_ZERO].freq);
	trace_data.little_util = arbiter_data.cl_stats[CL_ZERO].util;
	trace_data.little_nutil = (arbiter_data.cl_stats[CL_ZERO].util * arbiter_data.cl_stats[CL_ZERO].freq) / get_real_max_freq(CL_ZERO);;
	trace_data.Pgpu_in = Pgpu_in / 100;
	trace_data.Pbig_in =  Pbig_in / 100;
	trace_data.Plittle_in = Plittle_in / 100;
	trace_data.Pcpu_in = Pcpu_in / 100;
	trace_data.Ptot_in = Ptot_in / 100;
	trace_data.Pgpu_out = Pgpu_out;
	trace_data.Pbig_out =  Pbig_out;
	trace_data.Plittle_out = Plittle_out;
	trace_data.Pcpu_out = Pcpu_out;
	trace_data.Ptot_out = Pgpu_out + Pcpu_out;
	trace_data.skin_temp = arbiter_data.skin_temperature;
	trace_data.t0 = -1;
	trace_data.t1 = -1;
	trace_data.t2 = -1;
	trace_data.t3 = -1;
	trace_data.t4 = -1;
	trace_data.cp_temp = arbiter_data.cp_temperature;
	trace_data.currT = currT;
	trace_data.deltaT = deltaT;
	trace_data.gpu_freq_out = gpu_freq_limit;
	trace_data.big_freq_out = KHZ_TO_MHZ(cpu_freq_limits[CL_ONE]);
	trace_data.little_freq_out = KHZ_TO_MHZ(cpu_freq_limits[CL_ZERO]);
	trace_data.gpu_freq_req = arbiter_data.gpu_freq;
	trace_data.big_0_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ONE][0]);
	trace_data.big_1_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ONE][1]);
	trace_data.big_2_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ONE][2]);
	trace_data.big_3_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ONE][3]);
	trace_data.little_0_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ZERO][0]);
	trace_data.little_1_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ZERO][1]);
	trace_data.little_2_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ZERO][2]);
	trace_data.little_3_freq_in = KHZ_TO_MHZ(arbiter_data.cpu_freqs[CL_ZERO][3]);
	trace_data.Pgpu_req = Pgpu_req;
	trace_data.Pbig_req = Pbig_req;
	trace_data.Plittle_req = Plittle_req;
	trace_data.Pcpu_req = Pcpu_req;
	trace_data.Ptot_req = Ptot_req;
	trace_data.extra = extra;
	trace_data.online_cores = online_cores;
	trace_data.hotplug_cores_out = arbiter_data.config.cores_out;

	print_trace(&trace_data);
}

static void arbiter_poll(struct work_struct *work)
{
	gpu_ipa_dvfs_get_utilisation_stats(&arbiter_data.mali_stats);

	arbiter_data.gpu_load = arbiter_data.mali_stats.s.utilisation;

	get_cluster_stats(arbiter_data.cl_stats);

#ifndef CONFIG_SENSORS_SEC_THERMISTOR
	arbiter_data.max_sensor_temp = read_soc_temperature();
#endif
	arbiter_data.skin_temperature = arbiter_data.sensor->read_skin_temperature();
	arbiter_data.cp_temperature = get_humidity_sensor_temp();

	check_switch_ipa_onoff(arbiter_data.skin_temperature);

	if (arbiter_data.active)
		arbiter_calc(arbiter_data.skin_temperature/10);

	queue_arbiter_poll();
}

static int get_cluster_from_cpufreq_policy(struct cpufreq_policy *policy)
{
	return policy->cpu > 3 ? CL_ONE : CL_ZERO;
}

void ipa_cpufreq_requested(struct cpufreq_policy *policy, unsigned int freq)
{
	int cl_idx, i;
	unsigned int cpu;

	cl_idx = get_cluster_from_cpufreq_policy(policy);

	i = 0;
	for_each_cpu(cpu, arbiter_data.cl_stats[cl_idx].mask) {
		arbiter_data.cpu_freqs[cl_idx][i] = freq;

		i++;
	}
}

int ipa_register_thermal_sensor(struct ipa_sensor_conf *sensor)
{
	if (!sensor || !sensor->read_soc_temperature) {
		pr_err("Temperature sensor not initialised\n");
		return -EINVAL;
	}

	arbiter_data.sensor = sensor;

	if (!arbiter_data.sensor->read_skin_temperature)
		arbiter_data.sensor->read_skin_temperature = sec_therm_get_ap_temperature;

	return 0;
}

void ipa_mali_dvfs_requested(unsigned int freq)
{
	arbiter_data.gpu_freq = freq;
}

int thermal_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&thermal_notifier_list, nb);
}

int thermal_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&thermal_notifier_list, nb);
}

extern bool exynos_cpufreq_init_done;
static struct delayed_work init_work;

static void arbiter_init(struct work_struct *work)
{
	int i;

	if (!exynos_cpufreq_init_done) {
		pr_info("exynos_cpufreq not initialized. Deferring again...\n");
		queue_delayed_work(system_freezable_wq, &init_work,
				msecs_to_jiffies(500));
		return;
	}

	arbiter_data.gpu_freq_limit = get_ipa_dvfs_max_freq();
	arbiter_data.cpu_freq_limits[CL_ONE] = get_real_max_freq(CL_ONE);
	arbiter_data.cpu_freq_limits[CL_ZERO] = get_real_max_freq(CL_ZERO);
	for (i = 0; i < NR_CPUS; i++) {
		arbiter_data.cpu_freqs[CL_ONE][i] = get_real_max_freq(CL_ONE);
		arbiter_data.cpu_freqs[CL_ZERO][i] = get_real_max_freq(CL_ZERO);
	}

	setup_cpusmasks(arbiter_data.cl_stats);

	reset_arbiter_configuration(&arbiter_data.config);
	arbiter_data.debugfs_root = setup_debugfs(&arbiter_data.config);
	setup_sysfs(&arbiter_data);
	nr_little_coeffs = setup_cpufreq_tables(CL_ZERO);
	nr_big_coeffs = setup_cpufreq_tables(CL_ONE);
	setup_power_tables();

	/* reconfigure max */
	arbiter_data.config.little_max_power = freq_to_power(KHZ_TO_MHZ(arbiter_data.cpu_freq_limits[CL_ZERO]),
							nr_little_coeffs, little_cpu_coeffs) * cpumask_weight(arbiter_data.cl_stats[CL_ZERO].mask);

	arbiter_data.config.big_max_power = freq_to_power(KHZ_TO_MHZ(arbiter_data.cpu_freq_limits[CL_ONE]),
							nr_big_coeffs, big_cpu_coeffs) * cpumask_weight(arbiter_data.cl_stats[CL_ONE].mask);

	arbiter_data.config.gpu_max_power = kbase_platform_dvfs_freq_to_power(arbiter_data.gpu_freq_limit);

	arbiter_data.config.soc_max_power = arbiter_data.config.gpu_max_power +
		arbiter_data.config.big_max_power +
		arbiter_data.config.gpu_max_power;
	/* TODO when we introduce dynamic RoS power we need
	   to add a ros_max_power !! */
	arbiter_data.config.soc_max_power += arbiter_data.config.ros_power;

	INIT_DELAYED_WORK(&arbiter_data.work, arbiter_poll);

	arbiter_data.initialised = true;
	queue_arbiter_poll();

	pr_info("Intelligent Power Allocation initialised.\n");
}

#ifdef CONFIG_OF
static int __init get_dt_inform_ipa(void)
{
	u32 proper_val;
	int ret = 0;

	struct device_node *ipa_np;

	ipa_np = of_find_compatible_node(NULL, NULL, "samsung,exynos-ipa");

	if (!ipa_np)
		return -EINVAL;

	/* read control_temp */
	ret = of_property_read_u32(ipa_np, "control_temp", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of control_temp\n", __func__);
		return -EINVAL;
	} else {
		default_config.control_temp = proper_val;
		pr_info("[IPA] control_temp : %d\n", (u32)proper_val);
	}

	/* read temp_threshold */
	ret = of_property_read_u32(ipa_np, "temp_threshold", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of temp_threshold\n", __func__);
		return -EINVAL;
	} else {
		default_config.temp_threshold = proper_val;
		pr_info("[IPA] temp_threshold : %d\n", (u32)proper_val);
	}

	/* read enabled */
	ret = of_property_read_u32(ipa_np, "enabled", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of enabled\n", __func__);
		return -EINVAL;
	} else {
		default_config.enabled = proper_val;
		pr_info("[IPA] enabled : %d\n", (u32)proper_val);
	}

	/* read tdp */
	ret = of_property_read_u32(ipa_np, "tdp", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of tdp\n", __func__);
		return -EINVAL;
	} else {
		default_config.tdp = proper_val;
		pr_info("[IPA] tdp : %d\n", (u32)proper_val);
	}

	/* read boost */
	ret = of_property_read_u32(ipa_np, "boost", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of boost\n", __func__);
		return -EINVAL;
	} else {
		default_config.boost = proper_val;
		pr_info("[IPA] boost : %d\n", (u32)proper_val);
	}

	/* read ros_power */
	ret = of_property_read_u32(ipa_np, "ros_power", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ros_power\n", __func__);
		return -EINVAL;
	} else {
		default_config.ros_power = proper_val;
		pr_info("[IPA] ros_power : %d\n", (u32)proper_val);
	}

	/* read little_weight */
	ret = of_property_read_u32(ipa_np, "little_weight", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of little_weight\n", __func__);
		return -EINVAL;
	} else {
		default_config.little_weight = proper_val;
		pr_info("[IPA] little_weight : %d\n", (u32)proper_val);
	}

	/* read big_weight */
	ret = of_property_read_u32(ipa_np, "big_weight", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of big_weight\n", __func__);
		return -EINVAL;
	} else {
		default_config.big_weight = proper_val;
		pr_info("[IPA] big_weight : %d\n", (u32)proper_val);
	}

	/* read gpu_weight */
	ret = of_property_read_u32(ipa_np, "gpu_weight", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of gpu_weight\n", __func__);
		return -EINVAL;
	} else {
		default_config.gpu_weight = proper_val;
		pr_info("[IPA] gpu_weight : %d\n", (u32)proper_val);
	}

	/* read little_max_power */
	ret = of_property_read_u32(ipa_np, "little_max_power", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of little_max_power\n", __func__);
		return -EINVAL;
	} else {
		default_config.little_max_power = proper_val;
		pr_info("[IPA] little_max_power : %d\n", (u32)proper_val);
	}

	/* read big_max_power */
	ret = of_property_read_u32(ipa_np, "big_max_power", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of big_max_power\n", __func__);
		return -EINVAL;
	} else {
		default_config.big_max_power = proper_val;
		pr_info("[IPA] big_max_power : %d\n", (u32)proper_val);
	}

	/* read gpu_max_power */
	ret = of_property_read_u32(ipa_np, "gpu_max_power", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of gpu_max_power\n", __func__);
		return -EINVAL;
	} else {
		default_config.gpu_max_power = proper_val;
		pr_info("[IPA] gpu_max_power : %d\n", (u32)proper_val);
	}

	/* read hotplug_in_threshold */
	ret = of_property_read_u32(ipa_np, "hotplug_in_threshold", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of hotplug_in_threshold\n", __func__);
		return -EINVAL;
	} else {
		default_config.hotplug_in_threshold = (int) proper_val;
		pr_info("[IPA] hotplug_in_threshold : %d\n", (int)proper_val);
	}

	/* read hotplug_out_threshold */
	ret = of_property_read_u32(ipa_np, "hotplug_out_threshold", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of hotplug_out_threshold\n", __func__);
		return -EINVAL;
	} else {
		default_config.hotplug_out_threshold = proper_val;
		pr_info("[IPA] hotplug_out_threshold : %d\n", (u32)proper_val);
	}

	/* read enable_ctlr */
	ret = of_property_read_u32(ipa_np, "enable_ctlr", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of enable_ctlr\n", __func__);
		return -EINVAL;
	} else {
		default_config.enable_ctlr = proper_val;
		pr_info("[IPA] enable_ctlr : %d\n", (u32)proper_val);
	}

	/* read ctlr.mult */
	ret = of_property_read_u32(ipa_np, "ctlr.mult", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of enabled\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.mult = proper_val;
		pr_info("[IPA] ctlr.mult : %d\n", (u32)proper_val);
	}

	/* read ctlr.k_i */
	ret = of_property_read_u32(ipa_np, "ctlr.k_i", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.k_i\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.k_i = proper_val;
		pr_info("[IPA] ctlr.k_i : %d\n", (u32)proper_val);
	}

	/* read ctlr.k_d */
	ret = of_property_read_u32(ipa_np, "ctlr.k_d", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.k_d\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.k_d = proper_val;
		pr_info("[IPA] ctlr.k_d : %d\n", (u32)proper_val);
	}

	/* read ctlr.feed_forward */
	ret = of_property_read_u32(ipa_np, "ctlr.feed_forward", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.feed_forward\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.feed_forward = proper_val;
		pr_info("[IPA] ctlr.feed_forward : %d\n", (u32)proper_val);
	}

	/* read ctlr.integral_reset_value */
	ret = of_property_read_u32(ipa_np, "ctlr.integral_reset_value", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.integral_reset_value\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.integral_reset_value = proper_val;
		pr_info("[IPA] ctlr.integral_reset_value : %d\n", (u32)proper_val);
	}

	/* read ctlr.integral_cutoff */
	ret = of_property_read_u32(ipa_np, "ctlr.integral_cutoff", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.integral_cutoff\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.integral_cutoff = proper_val;
		pr_info("[IPA] ctlr.integral_cutoff : %d\n", (u32)proper_val);
	}

	/* read ctlr.integral_reset_threshold */
	ret = of_property_read_u32(ipa_np, "ctlr.integral_reset_threshold", &proper_val);
	if (ret) {
		pr_err("[%s] There is no Property of ctlr.integral_reset_threshold\n", __func__);
		return -EINVAL;
	} else {
		default_config.ctlr.integral_reset_threshold = proper_val;
		pr_info("[IPA] ctlr.integral_reset_threshold : %d\n", (u32)proper_val);
	}

	return 0;
}
#endif

static int __init ipa_init(void)
{
#ifdef CONFIG_OF
	if (get_dt_inform_ipa()) {
		pr_err("[%s] ERROR there are no DT inform\n", __func__);
		return -EINVAL;
	}
#endif
	INIT_DELAYED_WORK(&init_work, arbiter_init);
	queue_delayed_work(system_freezable_wq, &init_work, msecs_to_jiffies(500));

	pr_info("Deferring initialisation...");

	return 0;
}
late_initcall(ipa_init);

MODULE_DESCRIPTION("Intelligent Power Allocation Driver");
