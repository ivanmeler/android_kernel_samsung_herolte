/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_clk.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/err.h>
#include <linux/clk.h>
#include <linux/atomic.h>
#include <linux/sched.h>
#include <linux/of.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"

#include <../drivers/clk/samsung/clk.h>

void fimg2d_clk_on(struct fimg2d_control *ctrl)
{
	clk_enable(ctrl->clock);

	fimg2d_debug("%s : clock enable\n", __func__);
}

void fimg2d_clk_off(struct fimg2d_control *ctrl)
{
	clk_disable(ctrl->clock);

	fimg2d_debug("%s : clock disable\n", __func__);
}

int exynos8890_fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;

	pdata = ctrl->pdata;

	of_property_read_string_index(ctrl->dev->of_node,
		"clock-names", 0, (const char **)&(pdata->gate_clkname));

	fimg2d_info("Done fimg2d clock setup\n");

	return 0;
}

int fimg2d_clk_setup(struct fimg2d_control *ctrl)
{
	struct fimg2d_platdata *pdata;
	struct clk *sclk = NULL;
	int ret = 0;

	pdata = ctrl->pdata;

	if (ip_is_g2d_8j()) {
		if (exynos8890_fimg2d_clk_setup(ctrl)) {
			fimg2d_err("failed to setup clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
	} else {
		sclk = clk_get(ctrl->dev, pdata->clkname);
		if (IS_ERR(sclk)) {
			fimg2d_err("failed to get fimg2d clk\n");
			ret = -ENOENT;
			goto err_clk1;
		}
		fimg2d_info("fimg2d clk name: %s clkrate: %ld\n",
				pdata->clkname, clk_get_rate(sclk));
	}

	/* clock for gating */
	ctrl->clock = clk_get(ctrl->dev, pdata->gate_clkname);
	if (IS_ERR(ctrl->clock)) {
		fimg2d_err("failed to get gate clk\n");
		ret = -ENOENT;
		goto err_clk2;
	}

	if (clk_prepare(ctrl->clock))
		fimg2d_err("failed to prepare gate clock\n");

	fimg2d_info("gate clk: %s\n", pdata->gate_clkname);

	return ret;

err_clk2:
	if (sclk)
		clk_put(sclk);
	if (ctrl->clock)
		clk_put(ctrl->clock);

err_clk1:
	return ret;
}

void fimg2d_clk_release(struct fimg2d_control *ctrl)
{
	clk_unprepare(ctrl->clock);
	clk_put(ctrl->clock);
}
