/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos7890.c
 *
 * Copyright 2011 by S.LSI. Samsung Electronics Inc.
 * San#24, Nongseo-Dong, Giheung-Gu, Yongin, Korea
 *
 * Samsung SoC Mali-T604 DVFS driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software FoundatIon.
 */

/**
 * @file gpu_exynos7890.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/smc.h>

#include <mach/apm-exynos.h>
#include <mach/asv-exynos.h>
#ifdef MALI64_PORTING
#include <mach/asv-exynos7_cal.h>
#endif
#include <mach/pm_domains.h>
#include <mach/regs-pmu.h>

#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include "../mali_midg_regmap.h"

extern struct kbase_device *pkbdev;
#ifdef CONFIG_REGULATOR
#if defined(CONFIG_REGULATOR_S2MPS16)
extern int s2m_get_dvs_is_on(void);
#endif
#endif

#define CPU_MAX PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE

#ifdef CONFIG_EXYNOS_BUSMONITOR
void __iomem *g3d0_outstanding_regs;
void __iomem *g3d1_outstanding_regs;
#endif /* CONFIG_EXYNOS_BUSMONITOR */

/*  clk,vol,abb,min,max,down stay, pm_qos mem, pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{772, 850000, 0, 98, 100, 1, 0, 1552000, 400000, 1500000, 1300000},
	{700, 800000, 0, 98, 100, 1, 0, 1552000, 400000, 1500000, 1300000},
	{600, 800000, 0, 78,  85, 1, 0, 1552000, 413000, 1500000, 1300000},
	{544, 800000, 0, 78,  85, 1, 0, 1026000, 413000, 1500000, 1800000},
	{420, 800000, 0, 78,  85, 1, 0, 1026000, 267000,  900000, 1800000},
	{350, 800000, 0, 78,  85, 1, 0,  543000, 200000,       0, CPU_MAX},
	{266, 800000, 0, 78,  85, 1, 0,  416000, 160000,       0, CPU_MAX},
};

static int mif_min_table[] = {
	 100000,  133000,  167000,
	 276000,  348000,  416000,
	 543000,  632000,  828000,
	1026000, 1264000, 1456000,
	1552000,
};

static int hpm_freq_table[] = {
	/* 772, 700, 600, 544, 420, 350, 266 */
	3, 3, 3, 3, 2, 2, 2,
};

static gpu_attribute gpu_config_attributes[] = {
	{GPU_MAX_CLOCK, 772},
	{GPU_MAX_CLOCK_LIMIT, 700},
	{GPU_MIN_CLOCK, 266},
	{GPU_DVFS_START_CLOCK, 266},
	{GPU_DVFS_BL_CONFIG_CLOCK, 266},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 266},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 266},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 266},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 266},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 420},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 900000},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 37500},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 544},
	{GPU_TEMP_THROTTLING2, 350},
	{GPU_TEMP_THROTTLING3, 266},
	{GPU_TEMP_THROTTLING4, 266},
	{GPU_TEMP_TRIPPING, 266},
	{GPU_POWER_COEFF, 443}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 1},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 1},
	{GPU_PERF_GATHERING, 0},
#ifdef CONFIG_MALI_SEC_HWCNT
	{GPU_HWCNT_GATHERING, 1},
	{GPU_HWCNT_GPR, 1},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0},
	{GPU_HWCNT_CHOOSE_SHADER , 0x560},
	{GPU_HWCNT_CHOOSE_TILER , 0},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_PMQOS_INT_DISABLE, 1},
	{GPU_PMQOS_MIF_MAX_CLOCK, 1456000},
	{GPU_PMQOS_MIF_MAX_CLOCK_BASE, 700},
	{GPU_CL_DVFS_START_BASE, 700},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
};

extern void preload_balance_init(struct kbase_device *kbdev);
int gpu_device_specific_init(struct kbase_device *kbdev)
{
	preload_balance_init(kbdev);

	return 1;
}

int gpu_dvfs_decide_max_clock(struct exynos_context *platform)
{
	if (!platform)
		return -1;

	return 0;
}

void *gpu_get_config_attributes(void)
{
	return &gpu_config_attributes;
}

uintptr_t gpu_get_max_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MAX_CLOCK) * 1000;
}

uintptr_t gpu_get_min_freq(void)
{
	return gpu_get_attrib_data(gpu_config_attributes, GPU_MIN_CLOCK) * 1000;
}

struct clk *fin_pll;
struct clk *fout_g3d_pll;
struct clk *aclk_g3d;
struct clk *mout_g3d;
struct clk *sclk_hpm_g3d;
#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
//	return ((__raw_readl(EXYNOS_PMU_G3D_STATUS) & LOCAL_PWR_CFG) == LOCAL_PWR_CFG) ? 1 : 0;
	return 1;
}

int gpu_power_init(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;

	if (!platform)
		return -ENODEV;

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "power initialized\n");

	return 0;
}

int gpu_get_cur_clock(struct exynos_context *platform)
{
	if (!platform)
		return -ENODEV;

	if (!aclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}

	return clk_get_rate(aclk_g3d)/MHZ;
}

int gpu_register_dump(void)
{
	if (gpu_is_power_on() && !s2m_get_dvs_is_on()) {
		/* MCS Value check */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP,  0x10051224 , __raw_readl(EXYNOS7420_VA_SYSREG + 0x1224),
				"REG_DUMP: G3D_EMA_RF2_UHD_CON %x\n", __raw_readl(EXYNOS7420_VA_SYSREG + 0x1224));
		/* G3D PMU */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C4100, __raw_readl(EXYNOS_PMU_G3D_CONFIGURATION),
				"REG_DUMP: EXYNOS_PMU_G3D_CONFIGURATION %x\n", __raw_readl(EXYNOS_PMU_G3D_CONFIGURATION));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C4104, __raw_readl(EXYNOS_PMU_G3D_STATUS),
				"REG_DUMP: EXYNOS_PMU_G3D_STATUS %x\n", __raw_readl(EXYNOS_PMU_G3D_STATUS));
		/* G3D PLL */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x105C6100, __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL),
				"REG_DUMP: EXYNOS_PMU_GPU_DVS_CTRL %x\n", __raw_readl(EXYNOS_PMU_GPU_DVS_CTRL));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x10576104, __raw_readl(EXYNOS_PMU_GPU_DVS_STATUS),
				"REG_DUMP: GPU_DVS_STATUS %x\n", __raw_readl(EXYNOS_PMU_GPU_DVS_STATUS));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x10051234, __raw_readl(EXYNOS7420_VA_SYSREG + 0x1234),
				"REG_DUMP: G3D_G3DCFG_REG0 %x\n", __raw_readl(EXYNOS7420_VA_SYSREG + 0x1234));

#ifdef CONFIG_EXYNOS_BUSMONITOR
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14A002F0, __raw_readl(g3d0_outstanding_regs + 0x2F0),
				"REG_DUMP: read outstanding %x\n", __raw_readl(g3d0_outstanding_regs + 0x2F0));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14A003F0, __raw_readl(g3d0_outstanding_regs + 0x3F0),
				"REG_DUMP: write outstanding %x\n", __raw_readl(g3d0_outstanding_regs + 0x3F0));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14A202F0, __raw_readl(g3d1_outstanding_regs + 0x2F0),
				"REG_DUMP: read outstanding %x\n", __raw_readl(g3d1_outstanding_regs + 0x2F0));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14A203F0, __raw_readl(g3d1_outstanding_regs + 0x3F0),
				"REG_DUMP: write outstanding %x\n", __raw_readl(g3d1_outstanding_regs + 0x3F0));
#endif /* CONFIG_EXYNOS_BUSMONITOR */

		/* G3D PLL */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0000, __raw_readl(G3D_LOCK),
				"REG_DUMP: EXYNOS7890_G3D_PLL_LOCK %x\n", __raw_readl(G3D_LOCK));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0100, __raw_readl(G3D_CON),
				"REG_DUMP: EXYNOS7890_G3D_PLL_CON0 %x\n", __raw_readl(G3D_CON));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0104, __raw_readl(G3D_CON1),
				"REG_DUMP: EXYNOS7890_G3D_PLL_CON1 %x\n", __raw_readl(G3D_CON1));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0108, __raw_readl(G3D_CON2),
				"REG_DUMP: EXYNOS7890_G3D_PLL_CON2 %x\n", __raw_readl(G3D_CON2));

		/* G3D SRC */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0200, __raw_readl(EXYNOS7420_MUX_SEL_G3D),
				"REG_DUMP: EXYNOS7890_SRC_SEL_G3D %x\n", __raw_readl(EXYNOS7420_MUX_SEL_G3D));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0300, __raw_readl(EXYNOS7420_MUX_ENABLE_G3D),
				"REG_DUMP: EXYNOS7890_SRC_ENABLE_G3D %x\n", __raw_readl(EXYNOS7420_MUX_ENABLE_G3D));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0400, __raw_readl(EXYNOS7420_MUX_STAT_G3D),
				"REG_DUMP: EXYNOS7890_SRC_STAT_G3D %x\n", __raw_readl(EXYNOS7420_MUX_STAT_G3D));

		/* G3D DIV */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0600, __raw_readl(EXYNOS7420_DIV_G3D),
				"REG_DUMP: EXYNOS7890_DIV_G3D %x\n", __raw_readl(EXYNOS7420_DIV_G3D));
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0700, __raw_readl(EXYNOS7420_DIV_STAT_G3D),
				"REG_DUMP: EXYNOS7890_DIV_STAT_G3D %x\n", __raw_readl(EXYNOS7420_DIV_STAT_G3D));

		/* G3D ENABLE */
		GPU_LOG(DVFS_WARNING, LSI_REGISTER_DUMP, 0x14AA0B00, __raw_readl(EXYNOS7420_CLK_ENABLE_IP_G3D),
				"REG_DUMP: EXYNOS7890_ENABLE_IP_G3D %x\n", __raw_readl(EXYNOS7420_CLK_ENABLE_IP_G3D));
	} else {
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: Power Status %d, DVS Status %d\n", __func__, gpu_is_power_on(), s2m_get_dvs_is_on());
	}

	return 0;
}

static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	long g3d_rate_prev = -1;
	unsigned long g3d_rate = clk * MHZ;
	int ret = 0;
	int level = 0;

	if (aclk_g3d == 0)
		return -1;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}

	g3d_rate_prev = clk_get_rate(aclk_g3d);

	/* if changed the VPLL rate, set rate for VPLL and wait for lock time */
	if (g3d_rate != g3d_rate_prev) {

		ret = clk_set_parent(mout_g3d, fin_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [fin_pll]\n", __func__);
			goto err;
		}

		/*change g3d pll*/
		ret = clk_set_rate(fout_g3d_pll, g3d_rate);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_rate [fout_g3d_pll]\n", __func__);
			goto err;
		}

		level = gpu_dvfs_get_level(g3d_rate/MHZ);
		if (level < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to gpu_dvfs_get_level \n", __func__);
			goto err;
		}

		ret = clk_set_rate(sclk_hpm_g3d, (clk_get_rate(aclk_g3d)/hpm_freq_table[level]));
		if(ret < 0)
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_rate [sclk_hpm_g3d]\n", __func__);

		ret = clk_set_parent(mout_g3d, fout_g3d_pll);
		if (ret < 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_set_parent [fout_g3d_pll]\n", __func__);
			goto err;
		}

		g3d_rate_prev = g3d_rate;
	}

	platform->cur_clock = gpu_get_cur_clock(platform);

	if (platform->cur_clock != clk_get_rate(fout_g3d_pll)/MHZ)
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "clock value is wrong (aclk_g3d: %d, fout_g3d_pll: %d)\n",
				platform->cur_clock, (int) clk_get_rate(fout_g3d_pll)/MHZ);
	GPU_LOG(DVFS_DEBUG, LSI_CLOCK_VALUE, g3d_rate/MHZ, platform->cur_clock,
		"clock set: %ld, clock get: %d\n", g3d_rate/MHZ, platform->cur_clock);
err:
#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_unlock(&platform->exynos_pm_domain->access_lock);
#endif /* CONFIG_MALI_RT_PM */
	return ret;
}

static int gpu_get_clock(struct kbase_device *kbdev)
{
	struct exynos_context *platform = (struct exynos_context *) kbdev->platform_context;
	if (!platform)
		return -ENODEV;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	fin_pll = clk_get(kbdev->dev, "fin_pll");
	if (IS_ERR(fin_pll) || (fin_pll == NULL)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fin_pll]\n", __func__);
		return -1;
	}

	fout_g3d_pll = clk_get(kbdev->dev, "fout_g3d_pll");
	if (IS_ERR(fout_g3d_pll)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [fout_g3d_pll]\n", __func__);
		return -1;
	}

	aclk_g3d = clk_get(kbdev->dev, "aclk_g3d");
	if (IS_ERR(aclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [aclk_g3d]\n", __func__);
		return -1;
	}

	mout_g3d = clk_get(kbdev->dev, "mout_g3d");
	if (IS_ERR(mout_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [mout_g3d]\n", __func__);
		return -1;
	}

	sclk_hpm_g3d = clk_get(kbdev->dev, "sclk_hpm_g3d");
	if (IS_ERR(sclk_hpm_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [sclk_hpm_g3d]\n", __func__);
		return -1;
	}

	__raw_writel(0x1, EXYNOS7420_MUX_SEL_G3D);

	return 0;
}

int gpu_clock_init(struct kbase_device *kbdev)
{
	int ret;

	KBASE_DEBUG_ASSERT(kbdev != NULL);

	ret = gpu_get_clock(kbdev);
	if (ret < 0)
		return -1;

#ifdef CONFIG_EXYNOS_BUSMONITOR
	g3d0_outstanding_regs = ioremap(0x14A00000, SZ_1K);
	g3d1_outstanding_regs = ioremap(0x14A20000, SZ_1K);
#endif /* CONFIG_EXYNOS_BUSMONITOR */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "clock initialized\n");

	return 0;
}

int gpu_get_cur_voltage(struct exynos_context *platform)
{
	int ret = 0;
#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

	ret = regulator_get_voltage(g3d_regulator);
#endif /* CONFIG_REGULATOR */
	return ret;
}

static int gpu_set_voltage(struct exynos_context *platform, int vol)
{
	if (gpu_get_cur_voltage(platform) == vol)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set voltage in the power-off state!\n", __func__);
		return -1;
	}

#ifdef CONFIG_REGULATOR
	if (!g3d_regulator) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: regulator is not initialized\n", __func__);
		return -1;
	}

#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	regulator_sync_voltage(g3d_regulator);
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	if (regulator_set_voltage(g3d_regulator, vol, vol) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage, voltage: %d\n", __func__, vol);
		return -1;
	}
#endif /* CONFIG_REGULATOR */

	platform->cur_voltage = gpu_get_cur_voltage(platform);

	GPU_LOG(DVFS_DEBUG, LSI_VOL_VALUE, vol, platform->cur_voltage, "voltage set: %d, voltage get:%d\n", vol, platform->cur_voltage);

	return 0;
}

static int gpu_set_voltage_pre(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (!is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static int gpu_set_voltage_post(struct exynos_context *platform, bool is_up)
{
	if (!platform)
		return -ENODEV;

	if (is_up && platform->dynamic_abb_status)
		set_match_abb(ID_G3D, gpu_dvfs_get_cur_asv_abb());

	return 0;
}

static struct gpu_control_ops ctr_ops = {
	.is_power_on = gpu_is_power_on,
	.set_voltage = gpu_set_voltage,
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
	.set_clock_to_osc = NULL,
	.set_clock = gpu_set_clock,
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
	.enable_clock = NULL,
	.disable_clock = NULL,
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
extern int s2m_set_dvs_pin(bool gpio_val);
int gpu_enable_dvs(struct exynos_context *platform)
{
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	int level = 0;
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	if (!platform->dvs_status)
		return 0;

	if (!gpu_is_power_on()) {
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set dvs in the power-off state!\n", __func__);
		return -1;
	}

#if defined(CONFIG_REGULATOR_S2MPS16)
#ifdef CONFIG_EXYNOS_CL_DVFS_G3D
	level = gpu_dvfs_get_level(gpu_get_cur_clock(platform));
	exynos7890_cl_dvfs_stop(ID_G3D, level);
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */

	/* Do not need to enable dvs during suspending */
	if (!pkbdev->pm.suspending) {
		if (s2m_set_dvs_pin(true) != 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable dvs\n", __func__);
			return -1;
		}
	}
#endif /* CONFIG_REGULATOR_S2MPS16 */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is enabled (vol: %d)\n", gpu_get_cur_voltage(platform));
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform)
{
	if (!platform->dvs_status)
		return 0;

#if defined(CONFIG_REGULATOR_S2MPS16)
	if (s2m_set_dvs_pin(false) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to disable dvs\n", __func__);
		return -1;
	}
#endif /* CONFIG_REGULATOR_S2MPS16 */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is disabled (vol: %d)\n", gpu_get_cur_voltage(platform));
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
	int gpu_voltage = 0;

	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d regulator, 0x%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	gpu_voltage = get_match_volt(ID_G3D, platform->gpu_dvfs_config_clock*1000);

	if (gpu_voltage == 0)
		gpu_voltage = platform->gpu_default_vol;

	if (gpu_set_voltage(platform, gpu_voltage) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to set voltage [%d]\n", __func__, gpu_voltage);
		return -1;
	}

	if (platform->dvs_status)
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs GPIO PMU status enable\n");

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");

	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}
