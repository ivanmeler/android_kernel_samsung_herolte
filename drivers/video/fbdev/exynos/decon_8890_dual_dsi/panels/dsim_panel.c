
/* linux/drivers/video/exynos_decon/panel/dsim_panel.c
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

#include <linux/lcd.h>
#include "../dsim.h"

#include "dsim_panel.h"
#include "oled_backlight.h"
#include "panel_info.h"


#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#if defined(CONFIG_PANEL_S6E3FA3_FHD) || defined(CONFIG_PANEL_S6E3FA3_FHD)
#include "mdnie_lite_table_v.h"
#endif
#endif

#define MAX_PANEL 5

static struct dsim_panel_ops *panel_lists[MAX_PANEL];
static int panel_cnt = 0;


int dsim_register_panel(struct dsim_panel_ops *panel)
{
	int ret = 0;

	dsim_info("%s : name : %s\n", __func__, panel->name);

	if (panel_cnt >= MAX_PANEL) {
		ret = -ENOMEM;
	}
	panel_lists[panel_cnt] = panel;
	panel_cnt++;

	return ret;
}

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static int mdnie_lite_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0, i;

	for (i = 0; i < num; i++) {
		if (seq[i].cmd) {
			ret = dsim_write_hl_data(dsim, seq[i].cmd, seq[i].len);
			if (ret != 0) {
				dsim_err("%s failed.\n", __func__);
				return ret;
			}
		}
		if (seq[i].sleep)
			usleep_range(seq[i].sleep * 1000, seq[i].sleep * 1000);
	}
	return ret;
}

int mdnie_lite_send_seq(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("%s : %d : panel was not connected\n", __func__, dsim->id);
		return ret;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}

	mutex_lock(&panel->lock);
	ret = mdnie_lite_write_set(dsim, seq, num);
	mutex_unlock(&panel->lock);

	return ret;
}

int mdnie_lite_read(struct dsim_device *dsim, u8 addr, u8 *buf, u32 size)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("%s : %d : panel was not connected\n", __func__, dsim->id);
		return -EIO;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active\n", __func__);
		return -EIO;
	}

	mutex_lock(&panel->lock);
	ret = dsim_read_hl_data(dsim, addr, size, buf);
	mutex_unlock(&panel->lock);

	return ret;
}
#endif

static int dsim_panel_early_probe(struct dsim_device *dsim)
{
	int i;
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("Finding panel name : %s...\n", dsim->lcd_info.panel_name);
	if (dsim->lcd_info.panel_name == NULL) {
		dsim_err("ERR:%s:can't get panel's name\n", __func__);
		return -ENODEV;
	}

	for (i = 0; i < panel_cnt; i++) {
		dsim_info("%s\n", panel_lists[i]->name);
		if (strcmp(dsim->lcd_info.panel_name, panel_lists[i]->name) == 0) {
			break;
		}
	}
	if (i >= panel_cnt) {
		dsim_err("ERR:%s:failed to find panel ops.. \n", __func__);
		return -ENODEV;
	}
	panel->ops = panel_lists[i];

	if (panel->ops->early_probe) {
		ret = panel->ops->early_probe(dsim);
	}
	return ret;
}


static int dsim_panel_resume(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	if (panel->state == PANEL_STATE_SUSPENED) {
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			if (ret) {
				dsim_err("%s : failed to panel init\n", __func__);
				goto resume_err;
			}
		}
		panel->state = PANEL_STATE_RESUMED;
	}
resume_err:
	return ret;
}
static int dsim_panel_displayon(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("%s : %d : panel was not connected\n", __func__, dsim->id);
		return ret;
	}



	if (panel->state == PANEL_STATE_SUSPENED) {
		if (panel->ops->init) {
			ret = panel->ops->init(dsim);
			if (ret) {
				dsim_err("%s : failed to panel init\n", __func__);
				goto displayon_err;
			}
		}
		panel->state = PANEL_STATE_RESUMED;
	}

	panel_set_brightness(dsim, 1);

	if (panel->ops->displayon) {
		ret = panel->ops->displayon(dsim);
		if (ret) {
			dsim_err("%s : failed to panel display on\n", __func__);
			goto displayon_err;
		}
	}

displayon_err:

	return ret;
}

static int dsim_panel_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	char dev_name[16] = {0, };

	sprintf(dev_name, "panel_%d", dsim->id);

	dsim->lcd = lcd_device_register(dev_name, dsim->dev, &dsim->priv, NULL);
	if (IS_ERR(dsim->lcd)) {
		dsim_err("%s : faield to register lcd device\n", __func__);
		ret = PTR_ERR(dsim->lcd);
		goto probe_err;
	}

	ret = probe_backlight_drv(dsim);
	if (ret) {
		dsim_err("%s : failed to prbe backlight driver\n", __func__);
		goto probe_err;
	}
	panel->lcdConnected = PANEL_CONNECTED;
	if (dsim->id == 0)
		panel->state = PANEL_STATE_RESUMED;
	else
		panel->state = PANEL_STATE_SUSPENED;

	panel->temperature = NORMAL_TEMPERATURE;
	panel->acl_enable = 0;
	panel->current_acl = 0;
	panel->siop_enable = 0;
	panel->current_hbm = 0;
	panel->current_vint = 0;
	panel->adaptive_control = 1;
	panel->lux = -1;

	dsim->glide_display_size = 0;

	mutex_init(&panel->lock);
#ifdef CONFIG_EXYNOS_DECON_LCD_MCD
	panel->mcd_on = 0;
#endif
	if (panel->ops->probe) {
		ret = panel->ops->probe(dsim);
		if (ret) {
			dsim_err("%s : failed to probe panel\n", __func__);
			goto probe_err;
		}
	}
	if (panel->state == PANEL_STATE_SUSPENED) {
		ret = dsim_panel_resume(dsim);
		if (ret) {
			dsim_err("%s:failed to display on\n", __func__);
			goto probe_err;
		}
	}

#if defined(CONFIG_EXYNOS_DECON_LCD_SYSFS)
	lcd_init_sysfs(dsim);
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	if (panel->mdnie_support) {
		mdnie_register(&dsim->lcd->dev, dsim, (mdnie_w)mdnie_lite_send_seq, (mdnie_r)mdnie_lite_read, panel->coordinate, &tune_info);
		panel->mdnie_class = get_mdnie_class();
	}
#endif

probe_err:
	return ret;
}



static int dsim_panel_suspend(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("%s : %d : panel was not connected\n", __func__, dsim->id);
		return ret;
	}

	if (panel->state == PANEL_STATE_SUSPENED)
		goto suspend_err;

	panel->state = PANEL_STATE_SUSPENDING;

	if (panel->ops->exit) {
		ret = panel->ops->exit(dsim);
		if (ret) {
			dsim_err("%s : failed to panel exit \n", __func__);
			goto suspend_err;
		}
	}
	panel->state = PANEL_STATE_SUSPENED;

suspend_err:
	return ret;
}


static int dsim_panel_dump(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_info("%s was called\n", __func__);

	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("%s : %d : panel was not connected\n", __func__, dsim->id);
		return ret;
	}

	if (panel->ops->dump)
		ret = panel->ops->dump(dsim);

	return ret;
}
static struct mipi_dsim_lcd_driver mipi_lcd_driver = {
	.early_probe = dsim_panel_early_probe,
	.probe		= dsim_panel_probe,
	.displayon	= dsim_panel_displayon,
	.suspend	= dsim_panel_suspend,
	.resume		= dsim_panel_resume,
	.dump		= dsim_panel_dump,
};


int dsim_panel_ops_init(struct dsim_device *dsim)
{
	int ret = 0;

	if (dsim)
		dsim->panel_ops = &mipi_lcd_driver;

	return ret;
}

unsigned int lcdtype = 0;
EXPORT_SYMBOL(lcdtype);

static int __init get_lcd_type(char *arg)
{
	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);

