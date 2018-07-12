/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_clk.h
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

#ifndef __FIMG2D_CLK_H__
#define __FIMG2D_CLK_H__

#include <linux/clk-provider.h>

int fimg2d_clk_setup(struct fimg2d_control *ctrl);
void fimg2d_clk_release(struct fimg2d_control *ctrl);
void fimg2d_clk_on(struct fimg2d_control *ctrl);
void fimg2d_clk_off(struct fimg2d_control *ctrl);

#ifndef CONFIG_PM_RUNTIME
static inline int fimg2d5x_get_clk_cnt(struct clk *clk)
{
	return __clk_is_enabled(clk);
}
#else
#define	fimg2d5x_get_clk_cnt(clk)	true
#endif

#endif /* __FIMG2D_CLK_H__ */
