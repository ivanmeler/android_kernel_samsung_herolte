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
#include "../decon.h"

#ifdef CONFIG_PANEL_DDI_SPI
#include "ddi_spi.h"
#endif

#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
#include "mdnie.h"
#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_EXYNOS_DECON_LCD_MCD)
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
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F1, ARRAY_SIZE(SEQ_TEST_KEY_ON_F1));
	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if(panel->mcd_on == 0)	{	// mcd off
		pr_info("%s MCD off : %d\n", __func__, panel->mcd_on);
		mcd_write_set(dsim, SEQ_MCD_OFF_SET, ARRAY_SIZE(SEQ_MCD_OFF_SET));
	} else {					// mcd on
		pr_info("%s MCD on : %d\n", __func__, panel->mcd_on);
		mcd_write_set(dsim, SEQ_MCD_ON_SET, ARRAY_SIZE(SEQ_MCD_ON_SET));
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

	sprintf(buf, "SDC_%02X%02X%02X\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t window_type_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *priv = dev_get_drvdata(dev);

	sprintf(buf, "%02x %02x %02x\n", priv->id[0], priv->id[1], priv->id[2]);

	return strlen(buf);
}

static ssize_t brightness_table_show(struct device *dev,
	struct device_attribute *attr, char *buf)
{
	struct panel_private *panel = dev_get_drvdata(dev);

	char *pos = buf;
	int nit, i;
	for (i = 0; i <= UI_MAX_BRIGHTNESS; i++) {
		nit = panel->br_tbl[i];
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
#ifdef CONFIG_PANEL_S6E3HF4_WQHD
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
#endif


#ifdef CONFIG_PANEL_AID_DIMMING
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
		dsim_panel_set_brightness(dsim, 0);

	}

	return size;
}
static DEVICE_ATTR(adaptive_control, 0664, adaptive_control_show, adaptive_control_store);


#ifdef CONFIG_LCD_WEAKNESS_CCB

void ccb_set_mode(struct dsim_device *dsim, u8 ccb, int stepping)
{
	u8 ccb_cmd[3] = {0xB7, 0x00, 0x00};
	u8 secondval = 0;

	dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if(dsim->priv.panel_type == LCD_TYPE_S6E3HF4_WQHD) {
		ccb_cmd[1] = ccb;
		ccb_cmd[2] = 0x2A;
		dsim_write_hl_data(dsim, ccb_cmd, ARRAY_SIZE(ccb_cmd));
	} else if(dsim->priv.panel_type == LCD_TYPE_S6E3HA3_WQHD) {
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
	} else {
		pr_info("%s unknown panel \n", __func__);
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

static DEVICE_ATTR(lcd_type, 0444, lcd_type_show, NULL);
static DEVICE_ATTR(window_type, 0444, window_type_show, NULL);
static DEVICE_ATTR(manufacture_code, 0444, manufacture_code_show, NULL);
static DEVICE_ATTR(cell_id, 0444, cell_id_show, NULL);
static DEVICE_ATTR(brightness_table, 0444, brightness_table_show, NULL);
static DEVICE_ATTR(siop_enable, 0664, siop_enable_show, siop_enable_store);
static DEVICE_ATTR(temperature, 0664, temperature_show, temperature_store);
static DEVICE_ATTR(color_coordinate, 0444, color_coordinate_show, NULL);
static DEVICE_ATTR(manufacture_date, 0444, manufacture_date_show, NULL);
static DEVICE_ATTR(read_mtp, 0664, read_mtp_show, read_mtp_store);
#ifdef CONFIG_PANEL_S6E3HF4_WQHD
static DEVICE_ATTR(partial_disp, 0664, partial_disp_show, partial_disp_store);
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
static DEVICE_ATTR(aid_log, 0444, aid_log_show, NULL);
#endif

void lcd_init_sysfs(struct dsim_device *dsim)
{
	int ret = -1;

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

#ifdef CONFIG_PANEL_S6E3HF4_WQHD
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_partial_disp);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#if defined(CONFIG_SEC_FACTORY) && defined(CONFIG_EXYNOS_DECON_LCD_MCD)
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_mcd_mode);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif

#ifdef CONFIG_LCD_DOZE_MODE
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_alpm);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_adaptive_control);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);

#ifdef CONFIG_LCD_WEAKNESS_CCB
	ret = device_create_file(&dsim->priv.bd->dev ,&dev_attr_weakness_ccb);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
#ifdef CONFIG_PANEL_DDI_SPI
	ret = device_create_file(&dsim->lcd->dev ,&dev_attr_read_copr);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
#if defined(CONFIG_EXYNOS_DECON_MDNIE_LITE)
	ret = device_create_file(&dsim->lcd->dev, &dev_attr_lux);
	if (ret < 0)
		dev_err(&dsim->lcd->dev, "failed to add sysfs entries, %d\n", __LINE__);
#endif
}


