/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_pm.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/pm_runtime.h>
#include <linux/clk.h>
#include <linux/smc.h>

#include "s5p_mfc_pm.h"

#include "s5p_mfc_cmd.h"
#include "s5p_mfc_mem.h"

#define CLK_DEBUG

int s5p_mfc_init_pm(struct s5p_mfc_dev *dev)
{
	int ret = 0;

	dev->pm.clock = clk_get(dev->device, "aclk_mfc");

	if (IS_ERR(dev->pm.clock)) {
		printk(KERN_ERR "failed to get parent clock\n");
		ret = -ENOENT;
		goto err_p_clk;
	}

	ret = clk_prepare(dev->pm.clock);
	if (ret) {
		printk(KERN_ERR "clk_prepare() failed\n");
		return ret;
	}

	spin_lock_init(&dev->pm.clklock);
	atomic_set(&dev->pm.power, 0);
	atomic_set(&dev->clk_ref, 0);

	dev->pm.device = dev->device;
	dev->pm.clock_on_steps = 0;
	dev->pm.clock_off_steps = 0;
	pm_runtime_enable(dev->pm.device);

	clk_put(dev->pm.clock);

	return 0;

err_p_clk:
	clk_put(dev->pm.clock);

	return ret;
}

void s5p_mfc_final_pm(struct s5p_mfc_dev *dev)
{
	clk_put(dev->pm.clock);

	pm_runtime_disable(dev->pm.device);
}

int s5p_mfc_clock_on(struct s5p_mfc_dev *dev)
{
	int ret = 0;
	int state, val;
	unsigned long flags;

	dev->pm.clock_on_steps = 1;
	state = atomic_read(&dev->clk_ref);
	MFC_TRACE_DEV("** clock_on in: ref state(%d)\n", state);

	ret = clk_enable(dev->pm.clock);
	if (ret < 0) {
		mfc_err("clk_enable failed (%d)\n", ret);
		return ret;
	}
	dev->pm.clock_on_steps |= 0x1 << 1;

	if (dev->pm.base_type != MFCBUF_INVALID)
		s5p_mfc_init_memctrl(dev, dev->pm.base_type);

	dev->pm.clock_on_steps |= 0x1 << 2;
	if (dev->curr_ctx_drm && dev->is_support_smc) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		mfc_debug(3, "Begin: enable protection\n");
		ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_ENABLE);
		dev->pm.clock_on_steps |= 0x1 << 3;
		if (ret != SMC_TZPC_OK) {
			printk("Protection Enable failed! ret(%u)\n", ret);
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
			clk_disable(dev->pm.clock);
			return -EACCES;
		}
		mfc_debug(3, "End: enable protection\n");
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	}

	dev->pm.clock_on_steps |= 0x1 << 4;
	if (IS_MFCV6(dev)) {
		if ((!dev->wakeup_status) && (dev->sys_init_status)) {
			spin_lock_irqsave(&dev->pm.clklock, flags);
			if ((atomic_inc_return(&dev->clk_ref) == 1) &&
					FW_HAS_BUS_RESET(dev)) {
				val = MFC_READL(S5P_FIMV_MFC_BUS_RESET_CTRL);
				val &= ~(0x1);
				MFC_WRITEL(val, S5P_FIMV_MFC_BUS_RESET_CTRL);
				dev->pm.clock_on_steps |= 0x1 << 5;
			}
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
		} else {
			dev->pm.clock_on_steps |= 0x1 << 6;
			atomic_inc_return(&dev->clk_ref);
		}
	} else {
		dev->pm.clock_on_steps |= 0x1 << 7;
		atomic_inc_return(&dev->clk_ref);
	}

	dev->pm.clock_on_steps |= 0x1 << 8;
	state = atomic_read(&dev->clk_ref);
	mfc_debug(2, "+ %d\n", state);
	MFC_TRACE_DEV("** clock_on out: ref state(%d)\n", state);

	return 0;
}

/* Use only in functions that first instance is guaranteed, like mfc_init_hw() */
int s5p_mfc_clock_on_with_base(struct s5p_mfc_dev *dev,
				enum mfc_buf_usage_type buf_type)
{
	int ret;
	dev->pm.base_type = buf_type;
	ret = s5p_mfc_clock_on(dev);
	dev->pm.base_type = MFCBUF_INVALID;

	return ret;
}

void s5p_mfc_clock_off(struct s5p_mfc_dev *dev)
{
	int state, val;
	unsigned long timeout, flags;
	int ret = 0;

	dev->pm.clock_off_steps = 1;
	state = atomic_read(&dev->clk_ref);
	MFC_TRACE_DEV("** clock_off in: ref state(%d)\n", state);

	if (IS_MFCV6(dev)) {
		spin_lock_irqsave(&dev->pm.clklock, flags);
		dev->pm.clock_off_steps |= 0x1 << 1;
		if ((atomic_dec_return(&dev->clk_ref) == 0) &&
				FW_HAS_BUS_RESET(dev)) {
			MFC_WRITEL(0x1, S5P_FIMV_MFC_BUS_RESET_CTRL);

			timeout = jiffies + msecs_to_jiffies(MFC_BW_TIMEOUT);
			/* Check bus status */
			do {
				if (time_after(jiffies, timeout)) {
					mfc_err_dev("Timeout while resetting MFC.\n");
					break;
				}
				val = MFC_READL(S5P_FIMV_MFC_BUS_RESET_CTRL);
			} while ((val & 0x2) == 0);
			dev->pm.clock_off_steps |= 0x1 << 2;
		}
		spin_unlock_irqrestore(&dev->pm.clklock, flags);
	} else {
		atomic_dec(&dev->clk_ref);
	}

	dev->pm.clock_off_steps |= 0x1 << 3;
	state = atomic_read(&dev->clk_ref);
	if (state < 0) {
		mfc_err_dev("Clock state is wrong(%d)\n", state);
		atomic_set(&dev->clk_ref, 0);
		dev->pm.clock_off_steps |= 0x1 << 4;
	} else {
		if (dev->curr_ctx_drm && dev->is_support_smc) {
			mfc_debug(3, "Begin: disable protection\n");
			spin_lock_irqsave(&dev->pm.clklock, flags);
			dev->pm.clock_off_steps |= 0x1 << 5;
			ret = exynos_smc(SMC_PROTECTION_SET, 0,
					dev->id, SMC_PROTECTION_DISABLE);
			if (ret != SMC_TZPC_OK) {
				printk("Protection Disable failed! ret(%u)\n", ret);
				spin_unlock_irqrestore(&dev->pm.clklock, flags);
				clk_disable(dev->pm.clock);
				return;
			}
			mfc_debug(3, "End: disable protection\n");
			dev->pm.clock_off_steps |= 0x1 << 6;
			spin_unlock_irqrestore(&dev->pm.clklock, flags);
		}
		dev->pm.clock_off_steps |= 0x1 << 7;
		clk_disable(dev->pm.clock);
	}
	dev->pm.clock_off_steps |= 0x1 << 8;
	state = atomic_read(&dev->clk_ref);
	mfc_debug(2, "- %d\n", state);
	MFC_TRACE_DEV("** clock_off out: ref state(%d)\n", state);
}

int s5p_mfc_power_on(struct s5p_mfc_dev *dev)
{
	int ret;

	atomic_set(&dev->pm.power, 1);
	MFC_TRACE_DEV("++ Power on\n");

	ret = pm_runtime_get_sync(dev->pm.device);

	MFC_TRACE_DEV("-- Power on: ret(%d)\n", ret);

	return ret;
}

int s5p_mfc_power_off(struct s5p_mfc_dev *dev)
{
	int ret;

	MFC_TRACE_DEV("++ Power off\n");

	atomic_set(&dev->pm.power, 0);

	ret = pm_runtime_put_sync(dev->pm.device);
	MFC_TRACE_DEV("-- Power off: ret(%d)\n", ret);

	return ret;
}

int s5p_mfc_get_power_ref_cnt(struct s5p_mfc_dev *dev)
{
	return atomic_read(&dev->pm.power);
}

int s5p_mfc_get_clk_ref_cnt(struct s5p_mfc_dev *dev)
{
	return atomic_read(&dev->clk_ref);
}
