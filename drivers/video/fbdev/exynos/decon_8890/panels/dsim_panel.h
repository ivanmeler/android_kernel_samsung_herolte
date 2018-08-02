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

#ifdef CONFIG_FB_DSU
enum {
	DSU_CONFIG_WQHD = 0,
	DSU_CONFIG_FHD,
	DSU_CONFIG_HD,
	DSU_CONFIG_MAX
};

typedef struct {
	char* id_str;
	int xres;
	int yres;
	int	value;
} type_dsu_config;

static const type_dsu_config dsu_config[] = { {"WQHD", 1440, 2560, DSU_CONFIG_WQHD}, {"FHD", 1080, 1920, DSU_CONFIG_FHD}, {"HD", 720,1280, DSU_CONFIG_HD} };
#endif

/*lcd_init_sysfs() this function comes from lcd_sysfs.c */
#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
void lcd_init_sysfs(struct dsim_device *dsim);
#endif

#ifdef CONFIG_LCD_HMT
int hmt_set_mode(struct dsim_device *dsim, bool wakeup);
#endif

#ifdef CONFIG_LCD_WEAKNESS_CCB
void ccb_set_mode(struct dsim_device *dsim, u8 ccb, int stepping);
#endif

#ifdef CONFIG_LCD_ALPM
#define	ALPM_OFF				0
#define ALPM_ON					1
int alpm_set_mode(struct dsim_device *dsim, int enable);
#endif

#ifdef CONFIG_DSIM_ESD_REMOVE_DISP_DET
int esd_get_ddi_status(struct dsim_device *dsim,  int reg);
#endif

#ifdef CONFIG_LCD_DOZE_MODE
#define UNSUPPORT_ALPM					0
#define SUPPORT_30HZALPM				1
#define SUPPORT_1HZALPM					2
#endif
#endif
