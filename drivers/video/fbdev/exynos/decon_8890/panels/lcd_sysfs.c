/* linux/drivers/video/exynos_decon/panel/lcd_sysfs.c
 *
 * Header file for Samsung MIPI-DSI Panel SYSFS driver.
 *
 * Copyright (c) 2013 Samsung Electronics
 * JiHoon Kim <jihoonn.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/lcd.h>

#include "../dsim.h"
#include "dsim_panel.h"
#include "panel_info.h"
#include "dsim_backlight.h"
#include "../decon.h"

#ifdef CONFIG_FB_DSU
#include <linux/sec_ext.h>
#endif

#ifdef CONFIG_PANEL_DDI_SPI
#include "ddi_spi.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif

#ifdef CONFIG_DISPLAY_USE_INFO
#include "dpui.h"
#endif
#ifdef CONFIG_SUPPORT_POC_FLASH
#include "poc.h"
#endif

extern struct kset *devices_kset;

#if defined(CONFIG_EXYNOS_DECON_LCD_MCD)
static struct lcd_seq_info SEQ_MCD_ON_SET[] = {
	{(u8 *)SEQ_MCD_ON_SET1, ARRAY_SIZE(SEQ_MCD_ON_SET1), 0},
	{(u8 *)SEQ_MCD_ON_SET2, ARRAY_SIZE(SEQ_MCD_ON_SET2), 0},
	{(u8 *)SEQ_MCD_ON_SET3, ARRAY_SIZE(SEQ_MCD_ON_SET3), 0},
	{(u8 *)SEQ_MCD_ON_SET4, ARRAY_SIZE(SEQ_MCD_ON_SET4), 0},
	{(u8 *)SEQ_MCD_ON_SET5, ARRAY_SIZE(SEQ_MCD_ON_SET5), 0},
	{(u8 *)SEQ_MCD_ON_SET6, ARRAY_SIZE(SEQ_MCD_ON_SET6), 0},
	{(u8 *)SEQ_MCD_ON_SET7, ARRAY_SIZE(SEQ_MCD_ON_SET7), 0},
	{(u8 *)SEQ_MCD_ON_SET8, ARRAY_SIZE(SEQ_MCD_ON_SET8), 100},
	{(u8 *)SEQ_MCD_ON_SET9, ARRAY_SIZE(SEQ_MCD_ON_SET9), 0},
	{(u8 *)SEQ_MCD_ON_SET10, ARRAY_SIZE(SEQ_MCD_ON_SET10), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 100},
	{(u8 *)SEQ_MCD_ON_SET11, ARRAY_SIZE(SEQ_MCD_ON_SET11), 0},
	{(u8 *)SEQ_MCD_ON_SET12, ARRAY_SIZE(SEQ_MCD_ON_SET12), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 100},
};

static struct lcd_seq_info SEQ_MCD_OFF_SET[] = {
	{(u8 *)SEQ_MCD_OFF_SET1, ARRAY_SIZE(SEQ_MCD_OFF_SET1), 0},
	{(u8 *)SEQ_MCD_OFF_SET2, ARRAY_SIZE(SEQ_MCD_OFF_SET2), 0},
	{(u8 *)SEQ_MCD_OFF_SET3, ARRAY_SIZE(SEQ_MCD_OFF_SET3), 0},
	{(u8 *)SEQ_MCD_OFF_SET4, ARRAY_SIZE(SEQ_MCD_OFF_SET4), 0},
	{(u8 *)SEQ_MCD_OFF_SET5, ARRAY_SIZE(SEQ_MCD_OFF_SET5), 0},
	{(u8 *)SEQ_MCD_OFF_SET6, ARRAY_SIZE(SEQ_MCD_OFF_SET6), 0},
	{(u8 *)SEQ_MCD_OFF_SET7, ARRAY_SIZE(SEQ_MCD_OFF_SET7), 0},
	{(u8 *)SEQ_MCD_OFF_SET8, ARRAY_SIZE(SEQ_MCD_OFF_SET8), 100},
	{(u8 *)SEQ_MCD_OFF_SET9, ARRAY_SIZE(SEQ_MCD_OFF_SET9), 0},
	{(u8 *)SEQ_MCD_OFF_SET10, ARRAY_SIZE(SEQ_MCD_OFF_SET10), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 100},
	{(u8 *)SEQ_MCD_OFF_SET11, ARRAY_SIZE(SEQ_MCD_OFF_SET11), 0},
	{(u8 *)SEQ_MCD_OFF_SET12, ARRAY_SIZE(SEQ_MCD_OFF_SET12), 0},
	{(u8 *)SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE), 100},
};

#ifdef CONFIG_PANEL_S6E3HA5_WQHD
static struct lcd_seq_info SEQ_MCD_ON_SET_HA5[] = {
	{(u8 *)SEQ_MCD_ON_HA5_SET01, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET01), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET02, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET02), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET03, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET03), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET04, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET04), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET05, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET05), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET06, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET06), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET07, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET07), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET08, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET08), 100 },
	{(u8 *)SEQ_MCD_ON_HA5_SET09, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET09), 0 },
	{(u8 *)SEQ_MCD_ON_HA5_SET10, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET10), 100 },
	{(u8 *)SEQ_MCD_ON_HA5_SET11, ARRAY_SIZE(SEQ_MCD_ON_HA5_SET11), 100 },

};

static struct lcd_seq_info SEQ_MCD_OFF_SET_HA5[] = {
	{(u8 *)SEQ_MCD_OFF_HA5_SET01, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET01), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET02, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET02), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET03, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET03), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET04, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET04), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET05, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET05), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET06, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET06), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET07, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET07), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET08, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET08), 100 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET09, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET09), 0 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET10, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET10), 100 },
	{(u8 *)SEQ_MCD_OFF_HA5_SET11, ARRAY_SIZE(SEQ_MCD_OFF_HA5_SET11), 100 },
};
#endif

#ifdef CONFIG_PANEL_S6E3HF4_WQHD_HAECHI
static struct lcd_seq_info SEQ_MCD_ON_SET_HAECHI[] = {
	{(u8 *)SEQ_MCD_ON_HAECHI_SET01, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET01), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET02, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET02), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET03, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET03), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET04, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET04), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET05, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET05), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET06, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET06), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET07, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET07), 100 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET08, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET08), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET09, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET09), 0 },
	{(u8 *)SEQ_MCD_ON_HAECHI_SET10, ARRAY_SIZE(SEQ_MCD_ON_HAECHI_SET10), 100 },
};

static struct lcd_seq_info SEQ_MCD_OFF_SET_HAECHI[] = {
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET01, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET01), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET02, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET02), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET03, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET03), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET04, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET04), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET05, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET05), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET06, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET06), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET07, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET07), 100 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET08, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET08), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET09, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET09), 0 },
	{(u8 *)SEQ_MCD_OFF_HAECHI_SET10, ARRAY_SIZE(SEQ_MCD_OFF_HAECHI_SET10), 100 },
};

#endif

static int mcd_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
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
			usleep_range(seq[i].sleep * 1000 , seq[i].sleep * 1000);
	}
	return ret;
}


void mcd_mode_set(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;
	struct lcd_seq_info *mcd_seq;
	int mcd_seq_size;

	mcd_seq = (panel->mcd_on ? SEQ_MCD_ON_SET : SEQ_MCD_OFF_SET );
	mcd_seq_size = (panel->mcd_on ? ARRAY_SIZE(SEQ_MCD_ON_SET) : ARRAY_SIZE(SEQ_MCD_OFF_SET) );
#ifdef CONFIG_PANEL_S6E3HA5_WQHD
	if( dsim->priv.panel_type == LCD_TYPE_S6E3HA5_WQHD ) {
		mcd_seq = (panel->mcd_on ? SEQ_MCD_ON_SET_HA5 : SEQ_MCD_OFF_SET_HA5 ); // overwrite
		mcd_seq_size = (panel->mcd_on ? ARRAY_SIZE(SEQ_MCD_ON_SET_HA5) : ARRAY_SIZE(SEQ_MCD_OFF_SET_HA5) ); // overwrite
	}
#endif
#ifdef CONFIG_PANEL_S6E3HF4_WQHD_HAECHI
	mcd_seq = (panel->mcd_on ? SEQ_MCD_ON_SET_HAECHI : SEQ_MCD_OFF_SET_HAECHI); // overwrite
	mcd_seq_size = (panel->mcd_on ? ARRAY_SIZE(SEQ_MCD_ON_SET_HAECHI) : ARRAY_SIZE(SEQ_MCD_OFF_SET_HAECHI) ); // overwrite
#endif

	pr_info("%s MCD : %d\n", __func__, panel->mcd_on);

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	mcd_write_set(dsim, mcd_seq, mcd_seq_size);

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F1, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
}
static ssize_t mcd_mode_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->mcd_on);

	return strlen(buf);
}

static ssize_t mcd_mode_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	if ((priv->state == PANEL_STATE_RESUMED) && (priv->mcd_on != value)) {
		priv->mcd_on = value;
		mcd_mode_set(dsim);
	}

	dev_info(dev, "%s: %d\n", __func__, priv->mcd_on);

	return size;
}
static DEVICE_ATTR(mcd_mode, 0664, mcd_mode_show, mcd_mode_store);
#endif

#ifdef CONFIG_PANEL_GRAY_SPOT

void grayspot_test(struct dsim_device *dsim)
{
	struct panel_private *panel = &dsim->priv;

	switch(panel->gray_spot)
	{
	case GRAYSPOT_OFF:							// off
		dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		dsim_write_hl_data(dsim, SEQ_SOURCE_ON_SET, ARRAY_SIZE(SEQ_SOURCE_ON_SET));
		dsim_panel_set_elvss(dsim);
		dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		pr_info("%s off\n", __func__);
		break;
	case GRAYSPOT_ON:							// on
		dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
		dsim_write_hl_data(dsim, SEQ_SOURCE_OFF_SET, ARRAY_SIZE(SEQ_SOURCE_OFF_SET));
		dsim_write_hl_data(dsim, SEQ_GRAYSPOT_ELVSS_SET, ARRAY_SIZE(SEQ_GRAYSPOT_ELVSS_SET));
		dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
		pr_info("%s on\n", __func__);
		break;
	default:						// invalid
		pr_info("%s invalid input %d\n", __func__, panel->gray_spot);
		break;
	}
}
static ssize_t grayspot_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->gray_spot);

	return strlen(buf);
}

static ssize_t grayspot_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	if ((priv->state == PANEL_STATE_RESUMED) && (priv->gray_spot != value)) {
		priv->gray_spot = value;
		grayspot_test(dsim);
	}

	dev_info(dev, "%s: %d\n", __func__, priv->gray_spot);

	return size;
}

static DEVICE_ATTR(grayspot, 0664, grayspot_show, grayspot_store);
#endif

#ifdef CONFIG_LCD_HMT

static ssize_t hmt_brightness_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "index : %d, brightenss : %d\n", priv->hmt_br_index, priv->hmt_brightness);

	return strlen(buf);
}

static ssize_t hmt_brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;
	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(dev, "%s: panel is off\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_on == HMT_OFF) {
		dev_info(dev, "%s: hmt is not on\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_brightness != value) {
		mutex_lock(&priv->lock);
		priv->hmt_brightness = value;
		mutex_unlock(&priv->lock);
		dsim_panel_set_brightness_for_hmt(dsim, 0);
	}

	dev_info(dev, "%s: %d\n", __func__, value);
	return size;
}


int hmt_set_mode(struct dsim_device *dsim, bool wakeup)
{
	struct panel_private *priv = &(dsim->priv);
	unsigned char* hmt_forward = (unsigned char*) SEQ_HMT_AID_FORWARD1;
 	unsigned char* hmt_reverse = (unsigned char*) SEQ_HMT_AID_REVERSE1;
	unsigned int hmd_for_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_FORWARD1);
	unsigned int hmd_rev_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_REVERSE1);

	unsigned char* hmt_on = (unsigned char*) SEQ_HMT_ON1;
	unsigned char* hmt_off = (unsigned char*) SEQ_HMT_OFF1;
	unsigned int hmd_on_cmd_size = ARRAY_SIZE(SEQ_HMT_ON1);
	unsigned int hmd_off_cmd_size = ARRAY_SIZE(SEQ_HMT_OFF1);


#if defined(CONFIG_PANEL_S6E3HF4_WQHD) || defined(CONFIG_PANEL_S6E3HA5_WQHD)
	if (priv->panel_type == LCD_TYPE_S6E3HF4_WQHD && priv->panel_material == LCD_MATERIAL_DAISY) {
		hmt_forward = (unsigned char*) SEQ_HMT_AID_FORWARD1_A3;
		hmt_reverse = (unsigned char*) SEQ_HMT_AID_REVERSE1_A3;
		hmd_for_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_FORWARD1_A3);
		hmd_rev_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_REVERSE1_A3);
	}
#endif
#if defined(CONFIG_PANEL_S6E3HA5_WQHD)
	if (priv->panel_type == LCD_TYPE_S6E3HA5_WQHD ) {
		hmt_forward = (unsigned char*) SEQ_HMT_AID_FORWARD1_HA5;
		hmt_reverse = (unsigned char*) SEQ_HMT_AID_REVERSE1_HA5;
		hmd_for_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_FORWARD1_HA5);
		hmd_rev_cmd_size = ARRAY_SIZE(SEQ_HMT_AID_REVERSE1_HA5);

		hmt_on = (unsigned char*) SEQ_HMT_ON_HA5;
		hmt_off = (unsigned char*) SEQ_HMT_OFF_HA5;
		hmd_on_cmd_size = ARRAY_SIZE(SEQ_HMT_ON_HA5);
		hmd_off_cmd_size = ARRAY_SIZE(SEQ_HMT_OFF_HA5);
	}
#endif
	mutex_lock(&priv->lock);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if(priv->hmt_on == HMT_ON) {
		// on set
		dsim_write_hl_data(dsim, hmt_on, hmd_on_cmd_size);
		dsim_write_hl_data(dsim, hmt_reverse, hmd_rev_cmd_size);
		pr_info("%s on sysfs\n", __func__);
	} else if(priv->hmt_on == HMT_OFF) {
		// off set
		dsim_write_hl_data(dsim, hmt_off, hmd_off_cmd_size);
		dsim_write_hl_data(dsim, hmt_forward, hmd_for_cmd_size);
		pr_info("%s off sysfs\n", __func__);

	} else {
		pr_info("hmt state is invalid %d !\n", priv->hmt_on);
		return 0;
	}
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));
	if(wakeup)
		msleep(120);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	mutex_unlock(&priv->lock);

	if(priv->hmt_on == HMT_ON)
		dsim_panel_set_brightness_for_hmt(dsim, 1);
	else if (priv->hmt_on == HMT_OFF)
		dsim_panel_set_brightness(dsim, 1);
	else ;

	return 0;
}

static ssize_t hmt_on_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->hmt_on);

	return strlen(buf);
}

static ssize_t hmt_on_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;
	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 0, &value);
	if (rc < 0)
		return rc;

	if (priv->state != PANEL_STATE_RESUMED) {
		dev_info(dev, "%s: panel is off\n", __func__);
		return -EINVAL;
	}

	if (priv->hmt_on != value) {
		mutex_lock(&priv->lock);
		priv->hmt_prev_status = priv->hmt_on;
		priv->hmt_on = value;
		mutex_unlock(&priv->lock);
		dev_info(dev, "++%s: %d %d\n", __func__, priv->hmt_on, priv->hmt_prev_status);
		hmt_set_mode(dsim, false);
		dev_info(dev, "--%s: %d %d\n", __func__, priv->hmt_on, priv->hmt_prev_status);
	} else
		dev_info(dev, "%s: hmt already %s\n", __func__, value ? "on" : "off");

	return size;
}

static DEVICE_ATTR(hmt_bright, 0664, hmt_brightness_show, hmt_brightness_store);
static DEVICE_ATTR(hmt_on, 0664, hmt_on_show, hmt_on_store);

#endif

#ifdef CONFIG_LCD_ALPM

int display_on_retry_cnt = 0;

#define RDDPM_ADDR	0x0A
#define RDDPM_REG_SIZE 3
#define RDDPM_REG_DISPLAY_ON 0x04
#define READ_STATUS_RETRY_CNT 3

static int read_panel_status(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned char rddpm[RDDPM_REG_SIZE+1];
	int retry_cnt = READ_STATUS_RETRY_CNT;

read_rddpm_reg:
	ret = dsim_read_hl_data(dsim, RDDPM_ADDR, RDDPM_REG_SIZE, rddpm);
	if (ret != RDDPM_REG_SIZE) {
		dsim_err("%s : can't read RDDPM Reg\n", __func__);
		goto dump_exit;
	}

	dsim_info("%s : rddpm : %x\n", __func__, rddpm[0]);

	if ((rddpm[0] & RDDPM_REG_DISPLAY_ON) == 0)  {
		display_on_retry_cnt++;
		dsim_info("%s : display on was not setted retry : %d\n", __func__, retry_cnt);
		dsim_info("%s : display on was not setted total retry : %d\n", __func__, display_on_retry_cnt);
		if (retry_cnt) {
			dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
			retry_cnt--;
			goto read_rddpm_reg;
		}

		if (rddpm[0] & 0x80)
			dsim_info("Dump Panel : Status Booster Voltage Status : ON\n");
		else
			dsim_info("Dump Panel :  Booster Voltage Status : OFF\n");

		if (rddpm[0] & 0x40)
			dsim_info("Dump Panel :  Idle Mode : On\n");
		else
			dsim_info("Dump Panel :  Idle Mode : OFF\n");

		if (rddpm[0] & 0x20)
			dsim_info("Dump Panel :  Partial Mode : On\n");
		else
			dsim_info("Dump Panel :  Partial Mode : OFF\n");

		if (rddpm[0] & 0x10)
			dsim_info("Dump Panel :  Sleep OUT and Working Ok\n");
		else
			dsim_info("Dump Panel :  Sleep IN\n");

		if (rddpm[0] & 0x08)
			dsim_info("Dump Panel :  Normal Mode On and Working Ok\n");
		else
			dsim_info("Dump Panel :  Sleep IN\n");
	}
dump_exit:
	return ret;

}

static int alpm_write_set(struct dsim_device *dsim, struct lcd_seq_info *seq, u32 num)
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
			usleep_range(seq[i].sleep * 1000 , seq[i].sleep * 1000);
	}
	return ret;
}


#ifdef CONFIG_PANEL_S6E3HF4_WQHD
static struct lcd_seq_info SEQ_ALPM_2NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_ALPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_ALPM_2NIT_HF4), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
	{(u8 *)SEQ_MCLK_1SET_HF4, ARRAY_SIZE(SEQ_MCLK_1SET_HF4), 0},
};

static struct lcd_seq_info SEQ_ALPM_40NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_ALPM_40NIT_HF4, ARRAY_SIZE(SEQ_SELECT_ALPM_40NIT_HF4), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
	{(u8 *)SEQ_MCLK_1SET_HF4, ARRAY_SIZE(SEQ_MCLK_1SET_HF4), 0},
};

static struct lcd_seq_info SEQ_HLPM_2NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_HLPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_HLPM_2NIT_HF4), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
	{(u8 *)SEQ_MCLK_1SET_HF4, ARRAY_SIZE(SEQ_MCLK_1SET_HF4), 0},
};

static struct lcd_seq_info SEQ_HLPM_40NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_HLPM_40NIT_HF4, ARRAY_SIZE(SEQ_SELECT_HLPM_40NIT_HF4), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
	{(u8 *)SEQ_MCLK_1SET_HF4, ARRAY_SIZE(SEQ_MCLK_1SET_HF4), 0},
};

static struct lcd_seq_info SEQ_NORMAL_MODE_ON_SET[] = {
	{(u8 *)SEQ_MCLK_3SET_HF4, ARRAY_SIZE(SEQ_MCLK_3SET_HF4), 0},
	{(u8 *)SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON), 0},
};

// old panel 18
static struct lcd_seq_info SEQ_ALPM_2NIT_ON_SET_OLD[] = {
	{(u8 *)SEQ_SELECT_ALPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_ALPM_2NIT_HF4), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_ALPM_40NIT_ON_SET_OLD[] = {
	{(u8 *)SEQ_SELECT_ALPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_ALPM_2NIT_HF4), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_HLPM_2NIT_ON_SET_OLD[] = {
	{(u8 *)SEQ_SELECT_HLPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_HLPM_2NIT_HF4), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_HLPM_40NIT_ON_SET_OLD[] = {
	{(u8 *)SEQ_SELECT_HLPM_2NIT_HF4, ARRAY_SIZE(SEQ_SELECT_HLPM_2NIT_HF4), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_NORMAL_MODE_ON_SET_OLD[] = {
	{(u8 *)SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON), 0},
};


int alpm_set_mode(struct dsim_device *dsim, int enable)
{
	struct panel_private *priv = &(dsim->priv);

	if((enable < ALPM_OFF) && (enable > HLPM_ON_40NIT)) {
		pr_info("alpm state is invalid %d !\n", priv->alpm);
		return 0;
	}

	dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if(enable == ALPM_OFF) {
		if(priv->panel_rev >= 5)
			alpm_write_set(dsim, SEQ_NORMAL_MODE_ON_SET, ARRAY_SIZE(SEQ_NORMAL_MODE_ON_SET));
		else
			alpm_write_set(dsim, SEQ_NORMAL_MODE_ON_SET_OLD, ARRAY_SIZE(SEQ_NORMAL_MODE_ON_SET_OLD));
		if((priv->alpm_support == SUPPORT_LOWHZALPM) && (priv->current_alpm == ALPM_ON_40NIT)){
			dsim_write_hl_data(dsim, SEQ_AOD_LOWHZ_OFF, ARRAY_SIZE(SEQ_AOD_LOWHZ_OFF));
			dsim_write_hl_data(dsim, SEQ_AID_MOD_OFF, ARRAY_SIZE(SEQ_AID_MOD_OFF));
			pr_info("%s : Low hz support !\n", __func__);
		}
	} else {
		switch(enable) {
		case HLPM_ON_2NIT:
			if(priv->panel_rev >= 5)
				alpm_write_set(dsim, SEQ_HLPM_2NIT_ON_SET, ARRAY_SIZE(SEQ_HLPM_2NIT_ON_SET));
			else
				alpm_write_set(dsim, SEQ_HLPM_2NIT_ON_SET_OLD, ARRAY_SIZE(SEQ_HLPM_2NIT_ON_SET_OLD));
			pr_info("%s : HLPM_ON_2NIT !\n", __func__);
			break;
		case ALPM_ON_2NIT:
			if(priv->panel_rev >= 5)
				alpm_write_set(dsim, SEQ_ALPM_2NIT_ON_SET, ARRAY_SIZE(SEQ_ALPM_2NIT_ON_SET));
			else
				alpm_write_set(dsim, SEQ_ALPM_2NIT_ON_SET_OLD, ARRAY_SIZE(SEQ_ALPM_2NIT_ON_SET_OLD));
			pr_info("%s : ALPM_ON_2NIT !\n", __func__);
			break;
		case HLPM_ON_40NIT:
			if(priv->panel_rev >= 5)
				alpm_write_set(dsim, SEQ_HLPM_40NIT_ON_SET, ARRAY_SIZE(SEQ_HLPM_40NIT_ON_SET));
			else
				alpm_write_set(dsim, SEQ_HLPM_40NIT_ON_SET_OLD, ARRAY_SIZE(SEQ_HLPM_40NIT_ON_SET_OLD));
			pr_info("%s : HLPM_ON_40NIT !\n", __func__);
			break;
		case ALPM_ON_40NIT:
			if(priv->alpm_support == SUPPORT_LOWHZALPM) {
				dsim_write_hl_data(dsim, SEQ_2HZ_GPARA, ARRAY_SIZE(SEQ_2HZ_GPARA));
				dsim_write_hl_data(dsim, SEQ_2HZ_SET, ARRAY_SIZE(SEQ_2HZ_SET));
				dsim_write_hl_data(dsim, SEQ_AID_MOD_ON, ARRAY_SIZE(SEQ_AID_MOD_ON));
				pr_info("%s : Low hz support !\n", __func__);
			}
			if(priv->panel_rev >= 5)
				alpm_write_set(dsim, SEQ_ALPM_40NIT_ON_SET, ARRAY_SIZE(SEQ_ALPM_40NIT_ON_SET));
			else
				alpm_write_set(dsim, SEQ_ALPM_40NIT_ON_SET_OLD, ARRAY_SIZE(SEQ_ALPM_40NIT_ON_SET_OLD));
			pr_info("%s : ALPM_ON_40NIT !\n", __func__);
			break;
		default:
			pr_info("%s: input is out of range : %d \n", __func__, enable);
			break;
		}
		dsim_write_hl_data(dsim, HF4_A3_IRC_off, ARRAY_SIZE(HF4_A3_IRC_off));
	}

	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));

	usleep_range(17000, 17000);

	dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	read_panel_status(dsim);

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));


	priv->current_alpm = dsim->alpm = enable;

	return 0;
}

#else		// HA3
static struct lcd_seq_info SEQ_ALPM_2NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_ALPM_HA3, ARRAY_SIZE(SEQ_SELECT_ALPM_HA3), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_ALPM_40NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_ALPM_HA3, ARRAY_SIZE(SEQ_SELECT_ALPM_HA3), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_HLPM_2NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_HLPM_HA3, ARRAY_SIZE(SEQ_SELECT_HLPM_HA3), 0},
	{(u8 *)SEQ_2NIT_MODE_ON, ARRAY_SIZE(SEQ_2NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_HLPM_40NIT_ON_SET[] = {
	{(u8 *)SEQ_SELECT_HLPM_HA3, ARRAY_SIZE(SEQ_SELECT_HLPM_HA3), 0},
	{(u8 *)SEQ_40NIT_MODE_ON, ARRAY_SIZE(SEQ_40NIT_MODE_ON), 0},
};

static struct lcd_seq_info SEQ_NORMAL_MODE_ON_SET[] = {
	{(u8 *)SEQ_NORMAL_MODE_ON, ARRAY_SIZE(SEQ_NORMAL_MODE_ON), 0},
};


int alpm_set_mode(struct dsim_device *dsim, int enable)
{
	struct panel_private *priv = &(dsim->priv);

	if((enable < ALPM_OFF) && (enable > HLPM_ON_40NIT)) {
		pr_info("alpm state is invalid %d !\n", priv->alpm);
		return 0;
	}

	dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	usleep_range(17000, 17000);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if(enable == ALPM_OFF) {
		alpm_write_set(dsim, SEQ_NORMAL_MODE_ON_SET, ARRAY_SIZE(SEQ_NORMAL_MODE_ON_SET));
	} else {
		switch(enable) {
		case HLPM_ON_2NIT:
			alpm_write_set(dsim, SEQ_HLPM_2NIT_ON_SET, ARRAY_SIZE(SEQ_HLPM_2NIT_ON_SET));
			pr_info("%s : HLPM_ON_2NIT !\n", __func__);
			break;
		case ALPM_ON_2NIT:
			alpm_write_set(dsim, SEQ_ALPM_2NIT_ON_SET, ARRAY_SIZE(SEQ_ALPM_2NIT_ON_SET));
			pr_info("%s : ALPM_ON_2NIT !\n", __func__);
			break;
		case HLPM_ON_40NIT:
			alpm_write_set(dsim, SEQ_HLPM_40NIT_ON_SET, ARRAY_SIZE(SEQ_HLPM_40NIT_ON_SET));
			pr_info("%s : HLPM_ON_40NIT !\n", __func__);
			break;
		case ALPM_ON_40NIT:
			alpm_write_set(dsim, SEQ_ALPM_40NIT_ON_SET, ARRAY_SIZE(SEQ_ALPM_40NIT_ON_SET));
			pr_info("%s : ALPM_ON_40NIT !\n", __func__);
			break;
		default:
			pr_info("%s: input is out of range : %d \n", __func__, enable);
			break;
		}
		dsim_write_hl_data(dsim, HF4_A3_IRC_off, ARRAY_SIZE(HF4_A3_IRC_off));
	}

	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(SEQ_GAMMA_UPDATE_L));

	usleep_range(17000, 17000);

	dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	read_panel_status(dsim);

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	priv->current_alpm = dsim->alpm = enable;

	return 0;
}

#endif


static ssize_t alpm_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->alpm);

	dev_info(dev, "%s: %d\n", __func__, priv->alpm);

	return strlen(buf);
}

#if defined (CONFIG_SEC_FACTORY)
static int prev_brightness = 0;
#endif

static ssize_t alpm_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%9d", &value);
	dev_info(dev, "%s: %d \n", __func__, value);

	if(priv->alpm_support == UNSUPPORT_ALPM) {
		pr_info("%s this panel does not support ALPM!\n", __func__);
		return size;
	}

	mutex_lock(&priv->alpm_lock);
	if(priv->state == PANEL_STATE_RESUMED) {
		switch(value) {
		case ALPM_OFF:
			if(priv->current_alpm) {
#ifdef CONFIG_SEC_FACTORY
				priv->bd->props.brightness = prev_brightness;
#endif
				alpm_set_mode(dsim, ALPM_OFF);
				usleep_range(17000, 17000);
				dsim_panel_set_brightness(dsim, 1);
			} else {
				dev_info(dev, "%s: alpm current state : %d input : %d\n",
					__func__, priv->current_alpm, value);
			}
			break;
		case ALPM_ON_2NIT:
		case HLPM_ON_2NIT:
		case ALPM_ON_40NIT:
		case HLPM_ON_40NIT:
#ifdef CONFIG_SEC_FACTORY
			if(priv->alpm == ALPM_OFF) {
				prev_brightness = priv->bd->props.brightness;
				priv->bd->props.brightness = UI_MIN_BRIGHTNESS;
				dsim_panel_set_brightness(dsim, 1);
			}
#endif
			alpm_set_mode(dsim, value);
			break;
		default:
			dev_info(dev, "%s: input is out of range : %d \n", __func__, value);
			break;
		}
	} else {
		dev_info(dev, "%s: panel state is not active\n", __func__);
	}
	priv->alpm = value;

	mutex_unlock(&priv->alpm_lock);

	dev_info(dev, "%s: %d\n", __func__, priv->alpm);

	return size;
}
static DEVICE_ATTR(alpm, 0664, alpm_show, alpm_store);
#endif

#ifdef CONFIG_LCD_DOZE_MODE

#if defined (CONFIG_SEC_FACTORY)
static int prev_brightness = 0;
static int current_alpm_mode = 0;

#endif

static ssize_t alpm_doze_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct decon_device *decon = NULL;
	struct mutex *output_lock = NULL;

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%9d", &value);

	decon = (struct decon_device *)dsim->decon;
	if (decon != NULL)
		output_lock = &decon->output_lock;
#ifdef CONFIG_SEC_FACTORY
	dsim_info("%s: %d\n", __func__, value);
	current_alpm_mode = priv->alpm_mode;
	switch (value) {
		case ALPM_OFF:
			priv->alpm_mode = value;
			if ((dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) ||
				(dsim->dsim_doze == DSIM_DOZE_STATE_NORMAL)) {

				call_panel_ops(dsim, exitalpm, dsim);
				usleep_range(17000, 17000);
				if (prev_brightness) {
					priv->bd->props.brightness = prev_brightness - 1;
					dsim_panel_set_brightness(dsim, 1);
					prev_brightness = 0;
				}
				dsim->req_display_on = true;
				call_panel_ops(dsim, displayon, dsim);
			}
			break;
		case ALPM_ON_2NIT:
		case HLPM_ON_2NIT:
		case ALPM_ON_40NIT:
		case HLPM_ON_40NIT:
			priv->alpm_mode = value;
			if ((dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) ||
				(dsim->dsim_doze == DSIM_DOZE_STATE_NORMAL)) {
				if(current_alpm_mode == ALPM_OFF) {
					prev_brightness = priv->bd->props.brightness + 1;
					priv->bd->props.brightness = UI_MIN_BRIGHTNESS;
					dsim_panel_set_brightness(dsim, 1);
				}
				call_panel_ops(dsim, enteralpm, dsim);
				usleep_range(17000, 17000);
				dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
			}
			break;
		default:
			dsim_err("ERR:%s:undefined alpm mode : %d\n", __func__, value);
			break;
	}
#else
	switch (value) {
		case ALPM_ON_2NIT:
		case HLPM_ON_2NIT:
		case ALPM_ON_40NIT:
		case HLPM_ON_40NIT:
			if (output_lock != NULL)
				mutex_lock(output_lock);

			dsim_info("%s: %d\n", __func__, priv->alpm_mode);
			if (value != priv->alpm_mode) {
				priv->alpm_mode = value;
				if (dsim->dsim_doze == DSIM_DOZE_STATE_DOZE) {
					call_panel_ops(dsim, enteralpm, dsim);
				}
			}
			if (output_lock != NULL)
				mutex_unlock(output_lock);

			break;
		default:
			dsim_err("ERR:%s:undefined alpm mode : %d\n", __func__, value);
			break;
	}
#endif
	return size;
}

static ssize_t alpm_doze_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim_info("%s: %d\n", __func__, priv->alpm_mode);
	sprintf(buf, "%d\n", priv->alpm_mode);
	return strlen(buf);
}

static DEVICE_ATTR(alpm, 0664, alpm_doze_show, alpm_doze_store);
#endif
static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	char buf_print[3] = {0x00, };

	if((priv->id[0] == 0xff) && (priv->id[1] == 0xff) && (priv->id[2] == 0xff))	{
		buf_print[0] = buf_print[1] = buf_print[2] = 0x00;
	} else {
		buf_print[0] = priv->id[0];
		buf_print[1] = priv->id[1];
		buf_print[2] = priv->id[2];
	}

	sprintf(buf, "SDC_%02X%02X%02X\n", buf_print[0], buf_print[1], buf_print[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", priv->id[0], priv->id[1], priv->id[2]);
	pr_info("%s %02x %02x %02x\n", __func__, priv->id[0], priv->id[1], priv->id[2]);
	return strlen(buf);
}

static ssize_t SVC_OCTA_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		priv->date[0] , priv->date[1], priv->date[2], priv->date[3], priv->date[4],
		priv->date[5],priv->date[6], (priv->coordinate[0] &0xFF00)>>8,priv->coordinate[0] &0x00FF,
		(priv->coordinate[1]&0xFF00)>>8,priv->coordinate[1]&0x00FF);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	char *pos = buf;
	int nit, i, br_index;

	dsim = container_of(priv, struct dsim_device, priv);

	for (i = 0; i <= EXTEND_BRIGHTNESS; i++) {
		nit = priv->br_tbl[i];
		br_index = get_acutal_br_index(dsim, nit);
		nit = get_actual_br_value(dsim, br_index);
		pos += sprintf(pos, "%3d %3d\n", i, nit);
	}
	return pos - buf;
}

static ssize_t siop_enable_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u\n", priv->siop_enable);

	return strlen(buf);
}

static ssize_t siop_enable_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;
	else {
		if (priv->siop_enable != value) {
			dev_info(dev, "%s: %d, %d\n", __func__, priv->siop_enable, value);
			mutex_lock(&priv->lock);
			priv->siop_enable = value;
			mutex_unlock(&priv->lock);
#ifdef CONFIG_LCD_HMT
			if(priv->hmt_on == HMT_ON)
				dsim_panel_set_brightness_for_hmt(dsim, 1);
			else
#endif
				dsim_panel_set_brightness(dsim, 1);
		}
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-15, -14, 0, 1\n";

	strcat(buf, temp);
	return strlen(buf);
}

static ssize_t temperature_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtoint(buf, 10, &value);

	if (rc < 0)
		return rc;
	else {
		mutex_lock(&priv->lock);
		priv->temperature = value;
		mutex_unlock(&priv->lock);
#ifdef CONFIG_LCD_HMT
		if(priv->hmt_on == HMT_ON)
			dsim_panel_set_brightness_for_hmt(dsim, 1);
		else
#endif
			dsim_panel_set_brightness(dsim, 1);
		dev_info(dev, "%s: %d, %d\n", __func__, value, priv->temperature);
	}

	return size;
}

static ssize_t color_coordinate_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%u, %u\n", priv->coordinate[0], priv->coordinate[1]);
	return strlen(buf);
}

static ssize_t manufacture_date_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	u16 year;
	u8 month, day, hour, min;

	year = ((priv->date[0] & 0xF0) >> 4) + 2011;
	month = priv->date[0] & 0xF;
	day = priv->date[1] & 0x1F;
	hour = priv->date[2] & 0x1F;
	min = priv->date[3] & 0x3F;

	sprintf(buf, "%d, %d, %d, %d:%d\n", year, month, day, hour, min);
	return strlen(buf);
}

static ssize_t read_mtp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t read_mtp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	unsigned int reg, pos, length, i;
	unsigned char readbuf[256] = {0xff, };
	unsigned char writebuf[2] = {0xB0, };

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%x %d %d", &reg, &pos, &length);

	if (!reg || !length || pos < 0 || reg > 0xff || length > 255 || pos > 255)
		return -EINVAL;
	if (priv->state != PANEL_STATE_RESUMED)
		return -EINVAL;

	writebuf[1] = pos;
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	if (dsim_write_hl_data(dsim, writebuf, ARRAY_SIZE(writebuf)) < 0)
		dsim_err("fail to write global command.\n");

	if (dsim_read_hl_data(dsim, reg, length, readbuf) < 0)
		dsim_err("fail to read id on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("fail to write F0 on command.\n");

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
		dsim_err("fail to write test on fc command.\n");

	dsim_info("READ_Reg addr : %02x, start pos : %d len : %d \n", reg, pos, length);
	for (i = 0; i < length; i++)
		dsim_info("READ_Reg %dth : %02x \n", i + 1, readbuf[i]);

	return size;
}

#if defined(CONFIG_PANEL_S6E3HF4_WQHD) || defined(CONFIG_PANEL_S6E3HA5_WQHD)
static int set_partial_display(struct dsim_device *dsim)
{
	u32 st, ed;
	u8 area_cmd[5] = {0x30,};
	u8 partial_mode[1] = {0x12};
	u8 normal_mode[1] = {0x13};

	st =	dsim->lcd_info.partial_range[0];
	ed = dsim->lcd_info.partial_range[1];

	if (st || ed) {
		area_cmd[1] = (st >> 8) & 0xFF;/*select msb 1byte*/
		area_cmd[2] = st & 0xFF;
		area_cmd[3] = (ed >> 8) & 0xFF;/*select msb 1byte*/
		area_cmd[4] = ed & 0xFF;

		dsim_write_hl_data(dsim, area_cmd, ARRAY_SIZE(area_cmd));
		dsim_write_hl_data(dsim, partial_mode, ARRAY_SIZE(partial_mode));
	} else
		dsim_write_hl_data(dsim, normal_mode, ARRAY_SIZE(normal_mode));

	return 0;
}


static ssize_t partial_disp_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	sprintf((char *)buf, "%d, %d\n", dsim->lcd_info.partial_range[0], dsim->lcd_info.partial_range[1]);

	dev_info(dev, "%s: %d, %d\n", __func__, dsim->lcd_info.partial_range[0], dsim->lcd_info.partial_range[1]);

	return strlen(buf);
}

static ssize_t partial_disp_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	u32 st, ed;

	dsim = container_of(priv, struct dsim_device, priv);

	sscanf(buf, "%9d %9d" , &st, &ed);

	dev_info(dev, "%s: %d, %d\n", __func__, st, ed);

	if ((st >= dsim->lcd_info.yres || ed >= dsim->lcd_info.yres) || (st > ed)) {
		pr_err("%s:Invalid Input\n", __func__);
		return size;
	}

	dsim->lcd_info.partial_range[0] = st;
	dsim->lcd_info.partial_range[1] = ed;

	set_partial_display(dsim);

	return size;
}

static DEVICE_ATTR(partial_disp, 0664, partial_disp_show, partial_disp_store);
#endif


#ifdef CONFIG_PANEL_AID_DIMMING
static void show_aid_log(struct dsim_device *dsim)
{
	int i, j, k;
	struct dim_data *dim_data = NULL;
	struct SmtDimInfo *dimming_info = NULL;
#ifdef CONFIG_LCD_HMT
	struct SmtDimInfo *hmt_dimming_info = NULL;
#endif
	struct panel_private *panel = &dsim->priv;
	u8 temp[256];


	if (panel == NULL) {
		dsim_err("%s:panel is NULL\n", __func__);
		return;
	}

	dim_data = (struct dim_data*)(panel->dim_data);
	if (dim_data == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dimming_info = (struct SmtDimInfo*)(panel->dim_info);
	if (dimming_info == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}

	dsim_info("MTP VT : %d %d %d\n",
			dim_data->vt_mtp[CI_RED], dim_data->vt_mtp[CI_GREEN], dim_data->vt_mtp[CI_BLUE] );

	for(i = 0; i < VMAX; i++) {
		dsim_info("MTP V%d : %4d %4d %4d \n",
			vref_index[i], dim_data->mtp[i][CI_RED], dim_data->mtp[i][CI_GREEN], dim_data->mtp[i][CI_BLUE] );
	}

	for(i = 0; i < MAX_BR_INFO; i++) {
		memset(temp, 0, sizeof(temp));
		for(j = 1; j < OLED_CMD_GAMMA_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = dimming_info[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", dimming_info[i].gamma[j] + k);
		}
		dsim_info("nit :%3d %s\n", dimming_info[i].br, temp);
	}
#ifdef CONFIG_LCD_HMT
	hmt_dimming_info = (struct SmtDimInfo*)(panel->hmt_dim_info);
	if (hmt_dimming_info == NULL) {
		dsim_info("%s:dimming is NULL\n", __func__);
		return;
	}
	for(i = 0; i < HMT_MAX_BR_INFO; i++) {
		memset(temp, 0, sizeof(temp));
		for(j = 1; j < OLED_CMD_GAMMA_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = hmt_dimming_info[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", hmt_dimming_info[i].gamma[j] + k);
		}
		dsim_info("nit :%3d %s\n", hmt_dimming_info[i].br, temp);
	}
#endif
}


static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	show_aid_log(dsim);
	return strlen(buf);
}

static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		priv->code[0], priv->code[1], priv->code[2], priv->code[3], priv->code[4]);

	return strlen(buf);
}

static ssize_t cell_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X%02X\n",
		priv->date[0] , priv->date[1], priv->date[2], priv->date[3], priv->date[4],
		priv->date[5],priv->date[6], (priv->coordinate[0] &0xFF00)>>8,priv->coordinate[0] &0x00FF,
		(priv->coordinate[1]&0xFF00)>>8,priv->coordinate[1]&0x00FF);

	return strlen(buf);
}

#ifdef CONFIG_CHECK_OCTA_CHIP_ID
static ssize_t octa_id_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	int site, rework, poc;
	char cell_id[16];
	int i;
	unsigned char* octa_id;

	octa_id = &(priv->octa_id[1]);

	site = octa_id[0] & 0xf0;
	site >>= 4;
	rework = octa_id[0] & 0x0f;
	poc = octa_id[1] & 0x0f;

	dsim_info("site (%d), rework (%d), poc (%d)\n",
			site, rework, poc);

	dsim_info("<CELL ID>\n");
	for(i = 0; i < 16; i++) {
		cell_id[i] = octa_id[i+4];
		dsim_info("%x -> %c\n",octa_id[i+4], cell_id[i]);
	}

	sprintf(buf, "%d%d%d%02x%02x%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c%c\n",
		site, rework, poc, octa_id[2], octa_id[3],
		cell_id[0], cell_id[1], cell_id[2], cell_id[3],
		cell_id[4], cell_id[5], cell_id[6], cell_id[7],
		cell_id[8], cell_id[9], cell_id[10], cell_id[11],
		cell_id[12], cell_id[13], cell_id[14], cell_id[15]);

	return strlen(buf);
}
static DEVICE_ATTR(octa_id, 0444, octa_id_show, NULL);
#endif

#ifdef CONFIG_LCD_WEAKNESS_CCB
void ccb_set_mode(struct dsim_device *dsim, u8 ccb, int stepping)
{
	u8 ccb_cmd[3] = {0xB7, 0x00, 0x00};
	u8 secondval = 0;

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	switch( dsim->priv.panel_type ) {
#ifdef CONFIG_PANEL_S6E3HA5_WQHD
	case LCD_TYPE_S6E3HA5_WQHD:
		ccb_cmd[1] = ccb;
		ccb_cmd[2] = 0x2A;
		dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
		break;
#endif
	case LCD_TYPE_S6E3HF4_WQHD:
		ccb_cmd[1] = ccb;
		ccb_cmd[2] = 0x2A;
		dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
		break;
	case LCD_TYPE_S6E3HA3_WQHD:
		if((ccb & 0x0F) == 0x00) {		// off
			if(stepping) {
				ccb_cmd[1] = dsim->priv.current_ccb;
				for(secondval = 0x2A; secondval <= 0x3F; secondval += 1) {
					ccb_cmd[2] = secondval;
					dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
					msleep(17);
				}
			}
			ccb_cmd[1] = 0x00;
			ccb_cmd[2] = 0x3F;
			dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
		} else {						// on
			ccb_cmd[1] = ccb;
			if(stepping) {
				for(secondval = 0x3F; secondval >= 0x2A; secondval -= 1) {
					ccb_cmd[2] = secondval;
					dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
					if(secondval != 0x2A)
						msleep(17);
				}
			} else {
				ccb_cmd[2] = 0x2A;
				dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
			}
		}
		msleep(17);
		break;
	default:
		pr_info("%s unknown panel \n", __func__);
		break;
	}

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
}

static ssize_t weakness_ccb_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	return strlen(buf);
}

static ssize_t weakness_ccb_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int ret;
	int type, serverity;
	unsigned char set_ccb = 0x00;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	ret = sscanf(buf, "%d %d", &type, &serverity);

	if (ret < 0)
		return ret;

	dev_info(dev, "%s: type = %d serverity = %d\n", __func__, type, serverity);

	if(priv->ccb_support == UNSUPPORT_CCB) {
		pr_info("%s this panel does not support CCB!\n", __func__);
		return size;
	}

	if((type < 0) || (type > 3)) {
		dev_info(dev, "%s: type is out of range\n", __func__);
		ret = -EINVAL;
	} else if((serverity < 0) || (serverity > 9)) {
		dev_info(dev, "%s: serverity is out of range\n", __func__);
		ret = -EINVAL;
	} else {
		set_ccb = ((u8)(serverity) << 4);
		switch(type) {
		case 0:
			set_ccb = 0;
			dev_info(dev, "%s: disable ccb\n", __func__);
			break;
		case 1:
			set_ccb += 1;
			dev_info(dev, "%s: enable red\n", __func__);
			break;
		case 2:
			set_ccb += 5;
			dev_info(dev, "%s: enable green\n", __func__);
			break;
		case 3:
			if(serverity == 0) {
				set_ccb += 9;
				dev_info(dev, "%s: enable blue\n", __func__);
			} else {
				set_ccb = 0;
				set_ccb += 9;
				dev_info(dev, "%s: serverity is out of range, blue only support 0\n", __func__);
			}
			break;
		default:
			set_ccb = 0;
			break;
		}
		if(priv->current_ccb == set_ccb) {
			dev_info(dev, "%s: aleady set same ccb\n", __func__);
		} else {
			ccb_set_mode(dsim, set_ccb, 1);
			priv->current_ccb = set_ccb;
		}
	}
	dev_info(dev, "%s: --\n", __func__);

	return size;
}
static DEVICE_ATTR(weakness_ccb, 0664, weakness_ccb_show, weakness_ccb_store);

#endif


#ifdef CONFIG_PANEL_DDI_SPI
static int ddi_spi_enablespi_send(struct dsim_device *dsim)
{
	int ret =0;

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	dsim_write_hl_data(dsim, SEQ_B0_13, ARRAY_SIZE(SEQ_B0_13));
	dsim_write_hl_data(dsim, SEQ_D8_02, ARRAY_SIZE(SEQ_D8_02));
	dsim_write_hl_data(dsim, SEQ_B0_1F, ARRAY_SIZE(SEQ_B0_1F));
	dsim_write_hl_data(dsim, SEQ_FE_06, ARRAY_SIZE(SEQ_FE_06));
	msleep(30);
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));

	return ret;
}

static ssize_t read_copr_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	static int string_size = 70;
	char temp[string_size];
	int* copr;

	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	struct panel_private *panel;

	dsim = container_of(priv, struct dsim_device, priv);
	panel = &dsim->priv;

	if(panel->state == PANEL_STATE_RESUMED)
	{
		ddi_spi_enablespi_send(dsim);

		mutex_lock(&panel->lock);
		copr = ddi_spi_read_opr();
		mutex_unlock(&panel->lock);

		pr_info("%s %x %x %x %x %x %x %x %x %x %x \n", __func__,
				copr[0], copr[1], copr[2], copr[3], copr[4],
				copr[5], copr[6], copr[7], copr[8], copr[9]);

		snprintf((char *)temp, sizeof(temp), "%02x %02x %02x %02x %02x %02x %02x %02x %02x %02x\n",
			copr[0], copr[1], copr[2], copr[3], copr[4], copr[5], copr[6], copr[7], copr[8], copr[9]);
	}
	else
	{
		snprintf((char *)temp, sizeof(temp), "00 00 00 00 00 00 00 00 00 00\n");
	}

	strlcat(buf, temp, string_size);

	return strnlen(buf, string_size);

}

static DEVICE_ATTR(read_copr, 0444, read_copr_show, NULL);

#endif


#ifdef CONFIG_FB_DSU
static ssize_t resolution_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int i;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);
	for( i=0; i<ARRAY_SIZE(dsu_config); i++ ) {
		if( dsu_config[i].value == dsim->dsu_param_value ) {
			strcpy( buf, dsu_config[i].id_str );
			pr_info( "%s:%s,%d\n", __func__, dsu_config[i].id_str, dsu_config[i].value );
			return strlen(buf);
			break;
		}
	}

	strcpy(buf, "WQHD" );
	pr_err( "%s:(default)%s,%d\n", __func__, buf, DSU_CONFIG_WQHD );
	return strlen(buf);
}

static ssize_t resolution_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);
	int ret;
	int i;

	dsim = container_of(priv, struct dsim_device, priv);
	if( dsim->dsu_param_offset==0 ) {
		pr_err( "%s: failed. offset not exist\n", __func__ );
	}

	for( i=0; i<ARRAY_SIZE(dsu_config); i++ ) {
		if( !strncmp(dsu_config[i].id_str, buf, strlen(dsu_config[i].id_str)) ) {
			dsim->dsu_param_value = dsu_config[i].value;
			ret = sec_set_param((unsigned long) dsim->dsu_param_offset, dsim->dsu_param_value);
			pr_info( "%s:%s,%d,%d\n", __func__, dsu_config[i].id_str, dsim->dsu_param_value, ret );
			return size;
		}
	}

	pr_err( "%s: failed (%s)\n", __func__, buf );
	return size;
}
static DEVICE_ATTR(resolution, 0664, resolution_show, resolution_store);

#endif


#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
static ssize_t lux_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->lux);

	return strlen(buf);
}

static ssize_t lux_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_private *priv = dev_get_drvdata(dev);
	int value;
	int rc;

	rc = kstrtoint(buf, 0, &value);

	if (rc < 0)
		return rc;

	if (priv->lux != value) {
		mutex_lock(&priv->lock);
		priv->lux = value;
		mutex_unlock(&priv->lock);

		attr_store_for_each(priv->mdnie_class, attr->attr.name, buf, size);
	}

	return size;
}
static DEVICE_ATTR(lux, 0644, lux_show, lux_store);
#endif

static ssize_t adaptive_control_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d\n", priv->adaptive_control);

	return strlen(buf);
}

static ssize_t adaptive_control_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	int rc, value;
	struct dsim_device *dsim;
	struct panel_private *priv = dev_get_drvdata(dev);

	dsim = container_of(priv, struct dsim_device, priv);

	rc = kstrtouint(buf, 0, &value);

	if (rc < 0)
		return rc;

	if (priv->adaptive_control != value) {
		dev_info(dev, "%s: %d, %d\n", __func__, priv->adaptive_control, value);
		mutex_lock(&priv->lock);
		priv->adaptive_control = value;
		mutex_unlock(&priv->lock);
		dsim_panel_set_brightness(dsim, 1);

	}

	return size;
}

#ifdef CONFIG_DISPLAY_USE_INFO
/*
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_INFO, DPUI_TYPE_PANEL);

	return size;
}

/*
 * [DEV ONLY]
 * HW PARAM LOGGING SYSFS NODE
 */
static ssize_t dpui_dbg_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	int ret;

	update_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	ret = get_dpui_log(buf, DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);
	if (ret < 0) {
		pr_err("%s failed to get log %d\n", __func__, ret);
		return ret;
	}

	pr_info("%s\n", buf);
	return ret;
}

static ssize_t dpui_dbg_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	if (buf[0] == 'C' || buf[0] == 'c')
		clear_dpui_log(DPUI_LOG_LEVEL_DEBUG, DPUI_TYPE_PANEL);

	return size;
}


static DEVICE_ATTR(dpui, 0660, dpui_show, dpui_store);
static DEVICE_ATTR(dpui_dbg, 0660, dpui_dbg_show, dpui_dbg_store);
#endif

#ifdef CONFIG_SUPPORT_POC_FLASH
static ssize_t poc_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
	int ret;

	if (panel == NULL) {
		pr_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}

	if (panel->state != PANEL_STATE_RESUMED) {
		pr_err("%s:panel is not active\n", __func__);
		return -EAGAIN;
	}

	poc_dev = &panel->poc_dev;
	poc_info = &poc_dev->poc_info;

	ret = set_panel_poc(poc_dev, POC_OP_CHECKPOC);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to chkpoc (ret %d)\n", __func__, ret);
		return ret;
	}

	ret = set_panel_poc(poc_dev, POC_OP_CHECKSUM);
	if (unlikely(ret < 0)) {
		pr_err("%s, failed to chksum (ret %d)\n", __func__, ret);
		return ret;
	}

	snprintf(buf, PAGE_SIZE, "%d %d %02x\n", poc_info->poc,
			poc_info->poc_chksum[4], poc_info->poc_ctrl[3]);

	dev_info(dev, "%s poc:%d chk:%d gray:%02x\n", __func__, poc_info->poc,
			poc_info->poc_chksum[4], poc_info->poc_ctrl[3]);

	return strlen(buf);
}

static ssize_t poc_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t size)
{
	struct panel_private *panel = dev_get_drvdata(dev);
	struct panel_poc_device *poc_dev;
	struct panel_poc_info *poc_info;
	int rc, ret;
	unsigned int value;

	if (panel == NULL) {
		pr_err("PANEL:ERR:%s:panel is null\n", __func__);
		return -EINVAL;
	}
	poc_dev = &panel->poc_dev;
	poc_info = &poc_dev->poc_info;

	rc = kstrtouint(buf, (unsigned int)0, &value);
	if (rc < 0)
		return rc;

	if (!IS_VALID_POC_OP(value)) {
		pr_err("%s invalid poc_op %d\n", __func__, value);
		return -EINVAL;
	}

	if (value == POC_OP_WRITE || value == POC_OP_READ) {
		pr_err("%s unsupported poc_op %d\n", __func__, value);
		return size;
	}

	if (value == POC_OP_CANCEL) {
		atomic_set(&poc_dev->cancel, 1);
	} else {
		ret = set_panel_poc(poc_dev, value);
		if (unlikely(ret < 0)) {
			pr_err("%s, failed to poc_op %d(ret %d)\n", __func__, value, ret);
			mutex_unlock(&panel->lock);
			return -EINVAL;
		}
	}

	mutex_lock(&panel->lock);
	panel->poc_op = value;

	mutex_unlock(&panel->lock);

	pr_info("%s poc_op %d\n",
			__func__, panel->poc_op);

	return size;
}
static DEVICE_ATTR(poc, 0664, poc_show, poc_store);
#endif

static DEVICE_ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store);
static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(SVC_OCTA, 0444, SVC_OCTA_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(read_mtp, 0664, read_mtp_show, read_mtp_store);

void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;
	struct kernfs_node* SVC_sd;
	struct kobject* SVC;

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_lcd_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_window_type);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_manufacture_code);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_cell_id);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#ifdef CONFIG_CHECK_OCTA_CHIP_ID
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_octa_id);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_brightness_table);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_siop_enable);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_temperature);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_color_coordinate);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_manufacture_date);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef CONFIG_PANEL_AID_DIMMING
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_aid_log);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_read_mtp);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#if defined(CONFIG_PANEL_S6E3HF4_WQHD) || defined(CONFIG_PANEL_S6E3HA5_WQHD)
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#if defined(CONFIG_EXYNOS_DECON_LCD_MCD)
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_mcd_mode);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_PANEL_GRAY_SPOT
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_grayspot);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_HMT
#ifdef CONFIG_PANEL_AID_DIMMING
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_hmt_bright);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_hmt_on);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_ALPM
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_alpm);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_DOZE_MODE
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_alpm);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_WEAKNESS_CCB
	ret = device_create_file(&dsim->priv.bd->dev, &dev_attr_weakness_ccb);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_PANEL_DDI_SPI
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_read_copr);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_DISPLAY_USE_INFO
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_dpui);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_dpui_dbg);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_SUPPORT_POC_FLASH
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_poc);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_FB_DSU
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_resolution);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_adaptive_control);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

	ret = device_create_file(&dsim->lcd->dev, &dev_attr_SVC_OCTA);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

	/* to /sys/devices/svc/ */
	SVC_sd = sysfs_get_dirent(devices_kset->kobj.sd, "svc");
	if(IS_ERR_OR_NULL(SVC_sd)) {
		SVC = kobject_create_and_add("svc", &devices_kset->kobj);
		if(IS_ERR_OR_NULL(SVC))
			dsim_err("failed to create /sys/devices/svc already exist svc : 0x%p\n", SVC);
		else
			dsim_err("success to create /sys/devices/svc svc : 0x%p\n", SVC);
	} else {
		SVC = (struct kobject*)SVC_sd->priv;
		dsim_info("success to find svc_sd : 0x%p  svc : 0x%p\n", SVC_sd, SVC);
	}

	if(!IS_ERR_OR_NULL(SVC)) {
		ret = sysfs_create_link(SVC, &dsim->lcd->dev.kobj, "OCTA");
		if(ret)
			dsim_err("failed to create svc/OCTA/ \n");
		else
			dsim_info("success to create svc/OCTA/ \n");
	} else {
		dsim_err("failed to find svc kobject\n");
	}
}

