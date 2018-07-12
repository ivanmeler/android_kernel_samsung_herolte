/* linux/drivers/video/exynos_decon/panel/dsim_backlight.c
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


#include <linux/backlight.h>

#include "../dsim.h"
#include "dsim_backlight.h"
#include "panel_info.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "aid_dimming.h"
#endif

unsigned dynamic_lcd_type;

#ifdef CONFIG_PANEL_AID_DIMMING

static unsigned char *get_aid_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (u8 *)dimming_info[index].aid;

get_aid_err:
	return NULL;
}


#ifdef AID_INTERPOLATION

static void get_aid_interpolation(struct dsim_device *dsim, u8* aid)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;
	int current_pbr = panel->bd->props.brightness;


	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_aid_err;
	}

	if (current_pbr > UI_MAX_BRIGHTNESS)
		current_pbr = UI_MAX_BRIGHTNESS;

	if (panel->inter_aor_tbl == NULL)
		goto get_aid_err;
	aid[0] = 0xB1;
	aid[1] = panel->inter_aor_tbl[current_pbr * 2];
	aid[2] = panel->inter_aor_tbl[current_pbr * 2 + 1];

	dsim_info("%s %d aid : %x : %x : %x\n", __func__, current_pbr, aid[0], aid[1], aid[2]);

get_aid_err:
	aid = NULL;

}
#endif


unsigned int get_actual_br_value(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_br_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return dimming_info[index].br;

get_br_err:
	return 0;
}
static unsigned char *get_gamma_from_index(struct dsim_device *dsim, int index)
{
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = (struct SmtDimInfo *)panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming info is NULL\n", __func__);
		goto get_gamma_err;
	}

	if (index > MAX_BR_INFO)
		index = MAX_BR_INFO;

	return (unsigned char *)dimming_info[index].gamma;

get_gamma_err:
	return NULL;
}


static void dsim_panel_gamma_ctrl(struct dsim_device *dsim)
{
	u8 *gamma = NULL;

	gamma = get_gamma_from_index(dsim, dsim->priv.br_index);
	if (gamma == NULL) {
		dsim_err("%s :faied to get gamma\n", __func__);
		return;
	}

	if (dsim_write_hl_data(dsim, gamma, GAMMA_CMD_CNT) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);
}


static void dsim_panel_aid_ctrl(struct dsim_device *dsim)
{
	u8 *aid = NULL;
	unsigned char SEQ_AID[AID_LEN_MAX + 1] = { 0, };
	struct panel_private *panel = &dsim->priv;
	int i;

	aid = get_aid_from_index(dsim, dsim->priv.br_index);


	if (aid == NULL) {
		dsim_err("%s : faield to get aid value\n", __func__);
		return;
	}

	memcpy(SEQ_AID, panel->aid_set, panel->aid_len);
	memcpy(&SEQ_AID[panel->aid_reg_offset], aid + 1, AID_LEN_MAX -1);

	if (dsim_write_hl_data(dsim, SEQ_AID, panel->aid_len) < 0)
		dsim_err("%s : failed to write aid \n", __func__);

	dsim_info("aid len = %d\n", panel->aid_len);
	dsim_info("(dsim) aid %dth = %02x %02x %02x %02x %02x\n", i, SEQ_AID[0], SEQ_AID[1], SEQ_AID[2], SEQ_AID[3], SEQ_AID[4]);

}


static int low_level_set_brightness(struct dsim_device *dsim ,int force)
{
	dsim_info( "%s +\n", __func__ );

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC)) < 0)
		dsim_err("%s : fail to write F0 on command.\n", __func__);

	dsim_panel_gamma_ctrl(dsim);

	dsim_panel_aid_ctrl(dsim);

	if (dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE)) < 0)
		dsim_err("%s : failed to write gamma \n", __func__);

	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC)) < 0)
			dsim_err("%s : fail to write F0 on command.\n", __func__);
	if (dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0)) < 0)
		dsim_err("%s : fail to write F0 on command\n", __func__);

	dsim_info( "%s -\n", __func__ );
	return 0;
}

int get_acutal_br_index(struct dsim_device *dsim, int br)
{
	int i;
	int min;
	int gap;
	int index = 0;
	struct panel_private *panel = &dsim->priv;
	struct SmtDimInfo *dimming_info = panel->dim_info;

	if (dimming_info == NULL) {
		dsim_err("%s : dimming_info is NULL\n", __func__);
		return 0;
	}

	min = MAX_BRIGHTNESS;

	for (i = 0; i < MAX_BR_INFO; i++) {
		if (br > dimming_info[i].br)
			gap = br - dimming_info[i].br;
		else
			gap = dimming_info[i].br - br;

		if (gap == 0) {
			index = i;
			break;
		}

		if (gap < min) {
			min = gap;
			index = i;
		}
	}
	return index;
}

#endif

int dsim_panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
#ifndef CONFIG_PANEL_AID_DIMMING
	dsim_info("%s:this panel does not support dimming \n", __func__);
#else
	struct dim_data *dimming;
	struct panel_private *panel = &dsim->priv;
	int p_br = panel->bd->props.brightness;
	int acutal_br = 0;
	int real_br = 0;

	dsim_info( "%s +\n", __func__ );


	dimming = (struct dim_data *)panel->dim_data;
	if ((dimming == NULL) || (panel->br_tbl == NULL)) {
		dsim_info("%s : this panel does not support dimming\n", __func__);
		return ret;
	}
	acutal_br = panel->br_tbl[p_br];
	panel->br_index = get_acutal_br_index(dsim, acutal_br);
	real_br = get_actual_br_value(dsim, panel->br_index);

	if (panel->state != PANEL_STATE_RESUMED) {
		dsim_info("%s : panel is not active state..\n", __func__);
		goto set_br_exit;
	}

	dsim_info("%s : platform : %d, : mapping : %d, real : %d, index : %d, lx: %d\n",
		__func__, p_br, acutal_br, real_br, panel->br_index, panel->lux);

	if (!force)
		goto set_br_exit;

	if ((acutal_br == 0) || (real_br == 0))
		goto set_br_exit;

	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, force);
	if (ret) {
		dsim_err("%s failed to set brightness : %d\n", __func__, acutal_br);
	}
	mutex_unlock(&panel->lock);

set_br_exit:
	dsim_info( "%s -\n", __func__ );
#endif
	return ret;

}






static int panel_get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}


static int panel_set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (brightness < UI_MIN_BRIGHTNESS || brightness > EXTEND_BRIGHTNESS) {
		pr_alert("Brightness %d is out of range\n", brightness);
		ret = -EINVAL;
		goto exit_set;
	}

	ret = dsim_panel_set_brightness(dsim, 1);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:
	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = panel_get_brightness,
	.update_status = panel_set_brightness,
};


int dsim_backlight_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	panel->bd = backlight_device_register("panel", dsim->dev, &dsim->priv,
					&panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = EXTEND_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	return ret;
}
