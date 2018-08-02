/* linux/drivers/devfreq/exynos/exynos8890_bus_disp.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *              http://www.samsung.com
 *
 * Samsung EXYNOS8890 SoC DISP devfreq driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/types.h>
#include <linux/errno.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/list.h>
#include <linux/clk.h>

#include <soc/samsung/exynos-devfreq.h>

#include "../../../drivers/soc/samsung/pwrcal/pwrcal.h"
#include "../../../drivers/soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"
#include "../governor.h"

#define DEVFREQ_DISP_REBOOT_FREQ	(400000)

static struct exynos_devfreq_data *disp_data;

static int exynos8890_devfreq_disp_cmu_dump(struct device *dev,
					struct exynos_devfreq_data *data)
{
	cal_vclk_dbg_info(dvfs_disp);

	return 0;
}

static int exynos8890_devfreq_disp_reboot(struct device *dev,
					struct exynos_devfreq_data *data)
{
	u32 freq = DEVFREQ_DISP_REBOOT_FREQ;

	data->max_freq = freq;
	data->devfreq->max_freq = data->max_freq;

	mutex_lock(&data->devfreq->lock);
	update_devfreq(data->devfreq);
	mutex_unlock(&data->devfreq->lock);

	return 0;
}

static u32 exynos8890_devfreq_disp_get_target_freq(char *name, u32 freq)
{
	return cal_dfs_get_rate_by_member(dvfs_disp, name, freq);
}

static int exynos8890_devfreq_disp_get_freq(struct device *dev, u32 *cur_freq,
				struct exynos_devfreq_data *data)
{
	*cur_freq = (u32)clk_get_rate(data->clk);
	if (*cur_freq == 0) {
		dev_err(dev, "failed to get frequency from CAL\n");
		return -EINVAL;
	}

	return 0;
}

static int exynos8890_devfreq_disp_set_freq(struct device *dev,
					u32 old_freq, u32 new_freq,
					struct exynos_devfreq_data *data)
{
	if (clk_set_rate(data->clk, (unsigned long)new_freq)) {
		dev_err(dev, "failed to set frequency via CAL (%uKhz)\n",
				new_freq);
		return -EINVAL;
	}

	return 0;
}

static int exynos8890_devfreq_disp_init_freq_table(struct device *dev,
					struct exynos_devfreq_data *data)
{
	u32 max_freq, min_freq;
	unsigned long tmp_max, tmp_min;
	struct dev_pm_opp *target_opp;
	u32 flags = 0;
	int i;

	max_freq = (u32)cal_dfs_get_max_freq(dvfs_disp);
	if (!max_freq) {
		dev_err(dev, "failed to get max frequency\n");
		return -EINVAL;
	}

	dev_info(dev, "max_freq: %uKhz, get_max_freq: %uKhz\n",
			data->max_freq, max_freq);

	if (max_freq < data->max_freq) {
		rcu_read_lock();
		flags |= DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_max = (unsigned long)max_freq;
		target_opp = devfreq_recommended_opp(dev, &tmp_max, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(dev, "not found valid OPP for max_freq\n");
			return PTR_ERR(target_opp);
		}

		data->max_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	min_freq = (u32)cal_dfs_get_min_freq(dvfs_disp);
	if (!min_freq) {
		dev_err(dev, "failed to get min frequency\n");
		return -EINVAL;
	}

	dev_info(dev, "min_freq: %uKhz, get_min_freq: %uKhz\n",
			data->min_freq, min_freq);

	if (min_freq > data->min_freq) {
		rcu_read_lock();
		flags &= ~DEVFREQ_FLAG_LEAST_UPPER_BOUND;
		tmp_min = (unsigned long)min_freq;
		target_opp = devfreq_recommended_opp(dev, &tmp_min, flags);
		if (IS_ERR(target_opp)) {
			rcu_read_unlock();
			dev_err(dev, "not found valid OPP for min_freq\n");
			return PTR_ERR(target_opp);
		}

		data->min_freq = dev_pm_opp_get_freq(target_opp);
		rcu_read_unlock();
	}

	dev_info(dev, "min_freq: %uKhz, max_freq: %uKhz\n",
			data->min_freq, data->max_freq);

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq > data->max_freq ||
			data->opp_list[i].freq < data->min_freq)
			dev_pm_opp_disable(dev, (unsigned long)data->opp_list[i].freq);
	}

	return 0;
}

static int exynos8890_devfreq_disp_get_volt_table(struct device *dev,
				u32 *volt_table, struct exynos_devfreq_data *data)
{
	struct dvfs_rate_volt disp_rate_volt[data->max_state];
	int table_size;
	int i;

	table_size = cal_dfs_get_rate_asv_table(dvfs_disp, disp_rate_volt);
	if (!table_size) {
		dev_err(dev, "failed to get ASV table\n");
		return -ENODEV;
	}

	if (table_size != data->max_state) {
		dev_err(dev, "ASV table size is not matched\n");
		return -ENODEV;
	}

	for (i = 0; i < data->max_state; i++) {
		if (data->opp_list[i].freq != (u32)(disp_rate_volt[i].rate)) {
			dev_err(dev, "Freq table is not matched(%u:%u)\n",
				data->opp_list[i].freq, (u32)disp_rate_volt[i].rate);
			return -EINVAL;
		}
		volt_table[i] = (u32)disp_rate_volt[i].volt;
	}

	return 0;
}

static int exynos8890_devfreq_disp_init(struct device *dev,
				struct exynos_devfreq_data *data)
{
	data->clk = clk_get(dev, "dvfs_disp");
	if (IS_ERR_OR_NULL(data->clk)) {
		dev_err(dev, "failed get dvfs vclk\n");
		return -ENODEV;
	}

	return 0;
}

static int exynos8890_devfreq_disp_exit(struct device *dev,
				struct exynos_devfreq_data *data)
{
	clk_put(data->clk);

	return 0;
}

static int __init exynos8890_devfreq_disp_init_prepare(struct exynos_devfreq_data *data)
{
	data->ops.init = exynos8890_devfreq_disp_init;
	data->ops.exit = exynos8890_devfreq_disp_exit;
	data->ops.get_volt_table = exynos8890_devfreq_disp_get_volt_table;
	data->ops.get_freq = exynos8890_devfreq_disp_get_freq;
	data->ops.set_freq = exynos8890_devfreq_disp_set_freq;
	data->ops.init_freq_table = exynos8890_devfreq_disp_init_freq_table;
	data->ops.get_target_freq = exynos8890_devfreq_disp_get_target_freq;
	data->ops.reboot = exynos8890_devfreq_disp_reboot;
	data->ops.cmu_dump = exynos8890_devfreq_disp_cmu_dump;

	disp_data = data;

	return 0;
}

static int __init exynos8890_devfreq_disp_initcall(void)
{
	if (register_exynos_devfreq_init_prepare(DEVFREQ_DISP,
				exynos8890_devfreq_disp_init_prepare))
		return -EINVAL;

	return 0;
}
fs_initcall(exynos8890_devfreq_disp_initcall);
