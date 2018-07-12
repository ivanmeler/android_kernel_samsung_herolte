/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * EXYNOS - MP CPU frequency scaling support for EXYNOS Big.Little series
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed "as is" WITHOUT ANY WARRANTY of any
 * kind, whether express or implied; without even the implied warranty
 * of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/clk.h>
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/regulator/consumer.h>
#include <linux/cpufreq.h>
#include <linux/suspend.h>
#include <linux/module.h>
#include <linux/reboot.h>
#include <linux/delay.h>
#include <linux/cpu.h>
#include <linux/pm_qos.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/cpumask.h>
#include <linux/exynos-ss.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/apm-exynos.h>

#include <asm/smp_plat.h>
#include <asm/cputype.h>

#if defined(CONFIG_SEC_PM) && defined(CONFIG_MUIC_NOTIFIER)
#include <linux/muic/muic.h>
#include <linux/muic/muic_notifier.h>
#endif

#include <soc/samsung/cpufreq.h>
#include <soc/samsung/exynos-powermode.h>
#include <soc/samsung/asv-exynos.h>
#include <soc/samsung/tmu.h>
#include <soc/samsung/ect_parser.h>
#include <soc/samsung/exynos-pmu.h>

#define POWER_COEFF_15P		68 /* percore param */
#define POWER_COEFF_7P		10 /* percore  param */

#define VOLT_RANGE_STEP		25000
#define CLUSTER_ID(cl)		(cl ? ID_CL1 : ID_CL0)

#define LIMIT_FREQ_DIVIDER	4

#ifdef CONFIG_SMP
struct lpj_info {
	unsigned long   ref;
	unsigned int    freq;
};

static struct lpj_info global_lpj_ref;
#endif

/* For switcher */
static unsigned int freq_min[CL_END] __read_mostly;	/* Minimum (Big/Little) clock frequency */
static unsigned int freq_max[CL_END] __read_mostly;	/* Maximum (Big/Little) clock frequency */

static struct exynos_dvfs_info *exynos_info[CL_END];
static unsigned int volt_offset;
static struct cpufreq_freqs *freqs[CL_END];

static DEFINE_MUTEX(cpufreq_lock);
static DEFINE_MUTEX(cpufreq_scale_lock);

bool exynos_cpufreq_init_done;
static bool suspend_prepared = false;
#ifdef CONFIG_PM
#ifdef CONFIG_SCHED_HMP
static bool hmp_boosted = false;
#endif
static bool cluster1_hotplugged = false;
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
static int self_discharging;
#endif

/* Include CPU mask of each cluster */
cluster_type exynos_boot_cluster;
static cluster_type boot_cluster;
static struct cpumask cluster_cpus[CL_END];

DEFINE_PER_CPU(cluster_type, cpu_cur_cluster);

static struct pm_qos_constraints core_max_qos_const[CL_END];

static struct pm_qos_request boot_min_qos[CL_END];
static struct pm_qos_request boot_max_qos[CL_END];
static struct pm_qos_request core_min_qos[CL_END];
static struct pm_qos_request core_max_qos[CL_END];
static struct pm_qos_request core_min_qos_real[CL_END];
static struct pm_qos_request core_max_qos_real[CL_END];
static struct pm_qos_request exynos_mif_qos[CL_END];
static struct pm_qos_request ipa_max_qos[CL_END];
static struct pm_qos_request reboot_max_qos[CL_END];
#if defined(CONFIG_SEC_PM) && defined(CONFIG_MUIC_NOTIFIER)
static struct pm_qos_request jig_boot_max_qos[CL_END];
#endif

static int qos_max_class[CL_END] = {PM_QOS_CLUSTER0_FREQ_MAX, PM_QOS_CLUSTER1_FREQ_MAX};
static int qos_min_class[CL_END] = {PM_QOS_CLUSTER0_FREQ_MIN, PM_QOS_CLUSTER1_FREQ_MIN};
//static int qos_max_default_value[CL_END] = {PM_QOS_CLUSTER0_FREQ_MAX_DEFAULT_VALUE, PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE};
static int qos_min_default_value[CL_END] = {PM_QOS_CLUSTER0_FREQ_MIN_DEFAULT_VALUE, PM_QOS_CLUSTER1_FREQ_MIN_DEFAULT_VALUE};

/* For limit number of online cpus through cpuhotplug */
struct pm_qos_request cpufreq_cpu_hotplug_max_request;

/*
 * CPUFREQ init notifier
 */
static BLOCKING_NOTIFIER_HEAD(exynos_cpufreq_init_notifier_list);

int exynos_cpufreq_init_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_cpufreq_init_notifier_list, nb);
}
EXPORT_SYMBOL(exynos_cpufreq_init_register_notifier);

int exynos_cpufreq_init_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_cpufreq_init_notifier_list, nb);
}
EXPORT_SYMBOL(exynos_cpufreq_init_unregister_notifier);

static int exynos_cpufreq_init_notify_call_chain(unsigned long val)
{
	int ret = blocking_notifier_call_chain(&exynos_cpufreq_init_notifier_list, val, NULL);

	return notifier_to_errno(ret);
}

static BLOCKING_NOTIFIER_HEAD(exynos_cpufreq_smpl_warn_notifier_list);

static int exynos_cpufreq_smpl_warn_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&exynos_cpufreq_smpl_warn_notifier_list, nb);
}

static int exynos_cpufreq_smpl_warn_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&exynos_cpufreq_smpl_warn_notifier_list, nb);
}

int exynos_cpufreq_smpl_warn_notify_call_chain(void)
{
	int ret = blocking_notifier_call_chain(&exynos_cpufreq_smpl_warn_notifier_list, 0, NULL);
	return notifier_to_errno(ret);
}
EXPORT_SYMBOL(exynos_cpufreq_smpl_warn_notify_call_chain);

static int exynos_cpufreq_smpl_warn_notifier_call(
					struct notifier_block *notifer,
					unsigned long event, void *v)
{
	if (!exynos_info[CL_ONE]->en_smpl)
		return NOTIFY_OK;

	if (exynos_info[CL_ONE]->clear_smpl)
		exynos_info[CL_ONE]->clear_smpl();

	pr_info("%s : SMPL_WARN is cleared\n",__func__);

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_smpl_warn_notifier = {
	.notifier_call = exynos_cpufreq_smpl_warn_notifier_call,
};


#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
static void exynos_cpufreq_print_max_freq(void)
{
	int i, idx;

	pr_info("Boost Frequency\n");
	for (i = 1; i <= NR_CLUST1_CPUS; i++) {
		idx = exynos_info[CL_ONE]->max_support_idx_table[i];
		pr_info("Number of onlince core: %d -> max frequency: %d\n",
			i, exynos_info[CL_ONE]->freq_table[idx].frequency);
	}
}

/* input num of online core, return possible max freq */
static unsigned int exynos_cpufreq_get_possible_max_freq(int num_of_online)
{
	int idx;

	if (num_of_online > NR_CLUST1_CPUS) {
		pr_err("Input value is invalied! return possible max frequency\n");
		idx = exynos_info[CL_ONE]->max_support_idx_table[NR_CLUST1_CPUS];
	} else {
		idx = exynos_info[CL_ONE]->max_support_idx_table[num_of_online];
	}

	return  exynos_info[CL_ONE]->freq_table[idx].frequency;
}

static void exynos_cpufreq_verify_possible_freq(int *new_index)
{
	unsigned int cpu;
	unsigned int off_cnt = 0;

	for (cpu = nr_cpu_ids - 1; cpu >= NR_CLUST0_CPUS; cpu--)
		if (!cpu_online(cpu) && !exynos_cpu.power_state(cpu))
			off_cnt++;

	/* Check whether new freq is possible freq */
	if (*new_index >= exynos_info[CL_ONE]->max_support_idx_table[NR_CLUST1_CPUS - off_cnt])
		return;

	pr_err("Can't use boost freq (freq %d -> %d, big off_cnt %d\n",
			freqs[CL_ONE]->old, freqs[CL_ONE]->new, off_cnt);
	BUG();

	/* change index and new_freq to possible value */
	*new_index = exynos_info[CL_ONE]->max_support_idx_table[NR_CLUST1_CPUS - off_cnt];
	freqs[CL_ONE]->new = exynos_info[CL_ONE]->freq_table[*new_index].frequency;
}
#endif

static unsigned int get_limit_voltage(unsigned int voltage)
{
	BUG_ON(!voltage);
	if (voltage > LIMIT_COLD_VOLTAGE)
		return voltage;

	if (voltage + volt_offset > LIMIT_COLD_VOLTAGE)
		return LIMIT_COLD_VOLTAGE;

	if (ENABLE_MIN_COLD && volt_offset
		&& (voltage + volt_offset) < MIN_COLD_VOLTAGE)
		return MIN_COLD_VOLTAGE;

	return voltage + volt_offset;
}

static void init_cpumask_cluster_set(cluster_type cluster)
{
	unsigned int i;
	cluster_type set_cluster;

	for_each_cpu(i, cpu_possible_mask) {
		set_cluster = cluster;

		if ((cluster == CL_ZERO) && (i >= NR_CLUST0_CPUS)) {
			set_cluster = CL_ONE;
		} else if ((cluster == CL_ONE) && (i >= NR_CLUST1_CPUS)) {
			set_cluster = CL_ZERO;
		}

		cpumask_set_cpu(i, &cluster_cpus[set_cluster]);
		per_cpu(cpu_cur_cluster, i) = set_cluster;
	}
}

/*
 * get_cur_cluster - return current cluster
 *
 * You may reference this fuction directly, but it cannot be
 * standard of judging current cluster. If you make a decision
 * of operation by this function, it occurs system hang.
 */
static cluster_type get_cur_cluster(unsigned int cpu)
{
	return per_cpu(cpu_cur_cluster, cpu);
}

static unsigned int clk_get_freq(cluster_type cl)
{
	if (exynos_info[cl]->get_freq)
		return exynos_info[cl]->get_freq();

	else if (exynos_info[cl]->cpu_clk)
		return clk_get_rate(exynos_info[cl]->cpu_clk);

	pr_err("%s: clk_get_freq failed\n", cl? "CL_ONE" : "CL_ZERO");

	return 0;
}

static void set_boot_freq(cluster_type cluster)
{

	if (exynos_info[cluster]) {
		exynos_info[cluster]->boot_freq
				= clk_get_freq(cluster);
		pr_info("Cluster[%d] BootFreq: %dKHz\n", cluster,
						exynos_info[cluster]->boot_freq);
	}

	return;
}

static void set_resume_freq(cluster_type cluster)
{

	if (exynos_info[cluster])
		exynos_info[cluster]->resume_freq
				= clk_get_freq(cluster);
}

static unsigned int get_freq_volt(int cluster, unsigned int target_freq, int *idx)
{
	int index;
	int i;

	struct cpufreq_frequency_table *table = exynos_info[cluster]->freq_table;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END) {
		if (exynos_info[cluster]->boot_freq_idx != -1 &&
			target_freq == exynos_info[cluster]->boot_freq)
			index = exynos_info[cluster]->boot_freq_idx;
		else
			return -EINVAL;
	}

	if (idx)
		*idx = index;

	return exynos_info[cluster]->volt_table[index];
}

static unsigned int get_boot_freq(unsigned int cluster)
{
	if (exynos_info[cluster] == NULL)
		return 0;

	return exynos_info[cluster]->boot_freq;
}

static unsigned int get_boot_volt(int cluster)
{
	unsigned int boot_freq = get_boot_freq(cluster);

	return get_freq_volt(cluster, boot_freq, NULL);
}

static unsigned int get_resume_freq(unsigned int cluster)
{
	if (exynos_info[cluster] == NULL) {
		BUG_ON(1);
		return 0;
	}
	return exynos_info[cluster]->resume_freq;
}

int exynos_verify_speed(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	return cpufreq_frequency_table_verify(policy,
				exynos_info[cur]->freq_table);
}

struct cpufreq_frequency_table *cpufreq_get_info_table(unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_frequency_table *freq_table
					= exynos_info[cur]->freq_table;

	return freq_table;
}

unsigned int exynos_getspeed_cluster(cluster_type cluster)
{
	return clk_get_freq(cluster);
}

unsigned int exynos_getspeed(unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);

	if (check_cluster_idle_state(cpu))
		return freqs[cur]->old;
	else
		return exynos_getspeed_cluster(cur);
}

static unsigned int exynos_get_safe_volt(unsigned int old_index,
					unsigned int new_index,
					unsigned int cur)
{
	unsigned int safe_arm_volt = 0;
	struct cpufreq_frequency_table *freq_table
					= exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * reguired voltage level
	 */
	if (exynos_info[cur]->need_apll_change != NULL) {
		if (exynos_info[cur]->need_apll_change(old_index, new_index) &&
			(freq_table[new_index].frequency < exynos_info[cur]->mpll_freq_khz) &&
			(freq_table[old_index].frequency < exynos_info[cur]->mpll_freq_khz)) {
				safe_arm_volt = volt_table[exynos_info[cur]->pll_safe_idx];
		}
	}

	return safe_arm_volt;
}

/* Determine valid target frequency using freq_table */
int exynos5_frequency_table_target(struct cpufreq_policy *policy,
				   struct cpufreq_frequency_table *table,
				   unsigned int target_freq,
				   unsigned int relation,
				   unsigned int *index)
{
	unsigned int i;

	if (!cpu_online(policy->cpu))
		return -EINVAL;

	for (i = 0; (table[i].frequency != CPUFREQ_TABLE_END); i++) {
		unsigned int freq = table[i].frequency;
		if (freq == CPUFREQ_ENTRY_INVALID)
			continue;

		if (target_freq == freq) {
			*index = i;
			break;
		}
	}

	if (table[i].frequency == CPUFREQ_TABLE_END) {
		unsigned int cur = get_cur_cluster(policy->cpu);
		if (exynos_info[cur]->boot_freq_idx != -1 &&
			target_freq == exynos_info[cur]->boot_freq)
			*index = exynos_info[cur]->boot_freq_idx;
		else
			return -EINVAL;
	}

	return 0;
}

static int exynos_set_voltage(unsigned int cur_index,
								unsigned int new_index,
								unsigned int volt,
								unsigned int cluster)
{
	struct regulator *regulator = exynos_info[cluster]->regulator;
	struct cpufreq_frequency_table *freq_table = exynos_info[cluster]->freq_table;
	bool set_abb_first_than_volt = false;
	int ret = 0;

	if (exynos_info[cluster]->abb_table)
		set_abb_first_than_volt = is_set_abb_first(CLUSTER_ID(cluster),
										freq_table[cur_index].frequency,
										freq_table[new_index].frequency);

	if (!set_abb_first_than_volt) {
		ret = regulator_set_voltage(regulator, volt, volt + VOLT_RANGE_STEP);
		if (ret)
			goto out;

		exynos_info[cluster]->cur_volt = volt;
	}

	if (exynos_info[cluster]->abb_table)
		set_match_abb(CLUSTER_ID(cluster), exynos_info[cluster]->abb_table[new_index]);

	if (set_abb_first_than_volt) {
		ret = regulator_set_voltage(regulator, volt, volt + VOLT_RANGE_STEP);
		if (ret)
			goto out;

		exynos_info[cluster]->cur_volt = volt;
	}

out:
	return ret;
}

static int exynos_cpufreq_scale(unsigned int target_freq, unsigned int cpu)
{
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int *volt_table = exynos_info[cur]->volt_table;
	struct cpufreq_policy *policy = cpufreq_cpu_get(cpu);
	unsigned int new_index, old_index;
	unsigned int volt, safe_volt = 0;
	int ret = 0;
	unsigned int current_freq = freqs[cur]->old;

	if (!policy)
		return -EINVAL;

	freqs[cur]->cpu = cpu;
	freqs[cur]->new = target_freq;

	if (exynos5_frequency_table_target(policy, freq_table,
				freqs[cur]->old, CPUFREQ_RELATION_L, &old_index)) {
		ret = -EINVAL;
		goto put_policy;
	}

	if (exynos5_frequency_table_target(policy, freq_table,
				freqs[cur]->new, CPUFREQ_RELATION_L, &new_index)) {
		ret = -EINVAL;
		goto put_policy;
	}

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	/*
	 * check whether new freq is possilbe
	 * if not change new freq to possible max freq
	 */
	if (cur == CL_ONE &&
		new_index < exynos_info[cur]->max_support_idx_table[NR_CLUST1_CPUS])
		exynos_cpufreq_verify_possible_freq(&new_index);
#endif

	if (old_index == new_index)
		goto put_policy;

	/*
	 * ARM clock source will be changed APLL to MPLL temporary
	 * To support this level, need to control regulator for
	 * required voltage level
	 */
	safe_volt = exynos_get_safe_volt(old_index, new_index, cur);
	if (safe_volt)
		safe_volt = get_limit_voltage(safe_volt);

	volt = get_limit_voltage(volt_table[new_index]);

	/* Update policy current frequency */
	cpufreq_freq_transition_begin(policy, freqs[cur]);

	if (old_index > new_index)
		if (exynos_info[cur]->set_int_skew)
			exynos_info[cur]->set_int_skew(new_index);

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	if (!volt_offset)
		exynos_cl_dvfs_stop(CLUSTER_ID(cur), new_index);
#endif

	if (old_index > new_index) {
		if (pm_qos_request_active(&exynos_mif_qos[cur]))
			pm_qos_update_request(&exynos_mif_qos[cur],
					exynos_info[cur]->bus_table[new_index]);
	}

	/* When the new frequency is higher than current frequency */
	if ((old_index > new_index) && !safe_volt) {
		/* Firstly, voltage up to increase frequency */
		ret = exynos_set_voltage(old_index, new_index, volt, cur);
		if (ret)
			goto fail_dvfs;

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(volt);
	}

	if (safe_volt) {
		ret = exynos_set_voltage(old_index, new_index, safe_volt, cur);
		if (ret)
			goto fail_dvfs;

		if (exynos_info[cur]->set_ema)
			exynos_info[cur]->set_ema(safe_volt);
	}

	exynos_info[cur]->set_freq(old_index, new_index);

#ifdef CONFIG_SMP
	if (!global_lpj_ref.freq) {
		global_lpj_ref.ref = loops_per_jiffy;
		global_lpj_ref.freq = freqs[cur]->old;
	}

#ifndef CONFIG_ARM64
	loops_per_jiffy = cpufreq_scale(global_lpj_ref.ref,
			global_lpj_ref.freq, freqs[cur]->new);
#endif
#endif

	cpufreq_freq_transition_end(policy, freqs[cur], 0);

	/* When the new frequency is lower than current frequency */
	if ((old_index < new_index) || ((old_index > new_index) && safe_volt)) {
		/* down the voltage after frequency change */
		if (exynos_info[cur]->set_ema)
			 exynos_info[cur]->set_ema(volt);

		ret = exynos_set_voltage(old_index, new_index, volt, cur);
		if (ret) {
			/* save current frequency as frequency is changed*/
			freqs[cur]->old = target_freq;
			goto put_policy;
		}
	}

	if (old_index < new_index) {
		if (pm_qos_request_active(&exynos_mif_qos[cur]))
			pm_qos_update_request(&exynos_mif_qos[cur],
					exynos_info[cur]->bus_table[new_index]);
	}

	if (old_index < new_index)
		if (exynos_info[cur]->set_int_skew)
			exynos_info[cur]->set_int_skew(new_index);

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	if (!volt_offset)
		exynos_cl_dvfs_start(CLUSTER_ID(cur));
#endif

	cpufreq_cpu_put(policy);

	return 0;

fail_dvfs:
	cpufreq_freq_transition_end(policy, freqs[cur], ret);

	/* Recover old freq when voltage set failed */
	freqs[cur]->old = current_freq;

put_policy:
	cpufreq_cpu_put(policy);

	return ret;
}

void exynos_set_max_freq(int max_freq, unsigned int cpu)
{
	cluster_type cluster = get_cur_cluster(cpu);

	if (pm_qos_request_active(&ipa_max_qos[cluster]))
		pm_qos_update_request(&ipa_max_qos[cluster], max_freq);
	else
		pm_qos_add_request(&ipa_max_qos[cluster], qos_max_class[cluster], max_freq);
}

struct cpu_power_info {
	unsigned int load[4];
	unsigned int freq;
	cluster_type cluster;
	unsigned int temp;
};

unsigned int get_power_value(struct cpu_power_info *power_info)
{
	/* ps : power_static (mW)
         * pd : power_dynamic (mW)
	 */
	u64 temp;
	u64 pd_tot;
	unsigned int ps, total_power;
	unsigned int volt, maxfreq;
	int i, coeff;
	struct cpufreq_frequency_table *freq_table;
	unsigned int *volt_table;
	unsigned int freq = power_info->freq / 1000;
	/* converting frequency unit as MHz */
	cluster_type cluster = power_info->cluster;

	if (cluster == CL_ONE || cluster == CL_ZERO) {
		coeff = (cluster ? POWER_COEFF_15P : POWER_COEFF_7P);
		freq_table = exynos_info[cluster]->freq_table;
		volt_table = exynos_info[cluster]->volt_table;
		maxfreq = exynos_info[cluster]->freq_table[exynos_info[cluster]->max_support_idx].frequency;
	} else {
		BUG_ON(1);
	}

	if (power_info->freq < freq_min[power_info->cluster]) {
		pr_warn("%s: freq: %d for cluster: %d is less than min. freq: %d\n",
			__func__, power_info->freq, power_info->cluster,
			freq_min[power_info->cluster]);
		power_info->freq = freq_min[power_info->cluster];
	}

	if (power_info->freq > maxfreq) {
		pr_warn("%s: freq: %d for cluster: %d is greater than max. freq: %d\n",
			__func__, power_info->freq, power_info->cluster,
			maxfreq);
		power_info->freq = maxfreq;
	}

	for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
		if (power_info->freq == freq_table[i].frequency)
			break;
	}

	volt = volt_table[i] / 10000;

	/* TODO: pde_p is not linear to load, need to update equation */
	pd_tot = 0;
	temp = ((u64)(coeff * freq * volt * volt)) / 1000000;
	/* adding an extra 0 for division in order to compensate for the frequency unit change */
	for (i = 0; i < 4; i++) {
		if (power_info->load[i] > 100) {
			pr_err("%s: Load should be a percent value\n", __func__);
			BUG_ON(1);
		}
		pd_tot += temp * (power_info->load[i]+1);
	}
	total_power = (unsigned int)pd_tot / (unsigned int)100;

	/* pse = alpha ~ volt ~ temp */
	/* TODO: Update voltage, temperature variant PSE */
	ps = 0;

	total_power += ps;

	return total_power;
}

int get_real_max_freq(cluster_type cluster)
{
	return freq_max[cluster];
}

static void exynos_qos_nop(void *info)
{
}

static int exynos_enable_cpd(int cpu)
{
	release_cpd();

	return 1;
}

static int exynos_disable_cpd(int cpu)
{
	block_cpd();

	if (check_cluster_idle_state(cpu))
		smp_call_function_single(cpu, exynos_qos_nop, NULL, 0);

	return 1;
}

unsigned int g_clamp_cpufreqs[CL_END];

static unsigned int exynos_verify_pm_qos_limit(struct cpufreq_policy *policy,
		unsigned int target_freq, cluster_type cur)
{
#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0)
		return target_freq;
#endif
	target_freq = max((unsigned int)pm_qos_request(qos_min_class[cur]), target_freq);
	target_freq = min((unsigned int)pm_qos_request(qos_max_class[cur]), target_freq);
	target_freq = min(g_clamp_cpufreqs[cur], target_freq); /* add IPA clamp */

	return target_freq;
}

/* Set clock frequency */
static int exynos_target(struct cpufreq_policy *policy,
			  unsigned int target_freq,
			  unsigned int relation)
{
	cluster_type cur = get_cur_cluster(policy->cpu);
	struct cpufreq_frequency_table *freq_table = exynos_info[cur]->freq_table;
	unsigned int index;
	int ret = 0;
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA:%s:%d Called by %x, with target_freq %d", __PRETTY_FUNCTION__, __LINE__,
			(unsigned int) __builtin_return_address (0), target_freq);
#endif

	mutex_lock(&cpufreq_lock);

	if (exynos_info[cur]->blocked)
		goto out;

	if (target_freq == 0)
		target_freq = policy->min;

	if (exynos_info[cur]->check_smpl && exynos_info[cur]->check_smpl())
		goto out;

	/* if PLL bypass, frequency scale is skip */
	if (exynos_getspeed(policy->cpu) <= 24000)
		goto out;

	/* verify old frequency */
	if (freqs[cur]->old != exynos_getspeed(policy->cpu)) {
		printk("oops, sombody change clock  old clk:%d, cur clk:%d \n", freqs[cur]->old, exynos_getspeed(policy->cpu));
		BUG_ON(freqs[cur]->old != exynos_getspeed(policy->cpu));
	}

	/* verify pm_qos_lock */
	target_freq = exynos_verify_pm_qos_limit(policy, target_freq, cur);

#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA:%s:%d will apply %d ", __PRETTY_FUNCTION__, __LINE__, target_freq);
#endif

	if (cpufreq_frequency_table_target(policy, freq_table,
				target_freq, relation, &index)) {
		ret = -EINVAL;
		goto out;
	}

	target_freq = freq_table[index].frequency;
	if (target_freq > policy->max)
		target_freq = policy->max;

	/* disable cluster power down during scale */
	if (cur == CL_ONE)
		exynos_disable_cpd(policy->cpu);

	pr_debug("%s[%d]: new_freq[%d], index[%d]\n",
				__func__, cur, target_freq, index);

	exynos_ss_freq(cur, freqs[cur]->old, target_freq, ESS_FLAG_IN);
	/* frequency and volt scaling */
	ret = exynos_cpufreq_scale(target_freq, policy->cpu);
	exynos_ss_freq(cur, freqs[cur]->old, target_freq, ESS_FLAG_OUT);

	/* enable cluster power down  */
	if (cur == CL_ONE)
		exynos_enable_cpd(policy->cpu);

	if (ret < 0)
		goto out;

	/* save current frequency */
	freqs[cur]->old = target_freq;

out:

	mutex_unlock(&cpufreq_lock);

	return ret;
}

void ipa_set_clamp(int cpu, unsigned int clamp_freq, unsigned int gov_target)
{
	unsigned int freq = 0;
	unsigned int new_freq = 0;
	unsigned int prev_clamp_freq = 0;
	unsigned int cur = get_cur_cluster(cpu);
	struct cpufreq_policy *policy;

	prev_clamp_freq = g_clamp_cpufreqs[cur];
	g_clamp_cpufreqs[cur] = clamp_freq;

	if (cur == CL_ONE)
		gov_target = max((unsigned int)pm_qos_request(PM_QOS_CLUSTER1_FREQ_MIN), gov_target);
	else
		gov_target = max((unsigned int)pm_qos_request(PM_QOS_CLUSTER0_FREQ_MIN), gov_target);

	new_freq = min(clamp_freq, gov_target);
	freq = exynos_getspeed(cpu);

	if ((freq <= clamp_freq) && (prev_clamp_freq >= clamp_freq))
		return;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		return;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		return;
	}
#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->governor->name, "userspace") == 0)
			|| strcmp(policy->governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		return;
	}
#endif
#ifdef CONFIG_CPU_THERMAL_IPA_DEBUG
	trace_printk("IPA: %s:%d: set clamps for cpu %d to %d (curr was %d)",
		     __PRETTY_FUNCTION__, __LINE__, cpu, clamp_freq, freq);
#endif

	__cpufreq_driver_target(policy, new_freq, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);
}

#ifdef CONFIG_PM
static int exynos_cpufreq_suspend(struct cpufreq_policy *policy)
{
	return 0;
}

static int exynos_cpufreq_resume(struct cpufreq_policy *policy)
{
	int cl, cur;

	for (cl = 0; cl < CL_END; cl++)
		freqs[cl]->old = exynos_getspeed_cluster(cl);

	/* Update policy->cur value to resume frequency */
	cur = get_cur_cluster(policy->cpu);
	policy->cur = exynos_getspeed_cluster(cur);

	return 0;
}
#endif

static int __cpuinit exynos_cpufreq_cpu_up_notifier(struct notifier_block *notifier,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct cpumask mask;
	int cluster;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_ONLINE:
			cluster = get_cur_cluster(cpu);
			if (cluster == CL_ONE) {
				cpumask_and(&mask, cpu_coregroup_mask(cpu), cpu_online_mask);
				if (cpumask_weight(&mask) == 1)
					pm_qos_update_request(&boot_max_qos[cluster], freq_max[cluster]);
			}
			break;
		}
	}

	return NOTIFY_OK;
}

static int __cpuinit exynos_cpufreq_cpu_down_notifier(struct notifier_block *notifier,
					unsigned long action, void *hcpu)
{
	unsigned int cpu = (unsigned long)hcpu;
	struct device *dev;
	struct cpumask mask;
	int cluster;

	if (suspend_prepared)
		return NOTIFY_OK;

	dev = get_cpu_device(cpu);
	if (dev) {
		switch (action) {
		case CPU_DOWN_PREPARE:
			cluster = get_cur_cluster(cpu);
			if (cluster == CL_ONE) {
				cpumask_and(&mask, cpu_coregroup_mask(cpu), cpu_online_mask);
				if (cpumask_weight(&mask) == 1)
					pm_qos_update_request(&boot_max_qos[cluster], freq_min[cluster]);
			}
			break;
		case CPU_DOWN_PREPARE_FROZEN:
		case CPU_DOWN_LATE_PREPARE:
			mutex_lock(&cpufreq_lock);
			exynos_info[CL_ZERO]->blocked = true;
			exynos_info[CL_ONE]->blocked = true;
			mutex_unlock(&cpufreq_lock);
			break;
		case CPU_DOWN_FAILED:
		case CPU_DOWN_FAILED_FROZEN:
		case CPU_DEAD:
			mutex_lock(&cpufreq_lock);
			exynos_info[CL_ZERO]->blocked = false;
			exynos_info[CL_ONE]->blocked = false;
			mutex_unlock(&cpufreq_lock);
			break;
		}
	}
	return NOTIFY_OK;
}

static struct notifier_block __refdata exynos_cpufreq_cpu_up_nb = {
	.notifier_call = exynos_cpufreq_cpu_up_notifier,
	.priority = INT_MIN,
};

/*
 * This notifier should be perform before
 * cpufreq_cpu_notifier performs.
 */
static struct notifier_block __refdata exynos_cpufreq_cpu_down_nb = {
	.notifier_call = exynos_cpufreq_cpu_down_notifier,
	.priority = INT_MIN + 2,
};

/*
 * exynos_cpufreq_pm_notifier - block CPUFREQ's activities in suspend-resume
 *			context
 * @notifier
 * @pm_event
 * @v
 *
 * While cpufreq_disable == true, target() ignores every frequency but
 * boot_freq. The boot_freq value is the initial frequency,
 * which is set by the bootloader. In order to eliminate possible
 * inconsistency in clock values, we save and restore frequencies during
 * suspend and resume and block CPUFREQ activities. Note that the standard
 * suspend/resume cannot be used as they are too deep (syscore_ops) for
 * regulator actions.
 */
static int exynos_cpufreq_pm_notifier(struct notifier_block *notifier,
				       unsigned long pm_event, void *v)
{
	struct cpufreq_frequency_table *freq_table = NULL;
	unsigned int bootfreq;
	unsigned int abb_freq = 0;
	bool set_abb_first_than_volt = false;
	int volt, i, cl;
	int cl_index;

	switch (pm_event) {
	case PM_SUSPEND_PREPARE:
		/* Fix min/max freq to bootfreq for boot cluster */
		pm_qos_update_request(&boot_min_qos[CL_ZERO],
					exynos_info[CL_ZERO]->boot_freq);
		pm_qos_update_request(&boot_max_qos[CL_ZERO],
					exynos_info[CL_ZERO]->boot_freq);

		pm_qos_update_request(&boot_max_qos[CL_ONE], freq_min[CL_ONE]);

		mutex_lock(&cpufreq_lock);
		exynos_info[CL_ZERO]->blocked = true;
		exynos_info[CL_ONE]->blocked = true;

		for (cl = 0; cl < CL_END; cl++) {
			bootfreq = get_resume_freq(cl);

			volt = max(get_boot_volt(cl), get_freq_volt(cl, freqs[cl]->old, &cl_index));
			if ( volt <= 0) {
				printk("oops, strange voltage %s -> boot volt:%d, get_freq_volt:%d, freq:%d \n",
					(cl ? "CL_ONE" : "CL_ZERO"), get_boot_volt(cl),
					get_freq_volt(cl, freqs[cl]->old, &cl_index), freqs[cl]->old);
				BUG_ON(volt <= 0);
			}
			volt = get_limit_voltage(volt);

			if (exynos_info[cl]->abb_table)
				set_abb_first_than_volt = is_set_abb_first(CLUSTER_ID(cl), freqs[cl]->old, bootfreq);
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			exynos_cl_dvfs_stop(CLUSTER_ID(cl), cl_index);
#endif
			if (!set_abb_first_than_volt)
				if (regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP))
					goto err;

			exynos_info[cl]->cur_volt = volt;

			if (exynos_info[cl]->abb_table) {
				abb_freq = max(bootfreq, freqs[cl]->old);
				freq_table = exynos_info[cl]->freq_table;
				for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
					if (freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
						continue;
					if (freq_table[i].frequency == abb_freq) {
						set_match_abb(CLUSTER_ID(cl), exynos_info[cl]->abb_table[i]);
						break;
					}
				}
			}

			if (set_abb_first_than_volt)
				if (regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP))
					goto err;

			exynos_info[cl]->cur_volt = volt;

			if (exynos_info[cl]->set_ema)
				exynos_info[cl]->set_ema(volt);
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			exynos_cl_dvfs_start(CLUSTER_ID(cl));
#endif
			set_abb_first_than_volt = false;
		}

		suspend_prepared = true;
		mutex_unlock(&cpufreq_lock);

		pr_debug("PM_SUSPEND_PREPARE for CPUFREQ\n");

		break;
	case PM_POST_SUSPEND:
		pr_debug("PM_POST_SUSPEND for CPUFREQ\n");
		for (cl = 0; cl < CL_END; cl++) {
			set_resume_freq(cl);

			mutex_lock(&cpufreq_lock);
			exynos_info[cl]->blocked = false;
			mutex_unlock(&cpufreq_lock);
		}

		/* Recovery min/max frequency normally for boot cluster */
		pm_qos_update_request(&boot_min_qos[CL_ZERO], freq_min[CL_ZERO]);
		pm_qos_update_request(&boot_max_qos[CL_ZERO], freq_max[CL_ZERO]);

		pm_qos_update_request(&boot_max_qos[CL_ONE], INT_MAX);

		suspend_prepared = false;

		break;
	}
	return NOTIFY_OK;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_nb = {
	.notifier_call = exynos_cpufreq_pm_notifier,
};

#ifdef CONFIG_EXYNOS_THERMAL
static int exynos_cpufreq_tmu_notifier(struct notifier_block *notifier,
				       unsigned long event, void *v)
{
	int volt, cl;
	int *on = v;
	int ret = NOTIFY_OK;
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
	int cl_index = 0;
#endif
	int cold_offset = 0;

	if (event != TMU_COLD)
		return NOTIFY_OK;

	mutex_lock(&cpufreq_lock);
	if (*on) {
		if (volt_offset)
			goto out;
		else
			volt_offset = COLD_VOLT_OFFSET;

		for (cl = 0; cl < CL_END; cl++) {
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			get_freq_volt(cl, freqs[cl]->old, &cl_index);
#endif
			volt = exynos_info[cl]->cur_volt;
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			exynos_cl_dvfs_stop(CLUSTER_ID(cl), cl_index);
#endif
			volt = get_limit_voltage(volt);
			ret = regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP);
			if (ret) {
				ret = NOTIFY_BAD;
				goto out;
			}

			exynos_info[cl]->cur_volt = volt;

			if (exynos_info[cl]->set_ema)
				exynos_info[cl]->set_ema(volt);
		}
	} else {
		if (!volt_offset)
			goto out;
		else {
			cold_offset = volt_offset;
			volt_offset = 0;
		}
		for (cl = 0; cl < CL_END; cl++) {
			volt = get_limit_voltage(exynos_info[cl]->cur_volt - cold_offset);
#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			exynos_cl_dvfs_stop(CLUSTER_ID(cl), cl_index);
#endif
			if (exynos_info[cl]->set_ema)
				exynos_info[cl]->set_ema(volt);

			ret = regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP);
			if (ret) {
				ret = NOTIFY_BAD;
				goto out;
			}

			exynos_info[cl]->cur_volt = volt;

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
			exynos_cl_dvfs_start(CLUSTER_ID(cl));
#endif
		}
	}

out:
	mutex_unlock(&cpufreq_lock);

	return ret;
}

static struct notifier_block exynos_tmu_nb = {
	.notifier_call = exynos_cpufreq_tmu_notifier,
};
#endif

static int exynos_cpufreq_cpu_init(struct cpufreq_policy *policy)
{
	unsigned int cur = get_cur_cluster(policy->cpu);

	pr_debug("%s: cpu[%d]\n", __func__, policy->cpu);

	policy->cur = policy->min = policy->max = exynos_getspeed(policy->cpu);

	cpufreq_table_validate_and_show(policy, exynos_info[cur]->freq_table);

	/* set the transition latency value */
	policy->cpuinfo.transition_latency = 100000;

	if (cpumask_test_cpu(policy->cpu, &cluster_cpus[CL_ONE])) {
		cpumask_copy(policy->cpus, &cluster_cpus[CL_ONE]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CL_ONE]);
	} else {
		cpumask_copy(policy->cpus, &cluster_cpus[CL_ZERO]);
		cpumask_copy(policy->related_cpus, &cluster_cpus[CL_ZERO]);
	}

	return cpufreq_frequency_table_cpuinfo(policy, exynos_info[cur]->freq_table);
}

static struct cpufreq_driver exynos_driver = {
	.flags		= CPUFREQ_STICKY | CPUFREQ_HAVE_GOVERNOR_PER_POLICY,
	.verify		= exynos_verify_speed,
	.target		= exynos_target,
	.get		= exynos_getspeed,
	.init		= exynos_cpufreq_cpu_init,
	.name		= "exynos_cpufreq",
#ifdef CONFIG_PM
	.suspend	= exynos_cpufreq_suspend,
	.resume		= exynos_cpufreq_resume,
#endif
};

/************************** sysfs interface ************************/
#ifdef CONFIG_PM
static ssize_t show_cpufreq_smpl_warn_ctrl(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	ssize_t	nsize = 0;
	int cluster;

	for (cluster = 0; cluster < CL_END; cluster++)
		nsize += snprintf(&buf[nsize], 20, "cl[%d]:%d ", cluster, exynos_info[cluster]->en_smpl);

	nsize += snprintf(&buf[nsize], 2, "\n");

	return nsize;
}

static ssize_t store_cpufreq_smpl_warn_ctrl(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int input, cluster;

	if (!sscanf(buf, "%1d %1d", &cluster, &input))
		return -EINVAL;

	if (!cluster) {
		pr_err("Do not support smpl_warn for little cluster.\n");
		return -EBUSY;
	}

	if (exynos_info[cluster]->check_smpl()) {
		pr_err("Already happened smpl_warn.\n");
		return -EBUSY;
	}

	input ? exynos_info[cluster]->init_smpl() : exynos_info[cluster]->deinit_smpl();
	exynos_info[cluster]->en_smpl = !!input;

	return count;
}

static ssize_t show_cpufreq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	int i;
	ssize_t count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table_cluster1 = exynos_info[CL_ONE]->freq_table;
	struct cpufreq_frequency_table *freq_table_cluster0 = exynos_info[CL_ZERO]->freq_table;

	for (i = 0; freq_table_cluster1[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;
	for (i = 0; freq_table_cluster0[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (tbl_sz == 0)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table_cluster1[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table_cluster1[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
						freq_table_cluster1[i].frequency);
	}

	for (i = 0; freq_table_cluster0[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table_cluster0[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
					freq_table_cluster0[i].frequency / LIMIT_FREQ_DIVIDER);
	}

	count += snprintf(&buf[count - 1], 2, "\n");

	return count - 1;
}

static ssize_t show_cpufreq_min_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	ssize_t	nsize = 0;
	unsigned int cluster1_qos_min = pm_qos_request(qos_min_class[CL_ONE]);
	unsigned int cluster0_qos_min = pm_qos_request(qos_min_class[CL_ZERO]);

#ifdef CONFIG_SCHED_HMP
	if (cluster1_qos_min > 0 && get_hmp_boost())
#else
	if (cluster1_qos_min > 0)
#endif
	{
		if (cluster1_qos_min > freq_max[CL_ONE])
			cluster1_qos_min = freq_max[CL_ONE];
		else if (cluster1_qos_min < freq_min[CL_ONE])
			cluster1_qos_min = freq_min[CL_ONE];
		nsize = snprintf(buf, 10, "%u\n", cluster1_qos_min);
	} else if (cluster0_qos_min > 0) {
		if (cluster0_qos_min > freq_max[CL_ZERO])
			cluster0_qos_min = freq_max[CL_ZERO];
		if (cluster0_qos_min < freq_min[CL_ZERO])
			cluster0_qos_min = freq_min[CL_ZERO];
		nsize = snprintf(buf, 10, "%u\n", cluster0_qos_min / LIMIT_FREQ_DIVIDER);
	} else if (cluster0_qos_min == 0) {
		cluster0_qos_min = freq_min[CL_ZERO];
		nsize = snprintf(buf, 10, "%u\n", cluster0_qos_min / LIMIT_FREQ_DIVIDER);
	}

	return nsize;
}

static ssize_t store_cpufreq_min_limit(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int cluster1_input, cluster0_input;

	if (!sscanf(buf, "%8d", &cluster1_input))
		return -EINVAL;

	if (cluster1_input >= (int)freq_min[CL_ONE]) {
#ifdef CONFIG_SCHED_HMP
		if (!hmp_boosted) {
			if (set_hmp_boost(1) < 0)
				pr_err("%s: failed HMP boost enable\n",
							__func__);
			else
				hmp_boosted = true;
		}
#endif
		cluster1_input = min(cluster1_input, (int)freq_max[CL_ONE]);
		if (exynos_info[CL_ZERO]->boost_freq)
			cluster0_input = exynos_info[CL_ZERO]->boost_freq;
		else
			cluster0_input = core_max_qos_const[CL_ZERO].default_value;
	} else if (cluster1_input < (int)freq_min[CL_ONE]) {
#ifdef CONFIG_SCHED_HMP
		if (hmp_boosted) {
			if (set_hmp_boost(0) < 0)
				pr_err("%s: failed HMP boost disable\n",
							__func__);
			else
				hmp_boosted = false;
		}
#endif
		if (cluster1_input < 0) {
			cluster1_input = qos_min_default_value[CL_ONE];
			cluster0_input = qos_min_default_value[CL_ZERO];
		} else {
			cluster0_input = cluster1_input * LIMIT_FREQ_DIVIDER;
			if (cluster0_input > 0)
				cluster0_input = min(cluster0_input, (int)freq_max[CL_ZERO]);
			cluster1_input = qos_min_default_value[CL_ONE];
		}
	}

	if (pm_qos_request_active(&core_min_qos[CL_ONE]))
		pm_qos_update_request(&core_min_qos[CL_ONE], cluster1_input);
	if (pm_qos_request_active(&core_min_qos[CL_ZERO]))
		pm_qos_update_request(&core_min_qos[CL_ZERO], cluster0_input);

	return count;
}

static ssize_t show_cpufreq_max_limit(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	ssize_t	nsize = 0;
	unsigned int cluster1_qos_max = pm_qos_request(qos_max_class[CL_ONE]);
	unsigned int cluster0_qos_max = pm_qos_request(qos_max_class[CL_ZERO]);

	if (cluster1_qos_max > 0) {
		if (cluster1_qos_max < freq_min[CL_ONE])
			cluster1_qos_max = freq_min[CL_ONE];
		else if (cluster1_qos_max > freq_max[CL_ONE])
			cluster1_qos_max = freq_max[CL_ONE];
		nsize = snprintf(buf, 10, "%u\n", cluster1_qos_max);
	} else if (cluster0_qos_max > 0) {
		if (cluster0_qos_max < freq_min[CL_ZERO])
			cluster0_qos_max = freq_min[CL_ZERO];
		if (cluster0_qos_max > freq_max[CL_ZERO])
			cluster0_qos_max = freq_max[CL_ZERO];
		nsize = snprintf(buf, 10, "%u\n", cluster0_qos_max / LIMIT_FREQ_DIVIDER);
	} else if (cluster0_qos_max == 0) {
		cluster0_qos_max = freq_min[CL_ZERO];
		nsize = snprintf(buf, 10, "%u\n", cluster0_qos_max / LIMIT_FREQ_DIVIDER);
	}

	return nsize;
}

static void enable_nonboot_cluster_cpus(void)
{
	pm_qos_update_request(&cpufreq_cpu_hotplug_max_request, NR_CPUS);
}

static void disable_nonboot_cluster_cpus(void)
{
	pm_qos_update_request(&cpufreq_cpu_hotplug_max_request, NR_CLUST1_CPUS);
}

static ssize_t store_cpufreq_max_limit(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	int cluster1_input, cluster0_input;

	if (!sscanf(buf, "%8d", &cluster1_input))
		return -EINVAL;

	if (cluster1_input >= (int)freq_min[CL_ONE]) {
		if (cluster1_hotplugged) {
			enable_nonboot_cluster_cpus();
			cluster1_hotplugged = false;
		}

		cluster1_input = max(cluster1_input, (int)freq_min[CL_ONE]);
		cluster0_input = core_max_qos_const[CL_ZERO].default_value;
	} else if (cluster1_input < (int)freq_min[CL_ONE]) {
		if (cluster1_input < 0) {
			if (cluster1_hotplugged) {
				enable_nonboot_cluster_cpus();
				cluster1_hotplugged = false;
			}

			cluster1_input = core_max_qos_const[CL_ONE].default_value;
			cluster0_input = core_max_qos_const[CL_ZERO].default_value;
		} else {
			cluster0_input = cluster1_input * LIMIT_FREQ_DIVIDER;
			if (cluster0_input > 0)
				cluster0_input = max(cluster0_input, (int)freq_min[CL_ZERO]);
			cluster1_input = qos_min_default_value[CL_ONE];

			if (!cluster1_hotplugged) {
				disable_nonboot_cluster_cpus();
				cluster1_hotplugged = true;
			}
		}
	}

	if (pm_qos_request_active(&core_max_qos[CL_ONE]))
		pm_qos_update_request(&core_max_qos[CL_ONE], cluster1_input);
	if (pm_qos_request_active(&core_max_qos[CL_ZERO]))
		pm_qos_update_request(&core_max_qos[CL_ZERO], cluster0_input);

	return count;
}
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
static ssize_t show_cpufreq_self_discharging(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return sprintf(buf, "%d\n", self_discharging);
}

static ssize_t store_cpufreq_self_discharging(struct kobject *kobj, struct attribute *attr,
			      const char *buf, size_t count)
{
	int input;

	if (!sscanf(buf, "%d", &input))
		return -EINVAL;

	if (input > 0) {
		self_discharging = input;
		cpu_idle_poll_ctrl(true);
	}
	else {
		self_discharging = 0;
		cpu_idle_poll_ctrl(false);
	}

	return count;
}
#endif

inline ssize_t show_core_freq_table(char *buf, cluster_type cluster)
{
	int i, count = 0;
	size_t tbl_sz = 0, pr_len;
	struct cpufreq_frequency_table *freq_table = exynos_info[cluster]->freq_table;

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++)
		tbl_sz++;

	if (tbl_sz == 0)
		return -EINVAL;

	pr_len = (size_t)((PAGE_SIZE - 2) / tbl_sz);

	for (i = 0; freq_table[i].frequency != CPUFREQ_TABLE_END; i++) {
		if (freq_table[i].frequency != CPUFREQ_ENTRY_INVALID)
			count += snprintf(&buf[count], pr_len, "%d ",
					freq_table[i].frequency);
	}

	count += snprintf(&buf[count], 2, "\n");
	return count;
}

inline ssize_t show_core_freq(char *buf,
				cluster_type cluster,
				bool qos_flag) /* qos_flag : false is Freq_min, true is Freq_max */
{
	int qos_class = (qos_flag ? qos_max_class[cluster] : qos_min_class[cluster]);
	unsigned int qos_value = pm_qos_request(qos_class);
	unsigned int *freq_info = (qos_flag ? freq_max : freq_min);

	if (qos_value == 0)
		qos_value = freq_info[cluster];

	return snprintf(buf, PAGE_SIZE, "%u\n", qos_value);
}

inline ssize_t store_core_freq(const char *buf, size_t count,
				cluster_type cluster,
				bool qos_flag) /* qos_flag : false is Freq_min, true is Freq_max */
{
	struct pm_qos_request *core_qos_real = (qos_flag ? core_max_qos_real : core_min_qos_real);
	/* Need qos_flag's opposite freq_info for comparison */
	unsigned int *freq_info = (qos_flag ? freq_min : freq_max);
	int input;

	if (!sscanf(buf, "%8d", &input))
		return -EINVAL;

	if (input > 0) {
		if (qos_flag)
			input = max(input, (int)freq_info[cluster]);
		else
			input = min(input, (int)freq_info[cluster]);
	}

	if (pm_qos_request_active(&core_qos_real[cluster]))
		pm_qos_update_request(&core_qos_real[cluster], input);

	return count;
}

static ssize_t show_cluster1_freq_table(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return show_core_freq_table(buf, CL_ONE);
}

static ssize_t show_cluster1_min_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return show_core_freq(buf, CL_ONE, false);
}

static ssize_t show_cluster1_max_freq(struct kobject *kobj,
				struct attribute *attr, char *buf)
{
	return show_core_freq(buf, CL_ONE, true);
}

static ssize_t store_cluster1_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	return store_core_freq(buf, count, CL_ONE, false);
}

static ssize_t store_cluster1_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	return store_core_freq(buf, count, CL_ONE, true);
}

static ssize_t show_cluster0_freq_table(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return show_core_freq_table(buf, CL_ZERO);
}

static ssize_t show_cluster0_min_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return show_core_freq(buf, CL_ZERO, false);
}

static ssize_t show_cluster0_max_freq(struct kobject *kobj,
			     struct attribute *attr, char *buf)
{
	return show_core_freq(buf, CL_ZERO, true);
}

static ssize_t store_cluster0_min_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	return store_core_freq(buf, count, CL_ZERO, false);
}

static ssize_t store_cluster0_max_freq(struct kobject *kobj, struct attribute *attr,
					const char *buf, size_t count)
{
	return store_core_freq(buf, count, CL_ZERO, true);
}

define_one_global_ro(cluster1_freq_table);
define_one_global_rw(cluster1_min_freq);
define_one_global_rw(cluster1_max_freq);
define_one_global_ro(cluster0_freq_table);
define_one_global_rw(cluster0_min_freq);
define_one_global_rw(cluster0_max_freq);

static struct attribute *mp_attributes[] = {
	&cluster1_freq_table.attr,
	&cluster1_min_freq.attr,
	&cluster1_max_freq.attr,
	&cluster0_freq_table.attr,
	&cluster0_min_freq.attr,
	&cluster0_max_freq.attr,
	NULL
};

static struct attribute_group mp_attr_group = {
	.attrs = mp_attributes,
	.name = "mp-cpufreq",
};

#ifdef CONFIG_PM
static struct global_attr smpl_warn_ctrl =
		__ATTR(smpl_warn_ctrl, S_IRUGO | S_IWUSR,
			show_cpufreq_smpl_warn_ctrl, store_cpufreq_smpl_warn_ctrl);
static struct global_attr cpufreq_table =
		__ATTR(cpufreq_table, S_IRUGO, show_cpufreq_table, NULL);
static struct global_attr cpufreq_min_limit =
		__ATTR(cpufreq_min_limit, S_IRUGO | S_IWUSR,
			show_cpufreq_min_limit, store_cpufreq_min_limit);
static struct global_attr cpufreq_max_limit =
		__ATTR(cpufreq_max_limit, S_IRUGO | S_IWUSR,
			show_cpufreq_max_limit, store_cpufreq_max_limit);
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
static struct global_attr cpufreq_self_discharging =
		__ATTR(cpufreq_self_discharging, S_IRUGO | S_IWUSR,
			show_cpufreq_self_discharging, store_cpufreq_self_discharging);
#endif

/************************** sysfs end ************************/

static int exynos_cpufreq_reboot_notifier_call(struct notifier_block *this,
				   unsigned long code, void *_cmd)
{
	struct cpufreq_frequency_table *freq_table;
	unsigned int bootfreq;
	unsigned int abb_freq = 0;
	bool set_abb_first_than_volt = false;
	int volt, i, cl, cl_index;

	for (cl = 0; cl < CL_END; cl++) {
		if (exynos_info[cl]->reboot_limit_freq)
			pm_qos_add_request(&reboot_max_qos[cl], qos_max_class[cl],
						exynos_info[cl]->reboot_limit_freq);

		mutex_lock(&cpufreq_lock);
		exynos_info[cl]->blocked = true;
		mutex_unlock(&cpufreq_lock);

		bootfreq = get_boot_freq(cl);

		volt = max(get_boot_volt(cl),
				get_freq_volt(cl, freqs[cl]->old, &cl_index));
		volt = get_limit_voltage(volt);

		if (exynos_info[cl]->abb_table)
			set_abb_first_than_volt = is_set_abb_first(CLUSTER_ID(cl), freqs[cl]->old, max(bootfreq, freqs[cl]->old));

#ifdef CONFIG_EXYNOS_CL_DVFS_CPU
		exynos_cl_dvfs_stop(CLUSTER_ID(cl), cl_index);
#endif
		if (!set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP))
				goto err;

		exynos_info[cl]->cur_volt = volt;

		if (exynos_info[cl]->abb_table) {
			abb_freq = max(bootfreq, freqs[cl]->old);
			freq_table = exynos_info[cl]->freq_table;
			for (i = 0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				if (freq_table[i].frequency == CPUFREQ_ENTRY_INVALID)
					continue;
				if (freq_table[i].frequency == abb_freq) {
					set_match_abb(CLUSTER_ID(cl), exynos_info[cl]->abb_table[i]);
					break;
				}
			}
		}

		if (set_abb_first_than_volt)
			if (regulator_set_voltage(exynos_info[cl]->regulator, volt, volt + VOLT_RANGE_STEP))
				goto err;

		exynos_info[cl]->cur_volt = volt;

		if (exynos_info[cl]->set_ema)
			exynos_info[cl]->set_ema(volt);

		set_abb_first_than_volt = false;
	}

	return NOTIFY_DONE;
err:
	pr_err("%s: failed to set voltage\n", __func__);

	return NOTIFY_BAD;
}

static struct notifier_block exynos_cpufreq_reboot_notifier = {
	.notifier_call = exynos_cpufreq_reboot_notifier_call,
};

static int exynos_cluster1_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CLUST0_CPUS;

	freq = exynos_getspeed(cpu);
	if (freq >= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
			|| strcmp(policy->user_policy.governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cluster1_min_qos_notifier = {
	.notifier_call = exynos_cluster1_min_qos_handler,
	.priority = INT_MAX,
};

static int exynos_cluster1_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? 0 : NR_CLUST0_CPUS;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
			|| strcmp(policy->user_policy.governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif
	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cluster1_max_qos_notifier = {
	.notifier_call = exynos_cluster1_max_qos_handler,
	.priority = INT_MAX,
};

static int exynos_cluster0_min_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CLUST1_CPUS : 0;
	unsigned int threshold_freq;

#if defined(CONFIG_CPU_FREQ_GOV_INTERACTIVE)
	threshold_freq = cpufreq_interactive_get_hispeed_freq(0);
	if (!threshold_freq)
		threshold_freq = 1000000;	/* 1.0GHz */
#else
	threshold_freq = 1000000;	/* 1.0GHz */
#endif

	freq = exynos_getspeed(cpu);
	if (freq >= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
			|| strcmp(policy->user_policy.governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cluster0_min_qos_notifier = {
	.notifier_call = exynos_cluster0_min_qos_handler,
	.priority = INT_MAX,
};

static int exynos_cluster0_max_qos_handler(struct notifier_block *b, unsigned long val, void *v)
{
	int ret;
	unsigned long freq;
	struct cpufreq_policy *policy;
	int cpu = boot_cluster ? NR_CLUST1_CPUS : 0;

	freq = exynos_getspeed(cpu);
	if (freq <= val)
		goto good;

	policy = cpufreq_cpu_get(cpu);

	if (!policy)
		goto bad;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto bad;
	}

#if defined(CONFIG_CPU_FREQ_GOV_USERSPACE) || defined(CONFIG_CPU_FREQ_GOV_PERFORMANCE)
	if ((strcmp(policy->user_policy.governor->name, "userspace") == 0)
			|| strcmp(policy->user_policy.governor->name, "performance") == 0) {
		cpufreq_cpu_put(policy);
		goto good;
	}
#endif

	ret = __cpufreq_driver_target(policy, val, CPUFREQ_RELATION_H);
	cpufreq_cpu_put(policy);

	if (ret < 0)
		goto bad;

good:
	return NOTIFY_OK;
bad:
	return NOTIFY_BAD;
}

static struct notifier_block exynos_cluster0_max_qos_notifier = {
	.notifier_call = exynos_cluster0_max_qos_handler,
	.priority = INT_MAX,
};

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
extern struct cpumask hmp_fast_cpu_mask;
static int big_cpu_cnt;

static int cpufreq_policy_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpufreq_policy *policy = data;
	unsigned long max_freq;

	if (event != CPUFREQ_ADJUST || !policy->cpu)
		return 0;

	max_freq = exynos_cpufreq_get_possible_max_freq(big_cpu_cnt);

	if (policy->max != max_freq)
		cpufreq_verify_within_limits(policy, 0, max_freq);

	return 0;
}

/* Notifier for cpufreq policy change */
static struct notifier_block exynos_cpufreq_policy_nb = {
	.notifier_call = cpufreq_policy_notifier,
};

static int exynos_cpufreq_cpus_notifier(struct notifier_block *nb,
				    unsigned long event, void *data)
{
	struct cpumask mask;
	struct cpumask policy_update_mask;
	unsigned long timeout;

	switch (event) {
	case CPUS_DOWN_COMPLETE:
	case CPUS_UP_PREPARE:
		cpumask_copy(&mask, data);
		cpumask_and(&mask, &mask, &hmp_fast_cpu_mask);
		big_cpu_cnt = cpumask_weight(&mask);

		/*
		 * if policy_update_mask is 0, it means first hotplug in or last hotplug out
		 * of fast_cpu domain
		 */
		cpumask_clear(&policy_update_mask);
		cpumask_and(&policy_update_mask, &hmp_fast_cpu_mask, cpu_online_mask);
		if (!cpumask_weight(&policy_update_mask))
			return NOTIFY_OK;

		/* if cpufreq_update_policy is failed after 50ms, cancel cpu_up */
		timeout = jiffies + msecs_to_jiffies(50);
		while(cpufreq_update_policy(cpumask_first(&policy_update_mask))
							&& event == CPUS_UP_PREPARE) {
			if (time_after(jiffies, timeout))
				return NOTIFY_BAD;
		}
	}

	if (event == CPUS_UP_PREPARE) {
		mutex_lock(&cpufreq_lock);

		if (unlikely(freqs[CL_ONE]->old > exynos_cpufreq_get_possible_max_freq(big_cpu_cnt))) {
			pr_err("frequency(%d) is higher than possible max frequency of big cpu cnt: %d\n",
			freqs[CL_ONE]->old, big_cpu_cnt);
			mutex_unlock(&cpufreq_lock);

			return NOTIFY_BAD;
		}
		mutex_unlock(&cpufreq_lock);
	}

	return NOTIFY_OK;
}

static struct notifier_block exynos_cpufreq_cpus_nb = {
	.notifier_call = exynos_cpufreq_cpus_notifier,
	.priority = INT_MIN,
};
#endif

static int exynos_cpufreq_init(void)
{
	int i, ret = -EINVAL;
	cluster_type cluster;
	struct cpufreq_frequency_table *freq_table;
	struct cpufreq_policy *policy;

	boot_cluster = 0;

	g_clamp_cpufreqs[CL_ONE] = UINT_MAX;
	g_clamp_cpufreqs[CL_ZERO] = UINT_MAX;

	/* Get to boot_cluster_num - 0 for CL_ZERO; 1 for CL_ONE */
	boot_cluster = !MPIDR_AFFINITY_LEVEL(cpu_logical_map(0), 1);
	pr_debug("%s: boot_cluster is %s\n", __func__,
					boot_cluster == CL_ZERO ? "CL_ZERO" : "CL_ONE");
	exynos_boot_cluster = boot_cluster;

	init_cpumask_cluster_set(boot_cluster);

	for (cluster = 0; cluster < CL_END; cluster++) {
		exynos_info[cluster]->regulator = regulator_get(NULL, (cluster ? "vdd_mngs" : "vdd_apollo"));
		if (IS_ERR(exynos_info[cluster]->regulator)) {
			exynos_info[cluster]->regulator = regulator_get(NULL, (cluster ? "vdd_eagle" : "vdd_kfc"));
			if (IS_ERR(exynos_info[cluster]->regulator)) {
				pr_err("%s:failed to get resource vdd_%score\n", __func__,
												(cluster ? "cluster1" : "cluster0"));
				goto err_init;
			}
		}

		if (exynos_info[cluster]->set_freq == NULL) {
			pr_err("%s: No set_freq function (ERR)\n", __func__);
			goto err_init;
		}

		freq_max[cluster] = exynos_info[cluster]->
			freq_table[exynos_info[cluster]->max_support_idx].frequency;

		freq_min[cluster] = exynos_info[cluster]->
			freq_table[exynos_info[cluster]->min_support_idx].frequency;

		set_boot_freq(cluster);
		set_resume_freq(cluster);

		exynos_info[cluster]->cur_volt = regulator_get_voltage(exynos_info[cluster]->regulator);

		/* set initial old frequency */
		freqs[cluster]->old = exynos_getspeed_cluster(cluster);

		/*
		 * boot freq index is needed if minimum supported frequency
		 * greater than boot freq
		 */
			freq_table = exynos_info[cluster]->freq_table;
			exynos_info[cluster]->boot_freq_idx = -1;
			pr_debug("%s Core Max-Min frequency[%d - %d]KHz\n",
				cluster ? "CL_ONE" : "CL_ZERO", freq_max[cluster], freq_min[cluster]);

			for (i = L0; (freq_table[i].frequency != CPUFREQ_TABLE_END); i++) {
				if (exynos_info[cluster]->boot_freq == freq_table[i].frequency) {
					exynos_info[cluster]->boot_freq_idx = i;
					pr_debug("boot frequency[%d] index %d\n",
							exynos_info[cluster]->boot_freq, i);
				}

				if (freq_table[i].frequency > freq_max[cluster] ||
					freq_table[i].frequency < freq_min[cluster])
					freq_table[i].frequency = CPUFREQ_ENTRY_INVALID;
			}

		/* setup default qos constraints */
		core_max_qos_const[cluster].target_value = freq_max[cluster];
		core_max_qos_const[cluster].default_value = freq_max[cluster];
		pm_qos_update_constraints(qos_max_class[cluster], &core_max_qos_const[cluster]);

		pm_qos_add_notifier(qos_min_class[cluster],
							(cluster ? &exynos_cluster1_min_qos_notifier :
							&exynos_cluster0_min_qos_notifier));
		pm_qos_add_notifier(qos_max_class[cluster],
							(cluster ? &exynos_cluster1_max_qos_notifier :
							&exynos_cluster0_max_qos_notifier));

	}

	register_hotcpu_notifier(&exynos_cpufreq_cpu_up_nb);
	register_hotcpu_notifier(&exynos_cpufreq_cpu_down_nb);
	register_pm_notifier(&exynos_cpufreq_nb);
	register_reboot_notifier(&exynos_cpufreq_reboot_notifier);
#ifdef CONFIG_EXYNOS_THERMAL
	exynos_tmu_add_notifier(&exynos_tmu_nb);
#endif

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	cpufreq_register_notifier(&exynos_cpufreq_policy_nb, CPUFREQ_POLICY_NOTIFIER);
	register_cpus_notifier(&exynos_cpufreq_cpus_nb);
	exynos_cpufreq_print_max_freq();
#endif

	/* block frequency scale before acquire boot lock */
#if !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE) && !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_POWERSAVE)
	mutex_lock(&cpufreq_lock);
	exynos_info[CL_ZERO]->blocked = true;
	exynos_info[CL_ONE]->blocked = true;
	mutex_unlock(&cpufreq_lock);
#endif
	if (cpufreq_register_driver(&exynos_driver)) {
		pr_err("%s: failed to register cpufreq driver\n", __func__);
		goto err_cpufreq;
	}

	for (cluster = 0; cluster < CL_END; cluster++) {
		pm_qos_add_request(&core_min_qos[cluster], qos_min_class[cluster],
						qos_min_default_value[cluster]);
		pm_qos_add_request(&core_max_qos[cluster], qos_max_class[cluster],
						core_max_qos_const[cluster].default_value);
		pm_qos_add_request(&core_min_qos_real[cluster], qos_min_class[cluster],
						qos_min_default_value[cluster]);
		pm_qos_add_request(&core_max_qos_real[cluster], qos_max_class[cluster],
						core_max_qos_const[cluster].default_value);

		if (exynos_info[cluster]->boot_cpu_min_qos) {
			pm_qos_add_request(&boot_min_qos[cluster], qos_min_class[cluster],
						qos_min_default_value[cluster]);
			if (!exynos_info[cluster]->boot_cpu_min_qos_timeout)
				exynos_info[cluster]->boot_cpu_min_qos_timeout = 40 * USEC_PER_SEC;

			pm_qos_update_request_timeout(&boot_min_qos[cluster],
						exynos_info[cluster]->boot_cpu_min_qos,
						exynos_info[cluster]->boot_cpu_min_qos_timeout);
		}

		if (exynos_info[cluster]->boot_cpu_max_qos) {
			pm_qos_add_request(&boot_max_qos[cluster], qos_max_class[cluster],
						core_max_qos_const[cluster].default_value);
			if (!exynos_info[cluster]->boot_cpu_max_qos_timeout)
				exynos_info[cluster]->boot_cpu_max_qos_timeout = 40 * USEC_PER_SEC;

			pm_qos_update_request_timeout(&boot_max_qos[cluster],
						exynos_info[cluster]->boot_cpu_max_qos,
						exynos_info[cluster]->boot_cpu_max_qos_timeout);
		}

		if (exynos_info[cluster]->bus_table)
			pm_qos_add_request(&exynos_mif_qos[cluster], PM_QOS_BUS_THROUGHPUT, 0);
	}

	/* unblock frequency scale */
#if !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_PERFORMANCE) && !defined(CONFIG_CPU_FREQ_DEFAULT_GOV_POWERSAVE)
	mutex_lock(&cpufreq_lock);
	exynos_info[CL_ZERO]->blocked = false;
	exynos_info[CL_ONE]->blocked = false;
	mutex_unlock(&cpufreq_lock);
#endif

	/*
	 * forced call cpufreq target function.
	 * If target function is called by interactive governor when blocked cpufreq scale,
	 * interactive governor's target_freq is updated to new_freq. But, frequency is
	 * not changed because blocking cpufreq scale. And if governor request same frequency
	 * after unblocked scale, speedchange_task is not wakeup because new_freq and target_freq
	 * is same.
	 */

	policy = cpufreq_cpu_get(NR_CLUST0_CPUS);
	if (!policy)
		goto err_policy;

	if (!policy->user_policy.governor) {
		cpufreq_cpu_put(policy);
		goto err_policy;
	}

	smp_call_function_single(NR_CLUST0_CPUS, exynos_qos_nop, NULL, 0);
	cpufreq_cpu_put(policy);

	ret = cpufreq_sysfs_create_group(&mp_attr_group);
	if (ret) {
		pr_err("%s: failed to create mp-cpufreq sysfs interface\n", __func__);
		goto err_mp_attr;
	}

#ifdef CONFIG_PM
	ret = sysfs_create_file(power_kobj, &cpufreq_table.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_table sysfs interface\n", __func__);
		goto err_cpu_table;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_min_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_min_limit sysfs interface\n", __func__);
		goto err_cpufreq_min_limit;
	}

	ret = sysfs_create_file(power_kobj, &cpufreq_max_limit.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_max_limit sysfs interface\n", __func__);
		goto err_cpufreq_max_limit;
	}
#endif

#ifdef CONFIG_SW_SELF_DISCHARGING
	ret = sysfs_create_file(power_kobj, &cpufreq_self_discharging.attr);
	if (ret) {
		pr_err("%s: failed to create cpufreq_self_discharging sysfs interface\n", __func__);
		goto err_cpufreq_self_discharging;
	}
#endif

	exynos_cpufreq_init_done = true;
	exynos_cpufreq_init_notify_call_chain(CPUFREQ_INIT_COMPLETE);

	pr_info("%s: MP-CPUFreq Initialization Complete\n", __func__);
	return 0;

#ifdef CONFIG_SW_SELF_DISCHARGING
err_cpufreq_self_discharging:
#ifdef CONFIG_PM
	sysfs_remove_file(power_kobj, &cpufreq_max_limit.attr);
#endif
#endif
#ifdef CONFIG_PM
err_cpufreq_max_limit:
	sysfs_remove_file(power_kobj, &cpufreq_min_limit.attr);
err_cpufreq_min_limit:
	sysfs_remove_file(power_kobj, &cpufreq_table.attr);
err_cpu_table:
	cpufreq_sysfs_remove_group(&mp_attr_group);
#endif
err_policy:
err_mp_attr:
	for (cluster = 0; cluster < CL_END; cluster++) {
		if (exynos_info[cluster]) {
			if (exynos_info[cluster]->bus_table &&
				pm_qos_request_active(&exynos_mif_qos[cluster]))
				pm_qos_remove_request(&exynos_mif_qos[cluster]);

			if (pm_qos_request_active(&boot_max_qos[cluster]))
				pm_qos_remove_request(&boot_max_qos[cluster]);
			if (pm_qos_request_active(&boot_min_qos[cluster]))
				pm_qos_remove_request(&boot_min_qos[cluster]);

			if (pm_qos_request_active(&core_min_qos[cluster]))
				pm_qos_remove_request(&core_min_qos[cluster]);
			if (pm_qos_request_active(&core_max_qos[cluster]))
				pm_qos_remove_request(&core_max_qos[cluster]);
			if (pm_qos_request_active(&core_min_qos_real[cluster]))
				pm_qos_remove_request(&core_min_qos_real[cluster]);
			if (pm_qos_request_active(&core_max_qos_real[cluster]))
				pm_qos_remove_request(&core_max_qos_real[cluster]);
		}
	}
	cpufreq_unregister_driver(&exynos_driver);
err_cpufreq:
	unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	unregister_pm_notifier(&exynos_cpufreq_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_up_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_down_nb);
#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	unregister_cpus_notifier(&exynos_cpufreq_cpus_nb);
#endif
err_init:
	for (cluster = 0; cluster < CL_END; cluster++) {
		/* remove all pm_qos requests */
		if (exynos_info[cluster]) {
			/* Remove pm_qos notifiers */
			pm_qos_remove_notifier(qos_min_class[cluster],
								(cluster ? &exynos_cluster1_min_qos_notifier :
								&exynos_cluster0_min_qos_notifier));
			pm_qos_remove_notifier(qos_max_class[cluster],
								(cluster ? &exynos_cluster1_max_qos_notifier :
								&exynos_cluster0_max_qos_notifier));

			/* Release regulater handles */
			if (exynos_info[cluster]->regulator)
				regulator_put(exynos_info[cluster]->regulator);
		}
	}
	pr_err("%s: failed initialization\n", __func__);

	return ret;
}

static const struct of_device_id exynos_mp_cpufreq_match[] = {
	{
		.compatible = "samsung,exynos-mp-cpufreq",
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mp_cpufreq);

static struct platform_device_id exynos_mp_cpufreq_driver_ids[] = {
	{
		.name		= "exynos-mp-cpufreq",
	},
	{ },
};
MODULE_DEVICE_TABLE(platform, exynos_mp_cpufreq_driver_ids);

#if defined(CONFIG_ECT)
static void exynos_mp_cpufreq_parse_minlock(char *cluster_name, unsigned int frequency, unsigned int *mif_freq)
{
	int i;
	void *cpumif_block;
	struct ect_minlock_domain *domain;

	cpumif_block = ect_get_block(BLOCK_MINLOCK);
	if (cpumif_block == NULL)
		return;

	domain = ect_minlock_get_domain(cpumif_block, cluster_name);
	if (domain == NULL)
		return;

	for (i = 0; i < domain->num_of_level; ++i) {
		if (domain->level[i].main_frequencies == frequency) {
			*mif_freq = domain->level[i].sub_frequencies;
			return;
		}
	}

	*mif_freq = 0;
	return;
}

static int exynos_mp_cpufreq_parse_frequency(char *cluster_name, struct exynos_dvfs_info *ptr)
{
	int i;
	void *dvfs_block;
	struct ect_dvfs_domain *domain;

	dvfs_block = ect_get_block(BLOCK_DVFS);
	if (dvfs_block == NULL)
		return -ENODEV;

	domain = ect_dvfs_get_domain(dvfs_block, cluster_name);
	if (domain == NULL)
		return -ENODEV;

	ptr->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
				* (domain->num_of_level + 1), GFP_KERNEL);
	if (ptr->freq_table == NULL) {
		pr_err("%s: failed to allocate memory for freq_table\n", __func__);
		return -ENOMEM;
	}

	ptr->volt_table = kzalloc(sizeof(unsigned int)
				* domain->num_of_level, GFP_KERNEL);
	if (ptr->volt_table == NULL) {
		pr_err("%s: failed to allocate for volt_table\n", __func__);
		return -ENOMEM;
	}

	ptr->bus_table = kzalloc(sizeof(unsigned int)
				* domain->num_of_level, GFP_KERNEL);
	if (ptr->bus_table == NULL) {
		pr_err("%s: failed to allocate for bus_table\n", __func__);
		return -ENOMEM;
	}

	ptr->max_idx_num = domain->num_of_level;
	for (i = 0; i < domain->num_of_level; ++i) {
		ptr->freq_table[i].driver_data = i;
		ptr->freq_table[i].frequency = domain->list_level[i].level;
		ptr->volt_table[i] = 0;
		exynos_mp_cpufreq_parse_minlock(cluster_name, ptr->freq_table[i].frequency, &ptr->bus_table[i]);
	}

	return 0;
}
#endif

static int exynos_mp_cpufreq_parse_dt(struct device_node *np, cluster_type cl)
{
	struct exynos_dvfs_info *ptr = exynos_info[cl];
	struct cpufreq_dvfs_table *table_ptr;
	unsigned int table_size, i;
	const char *cluster_domain_name;
	char *cluster_name;
	int ret;
	int not_using_ect = true;

	if (!np) {
		pr_info("%s: cpufreq_dt is not existed. \n", __func__);
		return -ENODEV;
	}

	if (of_property_read_u32(np,(cl ? "cl1_idx_num" : "cl0_idx_num"),
				&ptr->max_idx_num))
		return -ENODEV;

	if (of_property_read_u32(np, (cl ? "cl1_max_support_idx" : "cl0_max_support_idx"),
				&ptr->max_support_idx))
		return -ENODEV;

	if (of_property_read_u32(np, (cl ? "cl1_min_support_idx" : "cl0_min_support_idx"),
				&ptr->min_support_idx))
		return -ENODEV;

	if (of_property_read_u32(np, (cl ? "cl1_boot_max_qos" : "cl0_boot_max_qos"),
				&ptr->boot_cpu_max_qos))
		return -ENODEV;

	if (of_property_read_u32(np, (cl ? "cl1_boot_min_qos" : "cl0_boot_min_qos"),
				&ptr->boot_cpu_min_qos))
		return -ENODEV;

	if (of_property_read_u32(np, (cl ? "cl1_en_ema" : "cl0_en_ema"),
				&ptr->en_ema))
		return -ENODEV;

	if (cl == CL_ZERO) {
		if (of_property_read_u32(np, "cl0_boost_freq", &ptr->boost_freq))
			return -ENODEV;
	}

	if (cl == CL_ONE) {
		if (of_property_read_u32(np, "cl1_en_smpl", &ptr->en_smpl))
			return -ENODEV;
		if (of_property_read_u32(np, "cl1_reboot_limit_freq", &ptr->reboot_limit_freq))
			return -ENODEV;
#if defined(CONFIG_SEC_PM) && defined(CONFIG_MUIC_NOTIFIER)
		if (of_property_read_u32(np, "cl1_jig_boot_max_qos", &ptr->jig_boot_cpu_max_qos))
			return -ENODEV;
#endif

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
		ptr->max_support_idx_table = kzalloc(sizeof(unsigned int)
				* (NR_CLUST1_CPUS + 1), GFP_KERNEL);
		ret = of_property_read_u32_array(np, "cl1_max_support_idx_table",
				(unsigned int *)ptr->max_support_idx_table, NR_CLUST1_CPUS + 1);
		if (ret < 0)
			return -ENODEV;

		/* change max_support_idx */
		ptr->max_support_idx = ptr->max_support_idx_table[1];
#endif
	}

	cluster_name = (cl ? "cl1_dvfs_table" :  "cl0_dvfs_table");

	if (of_property_read_string(np, (cl ? "cl1_dvfs_domain_name" : "cl0_dvfs_domain_name"),
				&cluster_domain_name))
		return -ENODEV;

#if defined(CONFIG_ECT)
	not_using_ect = exynos_mp_cpufreq_parse_frequency((char *)cluster_domain_name, ptr);
#endif
	if (not_using_ect) {
		/* memory alloc for table */
		ptr->freq_table = kzalloc(sizeof(struct cpufreq_frequency_table)
				* (ptr->max_idx_num + 1), GFP_KERNEL);
		if (ptr->freq_table == NULL) {
			pr_err("%s: failed to allocate memory for freq_table\n", __func__);
			return -ENOMEM;
		}

		ptr->volt_table = kzalloc(sizeof(unsigned int)
					* ptr->max_idx_num, GFP_KERNEL);
		if (ptr->volt_table == NULL) {
			pr_err("%s: failed to allocate for volt_table\n", __func__);
			return -ENOMEM;
		}

		ptr->bus_table = kzalloc(sizeof(unsigned int)
					* ptr->max_idx_num, GFP_KERNEL);
		if (ptr->bus_table == NULL) {
			pr_err("%s: failed to allocate for bus_table\n", __func__);
			return -ENOMEM;
		}

		/* temparary table for dt parse*/
		table_ptr = kzalloc(sizeof(struct cpufreq_dvfs_table)
				* ptr->max_idx_num, GFP_KERNEL);
		if (table_ptr == NULL) {
			pr_err("%s: failed to allocate for cpufreq_dvfs_table\n", __func__);
			kfree(table_ptr);
			return -ENOMEM;
		}
		table_size = sizeof(struct cpufreq_dvfs_table) / sizeof(unsigned int);

		ret = of_property_read_u32_array(np, cluster_name,
				(unsigned int *)table_ptr, table_size * (ptr->max_idx_num));
		if (ret < 0) {
			kfree(table_ptr);
			return -ENODEV;
		}

		/* freq/volt/bus table is initailized by DT */
		for (i = 0; i < ptr->max_idx_num; i++) {
			ptr->freq_table[i].driver_data = table_ptr[i].index;
			ptr->freq_table[i].frequency = table_ptr[i].frequency;
			ptr->volt_table[i] = table_ptr[i].voltage;
			ptr->bus_table[i] = table_ptr[i].bus_qos_lock;
		}

		kfree(table_ptr);
	}

	ptr->freq_table[ptr->max_idx_num].driver_data = ptr->max_idx_num;
	ptr->freq_table[ptr->max_idx_num].frequency = CPUFREQ_TABLE_END;

	return 0;
}


static int exynos_mp_cpufreq_probe(struct platform_device *pdev)
{
	int ret;
	cluster_type cluster;

	pm_qos_add_request(&cpufreq_cpu_hotplug_max_request, PM_QOS_CPU_ONLINE_MAX,
						PM_QOS_CPU_ONLINE_MAX_DEFAULT_VALUE);

	for (cluster = 0; cluster < CL_END; cluster++) {
		exynos_info[cluster] = kzalloc(sizeof(struct exynos_dvfs_info), GFP_KERNEL);
		if (!exynos_info[cluster]) {
			return -ENOMEM;
			goto err_init;
		}

		freqs[cluster] = kzalloc(sizeof(struct cpufreq_freqs), GFP_KERNEL);
		if (!freqs[cluster]) {
			ret = -ENOMEM;
			goto err_init;
		}
		/* parse dt */
		if (of_device_is_compatible(pdev->dev.of_node, "samsung,exynos-mp-cpufreq")) {
			ret = exynos_mp_cpufreq_parse_dt(pdev->dev.of_node, cluster);
			if (ret < 0) {
				pr_err("%s: parse_dt failed\n", __func__);
				goto err_init;
			}
		}
	}

	ret = exynos_cpufreq_cluster1_init(exynos_info[CL_ONE]);
	if (ret < 0)
		goto err_init;

	ret = exynos_cpufreq_cluster0_init(exynos_info[CL_ZERO]);
	if (ret < 0)
		goto err_init;

	ret = exynos_cpufreq_init();
	if (ret < 0)
		goto err_init;

	if (exynos_info[CL_ONE]->en_smpl && exynos_info[CL_ONE]->init_smpl) {
		exynos_cpufreq_smpl_warn_register_notifier(&exynos_cpufreq_smpl_warn_notifier);
		if (exynos_info[CL_ONE]->init_smpl() < 0) {
			pr_err("CL_ONE failed SMPL_WARN init\n");
			exynos_info[CL_ONE]->en_smpl = 0;
			exynos_info[CL_ONE]->check_smpl = NULL;
			exynos_cpufreq_smpl_warn_unregister_notifier(&exynos_cpufreq_smpl_warn_notifier);
		} else {
			ret = sysfs_create_file(power_kobj, &smpl_warn_ctrl.attr);
			if (ret < 0)
				pr_err("%s: failed to create smpl_warn_ctrl sysfs interface\n", __func__);
		}
	}

	return ret;

err_init:
	for (cluster = 0; cluster < CL_END; cluster++) {
		if (exynos_info[cluster]->bus_table)
			kfree(exynos_info[cluster]->bus_table);

		if (exynos_info[cluster]->volt_table)
			kfree(exynos_info[cluster]->volt_table);

		if (exynos_info[cluster]->freq_table)
			kfree(exynos_info[cluster]->freq_table);

		if (exynos_info[cluster])
			kfree(exynos_info[cluster]);

		if (freqs[cluster])
			kfree(freqs[cluster]);
	}

	pr_err("%s: failed initialization\n", __func__);

	return ret;
}

static int exynos_mp_cpufreq_remove(struct platform_device *pdev)
{
	cluster_type cluster;

	for (cluster = 0; cluster < CL_END; cluster++) {
		if (exynos_info[cluster]->bus_table &&
				pm_qos_request_active(&exynos_mif_qos[cluster]))
			pm_qos_remove_request(&exynos_mif_qos[cluster]);

		if (pm_qos_request_active(&boot_max_qos[cluster]))
			pm_qos_remove_request(&boot_max_qos[cluster]);
		if (pm_qos_request_active(&boot_min_qos[cluster]))
			pm_qos_remove_request(&boot_min_qos[cluster]);
		if (pm_qos_request_active(&core_min_qos[cluster]))
			pm_qos_remove_request(&core_min_qos[cluster]);
		if (pm_qos_request_active(&core_max_qos[cluster]))
			pm_qos_remove_request(&core_max_qos[cluster]);
		if (pm_qos_request_active(&core_min_qos_real[cluster]))
			pm_qos_remove_request(&core_min_qos_real[cluster]);
		if (pm_qos_request_active(&core_max_qos_real[cluster]))
			pm_qos_remove_request(&core_max_qos_real[cluster]);

		/* Remove pm_qos notifiers */
		pm_qos_remove_notifier(qos_min_class[cluster],
				(cluster ? &exynos_cluster1_min_qos_notifier :
				 &exynos_cluster0_min_qos_notifier));
		pm_qos_remove_notifier(qos_max_class[cluster],
				(cluster ? &exynos_cluster1_max_qos_notifier :
				 &exynos_cluster0_max_qos_notifier));

		/* Release regulater handles */
		if (exynos_info[cluster]->regulator)
			regulator_put(exynos_info[cluster]->regulator);

		/* free table momory */
		if (exynos_info[cluster]->bus_table)
			kfree(exynos_info[cluster]->bus_table);

		if (exynos_info[cluster]->volt_table)
			kfree(exynos_info[cluster]->volt_table);

		if (exynos_info[cluster]->freq_table)
			kfree(exynos_info[cluster]->freq_table);

		if (exynos_info[cluster])
			kfree(exynos_info[cluster]);

		if (freqs[cluster])
			kfree(freqs[cluster]);
	}
	unregister_reboot_notifier(&exynos_cpufreq_reboot_notifier);
	unregister_pm_notifier(&exynos_cpufreq_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_up_nb);
	unregister_hotcpu_notifier(&exynos_cpufreq_cpu_down_nb);

#if defined(CONFIG_EXYNOS_BIG_FREQ_BOOST)
	unregister_cpus_notifier(&exynos_cpufreq_cpus_nb);
#endif

#ifdef CONFIG_PM
	sysfs_remove_file(power_kobj, &cpufreq_min_limit.attr);
	sysfs_remove_file(power_kobj, &cpufreq_table.attr);
	cpufreq_sysfs_remove_group(&mp_attr_group);
#endif

	cpufreq_unregister_driver(&exynos_driver);
	return 0;
}

static struct platform_driver exynos_mp_cpufreq_driver = {
	.probe	= exynos_mp_cpufreq_probe,
	.remove	= exynos_mp_cpufreq_remove,
	.id_table = exynos_mp_cpufreq_driver_ids,
	.driver	= {
		.name	= "exynos-mp-cpufreq",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(exynos_mp_cpufreq_match),
	},
};

static struct platform_device exynos_mp_cpufreq_device = {
	.name	= "exynos-cpufreq",
	.id	= -1,
};

static int __init exynos_mp_cpufreq_init(void)
{
	int ret = 0;

	ret = platform_device_register(&exynos_mp_cpufreq_device);
	if (ret)
		return ret;

	return platform_driver_register(&exynos_mp_cpufreq_driver);
}
#ifdef CONFIG_CPU_FREQ_DEFAULT_GOV_INTERACTIVE
device_initcall(exynos_mp_cpufreq_init);
#else
late_initcall(exynos_mp_cpufreq_init);
#endif

#if defined(CONFIG_SEC_PM) && defined(CONFIG_MUIC_NOTIFIER)

/* In case of CCIC, check jig detection by boot param */
#if defined(CONFIG_MUIC_SUPPORT_CCIC) && defined(CONFIG_SEC_FACTORY)
static bool jig_is_attached = false;
static __init int get_jig_status(char *arg)
{
	jig_is_attached = true;	
	return 0;
}

early_param("jig", get_jig_status);
#else
static struct notifier_block cpufreq_muic_nb;
static bool jig_is_attached;

static int exynos_cpufreq_muic_notifier(struct notifier_block *nb,
				unsigned long action, void *data)
{
	muic_attached_dev_t attached_dev = *(muic_attached_dev_t *)data;

	switch (attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_FG_MUIC:
		if (action == MUIC_NOTIFY_CMD_DETACH)
			jig_is_attached = false;
		else if (action == MUIC_NOTIFY_CMD_ATTACH)
			jig_is_attached = true;
		else
			pr_err("%s - ACTION Error!\n", __func__);
		break;
	default:
		break;
	}

	return NOTIFY_DONE;
}
#endif
static int __init exynos_cpufreq_late_init(void)
{
	cluster_type cluster;
	unsigned long timeout = 500 * USEC_PER_SEC;

#ifdef CONFIG_SEC_FACTORY
	timeout = 100 * USEC_PER_SEC;
#endif

#if defined(CONFIG_MUIC_SUPPORT_CCIC) && defined(CONFIG_SEC_FACTORY)
	if (!jig_is_attached)
		return 0;
#else
	muic_notifier_register(&cpufreq_muic_nb,
			exynos_cpufreq_muic_notifier, MUIC_NOTIFY_DEV_CPUFREQ);

	if (!jig_is_attached)
		return 0;
#endif
	pr_info("%s: JIG is attached: boot cpu qos %lu sec\n", __func__,
			timeout / USEC_PER_SEC);

	for (cluster = 0; cluster < CL_END; cluster++) {
		if (!exynos_info[cluster]->jig_boot_cpu_max_qos)
			exynos_info[cluster]->jig_boot_cpu_max_qos =
				exynos_info[cluster]->boot_cpu_max_qos;

		if (exynos_info[cluster]->jig_boot_cpu_max_qos)
			pm_qos_add_request(&jig_boot_max_qos[cluster],
					qos_max_class[cluster],
					core_max_qos_const[cluster].default_value);
			pm_qos_update_request_timeout(&jig_boot_max_qos[cluster],
					exynos_info[cluster]->jig_boot_cpu_max_qos,
					timeout);
	}
	return 0;
}

late_initcall(exynos_cpufreq_late_init);
#endif /* CONFIG_SEC_PM && CONFIG_MUIC_NOTIFIER && CONFIG_MUIC_SUPPORT_CCIC is not defined */

static void __exit exynos_mp_cpufreq_exit(void)
{
	platform_driver_unregister(&exynos_mp_cpufreq_driver);
	platform_device_unregister(&exynos_mp_cpufreq_device);
}
module_exit(exynos_mp_cpufreq_exit);
