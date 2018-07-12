/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
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
#include <linux/io.h>
#include <linux/slab.h>
#include <linux/cpufreq.h>
#include <linux/of.h>

#include <soc/samsung/cpufreq.h>
#include <soc/samsung/exynos-pmu.h>

#include "../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../drivers/soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"

#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
#include <linux/sec_debug.h>
#endif

static struct exynos_dvfs_info *exynos_info[CL_END];

static unsigned int exynos_mp_cpufreq_cl0_get_freq(void)
{
	unsigned int freq;

	freq = (unsigned int)cal_dfs_get_rate(dvfs_little);
	if (freq <= 0)
		pr_err("CL_ZERO: falied cal_dfs_get_rate(%dKHz)\n", freq);

	return freq;

}

static unsigned int exynos_mp_cpufreq_cl1_get_freq(void)
{
	unsigned int freq;

	freq = (unsigned int)cal_dfs_get_rate(dvfs_big);
	if (freq <= 0)
		pr_err("CL_ONE: falied cal_dfs_get_rate(%dKHz)\n", freq);

	return freq;
}

static void exynos_mp_cpufreq_cl0_set_freq(unsigned int old_index,
						unsigned int new_index)
{
	if (cal_dfs_set_rate(dvfs_little,
		(unsigned long)exynos_info[CL_ZERO]->freq_table[new_index].frequency) < 0)
		pr_err("CL0 : failed to set_freq(%d -> %d)\n",old_index, new_index);
}

static void exynos_mp_cpufreq_cl0_set_ema(unsigned int volt)
{
	if (cal_dfs_set_ema(dvfs_little, volt) < 0)
		pr_err("failed to cl0_set_ema(volt %d)\n", volt);
}

static void exynos_mp_cpufreq_cl1_set_freq(unsigned int old_index,
		unsigned int new_index)
{
	if (cal_dfs_set_rate(dvfs_big,
		(unsigned long)exynos_info[CL_ONE]->freq_table[new_index].frequency) < 0)
		pr_err("CL1 : failed to set_freq(%d -> %d)\n",old_index, new_index);
}

static void exynos_mp_cpufreq_cl1_set_ema(unsigned int volt)
{
	if (cal_dfs_set_ema(dvfs_big, volt) < 0)
		pr_err("failed to cl1_set_ema(volt %d)\n", volt);
}

static int exynos_mp_cpufreq_init_smpl(void)
{
	int ret;

	ret = cal_dfs_ext_ctrl(dvfs_big, cal_dfs_initsmpl, 0);
	if (ret < 0) {
		pr_err("CL1 : SMPL_WARN init failed.\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos_mp_cpufreq_deinit_smpl(void)
{
	int ret;

	ret = cal_dfs_ext_ctrl(dvfs_big, cal_dfs_deinitsmpl, 0);
	if (ret < 0) {
		pr_err("CL1 : SMPL_WARN deinit failed.\n");
		return -EINVAL;
	}

	return 0;
}

#ifdef CONFIG_SEC_PM
static unsigned int smpl_warn_number = 0;
unsigned int get_smpl_warn_number(void)
{
	return smpl_warn_number;
}
EXPORT_SYMBOL(get_smpl_warn_number);

void clear_smpl_warn_number(void)
{
	smpl_warn_number = 0;
}
EXPORT_SYMBOL(clear_smpl_warn_number);
#endif
static int exynos_mp_cpufreq_check_smpl(void)
{
	int ret = 0;

	ret = cal_dfs_ext_ctrl(dvfs_big, cal_dfs_get_smplstatus, 0);

	if (ret == 0) {
		return ret;
	} else if (ret > 0) {
#ifdef CONFIG_SEC_PM
		smpl_warn_number++;
#ifdef CONFIG_SEC_DEBUG_EXTRA_INFO
		sec_debug_set_extra_info_smpl(smpl_warn_number);
#endif
#endif
		pr_info("CL1 : SMPL_WARN HAPPENED!\n");
		return ret;
	} else {
		pr_err("CL1 : SMPL_WARN check failed.\n");
	}

	return ret;
}

static void exynos_mp_cpufreq_clear_smpl(void)
{
	if (cal_dfs_ext_ctrl(dvfs_big, cal_dfs_setsmpl, 0) < 0)
		pr_err("CL1 : SMPL_WARN clear failed.\n");
}

static void exynos_mp_cpufreq_set_cal_ops(cluster_type cluster)
{
	/* set cal ops for little core */
	if (!cluster) {
		exynos_info[cluster]->set_freq = exynos_mp_cpufreq_cl0_set_freq;
		exynos_info[cluster]->get_freq = exynos_mp_cpufreq_cl0_get_freq;

		if (exynos_info[cluster]->en_ema)
			exynos_info[cluster]->set_ema = exynos_mp_cpufreq_cl0_set_ema;
		else
			exynos_info[cluster]->set_ema = NULL;
	/* set cal ops for big core */
	} else {
		exynos_info[cluster]->set_freq = exynos_mp_cpufreq_cl1_set_freq;
		exynos_info[cluster]->get_freq = exynos_mp_cpufreq_cl1_get_freq;

		if (exynos_info[cluster]->en_ema)
			exynos_info[cluster]->set_ema = exynos_mp_cpufreq_cl1_set_ema;
		else
			exynos_info[cluster]->set_ema = NULL;

		if (exynos_info[cluster]->en_smpl) {
			exynos_info[cluster]->init_smpl = exynos_mp_cpufreq_init_smpl;
			exynos_info[cluster]->deinit_smpl = exynos_mp_cpufreq_deinit_smpl;
			exynos_info[cluster]->clear_smpl = exynos_mp_cpufreq_clear_smpl;
			exynos_info[cluster]->check_smpl = exynos_mp_cpufreq_check_smpl;
		} else {
			exynos_info[cluster]->init_smpl = NULL;
			exynos_info[cluster]->clear_smpl = NULL;
			exynos_info[cluster]->check_smpl = NULL;
		}
	}
}

static int exynos_mp_cpufreq_init_cal_table(cluster_type cluster)
{
	int table_size, cl_id, i;
	struct dvfs_rate_volt *ptr_temp_table;
	struct exynos_dvfs_info *ptr = exynos_info[cluster];
	unsigned int cal_max_freq;
	unsigned int cal_max_support_idx = ptr->max_support_idx;

	if (!ptr->freq_table || !ptr->volt_table) {
		pr_err("%s: freq of volt table is NULL\n", __func__);
		return -EINVAL;
	}

	if (!cluster)
		cl_id = dvfs_little;
	else
		cl_id = dvfs_big;

	/* allocate to temporary memory for getting table from cal */
	ptr_temp_table = kzalloc(sizeof(struct dvfs_rate_volt)
				* ptr->max_idx_num, GFP_KERNEL);

	/* check freq_table with cal */
	table_size = cal_dfs_get_rate_asv_table(cl_id, ptr_temp_table);

	if (ptr->max_idx_num != table_size) {
		pr_err("%s: DT is not matched cal table size\n", __func__);
		kfree(ptr_temp_table);
		return -EINVAL;
	}

	cal_max_freq = cal_dfs_get_max_freq(cl_id);
	if (!cal_max_freq) {
		pr_err("%s: failed get max frequency from PWRCAL\n", __func__);
		kfree(ptr_temp_table);
		return -EINVAL;
	}

	for (i = 0; i< ptr->max_idx_num; i++) {
		if (ptr->freq_table[i].frequency != (unsigned int)ptr_temp_table[i].rate) {
			pr_err("%s: DT is not matched cal frequency_table(dt : %d, cal : %d\n",
					__func__, ptr->freq_table[i].frequency,
					(unsigned int)ptr_temp_table[i].rate);
			kfree(ptr_temp_table);
			return -EINVAL;
		} else {
			/* copy cal voltage to cpufreq driver voltage table */
			ptr->volt_table[i] = ptr_temp_table[i].volt;
		}

		if (ptr_temp_table[i].rate == cal_max_freq)
			cal_max_support_idx = i;
	}

	pr_info("CPUFREQ of %s CAL max_freq %lu KHz, DT max_freq %lu\n",
			cluster ? "CL1" : "CL0",
			ptr_temp_table[cal_max_support_idx].rate,
			ptr_temp_table[ptr->max_support_idx].rate);

	if (ptr->max_support_idx < cal_max_support_idx)
		ptr->max_support_idx = cal_max_support_idx;

	pr_info("CPUFREQ of %s Current max freq %lu KHz\n",
				cluster ? "CL1" : "CL0",
				ptr_temp_table[ptr->max_support_idx].rate);

	/* free temporary memory */
	kfree(ptr_temp_table);

	return 0;
}

static void exynos_mp_cpufreq_print_info(cluster_type cluster)
{
	int i;

	pr_info("CPUFREQ of %s max_support_idx %d, min_support_idx %d\n",
			cluster? "CL1" : "CL0",
			exynos_info[cluster]->max_support_idx,
			exynos_info[cluster]->min_support_idx);

	pr_info("CPUFREQ of %s max_boot_qos %d, min_boot_qos %d\n",
			cluster? "CL1" : "CL0",
			exynos_info[cluster]->boot_cpu_max_qos,
			exynos_info[cluster]->boot_cpu_min_qos);

	for (i = 0; i < exynos_info[cluster]->max_idx_num; i ++) {
		pr_info("CPUFREQ of %s : %2dL  %8d KHz  %7d uV (mif%8d KHz)\n",
			cluster? "CL1" : "CL0",
			exynos_info[cluster]->freq_table[i].driver_data,
			exynos_info[cluster]->freq_table[i].frequency,
			exynos_info[cluster]->volt_table[i],
			exynos_info[cluster]->bus_table[i]);
	}
}

static int exynos_mp_cpufreq_cal_init(cluster_type cluster)
{
	int ret;

	/* parsing cal rate, voltage table */
	ret = exynos_mp_cpufreq_init_cal_table(cluster);
	if (ret < 0) {
		goto err_init_cal;
	}

	/* print cluster DT information */
	exynos_mp_cpufreq_print_info(cluster);

	/* set ops for cal */
	exynos_mp_cpufreq_set_cal_ops(cluster);

	return 0;

err_init_cal:
	pr_err("%s: init_cal_table failed\n", __func__);
	return ret;
}

int exynos_cpufreq_cluster1_init(struct exynos_dvfs_info * info)
{
	if (info != NULL)
		exynos_info[CL_ONE] = info;
	else {
		pr_err("%s: exynos_info[CL_ONE] is NULL\n", __func__);
	}

	if (exynos_mp_cpufreq_cal_init(CL_ONE)) {
		pr_err("%s: CL_ONE: exynos_init failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}

int exynos_cpufreq_cluster0_init(struct exynos_dvfs_info * info)
{
	if (info != NULL)
		exynos_info[CL_ZERO] = info;
	else {
		pr_err("%s: exynos_info[CL_ZERO] is NULL\n", __func__);
	}

	if (exynos_mp_cpufreq_cal_init(CL_ZERO)) {
		pr_err("%s: CL_ZERO: exynos_init failed\n", __func__);
		return -EINVAL;
	}

	return 0;
}
