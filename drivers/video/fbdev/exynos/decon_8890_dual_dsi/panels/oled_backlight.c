/* linux/drivers/video/exynos_decon/panel/panel_backlight.c
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
#include "oled_backlight.h"
#include "panel_info.h"

#ifdef CONFIG_PANEL_AID_DIMMING
#include "smart_dimming_core.h"
#endif

#define ROUND_AID(x)		((x+50)/100)

static int set_oled_tset(struct dsim_device *dsim, int force)
{
	int ret = 0;
	int tset = 0;
	unsigned char SEQ_TSET[2] = {0xB8, };

	tset = (dsim->priv.temperature < 0) ? BIT(7) | abs(dsim->priv.temperature) : dsim->priv.temperature;

	SEQ_TSET[1] = tset;

	if ((ret = dsim_write_hl_data(dsim, SEQ_TSET, ARRAY_SIZE(SEQ_TSET))) < 0) {
		dsim_err("fail to write tset command.\n");
		ret = -EPERM;
	}
	dsim_info("%s temperature: %d, tset: %d\n",
		__func__, dsim->priv.temperature, SEQ_TSET[1]);

	return ret;
}


static int set_oled_acl(struct dsim_device *dsim, int force)
{
	int acl;
	int ret = 0;
	int phy_br;

	struct panel_info *cmd;
	struct panel_private *panel = &dsim->priv;
	int platform_br = panel->bd->props.brightness;

	cmd = &panel->command;
	phy_br = panel->mapping_tbl.phy_br_tbl[platform_br];

	if (force)
		goto set_acl;

	acl = (ACL_IS_ON(phy_br) && (!panel->current_hbm));
	if (acl == panel->cur_acl)
		goto set_acl_exit;

set_acl:
	if (acl) {
		dsim_info("ACL IS ON\n");
		ret = dsim_write_hl_data(dsim, SEQ_OPR_ACL_ON, ARRAY_SIZE(SEQ_OPR_ACL_ON));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd acl on opr \n", __func__);

		ret = dsim_write_hl_data(dsim, SEQ_ACL_ON, ARRAY_SIZE(SEQ_ACL_ON));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd alc on\n", __func__);
	} else {
		dsim_info("ACL IS OFF\n");
		ret = dsim_write_hl_data(dsim, SEQ_OPR_ACL_OFF, ARRAY_SIZE(SEQ_OPR_ACL_OFF));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd acl on opr \n", __func__);

		ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd alc on\n", __func__);
	}
	panel->cur_acl = acl;

set_acl_exit:
	return ret;
}


static int set_oled_elvss(struct dsim_device *dsim, int force)
{
	int ret = 0, idx;
	char elvss[ELVSS_LEN] = {0, };
	struct panel_private *panel = &dsim->priv;

	idx = panel->cur_br_idx;

	memcpy(&elvss[1], panel->elvss_set, ELVSS_LEN - 1);

	elvss[0] = 0xB6;
	elvss[1] = panel->command.aid_info[idx].mps;
	elvss[2] = panel->command.aid_info[idx].elv;

	if(UNDER_0(panel->temperature)) {
		elvss[22] += panel->command.aid_info[idx].elvss_offset[UNDER_ZERO];
	} else if(UNDER_MINUS_20(panel->temperature)) {
		elvss[22] += panel->command.aid_info[idx].elvss_offset[UNDER_MINUS_TWENTY];
	} else {
		elvss[22] += panel->command.aid_info[idx].elvss_offset[OVER_ZERO];
	}

	ret = dsim_write_hl_data(dsim, elvss, ELVSS_LEN);
	if (ret < 0)
		dsim_err("ERR:%s:failed to write aid\n", __func__);

	return ret;
}

static int set_oled_hbm(struct dsim_device *dsim, int force)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;

	if(!force)
		goto set_hbm_exit;

	if (panel->current_hbm) {
		dsim_info("%s HBM ON\n", __func__);
		ret = dsim_write_hl_data(dsim, SEQ_HBM_ON, ARRAY_SIZE(SEQ_HBM_ON));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd acl on opr \n", __func__);
	} else {
		dsim_info("%s HBM OFF\n", __func__);
		ret = dsim_write_hl_data(dsim, SEQ_HBM_OFF, ARRAY_SIZE(SEQ_HBM_OFF));
		if (ret < 0)
			dsim_err("ERR:%s:failed to write cmd acl on opr \n", __func__);
	}

set_hbm_exit:
	return ret;
}


static int set_oled_aid(struct dsim_device *dsim, int force)
{
	int ret = 0, idx;
	char aid_cmd[3] = {0, };
	int vertical, value;
	//struct aid_dimming_info *aid_info;
	struct panel_private *panel = &dsim->priv;
	struct decon_lcd *lcd_info = &dsim->lcd_info;

	idx = panel->cur_br_idx;
#if 0
	aid_info = (struct panel_private *)&panel->aid_info;

	if (aid_info == NULL) {
		dsim_err("ERR:%s:aid_info is NULL\n", __func__);
		ret = -EINVAL;
		goto set_aid_exit;
	}
#endif
	vertical = lcd_info->yres + lcd_info->vbp + lcd_info->vfp;
	value = ROUND_AID((vertical * panel->command.aid_info[idx].aor) / 1024);

	aid_cmd[0] = 0xB2;
	aid_cmd[1] = (char)((value & 0x0f00) >> 4);
	aid_cmd[2] = (char)(value & 0xff);

	dsim_info("set aid : %x : (%x : %x : %x)\n", value, aid_cmd[0], aid_cmd[1], aid_cmd[2]);

	ret = dsim_write_hl_data(dsim, aid_cmd, 3);
	if (ret < 0)
		dsim_err("ERR:%s:failed to write aid\n", __func__);

//set_aid_exit:
	return ret;
}



static int set_oled_gamma(struct dsim_device *dsim ,int force)
{
	int ret = 0, idx;
	struct gamma_cmd *dim_info;
	struct panel_private *panel = &dsim->priv;
	int size, i;

	idx = panel->cur_br_idx;
	dim_info = &panel->command.gamma_cmd[idx];

	dsim_info("%s\n", __func__);
	size = sizeof(dim_info->gamma);
	dsim_info("%s : size : %d\n", __func__, size);
	for (i=0;i<size;i++)
		printk("%x, ", dim_info->gamma[i]);
	printk("\n");

	ret = dsim_write_hl_data(dsim, dim_info->gamma, ARRAY_SIZE(dim_info->gamma));
	if (ret < 0)
		dsim_err("ERR:%s:failed to write gamma\n", __func__);

	return ret;
}


static int low_level_set_brightness(struct dsim_device *dsim ,int force)
{
	int ret = 0;
	struct panel_info *cmd;
	struct panel_private *panel = &dsim->priv;

	cmd = &panel->command;

	if (cmd == NULL) {
		dsim_err("ERR:%s:panel command is null\n", __func__);
		ret = -EINVAL;
		goto set_br_exit;
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("ERR:%s:faild to write L1 TEST KEY ON\n", __func__);

	ret = set_oled_hbm(dsim, force);
	if (ret) {
		dsim_err("ERR:%s:failed to write gamma\n", __func__);
	}

	ret = set_oled_gamma(dsim, force);
	if (ret) {
		dsim_err("ERR:%s:failed to write gamma\n", __func__);
	}

	ret = set_oled_aid(dsim, force);
	if (ret)
		dsim_err("ERR:%s:failed to write aid\n", __func__);

	ret = set_oled_elvss(dsim, force);
	if (ret)
		dsim_err("ERR:%s:failed to write elvss\n", __func__);

	ret = set_oled_acl(dsim, force);
	if (ret)
		dsim_err("ERR:%s:failed to write acl\n", __func__);

	ret = set_oled_tset(dsim, force);
	if (ret)
		dsim_err("ERR:%s:failed to write acl\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret < 0)
		dsim_err("ERR:%s:faild to write GAMMA UPDATE\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0)
		dsim_err("ERR:%s:faild to write L1 TEST KEY OFF\n", __func__);

set_br_exit:
	return ret;
}



int panel_set_brightness(struct dsim_device *dsim, int force)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	int platform_br = panel->bd->props.brightness;
	int prev_idx, prev_ref_br, prev_phy_br;
	int cur_force = force;
	unsigned int bIsHbm = 0;

	if (panel->op_state == PANEL_STAT_OFF) {
		dsim_err("WARN:%s:panel is not active\n", __func__);
		goto set_br_exit;
	}
#ifndef CONFIG_PANEL_AID_DIMMING
	dsim_info("This panel does not support aid dimming\n");
	goto set_br_exit;
#endif

	prev_idx = panel->cur_br_idx;
	prev_ref_br = panel->cur_ref_br;
	prev_phy_br = panel->cur_phy_br;

	panel->cur_br_idx = panel->mapping_tbl.idx_br_tbl[platform_br];
	panel->cur_ref_br = panel->mapping_tbl.ref_br_tbl[platform_br];
	panel->cur_phy_br = panel->mapping_tbl.phy_br_tbl[platform_br];

	bIsHbm = LEVEL_IS_HBM(platform_br);

	if(bIsHbm)
		panel->cur_br_idx = MAX_DIMMING_INFO_COUNT - 1;

	if(panel->current_hbm != bIsHbm)
		cur_force = 1;

	panel->current_hbm = bIsHbm;

	dsim_info("set brightness %d : platform : %d, br_idx : %d, phy_br : %d, acl : %d\n",
			dsim->id, platform_br, panel->cur_br_idx, panel->cur_phy_br, ACL_IS_ON(panel->cur_phy_br));

	if (cur_force)
		goto set_br;

	if (prev_idx == panel->cur_br_idx) {
		goto set_br_exit;
	}

set_br:
	mutex_lock(&panel->lock);

	ret = low_level_set_brightness(dsim, cur_force);
	if (ret) {
		dsim_err("ERR:%s:failed to set brightness\n", __func__);
	}

	mutex_unlock(&panel->lock);
set_br_exit:
	return ret;
}


static int get_brightness(struct backlight_device *bd)
{
	return bd->props.brightness;
}


static int set_brightness(struct backlight_device *bd)
{
	int ret = 0;
	int brightness = bd->props.brightness;
	struct panel_private *priv = bl_get_data(bd);
	struct dsim_device *dsim;

	dsim = container_of(priv, struct dsim_device, priv);

	if (priv->lcdConnected == PANEL_DISCONNECTED) {
		dsim_info("WARN:%d:%s:Panel disconnected\n", dsim->id, __func__);
		goto exit_set;
	}

	if (brightness < UI_MIN_BRIGHTNESS || brightness > UI_MAX_BRIGHTNESS) {
		printk(KERN_ALERT "Brightness should be in the range of 0 ~ 255\n");
		ret = -EINVAL;
		goto exit_set;
	}

	ret = panel_set_brightness(dsim, SET_BRIGHTNESS_NORMAL);
	if (ret) {
		dsim_err("%s : fail to set brightness\n", __func__);
		goto exit_set;
	}
exit_set:

	return ret;

}

static const struct backlight_ops panel_backlight_ops = {
	.get_brightness = get_brightness,
	.update_status = set_brightness,
};


int probe_backlight_drv(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	const char *backlight_dev_name[2] = {
		"panel",
		"panel_1"
	};

	panel->bd = backlight_device_register(backlight_dev_name[dsim->id], dsim->dev,
		&dsim->priv, &panel_backlight_ops, NULL);
	if (IS_ERR(panel->bd)) {
		dsim_err("%s:failed register backlight\n", __func__);
		ret = PTR_ERR(panel->bd);
	}

	panel->bd->props.max_brightness = UI_MAX_BRIGHTNESS;
	panel->bd->props.brightness = UI_DEFAULT_BRIGHTNESS;

	panel->cur_acl = OLED_ACL_UNDEFINED;

	return ret;
}

