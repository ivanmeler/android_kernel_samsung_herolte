/* drivers/gpu/t6xx/kbase/src/platform/gpu_exynos8890.c
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
 * @file gpu_exynos8890.c
 * DVFS
 */

#include <mali_kbase.h>

#include <linux/regulator/driver.h>
#include <linux/pm_qos.h>
#include <linux/delay.h>
#include <linux/smc.h>

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
#include <mach/asv-exynos.h>
#ifdef CONFIG_MALI_RT_PM
#include <../pwrcal/S5E8890/S5E8890-vclk.h>
#include <../pwrcal/pwrcal.h>
#include <mach/pm_domains-cal.h>
#endif
#else
#include <soc/samsung/asv-exynos.h>
#include <../drivers/soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h>
#include <../drivers/soc/samsung/pwrcal/pwrcal.h>
#include <soc/samsung/pm_domains-cal.h>
#include <soc/samsung/exynos-pmu.h>
#endif
#include <linux/apm-exynos.h>
#ifdef CONFIG_EXYNOS8890_BTS_OPTIMIZATION
#include <soc/samsung/bts.h>
#endif
#include <linux/clk.h>


#include "mali_kbase_platform.h"
#include "gpu_dvfs_handler.h"
#include "gpu_dvfs_governor.h"
#include "gpu_control.h"
#include "../mali_midg_regmap.h"


extern struct kbase_device *pkbdev;
#ifdef CONFIG_REGULATOR
#if defined(CONFIG_REGULATOR_S2MPS16)
#if LINUX_VERSION_CODE > KERNEL_VERSION(3, 17, 0)
#define EXYNOS_PMU_G3D_STATUS		0x4064
#define LOCAL_PWR_CFG				(0xF << 0)
#endif
extern int s2m_get_dvs_is_on(void);
#endif
#endif

#ifdef CONFIG_MALI_DVFS
#define CPU_MAX PM_QOS_CLUSTER1_FREQ_MAX_DEFAULT_VALUE
#else
#define CPU_MAX -1
#endif

#ifndef KHZ
#define KHZ (1000)
#endif

#ifdef CONFIG_EXYNOS_BUSMONITOR
void __iomem *g3d0_outstanding_regs;
void __iomem *g3d1_outstanding_regs;
#endif /* CONFIG_EXYNOS_BUSMONITOR */

/*  clk,vol,abb,min,max,down stay, pm_qos mem, pm_qos int, pm_qos cpu_kfc_min, pm_qos cpu_egl_max */
static gpu_dvfs_info gpu_dvfs_table_default[] = {
	{806, 850000, 0, 98, 100, 1, 0, 1794000, 413000, 1586000, CPU_MAX},
	{754, 850000, 0, 98, 100, 1, 0, 1794000, 413000, 1586000, CPU_MAX},
	{728, 850000, 0, 98, 100, 1, 0, 1794000, 413000, 1586000, CPU_MAX},
	{702, 850000, 0, 98, 100, 1, 0, 1794000, 413000, 1586000, CPU_MAX},
	{650, 800000, 0, 98, 100, 5, 0, 1794000, 413000, 1586000, 1560000},
	{600, 800000, 0, 78,  99, 9, 0, 1539000, 413000, 1378000, CPU_MAX},
	{546, 800000, 0, 78,  99, 1, 0, 1144000, 413000, 1170000, CPU_MAX},
	{419, 800000, 0, 78,  85, 1, 0, 1144000, 267000,  858000, CPU_MAX},
	{338, 800000, 0, 78,  85, 1, 0,  546000, 200000,       0, CPU_MAX},
	{260, 800000, 0, 78,  85, 1, 0,  421000, 160000,       0, CPU_MAX},
};

static int mif_min_table[] = {
	 100000,  133000,  167000,
	 276000,  348000,  416000,
	 543000,  632000,  828000,
	1026000, 1264000, 1456000,
	1552000,
};

static gpu_attribute gpu_config_attributes[GPU_CONFIG_LIST_END] = {
	{GPU_MAX_CLOCK, 650},
	{GPU_MAX_CLOCK_LIMIT, 650},
	{GPU_MIN_CLOCK, 260},
	{GPU_DVFS_START_CLOCK, 260},
	{GPU_DVFS_BL_CONFIG_CLOCK, 260},
	{GPU_GOVERNOR_TYPE, G3D_DVFS_GOVERNOR_INTERACTIVE},
	{GPU_GOVERNOR_START_CLOCK_DEFAULT, 260},
	{GPU_GOVERNOR_START_CLOCK_INTERACTIVE, 260},
	{GPU_GOVERNOR_START_CLOCK_STATIC, 260},
	{GPU_GOVERNOR_START_CLOCK_BOOSTER, 260},
	{GPU_GOVERNOR_TABLE_DEFAULT, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_INTERACTIVE, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_STATIC, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_BOOSTER, (uintptr_t)&gpu_dvfs_table_default},
	{GPU_GOVERNOR_TABLE_SIZE_DEFAULT, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_INTERACTIVE, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_STATIC, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_TABLE_SIZE_BOOSTER, GPU_DVFS_TABLE_LIST_SIZE(gpu_dvfs_table_default)},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_CLOCK, 419},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_LOAD, 95},
	{GPU_GOVERNOR_INTERACTIVE_HIGHSPEED_DELAY, 0},
	{GPU_DEFAULT_VOLTAGE, 800000},
	{GPU_COLD_MINIMUM_VOL, 0},
	{GPU_VOLTAGE_OFFSET_MARGIN, 25000},
	{GPU_TMU_CONTROL, 1},
	{GPU_TEMP_THROTTLING1, 650},
	{GPU_TEMP_THROTTLING2, 600},
	{GPU_TEMP_THROTTLING3, 546},
	{GPU_TEMP_THROTTLING4, 419},
	{GPU_TEMP_THROTTLING5, 338},
	{GPU_TEMP_TRIPPING, 260},
	{GPU_POWER_COEFF, 625}, /* all core on param */
	{GPU_DVFS_TIME_INTERVAL, 5},
	{GPU_DEFAULT_WAKEUP_LOCK, 1},
	{GPU_BUS_DEVFREQ, 0},
	{GPU_DYNAMIC_ABB, 0},
	{GPU_EARLY_CLK_GATING, 0},
	{GPU_DVS, 1},
	{GPU_PERF_GATHERING, 0},
#ifdef CONFIG_MALI_SEC_HWCNT
	{GPU_HWCNT_PROFILE, 0},
	{GPU_HWCNT_GATHERING, 1},
	{GPU_HWCNT_POLLING_TIME, 90},
	{GPU_HWCNT_UP_STEP, 3},
	{GPU_HWCNT_DOWN_STEP, 2},
	{GPU_HWCNT_GPR, 1},
	{GPU_HWCNT_DUMP_PERIOD, 50}, /* ms */
	{GPU_HWCNT_CHOOSE_JM , 0x56},
	{GPU_HWCNT_CHOOSE_SHADER , 0x560},
	{GPU_HWCNT_CHOOSE_TILER , 0x800},
	{GPU_HWCNT_CHOOSE_L3_CACHE , 0},
	{GPU_HWCNT_CHOOSE_MMU_L2 , 0x80},
#endif
	{GPU_RUNTIME_PM_DELAY_TIME, 50},
	{GPU_DVFS_POLLING_TIME, 30},
	{GPU_PMQOS_INT_DISABLE, 1},
	{GPU_PMQOS_MIF_MAX_CLOCK, 1539000},
	{GPU_PMQOS_MIF_MAX_CLOCK_BASE, 650},
	{GPU_CL_DVFS_START_BASE, 419},
	{GPU_DEBUG_LEVEL, DVFS_WARNING},
	{GPU_TRACE_LEVEL, TRACE_ALL},
#ifdef CONFIG_MALI_DVFS_USER
	{GPU_UDVFS_ENABLE, 1},
#endif
	{GPU_MO_MIN_CLOCK, 419},
	{GPU_BOOST_EGL_MIN_LOCK, 1872000},
};

#ifdef CONFIG_MALI_DVFS_USER
unsigned int gpu_get_config_attr_size(void)
{
	return sizeof(gpu_config_attributes);
}
#endif

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

struct clk *vclk_g3d;
#ifdef CONFIG_REGULATOR
struct regulator *g3d_regulator;
struct regulator *g3d_m_regulator;
#endif /* CONFIG_REGULATOR */

int gpu_is_power_on(void)
{
	unsigned int val;

#if LINUX_VERSION_CODE < KERNEL_VERSION(3, 17, 0)
	val = __raw_readl(EXYNOS_PMU_G3D_STATUS);
#else
	exynos_pmu_read(EXYNOS_PMU_G3D_STATUS, &val);
#endif

	return ((val & LOCAL_PWR_CFG) == LOCAL_PWR_CFG) ? 1 : 0;
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

	if (!vclk_g3d) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: clock is not initialized\n", __func__);
		return -1;
	}
	return cal_dfs_get_rate(dvfs_g3d)/KHZ;
}

int gpu_register_dump(void)
{
#ifdef CONFIG_MALI_RT_PM
#ifdef CONFIG_REGULATOR
#if defined(CONFIG_REGULATOR_S2MPS16)
	if (gpu_is_power_on() && !s2m_get_dvs_is_on()) {
#endif
#else
	if (gpu_is_power_on()) {
#endif
#endif
#ifdef MALI_SEC_INTEGRATION
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
#endif
#ifdef CONFIG_MALI_RT_PM
	} else {
#ifdef CONFIG_REGULATOR
#if defined(CONFIG_REGULATOR_S2MPS16)
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: Power Status %d, DVS Status %d\n", __func__, gpu_is_power_on(), s2m_get_dvs_is_on());
#endif
#else
		GPU_LOG(DVFS_WARNING, DUMMY, 0u, 0u, "%s: Power Status %d\n", __func__, gpu_is_power_on());
#endif
	}
#endif

	return 0;
}

#ifdef CONFIG_MALI_DVFS
static int gpu_set_clock(struct exynos_context *platform, int clk)
{
	unsigned long g3d_rate = clk * KHZ;
	int ret = 0;

#ifdef CONFIG_MALI_RT_PM
	if (platform->exynos_pm_domain)
		mutex_lock(&platform->exynos_pm_domain->access_lock);

	if (!gpu_is_power_on()) {
		ret = -1;
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "%s: can't set clock in the power-off state!\n", __func__);
		goto err;
	}
#endif /* CONFIG_MALI_RT_PM */

	cal_dfs_set_rate(dvfs_g3d, g3d_rate);

	platform->cur_clock = cal_dfs_get_rate(dvfs_g3d)/KHZ;

	GPU_LOG(DVFS_INFO, LSI_CLOCK_VALUE, g3d_rate/KHZ, platform->cur_clock,
		"[id: %x] clock set: %ld, clock get: %d\n", dvfs_g3d, g3d_rate/KHZ, platform->cur_clock);

#ifdef CONFIG_MALI_RT_PM
err:
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

	vclk_g3d = clk_get(kbdev->dev, "vclk_g3d");
	if (IS_ERR(vclk_g3d)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to clk_get [vclk_g3d]\n", __func__);
		return -1;
	}

	clk_prepare_enable(vclk_g3d);

	return 0;
}

static int gpu_enable_clock(struct exynos_context *platform)
{
	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: [vclk_g3d]\n", __func__);
	clk_prepare_enable(vclk_g3d);
	return 0;
}

static int gpu_disable_clock(struct exynos_context *platform)
{
	GPU_LOG(DVFS_DEBUG, DUMMY, 0u, 0u, "%s: [vclk_g3d]\n", __func__);
#ifdef CONFIG_EXYNOS8890_BTS_OPTIMIZATION
	bts_ext_scenario_set(TYPE_G3D, TYPE_G3D_FREQ, 0);
#endif
	clk_disable_unprepare(vclk_g3d);
	return 0;
}
#endif

int gpu_clock_init(struct kbase_device *kbdev)
{
#ifdef CONFIG_MALI_DVFS
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
#else
	vclk_g3d = clk_get(kbdev->dev, "vclk_g3d");
	clk_prepare_enable(vclk_g3d);
#endif
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
#ifdef CONFIG_MALI_DVFS
	.set_voltage = gpu_set_voltage,
	.set_voltage_pre = gpu_set_voltage_pre,
	.set_voltage_post = gpu_set_voltage_post,
	.set_clock_to_osc = NULL,
	.set_clock = gpu_set_clock,
	.set_clock_pre = NULL,
	.set_clock_post = NULL,
	.enable_clock = gpu_enable_clock,
	.disable_clock = gpu_disable_clock,
#endif
};

struct gpu_control_ops *gpu_get_control_ops(void)
{
	return &ctr_ops;
}

#ifdef CONFIG_REGULATOR
extern int s2m_set_dvs_pin(bool gpio_val);
int gpu_enable_dvs(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_RT_PM
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
	if (!platform->dvs_is_enabled)
	{
		level = gpu_dvfs_get_level(gpu_get_cur_clock(platform));
		exynos_cl_dvfs_stop(ID_G3D, level);
	}
#endif /* CONFIG_EXYNOS_CL_DVFS_G3D */
	/* Do not need to enable dvs during suspending */
	if (!pkbdev->pm.suspending) {
//		if (s2m_set_dvs_pin(true) != 0) {
		if (cal_dfs_ext_ctrl(dvfs_g3d, cal_dfs_dvs, 1) != 0) {
			GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to enable dvs\n", __func__);
			return -1;
		}
	}
#endif /* CONFIG_REGULATOR_S2MPS16 */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is enabled (vol: %d)\n", gpu_get_cur_voltage(platform));
#endif
	return 0;
}

int gpu_disable_dvs(struct exynos_context *platform)
{
	if (!platform->dvs_status)
		return 0;

#ifdef CONFIG_MALI_RT_PM
#if defined(CONFIG_REGULATOR_S2MPS16)
//	if (s2m_set_dvs_pin(false) != 0) {
	if (cal_dfs_ext_ctrl(dvfs_g3d, cal_dfs_dvs, 0) != 0) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to disable dvs\n", __func__);
		return -1;
	}
#endif /* CONFIG_REGULATOR_S2MPS16 */

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs is disabled (vol: %d)\n", gpu_get_cur_voltage(platform));
#endif
	return 0;
}

int gpu_regulator_init(struct exynos_context *platform)
{
#ifdef CONFIG_MALI_DVFS
	g3d_regulator = regulator_get(NULL, "vdd_g3d");
	if (IS_ERR(g3d_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d regulator, 0x%p\n", __func__, g3d_regulator);
		g3d_regulator = NULL;
		return -1;
	}

	g3d_m_regulator = regulator_get(NULL, "vdd_extra");
	if (IS_ERR(g3d_m_regulator)) {
		GPU_LOG(DVFS_ERROR, DUMMY, 0u, 0u, "%s: failed to get vdd_g3d_m regulator, 0x%p\n", __func__, g3d_m_regulator);
		g3d_m_regulator = NULL;
		return -1;
	}

	if (platform->dvs_status)
		GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "dvs GPIO PMU status enable\n");

	GPU_LOG(DVFS_INFO, DUMMY, 0u, 0u, "regulator initialized\n");
#endif

	return 0;
}
#endif /* CONFIG_REGULATOR */

int *get_mif_table(int *size)
{
	*size = ARRAY_SIZE(mif_min_table);
	return mif_min_table;
}
