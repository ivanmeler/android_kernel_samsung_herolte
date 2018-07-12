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
#include "oled_backlight.h"

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_EXYNOS_DECON_LCD_MCD)
void mcd_mode_set(struct dsim_device *dsim)
{
	int i = 0;
	struct panel_private *panel = &dsim->priv;

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));

	if(panel->mcd_on == 0) {
		pr_info("%s MCD off : %d\n", __func__, panel->mcd_on);

		for(i = 0; i < (sizeof(SEQ_MCD_OFF_SET1) / sizeof(SEQ_MCD_OFF_SET1[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET1[i], ARRAY_SIZE(SEQ_MCD_OFF_SET1[i]));

		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		msleep(100);

		for(i = 0; i < (sizeof(SEQ_MCD_OFF_SET2) / sizeof(SEQ_MCD_OFF_SET2[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_OFF_SET2[i], ARRAY_SIZE(SEQ_MCD_OFF_SET2[i]));

	} else {					// mcd on
		pr_info("%s MCD on : %d\n", __func__, panel->mcd_on);

		for(i = 0; i < (sizeof(SEQ_MCD_ON_SET1) / sizeof(SEQ_MCD_ON_SET1[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_ON_SET1[i], ARRAY_SIZE(SEQ_MCD_ON_SET1[i]));

		dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
		msleep(100);

		for(i = 0; i < (sizeof(SEQ_MCD_ON_SET2) / sizeof(SEQ_MCD_ON_SET2[0])); i++)
			dsim_write_hl_data(dsim, SEQ_MCD_ON_SET2[i], ARRAY_SIZE(SEQ_MCD_ON_SET2[i]));
	}

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



static ssize_t lcd_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "SDC_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%x %x %x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);

	char *pos = buf;
	int nit, i;
	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		nit = panel->mapping_tbl.ref_br_tbl[i];
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

	if (priv->siop_enable != value) {
		dev_info(dev, "%s: %d, %d\n", __func__, priv->siop_enable, value);
		mutex_lock(&priv->lock);
		priv->siop_enable = value;
		mutex_unlock(&priv->lock);
		panel_set_brightness(dsim, 1);
	}
	return size;
}

static ssize_t temperature_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	char temp[] = "-20, -19, 0, 1\n";

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
			panel_set_brightness(dsim, 1);
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

#ifdef CONFIG_PANEL_AID_DIMMING_OLD
static void show_aid_log(struct dsim_device *dsim)
{
	int i, j, k;
	struct dim_data *dim_data = NULL;
	struct SmtDimInfo *dimming_info = NULL;

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

#else


static ssize_t aid_log_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);
	int i, j, k;
	u8 temp[256];

	if (panel == NULL) {
		dsim_err("%s:panel is NULL\n", __func__);
		return strlen(buf);
	}

	dsim_info("VT : %d %d %d \n", panel->command.vt_mtp[0], panel->command.vt_mtp[1], panel->command.vt_mtp[2]);

	for(i = 0; i < VREF_POINT_CNT; i++)
		printk("mtp : %d %d %d \n", panel->command.mtp[i][0], panel->command.mtp[i][1], panel->command.mtp[i][2]);


	for(i = 0; i < MAX_DIMMING_INFO_COUNT; i++) {
		memset(temp, 0, sizeof(temp));
		for(j = 1; j < GAMMA_COMMAND_CNT; j++) {
			if (j == 1 || j == 3 || j == 5)
				k = panel->command.gamma_cmd[i].gamma[j++] * 256;
			else
				k = 0;
			snprintf(temp + strnlen(temp, 256), 256, " %d", panel->command.gamma_cmd[i].gamma[j] + k);
		}
		dsim_info("idx :%3d %s\n", i, temp);
	}

	return strlen(buf);
}

#endif

static ssize_t manufacture_code_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02X%02X%02X%02X%02X\n",
		priv->code[0], priv->code[1], priv->code[2], priv->code[3], priv->code[4]);

	return strlen(buf);
}

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
	int rc;
	int value;
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
		panel_set_brightness(dsim, 0);
	}

	return size;
}

static ssize_t panel_state_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%d", priv->pm_state);
	return strlen(buf);
}

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

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
		attr_store_for_each(priv->mdnie_class, attr->attr.name, buf, size);
#endif
	}

	return size;
}

static struct device_attribute panel_dev_attrs[] = {
	__ATTR(lcd_type, 0444, lcd_type_show, NULL),
	__ATTR(window_type, 0444, window_type_show, NULL),
	__ATTR(manufacture_code, 0444, manufacture_code_show, NULL),
	__ATTR(brightness_table, 0444, brightness_table_show, NULL),
	__ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store),
	__ATTR(temperature, 0664, temperature_show, temperature_store),
	__ATTR(color_coordinate, 0444, color_coordinate_show, NULL),
	__ATTR(manufacture_date, 0444, manufacture_date_show, NULL),
	__ATTR(read_mtp, 0664, read_mtp_show, read_mtp_store),
	__ATTR(aid_log, 0444, aid_log_show, NULL),
	__ATTR(state, 0444, panel_state_show, NULL),
	__ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store),
	__ATTR(lux, 0644, lux_show, lux_store)
};

void lcd_init_sysfs(struct dsim_device *dsim)
{
	int i;

	/* sysfs located /sys/class/lcd/panel/... */
	for (i = 0; i < ARRAY_SIZE(panel_dev_attrs); i++) {
		if (device_create_file(&dsim->lcd->dev, panel_dev_attrs + i)) {
			dev_err(&dsim->lcd->dev, "ERROR : Failed to create device file %d:%s\n",
				i, panel_dev_attrs[i].attr.name);
		}
	}

	return;
}



