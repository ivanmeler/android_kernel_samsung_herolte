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

irqreturn_t decon_s_irq_handler(int irq, void *dev_data)
{
	struct decon_device *decon = dev_data;
	ktime_t timestamp = ktime_get();
	u32 irq_sts_reg;

	spin_lock(&decon->slock);
	if ((decon->state == DECON_STATE_OFF) ||
		(decon->state == DECON_STATE_LPD)) {
		goto irq_end;
	}

	irq_sts_reg = decon_reg_get_interrupt_and_clear(decon->id);
	if (irq_sts_reg & INTERRUPT_DISPIF_VSTATUS_INT_EN) {
		decon->vsync_info.timestamp = timestamp;
		wake_up_interruptible_all(&decon->vsync_info.wait);
	}
	if (irq_sts_reg & INTERRUPT_FIFO_LEVEL_INT_EN) {
		DISP_SS_EVENT_LOG(DISP_EVT_UNDERRUN, &decon->sd, ktime_set(0, 0));
		decon_err("DECON-ext FIFO underrun\n");
	}
	if (irq_sts_reg & INTERRUPT_FRAME_DONE_INT_EN) {
		DISP_SS_EVENT_LOG(DISP_EVT_DECON_FRAMEDONE, &decon->sd, ktime_set(0, 0));
		decon_warn("DECON-ext frame done interrupt shouldn't happen\n");
		decon->frame_done_cnt_cur++;
		decon_lpd_trig_reset(decon);
	}
	if (irq_sts_reg & INTERRUPT_RESOURCE_CONFLICT_INT_EN)
		DISP_SS_EVENT_LOG(DISP_EVT_RSC_CONFLICT, &decon->sd,
				ktime_set(0, 0));

irq_end:
	spin_unlock(&decon->slock);
	return IRQ_HANDLED;
}

int decon_s_get_clocks(struct decon_device *decon)
{
	return 0;
}

void decon_s_set_clocks(struct decon_device *decon)
{
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	/* TODO: hard-coded 42 will be changed
	 * default MIC factor is 3, So default VCLK is 42 for calculating DISP */
	decon->vclk_factor = 42 * DECON_PIX_PER_CLK;
#endif
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
