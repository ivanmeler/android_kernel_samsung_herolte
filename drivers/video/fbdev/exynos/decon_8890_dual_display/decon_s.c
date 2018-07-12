/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/of_gpio.h>
#include <linux/clk-provider.h>

#include "decon.h"
#include "decon_helper.h"
#include "../../../../soc/samsung/pwrcal/pwrcal.h"
#include "../../../../soc/samsung/pwrcal/S5E8890/S5E8890-vclk.h"

irqreturn_t decon_s_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	ktime_t timestamp = ktime_get();
	u32 irq_sts_reg;
	spin_lock(&decon->slock);

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id);
	decon_info("irq : %x\n", irq_sts_reg);

	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_LPD)) {
		goto irq_end;
	}

	if (irq_sts_reg & INTERRUPT_DISPIF_VSTATUS_INT_EN) {
		decon->vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&decon->vsync_info.wait);
	}
	if (irq_sts_reg & INTERRUPT_FIFO_LEVEL_INT_EN) {
		DISP_SS_EVENT_LOG(DISP_EVT_UNDERRUN, &decon->sd, ktime_set(0, 0));
		decon_err("DECON-ext FIFO underrun\n");
	}
#if 1
	if (irq_sts_reg & INTERRUPT_FRAME_DONE_INT_EN) {
		DISP_SS_EVENT_LOG(DISP_EVT_DECON_FRAMEDONE, &decon->sd, ktime_set(0, 0));
		//decon_warn("DECON-ext frame done interrupt shouldn't happen\n");
		decon->frame_done_cnt_cur++;
		decon_lpd_trig_reset(decon);
	}
#endif
	if (irq_sts_reg & INTERRUPT_RESOURCE_CONFLICT_INT_EN)
		DISP_SS_EVENT_LOG(DISP_EVT_RSC_CONFLICT, &decon->sd,
				ktime_set(0, 0));

irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

int decon_s_get_clocks(struct decon_device *decon)
{
#if 0
	decon->res.dpll = clk_get(decon->dev, "disp_pll");
	if (IS_ERR_OR_NULL(decon->res.dpll)) {
		decon_err("failed to get disp_pll\n");
		return -ENODEV;
	}
#endif
	decon->res.pclk = clk_get(decon->dev, "decon_pclk");
	if (IS_ERR_OR_NULL(decon->res.pclk)) {
		decon_err("failed to get decon_pclk\n");
		return -ENODEV;
	}

	decon->res.eclk = clk_get(decon->dev, "eclk_user");
	if (IS_ERR_OR_NULL(decon->res.eclk)) {
		decon_err("failed to get eclk_user\n");
		return -ENODEV;
	}

	decon->res.eclk_leaf = clk_get(decon->dev, "eclk_leaf");
	if (IS_ERR_OR_NULL(decon->res.eclk_leaf)) {
		decon_err("failed to get eclk_leaf\n");
		return -ENODEV;
	}
#if 0 
	decon->res.vclk = clk_get(decon->dev, "vclk_user");
	if (IS_ERR_OR_NULL(decon->res.vclk)) {
		decon_err("failed to get vclk_user\n");
		return -ENODEV;
	}
#endif
	decon->res.vclk_leaf = clk_get(decon->dev, "vclk_leaf");
	if (IS_ERR_OR_NULL(decon->res.vclk_leaf)) {
		decon_err("failed to get vclk_leaf\n");
		return -ENODEV;
	}

	return 0;
}

void decon_s_set_clocks(struct decon_device *decon)
{

	struct device *dev = decon->dev;
	struct decon_clocks clks;
	struct decon_param p;

	decon_to_init_param(decon, &p);
	decon_reg_get_clock_ratio(&clks, p.lcd_info);


	decon_clk_set_rate(dev, decon->res.vclk_leaf,
			NULL, (3.3 * MHZ));

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
 	/* TODO: hard-coded 42 will be changed
 	 * default MIC factor is 3, So default VCLK is 42 for calculating DISP */
	decon->vclk_factor = clks.decon[CLK_ID_VCLK] * DECON_PIX_PER_CLK;
#endif
	/* ECLK */
	decon_clk_set_rate(dev, decon->res.eclk,
			NULL, clks.decon[CLK_ID_ECLK] * MHZ);
	decon_clk_set_rate(dev, decon->res.eclk_leaf,
			NULL, clks.decon[CLK_ID_ECLK] * MHZ);

	/* TODO: PCLK */
	/* TODO: ACLK */
	if (!IS_ENABLED(CONFIG_PM_DEVFREQ))
		cal_dfs_set_rate(dvfs_disp, clks.decon[CLK_ID_ACLK] * 1000);

	//decon_info("%s : vclk parent rate : %ld\n", clk_get_rate(decon->res.vclk_leaf.parent));

	decon_info("%s(%d):dpll %ld pclk %ld vclk %ld eclk %ld Mhz\n",
		__func__, decon->id,
		clk_get_rate(decon->res.dpll) / MHZ,
		clk_get_rate(decon->res.pclk) / MHZ,
		clk_get_rate(decon->res.vclk_leaf) / MHZ,
		clk_get_rate(decon->res.eclk_leaf) / MHZ);

}

int decon_s_register_irq(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct resource *res;
	int ret = 0;
 
	/* Get IRQ resource and register IRQ handler. */
	/* 0: FIFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_s_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 1: VStatus irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, decon_s_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 2: FrameDone irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	ret = devm_request_irq(dev, res->start, decon_s_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 3: Extra Interrupts: Resource Conflict irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	ret = devm_request_irq(dev, res->start, decon_s_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	return ret;
}
