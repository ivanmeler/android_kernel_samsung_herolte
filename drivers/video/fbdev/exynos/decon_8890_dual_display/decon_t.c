/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
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
#include "../../../../staging/android/sw_sync.h"

int decon_t_set_lcd_info(struct decon_device *decon)
{
	struct decon_lcd *lcd_info;

	if (decon->lcd_info != NULL)
		return 0;

	lcd_info = kzalloc(sizeof(struct decon_lcd), GFP_KERNEL);
	if (!lcd_info) {
		decon_err("could not allocate decon_lcd for wb\n");
		return -ENOMEM;
	}

	decon->lcd_info = lcd_info;
	decon->lcd_info->width = 1920;
	decon->lcd_info->height = 1080;
	decon->lcd_info->xres = 1920;
	decon->lcd_info->yres = 1080;
	decon->lcd_info->vfp = 2;
	decon->lcd_info->vbp = 20;
	decon->lcd_info->hfp = 20;
	decon->lcd_info->hbp = 20;
	decon->lcd_info->vsa = 2;
	decon->lcd_info->hsa = 20;
	decon->lcd_info->fps = 60;
	decon->pdata->out_type = DECON_OUT_WB;

	decon_info("decon_%d output size for writeback %dx%d\n", decon->id,
			decon->lcd_info->width, decon->lcd_info->height);

	return 0;
}

irqreturn_t decon_t_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	u32 irq_sts_reg;

	spin_lock(&decon->slock);
	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_LPD)) {
		goto irq_end;
	}

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id);

	if (irq_sts_reg & INTERRUPT_FIFO_LEVEL_INT_EN) {
		DISP_SS_EVENT_LOG(DISP_EVT_UNDERRUN, &decon->sd, ktime_set(0, 0));
		decon_err("DECON_T FIFO underrun\n");
	}
	if (irq_sts_reg & INTERRUPT_FRAME_DONE_INT_EN) {
		decon_lpd_trig_reset(decon);
		DISP_SS_EVENT_LOG(DISP_EVT_DECON_FRAMEDONE, &decon->sd, ktime_set(0, 0));
		decon_dbg("%s Frame Done is occured. timeline:%d, %d\n",
				__func__, decon->timeline->value, decon->timeline_max);
	}
	if (irq_sts_reg & INTERRUPT_RESOURCE_CONFLICT_INT_EN)
		DISP_SS_EVENT_LOG(DISP_EVT_RSC_CONFLICT, &decon->sd, ktime_set(0, 0));
irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

int decon_t_get_clocks(struct decon_device *decon)
{
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

	return 0;
}

void decon_t_set_clocks(struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct decon_clocks clks;
	struct decon_param p;

	decon_to_init_param(decon, &p);
	decon_reg_get_clock_ratio(&clks, p.lcd_info);

	/* ECLK */
	decon_clk_set_rate(dev, decon->res.eclk,
			NULL, clks.decon[CLK_ID_ECLK] * MHZ);
	decon_clk_set_rate(dev, decon->res.eclk_leaf,
			NULL, clks.decon[CLK_ID_ECLK] * MHZ);

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	/* TODO: hard-coded 42 will be changed
	 * default MIC factor is 3, So default VCLK is 42 for calculating DISP */
	decon->vclk_factor = 42 * DECON_PIX_PER_CLK;
#endif

	decon_dbg("%s: pclk %ld eclk %ld Mhz\n",
		__func__,
		clk_get_rate(decon->res.pclk) / MHZ,
		clk_get_rate(decon->res.eclk_leaf) / MHZ);

	return;
}

int decon_t_register_irq(struct platform_device *pdev, struct decon_device *decon)
{
	struct device *dev = decon->dev;
	struct resource *res;
	int ret = 0;

	/* Get IRQ resource and register IRQ handler. */
	/* 0: FIFO irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	ret = devm_request_irq(dev, res->start, decon_t_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 1: VStatus irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 1);
	ret = devm_request_irq(dev, res->start, decon_t_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 2: FrameDone irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 2);
	ret = devm_request_irq(dev, res->start, decon_t_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	/* 3: Extra Interrupts: Resource Conflict irq */
	res = platform_get_resource(pdev, IORESOURCE_IRQ, 3);
	ret = devm_request_irq(dev, res->start, decon_t_irq_handler, 0,
			pdev->name, decon);
	if (ret) {
		decon_err("failed to install irq\n");
		return ret;
	}

	return ret;
}
