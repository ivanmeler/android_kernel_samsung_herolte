/* linux/drivers/video/exynos_decon/panel/dsim_panel.h
 *
 * Header file for Samsung MIPI-DSI LCD Panel driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * Minwoo Kim <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef __DSIM_PANEL__
#define __DSIM_PANEL__




#include "../dsim.h"

int dsim_panel_ops_init(struct dsim_device *dsim);

/* dsim_panel_get_priv_ops() this function comes from XXXX_mipi_lcd.c */
struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim);


int dsim_register_panel(struct dsim_panel_ops *panel);

/*lcd_init_sysfs() this function comes from lcd_sysfs.c */
#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
void lcd_init_sysfs(struct dsim_device *dsim);
#endif

#ifdef CONFIG_LCD_HMT
int hmt_set_mode(struct dsim_device *dsim, bool wakeup);
#endif

#endif
