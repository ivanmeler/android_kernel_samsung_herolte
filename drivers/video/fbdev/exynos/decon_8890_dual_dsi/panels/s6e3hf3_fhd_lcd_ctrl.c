/*
 * drivers/video/decon/panels/s6e3hf3_lcd_ctrl.c
 *
 * Samsung SoC MIPI LCD CONTROL functions
 *
 * Copyright (c) 2016 Samsung Electronics
 *
 * Jiun Yu, <minwoo7945.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <video/mipi_display.h>
#include "../dsim.h"
#include "panel_info.h"
#include "dsim_panel.h"

static unsigned int hw_rev = 0; // for ddi

// hf4

static int s6e3hf4_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret = 0;
	unsigned char tmtp[S6E3HF4_MTP_DATE_SIZE] = {0, };

	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3HF4_COORDINATE_LEN] = {0,};

	dsim_info("%s:id-%d:was called\n", __func__, dsim->id);

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_FC\n", __func__, dsim->id);
	}

	ret = dsim_read_hl_data(dsim, S6E3HF4_ID_REG, S6E3HF4_ID_LEN, dsim->priv.id);
	if (ret != S6E3HF4_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HF4_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");


	ret = dsim_read_hl_data(dsim, S6E3HF4_MTP_ADDR, S6E3HF4_MTP_DATE_SIZE, tmtp);
	if (ret != S6E3HF4_MTP_DATE_SIZE) {
		dsim_err("ERR:%s:failed to read mtp value : %d\n", __func__, ret);
		goto read_fail;
	}

	memcpy(mtp, tmtp, S6E3HF4_MTP_SIZE);
	memcpy(dsim->priv.date, &tmtp[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3HF4_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HF4_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);
	ret = dsim_read_hl_data(dsim,S6E3HF4_COORDINATE_REG, S6E3HF4_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HF4_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_FC\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}


static const unsigned char S6E3HF4_FHD_SEQ_AOR_CONTROL[] = {
	0xB1,
	0x64, 0x60
};

static const unsigned char S6E3HF4_FHD_SEQ_TSET_ELVSS_SET[] = {
	0xB5,
	0x19,	/* temperature 25 */
	0xBC,	/* MPS_CON: ACL OFF */
	0x0A	/* ELVSS: MAX*/
};



static int s6e3hf4_fhd_early_init(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int ret = 0;
	unsigned int res;

	res = dsim->lcd_info.xres * dsim->lcd_info.yres;

	dsim_info("MDD : %d :  %s was called\n", dsim->id, __func__);
	dsim_info("xres : %d, yres : %d\n", dsim->lcd_info.xres, dsim->lcd_info.yres);

	/* DSC setting */
	msleep(5);

	ret = dsim_write_data(dsim, MIPI_DSI_DSC_PRA, S6E3HF4_SEQ_DSC_EN[0], S6E3HF4_SEQ_DSC_EN[1]);
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DSC_EN\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_data(dsim, MIPI_DSI_DSC_PPS,
						  (unsigned long)dsim->lcd_info.dsc_pps_tbl,
						  sizeof(dsim->lcd_info.dsc_pps_tbl));

	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PPS_SLICE4\n", __func__);
		goto init_exit;
	}

	/* Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SLEEP_OUT, ARRAY_SIZE(S6E3HF4_SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(120);

	ret = s6e3hf4_read_init_info(dsim, mtp, hbm);
	if (ret) {
		dsim_err("ERR:%s:failed to read info\n", __func__);
		goto init_exit;
	}

	/* Interface Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
#if 0
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
#endif
	if (res == 1080 * 1920) {
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SET_FHD_COL, ARRAY_SIZE(S6E3HF4_SEQ_SET_FHD_COL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SET_FHD_PAG, ARRAY_SIZE(S6E3HF4_SEQ_SET_FHD_PAG));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SCALER_WQHD_FHD, ARRAY_SIZE(S6E3HF4_SEQ_SCALER_WQHD_FHD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
	}


	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF4_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HF4_SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TE_START_SETTING, ARRAY_SIZE(S6E3HF4_SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_FFC_SET, ARRAY_SIZE(S6E3HF4_SEQ_FFC_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_FFC_SET\n", __func__);
		goto init_exit;
	}


#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_FHD_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HF4_FHD_SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HF4_FHD_SEQ_TSET_ELVSS_SET, ARRAY_SIZE(S6E3HF4_FHD_SEQ_TSET_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF4_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_UPDATE, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_UPDATE_L));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF4_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(S6E3HF4_SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif

#if 0
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}
#endif
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

	/* Common Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TE_ON, ARRAY_SIZE(S6E3HF4_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}


init_exit:
	return ret;
}

static int s6e3hf4_fhd_dump(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned char rx_buf[DSIM_DDI_ID_LEN + 1];

	dsim_info(" + %s\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_ON_F0\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_ON_FC\n", __func__);

	ret = dsim_read_hl_data(dsim, MIPI_DCS_GET_POWER_MODE, DSIM_DDI_ID_LEN, rx_buf);
	if (ret != DSIM_DDI_ID_LEN) {
		dsim_err("%s : can't read POWER_MODE Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's POWER_MODE Reg Value : %x ===\n", rx_buf[0]);

	if (rx_buf[0] & 0x80)
		dsim_info("* Booster Voltage Status : ON\n");
	else
		dsim_info("* Booster Voltage Status : OFF\n");

	if (rx_buf[0] & 0x40)
		dsim_info("* Idle Mode : On\n");
	else
		dsim_info("* Idle Mode : OFF\n");

	if (rx_buf[0] & 0x20)
		dsim_info("* Partial Mode : On\n");
	else
		dsim_info("* Partial Mode : OFF\n");

	if (rx_buf[0] & 0x10)
		dsim_info("* Sleep OUT and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rx_buf[0] & 0x08)
		dsim_info("* Normal Mode On and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rx_buf[0] & 0x04)
		dsim_info("* Display On and Working Ok\n");
	else
		dsim_info("* Display Off\n");

	ret = dsim_read_hl_data(dsim, MIPI_DCS_GET_SIGNAL_MODE, DSIM_DDI_ID_LEN, rx_buf);
	if (ret != DSIM_DDI_ID_LEN) {
		dsim_err("%s : can't read SIGNAL_MODE Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's SIGNAL_MODE Reg Value : %x ===\n", rx_buf[0]);

	if (rx_buf[0] & 0x80)
		dsim_info("* TE On\n");
	else
		dsim_info("* TE OFF\n");

	if (rx_buf[0] & 0x40)
		dsim_info("* TE MODE on\n");

	if (rx_buf[0] & 0x01) {
		/* get a value of protocol violation error */
		ret = dsim_read_hl_data(dsim, 0xEA, DSIM_DDI_ID_LEN, rx_buf);
		if (ret != DSIM_DDI_ID_LEN) {
			dsim_err("%s : can't read Panel's Protocol\n",__func__);
			goto dump_exit;
		}

		dsim_err("* Protocol violation: buf[0] = %x\n", rx_buf[0]);
		dsim_err("* Protocol violation: buf[1] = %x\n", rx_buf[1]);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_OFF_FC\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_OFF_F0\n", __func__);
dump_exit:
	dsim_info(" - %s\n", __func__);
	return ret;

}

extern int init_smart_dimming(struct panel_info *panel, char *refgamma, char *mtp);

static int s6e3hf4_fhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HF4_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3HF4_HBMGAMMA_LEN] = {0, };
#if 0
	unsigned char buf[5]= {0, };
	int t_ret;
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
	unsigned char refgamma[S6E3HF4_MTP_SIZE] = {
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

	if (panel->state == PANEL_STATE_SUSPENED) {
		ret = s6e3hf4_fhd_early_init(dsim, mtp, hbm);
		panel->state = PANEL_STATE_RESUMED;
	} else {
		s6e3hf4_read_init_info(dsim, mtp, hbm);
	}
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#if 0
	t_ret = dsim_read_hl_data(dsim, 0x0a, 5, buf);
	if (t_ret != 5) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
	}
	dsim_info("reg : %x %x %x %x %x\n", buf[0], buf[1], buf[2], buf[3], buf[4]);
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
	//panel->aid_info = (struct aid_dimming_info *)aid_info;

	init_smart_dimming(&panel->command, refgamma, mtp);

#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 1;
#endif

probe_exit:
	return ret;

}


static int s6e3hf4_fhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_DISPLAY_ON, ARRAY_SIZE(S6E3HF4_SEQ_DISPLAY_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_ON\n", __func__, dsim->id);
 		goto displayon_err;
	}

displayon_err:
	return ret;

}


static int s6e3hf4_fhd_exit(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_DISPLAY_OFF, ARRAY_SIZE(S6E3HF4_SEQ_DISPLAY_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_OFF\n", __func__, dsim->id);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SLEEP_IN, ARRAY_SIZE(S6E3HF4_SEQ_SLEEP_IN));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_IN\n", __func__, dsim->id);
		goto exit_err;
	}

	msleep(120);

exit_err:

	return ret;
}


int init_cnt = 0;


static int s6e3hf4_fhd_full_init(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned int res;

	res = dsim->lcd_info.xres * dsim->lcd_info.yres;

	dsim_info("MDD : %d :  %s was called\n", dsim->id, __func__);
	dsim_info("xres : %d, yres : %d\n", dsim->lcd_info.xres, dsim->lcd_info.yres);

	/* DSC setting */
	msleep(5);

	ret = dsim_write_data(dsim, MIPI_DSI_DSC_PRA, S6E3HF4_SEQ_DSC_EN[0], S6E3HF4_SEQ_DSC_EN[1]);
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_DSC_EN\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_data(dsim, MIPI_DSI_DSC_PPS,
						  (unsigned long)dsim->lcd_info.dsc_pps_tbl,
						  sizeof(dsim->lcd_info.dsc_pps_tbl));

	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_PPS_SLICE4\n", __func__);
		goto init_exit;
	}

	/* Sleep Out(11h) */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SLEEP_OUT, ARRAY_SIZE(S6E3HF4_SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(120);


	/* Interface Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
#if 0
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
#endif
	 if (res == 1080 * 1920) {
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SET_FHD_COL, ARRAY_SIZE(S6E3HF4_SEQ_SET_FHD_COL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SET_FHD_PAG, ARRAY_SIZE(S6E3HF4_SEQ_SET_FHD_PAG));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_SCALER_WQHD_FHD, ARRAY_SIZE(S6E3HF4_SEQ_SCALER_WQHD_FHD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF4_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HF4_SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TE_START_SETTING, ARRAY_SIZE(S6E3HF4_SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_FFC_SET, ARRAY_SIZE(S6E3HF4_SEQ_FFC_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_FFC_SET\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_FHD_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HF4_SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}
	/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HF4_FHD_SEQ_TSET_ELVSS_SET, ARRAY_SIZE(S6E3HF4_SEQ_TSET_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF4_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_UPDATE, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

		ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(S6E3HF4_SEQ_GAMMA_UPDATE_L));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}


	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF4_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(S6E3HF4_SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif

#if 0
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_FC\n", __func__);
		goto init_exit;
	}
#endif

	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF4_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}
#if 0
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_DISPLAY_ON, ARRAY_SIZE(S6E3HF4_SEQ_DISPLAY_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_ON\n", __func__, dsim->id);
 		goto init_exit;
	}
#endif
	/* Common Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF4_SEQ_TE_ON, ARRAY_SIZE(S6E3HF4_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}


	dsim_info("MDD : %d :  %s was done\n", dsim->id, __func__);

init_exit:
	return 0;
}


static int s6e3hf4_fhd_init(struct dsim_device *dsim)
{
	int ret = 0;
	//unsigned char mtp[S6E3HF4_MTP_SIZE] = {0, };
	//unsigned char hbm[S6E3HF4_HBMGAMMA_LEN] = {0, };

	dsim_info("DSIM Panel:id-%d:%s was called\n", dsim->id, __func__);

	ret = s6e3hf4_fhd_full_init(dsim);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to full init\n", __func__, dsim->id);
		goto err_init;
	}

	return ret;

err_init:
	dsim_err("%s : failed to init\n", __func__);
	return ret;
}


struct dsim_panel_ops s6e3hf4_panel_ops = {
	.name = "s6e3hf4_fhd",
	.early_probe = NULL,
	.probe		= s6e3hf4_fhd_probe,
	.displayon	= s6e3hf4_fhd_displayon,
	.exit		= s6e3hf4_fhd_exit,
	.init		= s6e3hf4_fhd_init,
	.dump 		= s6e3hf4_fhd_dump,
};



// hf3
static int s6e3hf3_read_init_info(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int i = 0;
	int ret = 0;
	unsigned char tmtp[S6E3HF3_MTP_DATE_SIZE] = {0, };

	struct panel_private *panel = &dsim->priv;
	unsigned char bufForCoordi[S6E3HF3_COORDINATE_LEN] = {0,};

	dsim_info("%s:id-%d:was called\n", __func__, dsim->id);

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_F0\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_ON_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_ON_FC\n", __func__, dsim->id);
	}

	ret = dsim_read_hl_data(dsim, S6E3HF3_ID_REG, S6E3HF3_ID_LEN, dsim->priv.id);
	if (ret != S6E3HF3_ID_LEN) {
		dsim_err("%s : can't find connected panel. check panel connection\n",__func__);
		panel->lcdConnected = PANEL_DISCONNECTED;
		goto read_exit;
	}

	dsim_info("READ ID : ");
	for (i = 0; i < S6E3HF3_ID_LEN; i++)
		dsim_info("%02x, ", dsim->priv.id[i]);
	dsim_info("\n");

	if(panel->id[1] == 1) {
		panel->panel_pos = PANEL_STAT_SLAVE;
		dsim_info("%s I'm slave type\n", __func__);
	} else {
		panel->panel_pos = PANEL_STAT_MASTER;
		dsim_info("%s I'm master type\n", __func__);
	}
	ret = dsim_read_hl_data(dsim, S6E3HF3_MTP_ADDR, S6E3HF3_MTP_DATE_SIZE, tmtp);
	if (ret != S6E3HF3_MTP_DATE_SIZE) {
		dsim_err("ERR:%s:failed to read mtp value : %d\n", __func__, ret);
		goto read_fail;
	}

	memcpy(mtp, tmtp, S6E3HF3_MTP_SIZE);
	memcpy(dsim->priv.date, &tmtp[40], ARRAY_SIZE(dsim->priv.date));
	dsim_info("READ MTP SIZE : %d\n", S6E3HF3_MTP_SIZE);
	dsim_info("=========== MTP INFO =========== \n");
	for (i = 0; i < S6E3HF3_MTP_SIZE; i++)
		dsim_info("MTP[%2d] : %2d : %2x\n", i, mtp[i], mtp[i]);
	ret = dsim_read_hl_data(dsim,S6E3HF3_COORDINATE_REG, S6E3HF3_COORDINATE_LEN, bufForCoordi);
	if (ret != S6E3HF3_COORDINATE_LEN) {
		dsim_err("fail to read coordinate on command.\n");
		goto read_fail;
	}
	dsim->priv.coordinate[0] = bufForCoordi[0] << 8 | bufForCoordi[1];	/* X */
	dsim->priv.coordinate[1] = bufForCoordi[2] << 8 | bufForCoordi[3];	/* Y */
	dsim_info("READ coordi : ");
	for(i = 0; i < 2; i++)
		dsim_info("%d, ", dsim->priv.coordinate[i]);
	dsim_info("\n");

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_OFF_FC));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_FC\n", __func__, dsim->id);
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_TEST_KEY_OFF_F0\n", __func__, dsim->id);
		goto read_fail;
	}
read_exit:
	return 0;

read_fail:
	return -ENODEV;

}


static const unsigned char S6E3HF3_FHD_SEQ_AOR_CONTROL[] = {
	0xB1,
	0x64, 0x60
};

static const unsigned char S6E3HF3_FHD_SEQ_TSET_ELVSS_SET[] = {
	0xB5,
	0x19,	/* temperature 25 */
	0xBC,	/* MPS_CON: ACL OFF */
	0x0A	/* ELVSS: MAX*/
};



static int s6e3hf3_fhd_early_init(struct dsim_device *dsim, unsigned char* mtp, unsigned char* hbm)
{
	int ret = 0;
	unsigned int res;

	res = dsim->lcd_info.xres * dsim->lcd_info.yres;

	dsim_info("MDD : %d :  %s was called\n", dsim->id, __func__);
	dsim_info("xres : %d, yres : %d\n", dsim->lcd_info.xres, dsim->lcd_info.yres);

	msleep(5);

	/* Sleep Out */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_STAND_ALONE_MODE, ARRAY_SIZE(S6E3HF3_SEQ_STAND_ALONE_MODE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_STAND_ALONE_MODE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SLEEP_OUT, ARRAY_SIZE(S6E3HF3_SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* interface setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MIC_SET, ARRAY_SIZE(S6E3HF3_SEQ_MIC_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC_SET\n", __func__);
		goto init_exit;
	}

	if (res == 1080 * 1920) {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SET_FHD_COL, ARRAY_SIZE(S6E3HF3_SEQ_SET_FHD_COL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SET_FHD_PAG, ARRAY_SIZE(S6E3HF3_SEQ_SET_FHD_PAG));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SCALER_WQHD_FHD, ARRAY_SIZE(S6E3HF3_SEQ_SCALER_WQHD_FHD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
	}

	msleep(120);

	/* read information */
	ret = s6e3hf3_read_init_info(dsim, mtp, hbm);
	if (ret) {
		dsim_err("ERR:%s:failed to read info\n", __func__);
		goto init_exit;
	}

	/* common setting */
	if(dsim->priv.panel_pos == PANEL_STAT_SLAVE) {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SLAVE_MODE, ARRAY_SIZE(S6E3HF3_SEQ_SLAVE_MODE));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_SLAVE_MODE\n", __func__);
			goto init_exit;
		}
	} else {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MASTER_MODE, ARRAY_SIZE(S6E3HF3_SEQ_MASTER_MODE));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MASTER_MODE\n", __func__);
			goto init_exit;
		}
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_ON, ARRAY_SIZE(S6E3HF3_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF3_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_START_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_PENTILE_CONTROL_SETTING, ARRAY_SIZE(S6E3HF3_PENTILE_CONTROL_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_PENTILE_CONTROL_SETTING\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
	/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_FHD_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HF3_FHD_SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}

	/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HF3_FHD_SEQ_TSET_ELVSS_SET, ARRAY_SIZE(S6E3HF3_FHD_SEQ_TSET_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF3_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_UPDATE, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_UPDATE_L));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;
}

static int s6e3hf3_fhd_dump(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned char rx_buf[DSIM_DDI_ID_LEN + 1];

	dsim_info(" + %s\n", __func__);
	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(SEQ_TEST_KEY_ON_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_ON_F0\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_ON_FC, ARRAY_SIZE(SEQ_TEST_KEY_ON_FC));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_ON_FC\n", __func__);

	ret = dsim_read_hl_data(dsim, MIPI_DCS_GET_POWER_MODE, DSIM_DDI_ID_LEN, rx_buf);
	if (ret != DSIM_DDI_ID_LEN) {
		dsim_err("%s : can't read POWER_MODE Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's POWER_MODE Reg Value : %x ===\n", rx_buf[0]);

	if (rx_buf[0] & 0x80)
		dsim_info("* Booster Voltage Status : ON\n");
	else
		dsim_info("* Booster Voltage Status : OFF\n");

	if (rx_buf[0] & 0x40)
		dsim_info("* Idle Mode : On\n");
	else
		dsim_info("* Idle Mode : OFF\n");

	if (rx_buf[0] & 0x20)
		dsim_info("* Partial Mode : On\n");
	else
		dsim_info("* Partial Mode : OFF\n");

	if (rx_buf[0] & 0x10)
		dsim_info("* Sleep OUT and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rx_buf[0] & 0x08)
		dsim_info("* Normal Mode On and Working Ok\n");
	else
		dsim_info("* Sleep IN\n");

	if (rx_buf[0] & 0x04)
		dsim_info("* Display On and Working Ok\n");
	else
		dsim_info("* Display Off\n");

	ret = dsim_read_hl_data(dsim, MIPI_DCS_GET_SIGNAL_MODE, DSIM_DDI_ID_LEN, rx_buf);
	if (ret != DSIM_DDI_ID_LEN) {
		dsim_err("%s : can't read SIGNAL_MODE Reg\n",__func__);
		goto dump_exit;
	}

	dsim_info("=== Panel's SIGNAL_MODE Reg Value : %x ===\n", rx_buf[0]);

	if (rx_buf[0] & 0x80)
		dsim_info("* TE On\n");
	else
		dsim_info("* TE OFF\n");

	if (rx_buf[0] & 0x40)
		dsim_info("* TE MODE on\n");

	if (rx_buf[0] & 0x01) {
		/* get a value of protocol violation error */
		ret = dsim_read_hl_data(dsim, 0xEA, DSIM_DDI_ID_LEN, rx_buf);
		if (ret != DSIM_DDI_ID_LEN) {
			dsim_err("%s : can't read Panel's Protocol\n",__func__);
			goto dump_exit;
		}

		dsim_err("* Protocol violation: buf[0] = %x\n", rx_buf[0]);
		dsim_err("* Protocol violation: buf[1] = %x\n", rx_buf[1]);
	}

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_FC, ARRAY_SIZE(SEQ_TEST_KEY_OFF_FC));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_OFF_FC\n", __func__);

	ret = dsim_write_hl_data(dsim, SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(SEQ_TEST_KEY_OFF_F0));
	if (ret < 0)
		dsim_err("%s : fail to write CMD : KEY_OFF_F0\n", __func__);
dump_exit:
	dsim_info(" - %s\n", __func__);
	return ret;

}

extern int init_smart_dimming(struct panel_info *panel, char *refgamma, char *mtp);

static int s6e3hf3_fhd_probe(struct dsim_device *dsim)
{
	int ret = 0;
	struct panel_private *panel = &dsim->priv;
	unsigned char mtp[S6E3HF3_MTP_SIZE] = {0, };
	unsigned char hbm[S6E3HF3_HBMGAMMA_LEN] = {0, };
#if 0
	unsigned char buf[5]= {0, };
	int t_ret;
#endif
#ifdef CONFIG_PANEL_AID_DIMMING
	unsigned char refgamma[S6E3HF3_MTP_SIZE] = {
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

	if (panel->state == PANEL_STATE_SUSPENED) {
		ret = s6e3hf3_fhd_early_init(dsim, mtp, hbm);
		panel->state = PANEL_STATE_RESUMED;
	} else {
		s6e3hf3_read_init_info(dsim, mtp, hbm);
	}
	if (panel->lcdConnected == PANEL_DISCONNECTED) {
		dsim_err("dsim : %s lcd was not connected\n", __func__);
		goto probe_exit;
	}
#ifdef CONFIG_PANEL_AID_DIMMING
	//panel->aid_info = (struct aid_dimming_info *)aid_info;

	init_smart_dimming(&panel->command, refgamma, mtp);

#endif
#ifdef CONFIG_EXYNOS_DECON_MDNIE_LITE
	panel->mdnie_support = 0;
#endif

probe_exit:
	return ret;

}


static int s6e3hf3_fhd_displayon(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DISPLAY_ON, ARRAY_SIZE(S6E3HF3_SEQ_DISPLAY_ON));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_ON\n", __func__, dsim->id);
 		goto displayon_err;
	}

displayon_err:
	return ret;

}


static int s6e3hf3_fhd_exit(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel : %d : %s was called\n", dsim->id, __func__);

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_DISPLAY_OFF, ARRAY_SIZE(S6E3HF3_SEQ_DISPLAY_OFF));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_DISPLAY_OFF\n", __func__, dsim->id);
		goto exit_err;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SLEEP_IN, ARRAY_SIZE(S6E3HF3_SEQ_SLEEP_IN));
	if (ret != 0) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to write SEQ_SLEEP_IN\n", __func__, dsim->id);
		goto exit_err;
	}

	msleep(120);

exit_err:

	return ret;
}


static int s6e3hf3_fhd_full_init(struct dsim_device *dsim)
{
	int ret = 0;
	unsigned int res;

	res = dsim->lcd_info.xres * dsim->lcd_info.yres;

	dsim_info("MDD : %d :  %s was called\n", dsim->id, __func__);
	dsim_info("xres : %d, yres : %d\n", dsim->lcd_info.xres, dsim->lcd_info.yres);

	msleep(5);


	/* Sleep Out */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_ON_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_ON_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_ON_F0\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_STAND_ALONE_MODE, ARRAY_SIZE(S6E3HF3_SEQ_STAND_ALONE_MODE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_STAND_ALONE_MODE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SLEEP_OUT, ARRAY_SIZE(S6E3HF3_SEQ_SLEEP_OUT));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
		goto init_exit;
	}
	msleep(5);

	/* interface setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MIC_SET, ARRAY_SIZE(S6E3HF3_SEQ_MIC_SET));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MIC_SET\n", __func__);
		goto init_exit;
	}

	if (res == 1080 * 1920) {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SET_FHD_COL, ARRAY_SIZE(S6E3HF3_SEQ_SET_FHD_COL));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SET_FHD_PAG, ARRAY_SIZE(S6E3HF3_SEQ_SET_FHD_PAG));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SCALER_WQHD_FHD, ARRAY_SIZE(S6E3HF3_SEQ_SCALER_WQHD_FHD));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : SEQ_SLEEP_OUT\n", __func__);
			goto init_exit;
		}
	}

	msleep(120);


	/* common setting */
	if(dsim->priv.panel_pos == PANEL_STAT_SLAVE) {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_SLAVE_MODE, ARRAY_SIZE(S6E3HF3_SEQ_SLAVE_MODE));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_SLAVE_MODE\n", __func__);
			goto init_exit;
		}
	} else {
		ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_MASTER_MODE, ARRAY_SIZE(S6E3HF3_SEQ_MASTER_MODE));
		if (ret < 0) {
			dsim_err("%s : fail to write CMD : S6E3HF3_SEQ_MASTER_MODE\n", __func__);
			goto init_exit;
		}
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_ON, ARRAY_SIZE(S6E3HF3_SEQ_TE_ON));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_ON\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TSP_TE, ARRAY_SIZE(S6E3HF3_SEQ_TSP_TE));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TSP_TE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ERR_FG_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_ERR_FG_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_ERR_FG_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TE_START_SETTING, ARRAY_SIZE(S6E3HF3_SEQ_TE_START_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TE_START_SETTING\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_PENTILE_CONTROL_SETTING, ARRAY_SIZE(S6E3HF3_PENTILE_CONTROL_SETTING));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : S6E3HF3_PENTILE_CONTROL_SETTING\n", __func__);
		goto init_exit;
	}

#ifndef CONFIG_PANEL_AID_DIMMING
		/* Brightness Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_CONDITION_SET, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_CONDITION_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_CONDITION_SET\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_FHD_SEQ_AOR_CONTROL, ARRAY_SIZE(S6E3HF3_FHD_SEQ_AOR_CONTROL));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_AOR_CONTROL\n", __func__);
		goto init_exit;
	}

		/* elvss */
	ret = dsim_write_hl_data(dsim, S6E3HF3_FHD_SEQ_TSET_ELVSS_SET, ARRAY_SIZE(S6E3HF3_FHD_SEQ_TSET_ELVSS_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_TSET_ELVSS_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_VINT_SET, ARRAY_SIZE(S6E3HF3_SEQ_VINT_SET));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_VINT_SET\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_UPDATE, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_UPDATE));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_GAMMA_UPDATE_L, ARRAY_SIZE(S6E3HF3_SEQ_GAMMA_UPDATE_L));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_GAMMA_UPDATE\n", __func__);
		goto init_exit;
	}

	/* ACL Setting */
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF\n", __func__);
		goto init_exit;
	}
	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_ACL_OFF_OPR_AVR, ARRAY_SIZE(S6E3HF3_SEQ_ACL_OFF_OPR_AVR));
	if (ret < 0) {
		dsim_err(":%s fail to write CMD : SEQ_ACL_OFF_OPR\n", __func__);
		goto init_exit;
	}
#endif

	ret = dsim_write_hl_data(dsim, S6E3HF3_SEQ_TEST_KEY_OFF_F0, ARRAY_SIZE(S6E3HF3_SEQ_TEST_KEY_OFF_F0));
	if (ret < 0) {
		dsim_err("%s : fail to write CMD : SEQ_TEST_KEY_OFF_F0\n", __func__);
		goto init_exit;
	}

init_exit:
	return ret;

}


static int s6e3hf3_fhd_init(struct dsim_device *dsim)
{
	int ret = 0;

	dsim_info("DSIM Panel:id-%d:%s was called\n", dsim->id, __func__);

	ret = s6e3hf3_fhd_full_init(dsim);
	if (ret) {
		dsim_err("ERR:PANEL:%s:id-%d:fail to full init\n", __func__, dsim->id);
		goto err_init;
	}

	return ret;

err_init:
	dsim_err("%s : failed to init\n", __func__);
	return ret;
}


struct dsim_panel_ops s6e3hf3_panel_ops = {
	.name = "s6e3hf3_fhd",
	.early_probe = NULL,
	.probe		= s6e3hf3_fhd_probe,
	.displayon	= s6e3hf3_fhd_displayon,
	.exit		= s6e3hf3_fhd_exit,
	.init		= s6e3hf3_fhd_init,
	.dump 		= s6e3hf3_fhd_dump,
};





static int __init register_panel(void)
{
	int ret = 0;

	if(hw_rev == 0) {
		ret = dsim_register_panel(&s6e3hf4_panel_ops);
		if (ret) {
			dsim_err("ERR:%s:failed to register panel\n", __func__);
		}
	} else {
		ret = dsim_register_panel(&s6e3hf3_panel_ops);
		if (ret) {
			dsim_err("ERR:%s:failed to register panel\n", __func__);
		}
	}
	return 0;
}
arch_initcall(register_panel);

static int __init get_lcd_type(char *arg)
{
	unsigned int lcdtype;

	get_option(&arg, &lcdtype);


	dsim_info("--- Parse LCD TYPE ---\n");
	dsim_info("LCDTYPE : %x\n", lcdtype);

	return 0;
}
early_param("lcdtype", get_lcd_type);


static int __init get_hw_rev(char *arg)
{
	get_option(&arg, &hw_rev);
	dsim_info("hw_rev : %d\n", hw_rev);

	return 0;
}

early_param("androidboot.hw_rev", get_hw_rev);

