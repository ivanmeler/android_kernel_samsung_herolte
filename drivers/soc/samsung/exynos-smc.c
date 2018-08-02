/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * EXYNOS SMC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/smc.h>

#define CONFIG_EXYNOS_SMC_LOGGING

#ifdef CONFIG_EXYNOS_SMC_LOGGING
#define EXYNOS_SMC_LOG_SIZE	1024

struct exynos_smc_log_entry {
        u64 cpu_clk;
        u32 cmd;
	u32 arg1;
	u32 arg2;
	u32 arg3;
};

static DEFINE_SPINLOCK(drm_smc_log_lock);

struct exynos_smc_log_entry drm_smc_log[EXYNOS_SMC_LOG_SIZE];

static unsigned int drm_smc_log_idx;
#endif

int exynos_smc(unsigned long cmd, unsigned long arg1, unsigned long arg2, unsigned long arg3)
{
#ifdef CONFIG_EXYNOS_SMC_LOGGING
	unsigned long flags;
	unsigned int ret;
#endif

#ifdef CONFIG_EXYNOS_SMC_LOGGING
	if ((uint32_t)cmd >= SMC_PROTECTION_SET && (uint32_t)cmd <= MC_FC_SET_CFW_PROT) {
		pr_debug("%s: cmd: 0x%x, arg1: 0x%x, arg2: 0x%x, arg3: 0x%x\n",
			__func__, (u32)cmd, (u32)arg1, (u32)arg2, (u32)arg3);
		spin_lock_irqsave(&drm_smc_log_lock, flags);
		drm_smc_log[drm_smc_log_idx].cpu_clk = local_clock();
		drm_smc_log[drm_smc_log_idx].cmd = (u32)cmd;
		drm_smc_log[drm_smc_log_idx].arg1 = (u32)arg1;
		drm_smc_log[drm_smc_log_idx].arg2 = (u32)arg2;
		drm_smc_log[drm_smc_log_idx].arg3 = (u32)arg3;
		drm_smc_log_idx++;
		if (drm_smc_log_idx == EXYNOS_SMC_LOG_SIZE)
			drm_smc_log_idx = 0;
		spin_unlock_irqrestore(&drm_smc_log_lock, flags);
	}
#endif

#ifndef CONFIG_EXYNOS_SMC_LOGGING
	return __exynos_smc(cmd, arg1, arg2, arg3);
#else
	ret = __exynos_smc(cmd, arg1, arg2, arg3);
	if ((uint32_t)cmd >= SMC_PROTECTION_SET && (uint32_t)cmd <= MC_FC_SET_CFW_PROT) {
		spin_lock_irqsave(&drm_smc_log_lock, flags);
		drm_smc_log[drm_smc_log_idx].cpu_clk = local_clock();
		drm_smc_log[drm_smc_log_idx].cmd = (u32)cmd;
		drm_smc_log[drm_smc_log_idx].arg1 = (u32)ret;
		/* Magic Code for Debugging */
		drm_smc_log[drm_smc_log_idx].arg2 = (u32)0xAFAFAFAF;
		drm_smc_log[drm_smc_log_idx].arg3 = (u32)arg3;
		drm_smc_log_idx++;
		if (drm_smc_log_idx == EXYNOS_SMC_LOG_SIZE)
			drm_smc_log_idx = 0;
		spin_unlock_irqrestore(&drm_smc_log_lock, flags);
	}
	return ret;
#endif
}
