/* linux/drivers/video/exynos_decon/panel/dsim_backlight.h
 *
 * Header file for Samsung MIPI-DSI Backlight driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DSIM_BACKLIGHT__
#define __DSIM_BACKLIGHT__

#include "../dsim.h"

int dsim_backlight_probe(struct dsim_device *dsim);
int dsim_panel_set_brightness(struct dsim_device *dsim, int force);
int dsim_panel_set_brightness_for_hmt(struct dsim_device *dsim, int force);
int get_acutal_br_index(struct dsim_device *dsim, int br);
unsigned int get_actual_br_value(struct dsim_device *dsim, int index);
void dsim_panel_set_elvss(struct dsim_device *dsim);

#endif

