/*
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __SAMSUNG_DECON_HELPER_H__
#define __SAMSUNG_DECON_HELPER_H__

#include <linux/device.h>

#include "decon.h"

int decon_clk_set_parent(struct device *dev, const char *c, const char *p);
int decon_clk_set_rate(struct device *dev, struct clk *clk,
		const char *conid, unsigned long rate);
unsigned long decon_clk_get_rate(struct device *dev, const char *clkid);
void decon_to_psr_info(struct decon_device *decon, struct decon_mode_info *psr);
void decon_to_init_param(struct decon_device *decon, struct decon_param *p);
u32 decon_get_bpp(enum decon_pixel_format fmt);
int decon_get_plane_cnt(enum decon_pixel_format format);

#endif /* __SAMSUNG_DECON_HELPER_H__ */
