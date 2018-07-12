#include "pwrcal.h"
#include "pwrcal-env.h"
#include "pwrcal-pmu.h"
#include "pwrcal-dfs.h"
#include "pwrcal-clk.h"
#include "pwrcal-vclk.h"
#include "pwrcal-rae.h"
#include "pwrcal-asv.h"
#include "pwrcal-dram.h"
#include <linux/exynos-ss.h>

#define MARGIN_UNIT 6250

int offset_percent;
int set_big_volt;
int set_lit_volt;
int set_int_volt;
int set_mif_volt;
int set_g3d_volt;
int set_disp_volt;
int set_cam_volt;

static int __init get_big_volt(char *str)
{
	get_option(&str, &set_big_volt);
	return 0;
}
early_param("big", get_big_volt);

static int __init get_lit_volt(char *str)
{
	get_option(&str, &set_lit_volt);
	return 0;
}
early_param("lit", get_lit_volt);

static int __init get_int_volt(char *str)
{
	get_option(&str, &set_int_volt);
	return 0;
}
early_param("int", get_int_volt);

static int __init get_mif_volt(char *str)
{
	get_option(&str, &set_mif_volt);
	return 0;
}
early_param("mif", get_mif_volt);

static int __init get_g3d_volt(char *str)
{
	get_option(&str, &set_g3d_volt);
	return 0;
}
early_param("g3d", get_g3d_volt);

static int __init get_disp_volt(char *str)
{
	get_option(&str, &set_disp_volt);
	return 0;
}
early_param("disp", get_disp_volt);

static int __init get_cam_volt(char *str)
{
	get_option(&str, &set_cam_volt);
	return 0;
}
early_param("cam", get_cam_volt);

static int __init get_offset_volt(char *str)
{
	get_option(&str, &offset_percent);
	return 0;
}
early_param("volt_offset_percent", get_offset_volt);

static int set_percent_offset(int actual_volt)
{
	int margin_volt = 0;
	if (offset_percent) {
		margin_volt = actual_volt + ((actual_volt * offset_percent)/100);
		if (offset_percent < -5) {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else if (offset_percent < 0) {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= MARGIN_UNIT + ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else if (offset_percent <= 5) {
			if ((actual_volt * offset_percent/100) % MARGIN_UNIT != 0)
				margin_volt += MARGIN_UNIT - ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		} else {
			if (((actual_volt * offset_percent)/100) % MARGIN_UNIT != 0)
				margin_volt -= ((actual_volt * offset_percent)/100) % MARGIN_UNIT;
		}
		return margin_volt;
	} else {
		return actual_volt;
	}
}

unsigned int cal_clk_get(char *name)
{
	unsigned int ret;
	ret = _cal_vclk_get(name);

	if (!ret)
		ret = _cal_clk_get(name);

	return ret;
}

unsigned int cal_clk_is_enabled(unsigned int id)
{
	return 0;
}

int cal_clk_setrate(unsigned int id, unsigned long rate)
{
	struct vclk *vclk;
	struct pwrcal_clk *clk;

	vclk = cal_get_vclk(id);
	if (vclk)
		return vclk_setrate(vclk, rate);

	clk = cal_get_clk(id);
	if (clk)
		return pwrcal_clk_set_rate(clk, (unsigned long long)rate);

	return -1;
}

unsigned long cal_clk_getrate(unsigned int id)
{
	struct vclk *vclk;
	struct pwrcal_clk *clk;

	vclk = cal_get_vclk(id);
	if (vclk)
		return vclk_getrate(vclk);

	clk = cal_get_clk(id);
	if (clk)
		return pwrcal_clk_get_rate(clk);

	return 0;
}

int cal_clk_enable(unsigned int id)
{
	struct vclk *vclk;
	struct pwrcal_clk *clk;

	vclk = cal_get_vclk(id);
	if (vclk)
		return vclk_enable(vclk);

	clk = cal_get_clk(id);
	if (clk)
		return pwrcal_clk_enable(clk);

	return -1;
}

int cal_clk_disable(unsigned int id)
{
	struct vclk *vclk;
	struct pwrcal_clk *clk;

	vclk = cal_get_vclk(id);
	if (vclk)
		return vclk_disable(vclk);

	clk = cal_get_clk(id);
	if (clk)
		return pwrcal_clk_disable(clk);

	return -1;
}


int cal_pd_control(unsigned int id, int on)
{
	struct cal_pd *pd;
	unsigned int index;

	if ((id & 0xFFFF0000) != BLKPWR_MAGIC)
		return -1;

	index = id & 0x0000FFFF;

	if (index >= pwrcal_blkpwr_size)
		return -1;

	pd = pwrcal_blkpwr_list[index];

	if (cal_pd_ops.pd_control)
		return cal_pd_ops.pd_control(pd, on);

	return -1;
}

int cal_pd_status(unsigned int id)
{
	struct cal_pd *pd;
	unsigned int index;

	if ((id & 0xFFFF0000) != BLKPWR_MAGIC)
		return -1;

	index = id & 0x0000FFFF;

	if (index >= pwrcal_blkpwr_size)
		return -1;

	pd = pwrcal_blkpwr_list[index];

	if (cal_pd_ops.pd_status)
		return cal_pd_ops.pd_status(pd);

	return -1;
}



int cal_pm_enter(int mode)
{
	if (cal_pm_ops.pm_enter)
		cal_pm_ops.pm_enter(mode);

	return 0;
}

int cal_pm_exit(int mode)
{
	if (cal_pm_ops.pm_exit)
		cal_pm_ops.pm_exit(mode);

	return 0;
}

int cal_pm_earlywakeup(int mode)
{
	if (cal_pm_ops.pm_earlywakeup)
		cal_pm_ops.pm_earlywakeup(mode);

	return 0;
}


unsigned int cal_dfs_get(char *name)
{
	int id;

	for (id = 0; id < vclk_dfs_list_size; id++)
		if (!strcmp(vclk_dfs_list[id]->vclk.name, name))
			return vclk_group_dfs + id;

	return 0xFFFFFFFF;
}

unsigned long cal_dfs_get_max_freq(unsigned int id)
{
	struct vclk *vclk;

	vclk = cal_get_vclk(id);
	if (!vclk)
		return 0;

	return dfs_get_max_freq(vclk);
}

unsigned long cal_dfs_get_min_freq(unsigned int id)
{
	struct vclk *vclk;

	vclk = cal_get_vclk(id);
	if (!vclk)
		return 0;

	return dfs_get_min_freq(vclk);
}

int cal_dfs_set_rate(unsigned int id, unsigned long rate)
{
	struct pwrcal_vclk_dfs *dfs;
	struct vclk *vclk;
	unsigned long flag;
	int ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "cal_dfs_set_rate";
#endif
	vclk = cal_get_vclk(id);
	if (!vclk)
		return -1;

	dfs = to_dfs(vclk);

	spin_lock_irqsave(dfs->lock, flag);

	if (!vclk->ref_count) {
		vclk->vfreq = rate;
		goto out;
	}

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);

	if (dfs->table->private_trans)
		ret = dfs->table->private_trans(vclk->vfreq, rate, dfs->table);
	else if (vclk->ops->set_rate)
		ret = vclk->ops->set_rate(vclk, rate);
	else
		ret = -1;

	if (!ret) {
		vclk->vfreq = rate;
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
out:
	spin_unlock_irqrestore(dfs->lock, flag);
	return ret;
}

int cal_dfs_set_rate_switch(unsigned int id, unsigned long switch_rate)
{
	struct pwrcal_vclk_dfs *dfs;
	struct vclk *vclk;
	unsigned long flag;
	int ret = 0;

	vclk = cal_get_vclk(id);
	if (!vclk)
		return -1;

	dfs = to_dfs(vclk);

	spin_lock_irqsave(dfs->lock, flag);

	if (!vclk->ref_count) {
		ret = -1;
		goto out;
	}

	if (dfs->table->private_switch)
		ret = dfs->table->private_switch(vclk->vfreq, switch_rate, dfs->table);
	else if (vclk->ops->set_rate)
		ret = vclk->ops->set_rate(vclk, switch_rate);
	else
		ret = -1;

	if (!ret)
		vclk->vfreq = switch_rate;
out:
	spin_unlock_irqrestore(dfs->lock, flag);
	return ret;
}

unsigned long cal_dfs_cached_get_rate(unsigned int id)
{
	struct pwrcal_vclk_dfs *dfs;
	struct vclk *vclk;
	unsigned long flag;
	unsigned long ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "cal_dfs_get_rate";
#endif
	vclk = cal_get_vclk(id);
	if (!vclk)
		return 0;

	dfs = to_dfs(vclk);

	spin_lock_irqsave(dfs->lock, flag);

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);

	if (!vclk->ref_count) {
		pr_err("%s : %s reference count is zero \n", __func__, vclk->name);
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
		goto out;
	}

	ret = vclk->vfreq;

	exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
out:
	spin_unlock_irqrestore(dfs->lock, flag);
	return ret;
}

unsigned long cal_dfs_get_rate(unsigned int id)
{
	struct pwrcal_vclk_dfs *dfs;
	struct vclk *vclk;
	unsigned long flag;
	unsigned long ret = 0;
#ifdef CONFIG_EXYNOS_SNAPSHOT_CLK
	const char *name = "cal_dfs_get_rate";
#endif
	vclk = cal_get_vclk(id);
	if (!vclk)
		return 0;

	dfs = to_dfs(vclk);

	spin_lock_irqsave(dfs->lock, flag);

	exynos_ss_clk(vclk, name, ESS_FLAG_IN);

	if (!vclk->ref_count) {
		pr_err("%s : %s reference count is zero \n", __func__, vclk->name);
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
		goto out;
	}

	if (dfs->table->private_getrate)
		ret = dfs->table->private_getrate(dfs->table);
	else
		ret = vclk->ops->get_rate(vclk);

	if (ret > 0) {
		vclk->vfreq = (unsigned long)ret;
		exynos_ss_clk(vclk, name, ESS_FLAG_OUT);
	} else
		exynos_ss_clk(vclk, name, ESS_FLAG_ON);
out:
	spin_unlock_irqrestore(dfs->lock, flag);
	return ret;
}


static struct vclk_dfs_ops *get_dfsops(unsigned int id)
{
	struct vclk *vclk;

	vclk = cal_get_vclk(id);
	if (!vclk)
		return NULL;

	return (to_dfs(vclk))->dfsops;
}

int cal_dfs_get_rate_table(unsigned int id, unsigned long *table)
{
	struct vclk_dfs_ops *dfsops = get_dfsops(id);

	if (dfsops)
		if (dfsops->get_rate_table)
			return dfsops->get_rate_table(table);

	return 0;
}

int cal_dfs_get_asv_table(unsigned int id, unsigned int *table)
{
	struct vclk_dfs_ops *dfsops = get_dfsops(id);
	int num_of_entry, i;
	int volt_offset = 0;
	int org_volt, percent_volt;

	if (dfsops->get_margin_param)
		volt_offset = dfsops->get_margin_param(id);

	if (dfsops->get_asv_table) {
		num_of_entry = dfsops->get_asv_table(table);

		for (i = 0; i < num_of_entry; i++) {
			org_volt = (int)table[i];
			percent_volt = set_percent_offset(org_volt);
			table[i] = (unsigned int)(percent_volt + volt_offset);
			pr_info("L%2d: %7d uV, percent_offset(%d)-> %7d uV, volt_offset(%d uV)-> %7duV\n",
						i, org_volt, offset_percent,
						percent_volt, volt_offset, table[i]);
		}
		return num_of_entry;
	}

	return 0;
}

void cal_dfs_set_volt_margin(unsigned int id, int volt)
{
	struct vclk *vclk;
	struct pwrcal_vclk_dfs *dfs;

	vclk = cal_get_vclk(id);
	dfs = to_dfs(vclk);

	dfs->volt_margin = volt;
}

unsigned long cal_dfs_get_rate_by_member(unsigned int id,
						char *member,
						unsigned long rate)
{
	struct vclk_dfs_ops *dfsops = get_dfsops(id);

	if (dfsops)
		if (dfsops->get_target_rate)
			return dfsops->get_target_rate(member, rate);

	return 0;
}

int cal_dfs_set_ema(unsigned int id, unsigned int volt)
{
	struct vclk_dfs_ops *dfsops = get_dfsops(id);

	if (dfsops)
		if (dfsops->set_ema)
			return dfsops->set_ema(volt);

	return -1;
}

int cal_dfs_ext_ctrl(unsigned int id,
			enum cal_dfs_ext_ops ops,
			int para)
{
	struct vclk_dfs_ops *dfsops = get_dfsops(id);

	if (dfsops) {
		switch (ops) {
		case cal_dfs_initsmpl:
			if (dfsops->init_smpl)
				return dfsops->init_smpl();
			break;
		case cal_dfs_deinitsmpl:
			if (dfsops->deinit_smpl)
				return dfsops->deinit_smpl();
			break;
		case cal_dfs_setsmpl:
			if (dfsops->set_smpl)
				return dfsops->set_smpl();
			break;
		case cal_dfs_get_smplstatus:
			if (dfsops->get_smpl)
				return dfsops->get_smpl();
			break;
		case cal_dfs_dvs:
			if (dfsops->dvs)
				return dfsops->dvs(para);
			break;
		case cal_dfs_mif_is_dll_on:
			if (dfsops->is_dll_on)
				return dfsops->is_dll_on();
			break;
		case cal_dfs_cpu_idle_clock_down:
			if (dfsops->cpu_idle_clock_down)
				return dfsops->cpu_idle_clock_down(para);
			break;
		case cal_dfs_ctrl_clk_gate:
			if (dfsops->ctrl_clk_gate)
				return dfsops->ctrl_clk_gate(para);
			break;
		case cal_dfs_rate_lock:
			if (dfsops->rate_lock)
				return dfsops->rate_lock(para);
		default:
			return -1;
		}
	}
	return -1;

}

int cal_dfs_get_rate_asv_table(unsigned int id,
					struct dvfs_rate_volt *table)
{
	struct vclk *vclk;
	struct pwrcal_vclk_dfs *dfs;
	int idx;
	int num_of_entry;
	unsigned long rate[48];
	unsigned int volt[48];
	int tmp;

	vclk = cal_get_vclk(id);
	if (!vclk)
		return 0;

	dfs = to_dfs(vclk);

	num_of_entry = cal_dfs_get_rate_table(id, rate);
	if (num_of_entry == 0)
		return 0;

	if (num_of_entry != cal_dfs_get_asv_table(id, volt))
		return 0;

	for (idx = 0; idx < num_of_entry; idx++) {
		table[idx].rate = rate[idx];
		tmp = (int)(volt[idx]) + dfs->volt_margin;
		table[idx].volt = (unsigned int)tmp;
	}

	return num_of_entry;
}

int cal_asv_pmic_info(void)
{
	if (cal_asv_ops.asv_pmic_info)
		return cal_asv_ops.asv_pmic_info();

	return -1;
}

void cal_asv_print_info(void)
{
	if (cal_asv_ops.print_asv_info)
		cal_asv_ops.print_asv_info();
}

void cal_rcc_print_info(void)
{
	if (cal_asv_ops.print_rcc_info)
		cal_asv_ops.print_rcc_info();
}

int cal_asv_set_rcc_table(void)
{
	if (cal_asv_ops.set_rcc_table)
		return cal_asv_ops.set_rcc_table();
	return -1;
}

void cal_asv_set_grp(unsigned int id, unsigned int asvgrp)
{
	if (cal_asv_ops.set_grp)
		cal_asv_ops.set_grp(id, asvgrp);
}

int cal_asv_get_grp(unsigned int id, unsigned int lv)
{
	if (cal_asv_ops.get_grp)
		return cal_asv_ops.get_grp(id, lv);

	return -1;
}

void cal_asv_set_tablever(unsigned int version)
{
	if (cal_asv_ops.set_tablever)
		cal_asv_ops.set_tablever(version);
}

int cal_asv_get_tablever(void)
{
	if (cal_asv_ops.get_tablever)
		return cal_asv_ops.get_tablever();

	return -1;
}

void cal_asv_set_ssa0(unsigned int id, unsigned int ssa0)
{
	if (cal_asv_ops.set_ssa0)
		cal_asv_ops.set_ssa0(id, ssa0);
}

int cal_asv_get_ids_info(unsigned int domain)
{
	if (cal_asv_ops.get_ids_info)
		return cal_asv_ops.get_ids_info(domain);

	return -1;
}

void cal_dram_print_info(void)
{
	if (cal_dram_ops.print_dram_info)
		cal_dram_ops.print_dram_info();
}

int cal_init(void)
{
	static int pwrcal_vclk_initialized;

	if (pwrcal_vclk_initialized == 1)
		return 0;

	cal_rae_init();
	clk_init();
	dfs_init();
	vclk_init();

	if (cal_asv_ops.asv_init) {
		if (cal_asv_ops.asv_init())
			return -1;
#ifdef PWRCAL_TARGET_LINUX
		if (cal_asv_ops.print_asv_info)
			cal_asv_ops.print_asv_info();
#endif
	}

	if (cal_pm_ops.pm_init)
		cal_pm_ops.pm_init();

	vclk_unused_disable();

	if (cal_pd_ops.pd_init)
		if (cal_pd_ops.pd_init())
			return -1;


	pwrcal_vclk_initialized = 1;

	return 0;
}
