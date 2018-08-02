/*
 * drivers/video/decon/panels/s6e3hf2_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2014 Samsung Electronics
 *
 * Jiun Yu, <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include "../dsim.h"
#include "panel_info.h"
#include "dsim_panel.h"

static const unsigned int s6e3fa3_ref_br_tbl[256] = {
	2, 2, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14, 15,
	16, 17, 19, 20, 21, 22, 24, 25, 25, 27, 29, 30, 32, 34, 37, 39,
	41, 44, 47, 47, 50, 50, 53, 56, 56, 60, 60, 64, 64, 68, 68, 68,
	72, 72, 72, 77, 77, 77, 82, 82, 82, 87, 87, 87, 87, 93, 93, 93,
	98, 98, 98, 98, 105, 105, 105, 105, 111, 111, 111, 111, 119, 119, 119, 119,
	119, 126, 126, 126, 126, 126, 126, 126, 126, 126, 126, 134, 134, 134, 134, 134,
	143, 143, 143, 143, 143, 143, 152, 152, 152, 152, 152, 152, 152, 162, 162, 162,
	162, 162, 162, 172, 172, 172, 172, 172, 172, 172, 172, 183, 183, 183, 183, 183,
	183, 183, 195, 195, 195, 195, 195, 195, 195, 195, 207, 207, 207, 207, 207, 207,
	207, 207, 207, 220, 220, 220, 220, 220, 220, 220, 234, 234, 234, 234, 234, 234,
	234, 234, 234, 249, 249, 249, 249, 249, 249, 249, 249, 249, 265, 265, 265, 265,
	265, 265, 265, 265, 265, 265, 282, 282, 282, 282, 282, 282, 282, 282, 282, 282,
	300, 300, 300, 300, 300, 300, 300, 300, 300, 300, 316, 316, 316, 316, 316, 316,
	316, 316, 316, 316, 316, 316, 333, 333, 333, 333, 350, 350, 350, 350, 350, 350,
	350, 350, 350, 357, 357, 357, 357, 357, 372, 372, 372, 372, 372, 372, 380, 380,
	380, 380, 387, 387, 387, 395, 395, 395, 395, 395, 403, 403, 403, 412, 412, 420,
};

static const unsigned int s6e3fa3_idx_br_tbl[256] = {
	0, 0, 0, 1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14,
	15, 16, 17, 18, 19, 20, 21, 21, 22, 23, 24, 25, 26, 27, 28, 29,
	30, 31, 31, 32, 32, 33, 34, 34, 35, 35, 36, 36, 37, 37, 37, 38,
	38, 38, 39, 39, 39, 40, 40, 40, 41, 41, 41, 41, 42, 42, 42, 43,
	43, 43, 43, 44, 44, 44, 44, 45, 45, 45, 45, 46, 46, 46, 46, 46,
	47, 47, 47, 47, 47, 47, 47, 47, 47, 47, 48, 48, 48, 48, 48, 49,
	49, 49, 49, 49, 49, 50, 50, 50, 50, 50, 50, 50, 51, 51, 51, 51,
	51, 51, 52, 52, 52, 52, 52, 52, 52, 52, 53, 53, 53, 53, 53, 53,
	53, 54, 54, 54, 54, 54, 54, 54, 54, 55, 55, 55, 55, 55, 55, 55,
	55, 55, 56, 56, 56, 56, 56, 56, 56, 57, 57, 57, 57, 57, 57, 57,
	57, 57, 58, 58, 58, 58, 58, 58, 58, 58, 58, 59, 59, 59, 59, 59,
	59, 59, 59, 59, 59, 60, 60, 60, 60, 60, 60, 60, 60, 60, 60, 61,
	61, 61, 61, 61, 61, 61, 61, 61, 61, 62, 62, 62, 62, 62, 62, 62,
	62, 62, 62, 62, 62, 63, 63, 63, 63, 64, 64, 64, 64, 64, 64, 64,
	64, 64, 65, 65, 65, 65, 65, 67, 67, 67, 67, 67, 67, 68, 68, 68,
	68, 69, 69, 69, 70, 70, 70, 70, 70, 71, 71, 71, 72, 72, 73,
};

static int s6e3fa3_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret = 0;
	unsigned char tmtp[S6E3FA3_MTP_DATE_SIZE] = {0, };

	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3FA3_COORDINATE_LEN] = {0,};

	dsim_info("%s:id-%d:was called\n", __func__, dsim->id);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_FC\n", __func__, dsim->id);
	}

	ret = dsim_read_hl_data(dsim, S6E3FA3_ID_REG, S6E3FA3_ID_LEN, dsim->priv.id);
	if (ret != S6E3FA3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3FA3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");


	ret = dsim_read_hl_data(dsim, S6E3FA3_MTP_ADDR, S6E3FA3_MTP_DATE_SIZE, tmtp);
	if (ret != S6E3FA3_MTP_DATE_SIZE) {
		dsim_err("ERR:%s:failed to read mtp value : %d\n", __func__, ret);
		goto read_fail;
	}

	memcpy(mtp, tmtp, S6E3FA3_MTP_SIZE);
	memcpy(dsim->priv.date, &tmtp[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3FA3_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3FA3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);
	ret = dsim_read_hl_data(dsim, S6E3FA3_COORDINATE_REG, S6E3FA3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3FA3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_FC\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}

static int s6e3fa3_fhd_dump(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("MDD : %s was called\n", __func__);

	return ret;
}

extern int init_smart_dimming(struct panel_info *panel, char *refgamma, char *mtp);

static int s6e3fa3_fhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3FA3_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3FA3_HBMGAMMA_LEN] = {0, };
#ifdef CONFIG_PANEL_AID_DIMMING
	unsigned char refgamma[S6E3FA3_MTP_SIZE] = {
		0x01, 0x00, 0x01, 0x00, 0x01, 0x00,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x80, 0x80, 0x80,
		0x00, 0x00, 0x00,
		0x00, 0x00
	};
#endif
	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = s6e3fa3_read_init_info(dsim, mtp, hbm);
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_PANEL_AID_DIMMING
	panel->mapping_tbl.idx_br_tbl = s6e3fa3_idx_br_tbl;
	panel->mapping_tbl.ref_br_tbl = s6e3fa3_ref_br_tbl;
	panel->mapping_tbl.phy_br_tbl = s6e3fa3_ref_br_tbl;

	//panel->aid_info = (struct aid_dimming_info *)aid_info;

	init_smart_dimming(&panel->command, refgamma, mtp);

#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	return ret;

}


static int s6e3fa3_fhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_ON, ARRAY_SIZE(SEQ_DISPLAY_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_ON\n", __func__, dsim->id);
 		goto displayon_err;
	}

displayon_err:
	return ret;

}


static int s6e3fa3_fhd_exit(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, SEQ_DISPLAY_OFF, ARRAY_SIZE(SEQ_DISPLAY_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_OFF\n", __func__, dsim->id);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_IN, ARRAY_SIZE(SEQ_SLEEP_IN));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_IN\n", __func__, dsim->id);
		goto exit_err;
	}

	msleep(120);

exit_err:

	return ret;
}


static int s6e3fa3_fhd_full_init(struct dsim_device *dsim)
{
	int ret = 0;

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_STAND_ALONE_MODE, ARRAY_SIZE(SEQ_STAND_ALONE_MODE));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_full_init;
	}

	/* 1. Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, SEQ_SLEEP_OUT, ARRAY_SIZE(SEQ_SLEEP_OUT));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_OUT\n", __func__, dsim->id);
		goto err_full_init;
	}
	msleep(20);

	ret = dsim_write_hl_data(dsim, SEQ_TE_MODE, ARRAY_SIZE(SEQ_TE_MODE));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
		goto err_full_init;
	}
	/* Common Setting */
	ret = dsim_write_hl_data(dsim, SEQ_TE_ON, ARRAY_SIZE(SEQ_TE_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TE_ON\n", __func__, dsim->id);
		goto err_full_init;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(SEQ_GAMMA_CONDITION_SET));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_GAMMA_CONDITION_SET\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DEFAULT_AID_SETTING, ARRAY_SIZE(SEQ_DEFAULT_AID_SETTING));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DEFAULT_AID_SETTING\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_DEFAULT_ELVSS_SET, ARRAY_SIZE(SEQ_DEFAULT_ELVSS_SET));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DEFAULT_ELVSS_SET\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_GAMMA_UPDATE, ARRAY_SIZE(SEQ_GAMMA_UPDATE));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_GAMMA_UPDATE\n", __func__, dsim->id);
		goto err_full_init;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, SEQ_OPR_ACL_OFF, ARRAY_SIZE(SEQ_OPR_ACL_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_OPR_ACL_OFF\n", __func__, dsim->id);
		goto err_full_init;
	}

	ret = dsim_write_hl_data(dsim, SEQ_ACL_OFF, ARRAY_SIZE(SEQ_ACL_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_ACL_OFF\n", __func__, dsim->id);
		goto err_full_init;
	}
#endif

	msleep(120);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto err_full_init;
	}

	return ret;

err_full_init :
	dsim_err("ERR:PANEL:%s:id-%d:failed to full init\n", __func__, dsim->id);
	return ret;

}


static int s6e3fa3_fhd_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel:id-%d:%s was called\n", dsim->id, __func__);

	ret = s6e3fa3_fhd_full_init(dsim);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to full init\n", __func__, dsim->id);
		goto err_init;
	}

	return ret;

err_init:
	dsim_err("%s : failed to init\n", __func__);
	return ret;
}


struct dsim_panel_ops s6e3fa3_panel_ops = {
	.name = "s6e3fa3_fhd",
	.early_probe = NULL,
	.probe		= s6e3fa3_fhd_probe,
	.displayon	= s6e3fa3_fhd_displayon,
	.exit		= s6e3fa3_fhd_exit,
	.init		= s6e3fa3_fhd_init,
	.dump 		= s6e3fa3_fhd_dump,
};

#if 0
struct dsim_panel_ops *dsim_panel_get_priv_ops(struct dsim_device *dsim)
{
	return &s6e3fa3_panel_ops;
}
#endif

static int __init s6e3fa3_register_panel(void)
{
	int ret = 0; 

	ret = dsim_register_panel(&s6e3fa3_panel_ops);
	if (ret) {
		dsim_err("ERR:%s:failed to register panel\n", __func__);
	}
	return 0;
}
arch_initcall(s6e3fa3_register_panel);

static int __init s6e3fa3_get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);

	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", s6e3fa3_get_lcd_type);
