/* linux/drivers/video/exynos_decon/panel/oled_backlight.h.h
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

#ifndef __OLED_BACKLIGHT__
#define __OLED_BACKLIGHT__

#include "../dsim.h"


int dsim_backlight_probe(struct dsim_device *dsim);
int dsim_panel_set_brightness(struct dsim_device *dsim, int force);
int dsim_panel_set_brightness_for_hmt(struct dsim_device *dsim, int force);
int dsim_panel_set_acl_step(struct dsim_device *dsim, int old_level, int new_level, int step, int pos );

#endif


