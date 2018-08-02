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



#define SET_BRIGHTNESS_FORCE	1
#define SET_BRIGHTNESS_NORMAL	0


#define OLED_ACL_OFF 0
#define OLED_ACL_ON	1
#define OLED_ACL_UNDEFINED -1



int probe_backlight_drv(struct dsim_device *dsim);
int panel_set_brightness(struct dsim_device *dsim, int force);
//int panel_set_brightness_for_hmt(struct dsim_device *dsim, int force);

#endif


