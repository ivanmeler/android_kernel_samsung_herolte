/* linux/drivers/video/exynos/decon_8890/decon_reg_8890.c
 *
 * Copyright 2013-2015 Samsung Electronics
 *      Jiun Yu <jiun.yu@samsung.com>
 *
 * Jiun Yu <jiun.yu@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

/* use this definition when you test CAL on firmware */
/* #define FW_TEST */
#ifdef FW_TEST
#include "decon_fw.h"
#define __iomem
#else
#include "decon.h"
#endif

/******************* CAL raw functions implementation *************************/
void decon_reg_set_disp_ss_cfg(u32 id, void __iomem *disp_ss_regs,
	u32 dsi_idx, struct decon_mode_info *psr)
{
	u32 val;

	if (!disp_ss_regs) {
		decon_err("decon%d disp_ss_regs is not mapped\n", id);
		return;
	}

	val = readl(disp_ss_regs + DISP_CFG);

	switch (id) {
	case 0:
		val = ((val & (~DISP_CFG_SYNC_MODE0_MASK)) | DISP_CFG_SYNC_MODE0_TE(dsi_idx));
		if ((psr->out_type == DECON_OUT_DSI) && (dsi_idx <= 2)) {
			val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(dsi_idx)));
			val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF0(dsi_idx) |
				DISP_CFG_DSIM_PATH_CFG0_EN(dsi_idx);
			if (psr->dsi_mode == DSI_MODE_DUAL_DSI) {
				val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(1)));
				val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF1(1) |
					DISP_CFG_DSIM_PATH_CFG0_EN(1);
			}
		} else if (psr->out_type == DECON_OUT_EDP) {
			val |= DISP_CFG_DP_PATH_CFG0_EN;
		}
		break;

	case 1:
		val = ((val & (~DISP_CFG_SYNC_MODE1_MASK)) | DISP_CFG_SYNC_MODE1_TE(dsi_idx));
		if ((psr->out_type == DECON_OUT_DSI) && (dsi_idx <= 2)) {
			val = (val & (~DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(dsi_idx)));
			val |= DISP_CFG_DSIM_PATH_CFG1_DISP_IF2(dsi_idx) |
				DISP_CFG_DSIM_PATH_CFG0_EN(dsi_idx);
		}
		break;

	default:
		break;
	}
	writel(val, disp_ss_regs + DISP_CFG);
	decon_dbg("Display Configuration(DISP_CFG) value is 0x%x\n", val);
}

int decon_reg_reset(u32 id)
{
	int tries;

	decon_write(id, GLOBAL_CONTROL, GLOBAL_CONTROL_SRESET);
	for (tries = 2000; tries; --tries) {
		if (~decon_read(id, GLOBAL_CONTROL) & GLOBAL_CONTROL_SRESET)
			break;
		udelay(10);
	}

	if (!tries) {
		decon_err("decon%d failed to reset Decon\n", id);
		return -EBUSY;
	}

	return 0;
}

void decon_reg_set_clkgate_mode(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if (id == 0)
		decon_write_mask(id, CLOCK_CONTROL_0, val, CLOCK_CONTROL_0_F_MASK);
	else if (id == 1)
		decon_write_mask(id, CLOCK_CONTROL_0, val, CLOCK_CONTROL_0_S_MASK);
	else if (id == 2)
		decon_write_mask(id, CLOCK_CONTROL_0, val, CLOCK_CONTROL_0_T_MASK);
}

void decon_reg_set_vclk_freerun(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if (id == 0)
		decon_write_mask(0, DISPIF0_DISPIF1_CONTROL, val, DISPIF0_DISPIF1_CONTROL_FREE_RUN_EN);
	else if (id == 1)
		decon_write_mask(0, DISPIF2_CONTROL, val, DISPIF2_CONTROL_FREE_RUN_EN);
	else if (id == 2)
		decon_write_mask(0, DISPIF3_CONTROL, val, DISPIF3_CONTROL_FREE_RUN_EN);
}

/*
 * VCLK is the video source clock of DECON
 * NUM   : NUM_VALUE_OF_VCLK[7:0]
 * DENOM : DENOM_VALUE_OF_VCLK[23:16]
*/
void decon_reg_set_vclk_divider(u32 id, u32 denom, u32 num)
{
	u32 val;

	if (num == 1)
		denom = (denom * 2) - 1;
	else {
		num = num - 1;
		denom = denom - 1;
	}

	val = DIVIDER_DENOM_VALUE_OF_CLK_F(denom)
		| DIVIDER_NUM_VALUE_OF_CLK_F(num);

	/* VCLK controlled by DECON_F(id=0) only */
	if (id == 0)
		decon_write(0, VCLK_DIVIDER_CONTROL, val);
	else if (id == 1)
		decon_write(0, VCLK2_DIVIDER_CONTROL, val);
	else if (id == 2)
		decon_write(0, VCLK3_DIVIDER_CONTROL, val);
}

/*
 * ECLK is the video source clock of DECON
 * NUM   : NUM_VALUE_OF_VCLK[7:0]
 * DENOM : DENOM_VALUE_OF_VCLK[23:16]
*/
void decon_reg_set_eclk_divider(u32 id, u32 denom, u32 num)
{
	u32 val;

	if (num == 1)
		denom = (denom * 2) - 1;
	else {
		num = num - 1;
		denom = denom - 1;
	}

	val = DIVIDER_DENOM_VALUE_OF_CLK_F(denom)
		| DIVIDER_NUM_VALUE_OF_CLK_F(num);

	decon_write(id, ECLK_DIVIDER_CONTROL, val);
}

void decon_reg_set_sram_share(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	if (id == 0)
		decon_write_mask(id, SRAM_SHARE_ENABLE, val, (SRAM_SHARE_ENABLE_DSC_F | SRAM_SHARE_ENABLE_F));
}

void decon_reg_set_data_path(u32 id, enum decon_data_path data_path, enum decon_enhance_path enhance_path)
{
	u32 val, mask;

	val = DATA_PATH_CONTROL_ENHANCE(enhance_path) | DATA_PATH_CONTROL_PATH(data_path);
	mask = DATA_PATH_CONTROL_ENHANCE_MASK | DATA_PATH_CONTROL_PATH_MASK;

	decon_write_mask(id, DATA_PATH_CONTROL, val, mask);
}

void decon_reg_get_data_path(u32 id, enum decon_data_path *data_path, enum decon_enhance_path *enhance_path)
{
	u32 val;

	val = decon_read(id, DATA_PATH_CONTROL);

	*data_path = DATA_PATH_CONTROL_PATH_GET(val);
	*enhance_path = DATA_PATH_CONTROL_ENHANCE_GET(val);
}

void decon_reg_set_operation_mode(u32 id, enum decon_psr_mode mode)
{
	if (mode == DECON_MIPI_COMMAND_MODE)
		decon_write_mask(id, GLOBAL_CONTROL, GLOBAL_CONTROL_OPERATION_MODE_I80IF_F,
						 GLOBAL_CONTROL_OPERATION_MODE_F);
	else
		decon_write_mask(id, GLOBAL_CONTROL, GLOBAL_CONTROL_OPERATION_MODE_RGBIF_F,
						 GLOBAL_CONTROL_OPERATION_MODE_F);
}

void decon_reg_set_blender_bg_image_size(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	u32 width, val, mask;

	width = lcd_info->xres;

	if (dsi_mode == DSI_MODE_DUAL_DSI)
		width = width * 2;

	val = BLENDER_BG_HEIGHT_F(lcd_info->yres) | BLENDER_BG_WIDTH_F(width);
	mask = BLENDER_BG_HEIGHT_MASK | BLENDER_BG_WIDTH_MASK;
	decon_write_mask(id, BLENDER_BG_IMAGE_SIZE_0, val, mask);

	val = (lcd_info->yres) * width;
	decon_write_mask(id, BLENDER_BG_IMAGE_SIZE_1, val, ~0);
}

void decon_reg_get_blender_bg_image_size(u32 id, u32 *p_width, u32 *p_height)
{
	u32 val;

	val = decon_read(id, BLENDER_BG_IMAGE_SIZE_0);

	*p_width = BLENDER_BG_WIDTH_GET(val);
	*p_height = BLENDER_BG_HEIGHT_GET(val);
}

void decon_reg_set_underrun_scheme(u32 id, enum decon_hold_scheme mode)
{
	if (id == 0)
		decon_write_mask(0, DISPIF0_DISPIF1_CONTROL, DISPIF0_DISPIF1_CONTROL_UNDERRUN_SCHEME_F(mode), DISPIF0_DISPIF1_CONTROL_UNDERRUN_SCHEME_MASK);
	else if (id == 1)
		decon_write_mask(0, DISPIF2_CONTROL, DISPIF2_CONTROL_UNDERRUN_SCHEME_F(mode), DISPIF2_CONTROL_UNDERRUN_SCHEME_MASK);
	else if (id == 2)
		decon_write_mask(0, DISPIF3_CONTROL, DISPIF3_CONTROL_UNDERRUN_SCHEME_F(mode), DISPIF3_CONTROL_UNDERRUN_SCHEME_MASK);
}

void decon_reg_set_rgb_order(u32 id, enum decon_rgb_order order)
{
	if (id == 0)
		decon_write_mask(0, DISPIF0_DISPIF1_CONTROL, DISPIF0_DISPIF1_CONTROL_OUT_RGB_ORDER_F(order), DISPIF0_DISPIF1_CONTROL_OUT_RGB_ORDER_MASK);
	else if (id == 1)
		decon_write_mask(0, DISPIF2_CONTROL, DISPIF2_CONTROL_OUT_RGB_ORDER_F(order), DISPIF2_CONTROL_OUT_RGB_ORDER_MASK);
	else if (id == 2)
		decon_write_mask(0, DISPIF3_CONTROL, DISPIF3_CONTROL_OUT_RGB_ORDER_F(order), DISPIF3_CONTROL_OUT_RGB_ORDER_MASK);
}

void decon_reg_set_frame_fifo_size(u32 id, enum decon_dsi_mode dsi_mode, u32 width, u32 height)
{
	u32 val;

	val = FRAME_FIFO_HEIGHT_F(height) | FRAME_FIFO_WIDTH_F(width);
	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_0, val);

	val = height * width;
	decon_write(id, FRAME_FIFO_0_SIZE_CONTROL_1, val);

	if (dsi_mode == DSI_MODE_DUAL_DSI) {
		val = FRAME_FIFO_HEIGHT_F(height) | FRAME_FIFO_WIDTH_F(width);
		decon_write(id, FRAME_FIFO_1_SIZE_CONTROL_0, val);

		val = height * width;
		decon_write(id, FRAME_FIFO_1_SIZE_CONTROL_1, val);
	}
}

void decon_reg_get_frame_fifo_size(u32 id, int dsi_idx, u32 *w, u32 *h)
{
	u32 val;

	if (id == 0) {
		if (dsi_idx == 0)
			val = decon_read(id, FRAME_FIFO_0_SIZE_CONTROL_0);
		else if (dsi_idx == 1)
			val = decon_read(id, FRAME_FIFO_1_SIZE_CONTROL_0);
	} else {
		/* TODO: Other DECON case will be implemented */
	}

	*w = FRAME_FIFO_WIDTH_GET(val);
	*h = FRAME_FIFO_HEIGHT_GET(val);
}

void decon_reg_set_splitter(u32 id, enum decon_dsi_mode dsi_mode, u32 width, u32 height)
{
	u32 val;
	u32 cr_0_setting_guide = 0;	/* 0 : guide about 1/2 mic, 1 : UM's guide about 1/2 mic */
	enum decon_mic_comp_ratio cr;

	if (id == 0) {
		if (dsi_mode == DSI_MODE_DUAL_DSI) {

			cr = (decon_read_mask(id, MIC_CONTROL, MIC_PARA_CR_CTRL_MASK)) >> MIC_PARA_CR_CTRL_SHIFT;	/* 0 : 1/2, 1 : 1/3 */

			if ((cr_0_setting_guide == 0) && (cr == MIC_COMP_RATIO_1_2)) {
				decon_write(id, SPLITTER_CONTROL_0, (width / 2));

				val = SPLITTER_HEIGHT_F(height) | SPLITTER_WIDTH_F(width);
				decon_write(id, SPLITTER_SIZE_CONTROL_0, val);

				val = height * width;
				decon_write(id, SPLITTER_SIZE_CONTROL_1, val);
			} else {
				decon_write(id, SPLITTER_CONTROL_0, width);

				val = SPLITTER_HEIGHT_F(height) | SPLITTER_WIDTH_F((width) * 2);
				decon_write(id, SPLITTER_SIZE_CONTROL_0, val);

				val = height * width * 2;
				decon_write(id, SPLITTER_SIZE_CONTROL_1, val);
			}
		} else {
			decon_write(id, SPLITTER_CONTROL_0, 0);

			val = SPLITTER_HEIGHT_F(height) | SPLITTER_WIDTH_F(width);
			decon_write(id, SPLITTER_SIZE_CONTROL_0, val);

			val = height * width;
			decon_write(id, SPLITTER_SIZE_CONTROL_1, val);
		}
	}
}

void decon_reg_get_splitter_size(u32 id, u32 *w, u32 *h)
{
	u32 val;

	val = decon_read(id, SPLITTER_SIZE_CONTROL_0);

	*w = SPLITTER_WIDTH_GET(val);
	*h = SPLITTER_HEIGHT_GET(val);
}

void decon_reg_set_dispif_porch(u32 id, int dsi_idx, struct decon_lcd *lcd_info)
{
	u32 val;

	val = DISPIF_TIMING_VBPD_F(lcd_info->vbp) | DISPIF_TIMING_VFPD_F(lcd_info->vfp);
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_TIMING_CONTROL_0, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_TIMING_CONTROL_0, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_TIMING_CONTROL_0, val);
	else if (id == 2)
		decon_write(0, DISPIF3_TIMING_CONTROL_0, val);

	val = DISPIF_TIMING_VSPD_F(lcd_info->vsa);
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_TIMING_CONTROL_1, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_TIMING_CONTROL_1, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_TIMING_CONTROL_1, val);
	else if (id == 2)
		decon_write(0, DISPIF3_TIMING_CONTROL_1, val);

	val = DISPIF_TIMING_HBPD_F(lcd_info->hbp) | DISPIF_TIMING_HFPD_F(lcd_info->hfp);
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_TIMING_CONTROL_2, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_TIMING_CONTROL_2, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_TIMING_CONTROL_2, val);
	else if (id == 2)
		decon_write(0, DISPIF3_TIMING_CONTROL_2, val);

	val = DISPIF_TIMING_HSPD_F(lcd_info->hsa);
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_TIMING_CONTROL_3, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_TIMING_CONTROL_3, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_TIMING_CONTROL_3, val);
	else if (id == 2)
		decon_write(0, DISPIF3_TIMING_CONTROL_3, val);
}

void decon_reg_set_dispif_size(u32 id, int dsi_idx, u32 width, u32 height)
{
	u32 val;

	val = DISPIF_HEIGHT_F(height) | DISPIF_WIDTH_F(width);
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_SIZE_CONTROL_0, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_SIZE_CONTROL_0, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_SIZE_CONTROL_0, val);
	else if (id == 2)
		decon_write(0, DISPIF3_SIZE_CONTROL_0, val);

	val = width * height;
	if (id == 0) {
		if (dsi_idx == 0)
			decon_write(0, DISPIF0_SIZE_CONTROL_1, val);
		else if (dsi_idx == 1)
			decon_write(0, DISPIF1_SIZE_CONTROL_1, val);
	} else if (id == 1)
		decon_write(0, DISPIF2_SIZE_CONTROL_1, val);
	else if (id == 2)
		decon_write(0, DISPIF3_SIZE_CONTROL_1, val);
}

void decon_reg_get_dispif_size(u32 id, int dsi_idx, u32 *p_width, u32 *p_height)
{
	u32 val;

	if (id == 0) {
		if (dsi_idx == 0)
			val = decon_read(0, DISPIF0_SIZE_CONTROL_0);
		else if (dsi_idx == 1)
			val = decon_read(0, DISPIF1_SIZE_CONTROL_0);
	} else if (id == 1)
		val = decon_read(0, DISPIF2_SIZE_CONTROL_0);
	else if (id == 2)
		val = decon_read(0, DISPIF3_SIZE_CONTROL_0);

	*p_width = DISPIF_WIDTH_GET(val);
	*p_height = DISPIF_HEIGHT_GET(val);
}

void decon_reg_set_comp_size(u32 id, enum decon_mic_comp_ratio cr, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	u32 ratio_type; /* 0 : 12N, 1: 12N4, 2 : 12N8, 3 : ratio2 */
	u32 mic_width_in_bytes, mic_dummy_in_bytes = 0;
	u32 width;
	u32 temp_size, odd_n = 0;

	if (dsi_mode == DSI_MODE_DUAL_DSI)
		width = lcd_info->xres * 2;
	else
		width = lcd_info->xres;

	if (cr) {
		/* 1/3 ratio */
		temp_size = width / 12;
		if (temp_size % 2)
			odd_n = 1;
		else
			odd_n = 0;

		if ((width % 12) == 0) {  /* 12n */
			ratio_type = 0;

			if (dsi_mode) {
				if (odd_n)
					mic_dummy_in_bytes = 12;
				else
					mic_dummy_in_bytes = 0;
			} else
				mic_dummy_in_bytes = 0;
		} else if ((width % 12) == 4) { /* 12n+4 */
			ratio_type = 1;

			if (dsi_mode) {
				if (odd_n)
					mic_dummy_in_bytes = 8;
				else
					mic_dummy_in_bytes = 20;
			} else
				mic_dummy_in_bytes = 16;
		} else if ((width % 12) == 8) {  /* 12n+8 */
			ratio_type = 2;

			if (dsi_mode) {
				if (odd_n)
					mic_dummy_in_bytes = 4;
				else
					mic_dummy_in_bytes = 16;
			} else
				mic_dummy_in_bytes = 8;
		} else {
			decon_err("\nUnsupported Width\n");
		}

		mic_width_in_bytes = (width * 2) / (1 + dsi_mode) + mic_dummy_in_bytes;
	} else {
		/* 1/2 ratio */
		if (dsi_mode) {
			if (width % 8) {
				decon_err("\nUnsupported Width\n");
			}
		} else {
			if (width % 4) {
				decon_err("\nUnsupported Width\n");
			}
		}

		ratio_type = 3;

#if DECON_CR0_SETTING
		mic_width_in_bytes = (width / 2) * 3 / (1 + dsi_mode);
#else
		mic_width_in_bytes = (width / 2) * 3;
#endif

		mic_dummy_in_bytes = 0;
	}

	decon_dbg("\nratio_type=%d\n", ratio_type);

	/* set MIC Dummy Size in Bytes */
	decon_write_mask(id, MIC_CONTROL, MIC_DUMMY_F(mic_dummy_in_bytes), MIC_DUMMY_MASK);

	/* set MIC Comp. Size in Bytes */
	decon_write_mask(id, MIC_SIZE_CONTROL, MIC_WIDTH_C_F(mic_width_in_bytes), MIC_WIDTH_C_MASK);

	if (cr) {	/* 1/3 ratio */
		/* set Splitter Size */
		decon_reg_set_splitter(id, dsi_mode, (mic_width_in_bytes / 3), ((lcd_info->yres) / 2));

		/* set Frame FIFO Size */
		decon_reg_set_frame_fifo_size(id, dsi_mode, (mic_width_in_bytes / 3), ((lcd_info->yres) / 2));

		/* set Display Size */
		decon_reg_set_dispif_size(id, 0, (mic_width_in_bytes / 3 / 2), (lcd_info->yres));
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, (mic_width_in_bytes / 3 / 2), (lcd_info->yres));
	} else {
#if DECON_CR0_SETTING
		/* set Splitter Size */
		decon_reg_set_splitter(id, dsi_mode, (mic_width_in_bytes / 3), (lcd_info->yres));

		/* set Frame FIFO Size */
		decon_reg_set_frame_fifo_size(id, dsi_mode, (mic_width_in_bytes / 3), (lcd_info->yres));

		/* set Display Size */
		decon_reg_set_dispif_size(id, 0, (mic_width_in_bytes / 3), (lcd_info->yres));
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, (mic_width_in_bytes / 3), (lcd_info->yres));
#else
		if (dsi_mode == DSI_MODE_DUAL_DSI) {
			/* set Splitter Size */
			decon_reg_set_splitter(id, dsi_mode, (mic_width_in_bytes / 3), (lcd_info->yres));

			/* set Frame FIFO Size */
			decon_reg_set_frame_fifo_size(id, dsi_mode, (mic_width_in_bytes / 3 / 2), (lcd_info->yres));

			/* set Display Size */
			decon_reg_set_dispif_size(id, 0, (mic_width_in_bytes / 3 / 2), (lcd_info->yres));
			decon_reg_set_dispif_size(id, 1, (mic_width_in_bytes / 3 / 2), (lcd_info->yres));

		} else {
			/* set Splitter Size */
			decon_reg_set_splitter(id, dsi_mode, (mic_width_in_bytes / 3), (lcd_info->yres));

			/* set Frame FIFO Size */
			decon_reg_set_frame_fifo_size(id, dsi_mode, (mic_width_in_bytes / 3), (lcd_info->yres));

			/* set Display Size */
			decon_reg_set_dispif_size(id, 0, (mic_width_in_bytes / 3), (lcd_info->yres));
		}
#endif
	}
}

void decon_reg_config_mic(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	enum decon_mic_comp_ratio cr = lcd_info->mic_ratio;	/* 0 : 1/2, 1 : 1/3 */

	if (id == 0) {
		/* set slice mode */
		decon_write_mask(id, MIC_CONTROL, MIC_SLICE_NUM_F(dsi_mode), MIC_SLICE_NUM_MASK);

		/* set pixel order */
		decon_write_mask(id, MIC_CONTROL, MIC_PIXEL_ORDER_F(cr), MIC_PIXEL_ORDER_MASK);	/* x1/2: should be 0 for display with DDI, x1/3: 1 */

		/* set compratio */
		decon_write_mask(id, MIC_CONTROL, MIC_PARA_CR_CTRL_F(cr), MIC_PARA_CR_CTRL_MASK);

		/* set compression size */
		decon_reg_set_comp_size(id, cr, dsi_mode, lcd_info);
	}
}

void decon_reg_update_req_global(u32 id)
{
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, SHADOW_REG_UPDATE_REQ_GLOBAL);
}

void decon_reg_update_req_window(u32 id, u32 win_idx)
{
	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, SHADOW_REG_UPDATE_REQ_WIN(win_idx));
}

void decon_reg_all_win_shadow_update_req(u32 id)
{
	u32 mask;

	/* DECON_F has 8 windows */
	if (!id)
		mask = SHADOW_REG_UPDATE_REQ_FOR_DECON_F;
	/* DECON_S/T has 4 windows */
	else
		mask = SHADOW_REG_UPDATE_REQ_FOR_DECON_T;

	decon_write_mask(id, SHADOW_REG_UPDATE_REQ, ~0, mask);
}

void decon_reg_direct_on_off(u32 id, u32 en)
{
	u32 val = en ? ~0 : 0;

	decon_write_mask(id, GLOBAL_CONTROL, val, GLOBAL_CONTROL_DECON_EN | GLOBAL_CONTROL_DECON_EN_F);
}

void decon_reg_per_frame_off(u32 id)
{
	decon_write_mask(id, GLOBAL_CONTROL, 0, GLOBAL_CONTROL_DECON_EN_F);
}

void decon_reg_configure_lcd(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	enum decon_data_path d_path;
	enum decon_enhance_path e_path;

	decon_reg_set_rgb_order(id, DECON_RGB);

	decon_reg_set_dispif_porch(id, 0, lcd_info);

	if (dsi_mode == DSI_MODE_DUAL_DSI)
		decon_reg_set_dispif_porch(id, 1, lcd_info);

	if (lcd_info->mic_enabled) {
		if (id != 0)
			decon_err("\n   [ERROR!!!] decon.%d doesn't support MIC\n", id);
		decon_reg_config_mic(id, dsi_mode, lcd_info);

		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	} else if (lcd_info->dsc_enabled) {
		if (lcd_info->dsc_cnt == 1)
			decon_reg_set_data_path(id,
					DATAPATH_ENC0_SPLITTERBYPASS_U0FF_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else if (lcd_info->dsc_cnt == 2)
			decon_reg_set_data_path(id,
					DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_err("not supported data path for %d DSC of decon%d",
					lcd_info->dsc_cnt, id);

		dsc_reg_init(id, dsi_mode, lcd_info);
	} else {
		decon_reg_set_splitter(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_frame_fifo_size(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_dispif_size(id, 0, lcd_info->xres, lcd_info->yres);
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, lcd_info->xres, lcd_info->yres);

		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	}

	decon_reg_set_vclk_freerun(id, 1);
	decon_reg_direct_on_off(id, 0);

	decon_reg_get_data_path(id, &d_path, &e_path);
	decon_dbg("%s: decon%d, enhance path(0x%x), data path(0x%x)\n",
			__func__, id, e_path, d_path);
}

#ifdef CONFIG_FB_DSU
void decon_reg_configure_lcd_dsu(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	pr_info( "%s.\n", __func__ );

	decon_reg_set_rgb_order(id, DECON_RGB);

	decon_reg_set_dispif_porch(id, 0, lcd_info);

	if (dsi_mode == DSI_MODE_DUAL_DSI)
		decon_reg_set_dispif_porch(id, 1, lcd_info);

	if (lcd_info->mic_enabled) {
		if (id != 0)
			decon_err("\n   [ERROR!!!] decon.%d doesn't support MIC\n", id);
		decon_reg_config_mic(id, dsi_mode, lcd_info);

		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	} else if (lcd_info->dsc_enabled) {
		if (lcd_info->dsc_cnt == 1)
			decon_reg_set_data_path(id,
					DATAPATH_ENC0_SPLITTERBYPASS_U0FF_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else if (lcd_info->dsc_cnt == 2)
			decon_reg_set_data_path(id,
					DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_err("not supported data path for %d DSC of decon%d",
					lcd_info->dsc_cnt, id);

		dsc_reg_init(id, dsi_mode, lcd_info);
	} else {
		decon_reg_set_splitter(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_frame_fifo_size(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_dispif_size(id, 0, lcd_info->xres, lcd_info->yres);
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, lcd_info->xres, lcd_info->yres);

		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	}
}

#endif

void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode)
{
	u32 val, mask;

	mask = HW_SW_TRIG_CONTROL_HW_TRIG_EN;

	if (mode == DECON_SW_TRIG) {
		/* To support per-frameoff TRIG_AUTO_MASK disabled */
		mask |= HW_SW_TRIG_CONTROL_TRIG_AUTO_MASK_TRIG;
		val = 0;
	} else {
		val = ~0;
	}

	decon_write_mask(id, HW_SW_TRIG_CONTROL, val, mask);
}


u32 decon_reg_get_linecnt(u32 id, int dispif_idx)
{
	u32 val;

	if (dispif_idx == 0)
		val = decon_read_mask(0, DISPIF_LINE_COUNT, DISPIF_LINE_COUNT_DISPIF0_MASK) >> DISPIF_LINE_COUNT_DISPIF0_SHIFT;
	else if (dispif_idx == 1)
		val = decon_read_mask(0, DISPIF_LINE_COUNT, DISPIF_LINE_COUNT_DISPIF1_MASK) >> DISPIF_LINE_COUNT_DISPIF1_SHIFT;
	else if (dispif_idx == 2)
		val = decon_read_mask(0, DISPIF2_LINE_COUNT, DISPIF2_LINE_COUNT_MASK) >> DISPIF2_LINE_COUNT_SHIFT;
	else if (dispif_idx == 3)
		val = decon_read_mask(0, DISPIF3_LINE_COUNT, DISPIF3_LINE_COUNT_MASK) >> DISPIF3_LINE_COUNT_SHIFT;

	return val;
}

/* timeout : usec */
int decon_reg_wait_linecnt_is_zero_timeout(u32 id, int dsi_idx, unsigned long timeout)
{
	unsigned long delay_time = 100;
	int disp_idx = (!id) ? id : (id + 1);
	unsigned long cnt = timeout / delay_time;
	u32 linecnt;

	do {
		linecnt = decon_reg_get_linecnt(id, disp_idx);
		if (!linecnt)
			break;
		cnt--;
		udelay(delay_time);
	} while (cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout linecount is zero(%u)\n", id, linecnt);
		return -EBUSY;
	}

	return 0;
}

u32 decon_reg_get_idle_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CONTROL);
	if (val & GLOBAL_CONTROL_IDLE_STATUS)
		return 1;

	return 0;
}

int decon_reg_wait_idle_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_idle_status(id);
		cnt--;
		udelay(delay_time);
	} while (!status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon idle status(%u)\n", id, status);
		return -EBUSY;
	}

	return 0;
}

u32 decon_reg_get_run_status(u32 id)
{
	u32 val;

	val = decon_read(id, GLOBAL_CONTROL);
	if (val & GLOBAL_CONTROL_RUN_STATUS)
		return 1;

	return 0;
}

/* Determine that DECON is perfectly shuttled off through checking this * function */
int decon_reg_wait_run_is_off_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_run_status(id);
		cnt--;
		udelay(delay_time);
	} while (status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon run is shut-off(%u)\n", id, status);
		return -EBUSY;
	}

	return 0;
}

int decon_reg_wait_run_status_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 10;
	unsigned long cnt = timeout / delay_time;
	u32 status;

	do {
		status = decon_reg_get_run_status(id);
		cnt--;
		udelay(delay_time);
	} while (!status && cnt);

	if (!cnt) {
		decon_err("decon%d wait timeout decon run status(%u)\n", id, status);
		return -EBUSY;
	}

	return 0;
}

void decon_reg_clear_int_all(u32 id)
{
	u32 mask;

	/* mask = INTERRUPT_DISPIF_VSTATUS | INTERRUPT_RESOURCE_CONFLICT | INTERRUPT_FRAME_DONE | INTERRUPT_FIFO_LEVEL; */
	mask = ~0;
	decon_write_mask(id, INTERRUPT_PENDING, ~0, mask);
}

u32 decon_get_win_channel(u32 id, u32 win_idx, enum decon_idma_type type)
{
	u32 ch_id;

	switch (type) {
	case IDMA_G0_S:
	case IDMA_G0:
		ch_id = 7;
		break;
	case IDMA_G1:
		ch_id = 0;
		break;
	case IDMA_G2:
		ch_id = 1;
		break;
	case IDMA_G3:
		ch_id = 2;
		break;
	case IDMA_VG0:
		ch_id = 3;
		break;
	case IDMA_VG1:
		ch_id = 4;
		break;
	case IDMA_VGR0:
		ch_id = 5;
		break;
	case IDMA_VGR1:
		ch_id = 6;
		break;
	default:
		decon_dbg("decon%d channel(0x%x) is not valid\n", id, type);
		return -EINVAL;
	}

	return ch_id;
}

void decon_reg_config_win_channel(u32 id, u32 win_idx, enum decon_idma_type type)
{
	u32 ch_id;

	ch_id = decon_get_win_channel(id, win_idx, type);

	decon_write_mask(id, WIN_CONTROL(win_idx), WIN_CONTROL_CHMAP_F(ch_id), WIN_CONTROL_CHMAP_MASK);
}


/* wait until shadow update is finished */
int decon_reg_wait_for_update_timeout(u32 id, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;
	struct decon_device *decon = get_decon_drvdata(id);

	while (decon_read(id, SHADOW_REG_UPDATE_REQ) && --cnt){
		if (decon->pdata->psr_mode == DECON_MIPI_COMMAND_MODE) {
			if (decon->ignore_vsync)
				goto wait_exit;
		}
		udelay(delay_time);
	}

	if (!cnt) {
		decon_err("decon%d timeout of updating decon registers\n", id);
		return -EBUSY;
	}
wait_exit:
	return 0;
}


/* wait until shadow update is finished */
int decon_reg_wait_for_window_update_timeout(u32 id, u32 win_idx, unsigned long timeout)
{
	unsigned long delay_time = 100;
	unsigned long cnt = timeout / delay_time;

	while ((decon_read(id, SHADOW_REG_UPDATE_REQ) & SHADOW_REG_UPDATE_REQ_WIN(win_idx)) && --cnt)
		udelay(delay_time);

	if (!cnt) {
		decon_err("decon%d timeout of updating decon window registers\n", id);
		return -EBUSY;
	}

	return 0;
}



/* Is it need to do hw trigger unmask and mask asynchronously in case of dual DSI */
/* enable(unmask) / disable(mask) hw trigger */
void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr, enum decon_set_trig en)
{
	u32 val = (en == DECON_TRIG_ENABLE) ? ~0 : 0;
	u32 mask;

	if (psr->psr_mode == DECON_VIDEO_MODE)
		return;

	if (psr->trig_mode == DECON_SW_TRIG) {
		val = (en == DECON_TRIG_ENABLE) ? ~0 : 0;
		mask = HW_SW_TRIG_CONTROL_SW_TRIG;
	} else {
		val = (en == DECON_TRIG_DISABLE) ? ~0 : 0;
		mask = HW_SW_TRIG_CONTROL_HW_TRIG_MASK;
	}

	decon_write_mask(id, HW_SW_TRIG_CONTROL, val, mask);
}

/****************** DSC related functions ********************/
void dsc_reg_set_reset(u32 encoder_id)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg |= DSC_CONTROL0_SW_RESET;
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}
void dsc_reg_set_swap(u32 encoder_id, u32 uBit, u32 uByte, u32 uWord)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~((DSC_CONTROL0_BIT_SWAP_MASK | DSC_CONTROL0_BYTE_SWAP_MASK | DSC_CONTROL0_WORD_SWAP_MASK));
	reg |= (DSC_CONTROL0_BIT_SWAP(uBit) | DSC_CONTROL0_BYTE_SWAP(uByte) | DSC_CONTROL0_WORD_SWAP(uWord));
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}
void dsc_reg_set_flatness_det_th(u32 encoder_id, u32 uThreshold)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~(DSC_CONTROL0_FLATNESS_DET_TH_F_MASK);
	reg |= DSC_CONTROL0_FLATNESS_DET_TH_F(uThreshold);
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}

void dsc_reg_set_slice_mode_change(u32 encoder_id, u32 uEn)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~(DSC_CONTROL0_SLICE_MODE_CH_F_MASK);
	reg |= DSC_CONTROL0_SLICE_MODE_CH_F(uEn);
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}
void dsc_reg_set_encoder_bypass(u32 encoder_id, u32 uEn)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~(DSC_CONTROL0_DSC_BYPASS_F_MASK);
	reg |= DSC_CONTROL0_DSC_BYPASS_F(uEn);
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}
void dsc_reg_set_auto_clock_gating(u32 encoder_id, u32 uEn)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~(DSC_CONTROL0_DSC_CG_EN_F_MASK);
	reg |= DSC_CONTROL0_DSC_CG_EN_F(uEn);
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}

void dsc_reg_set_dual_slice_mode(u32 encoder_id, u32 uEn)
{
	u32 reg;

	reg =  dsc_read(encoder_id, DSC_CONTROL0);
	reg &= ~(DSC_CONTROL0_DUAL_SLICE_EN_F_MASK);
	reg |= DSC_CONTROL0_DUAL_SLICE_EN_F(uEn);
	dsc_write(encoder_id, DSC_CONTROL0, reg);
}
void dsc_reg_set_vertical_porch(u32 encoder_id, u32 uVFP, u32 uVSW, u32 uVBP)
{
	u32 reg;

	reg = (DSC_CONTROL1_DSC_V_FRONT_F(uVFP) | DSC_CONTROL1_DSC_V_SYNC_F(uVSW) | DSC_CONTROL1_DSC_V_BACK_F(uVBP));
	dsc_write(encoder_id, DSC_CONTROL1, reg);
}
void dsc_reg_set_horizontal_porch(u32 encoder_id, u32 uHFP, u32 uHSW, u32 uHBP)
{
	u32 reg;

	reg = (DSC_CONTROL2_DSC_H_FRONT_F(uHFP) | DSC_CONTROL2_DSC_H_SYNC_F(uHSW) | DSC_CONTROL2_DSC_H_BACK_F(uHBP));
	dsc_write(encoder_id, DSC_CONTROL2, reg);
}

void dsc_reg_set_input_pixel_count(u32 encoder_id, u32 picture_width, u32 picture_height)
{
	u32 reg;

	reg = (picture_width*picture_height);
	dsc_write(encoder_id, DSC_IN_PIXEL_COUNT, reg);
}
void dsc_reg_set_comp_pixel_count(u32 encoder_id)
{
	u32 reg;
	reg = decon_read(0, FRAME_FIFO_0_SIZE_CONTROL_1);
	dsc_write(encoder_id, DSC_COMP_PIXEL_COUNT, reg);
}

/*
#########################################################################################
						dsc PPS Configuration
#########################################################################################
*/
u32 dsc_reg_get_num_extra_mux_bits(u32 SliceHeight, u32 chunk_size)
{
	u32 uNum_Extra_Mux_Bits;
	u32 uSliceBits;

	uNum_Extra_Mux_Bits = DSC_NUM_EXTRA_MUX_BIT;
	uSliceBits = 8*chunk_size*SliceHeight;
	while ((uSliceBits-uNum_Extra_Mux_Bits)%48)
		uNum_Extra_Mux_Bits--;

	return	uNum_Extra_Mux_Bits;
}
u32 dsc_reg_get_slice_height(u32 picture_height, u32 slice_width)
{

	u32 slice_height;
	u32 uMinSliceSize;
	u32 uQuotient = 1;
	u32 i, uCnt = 0;
	u32 uTemp[] = {0,};
	decon_dbg("\npicture_height:%d", picture_height);
	decon_dbg("\nslice_width:%d", slice_width);

	while (1) {
		if ((picture_height%uQuotient) == 0) {
			uTemp[uCnt] = uQuotient;
			uCnt++;
		}
		uQuotient++;
		if (picture_height/2 < uQuotient) {
			uTemp[uCnt-1] = picture_height;
			break;
		}
	}
	for (i = 0; i < uCnt; i++)
		decon_dbg("\nQuotient[%d]:%d", i, uTemp[i]);
	for (i = 0; i < uCnt; i++) {
		slice_height = uTemp[i];
		uMinSliceSize = slice_width*slice_height;
		if (uMinSliceSize > DSC_MIN_SLICE_SIZE)
			break;
	}

	decon_dbg("\n SliceHeight: %d", slice_height);
	return	slice_height;
}

void dsc_reg_set_pps_0_3_dsc_version(u32 encoder_id, u32 ver)
{
	u32 reg;

	reg = dsc_read(encoder_id, DSC_PPS00_03);
	reg &= ~DSC_PPS00_03_DSC_VER_MASK;
	reg |= DSC_PPS00_03_DSC_VER(ver);
	dsc_write(encoder_id, DSC_PPS00_03, reg);
}

void dsc_reg_set_pps_6_7_picture_height(u32 encoder_id, u32 height)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS04_07);
	reg &= ~DSC_PPS06_07_PICTURE_HEIGHT_MASK;
	reg |= DSC_PPS06_07_PICTURE_HEIGHT(height);
	dsc_write(encoder_id, DSC_PPS04_07, reg);
}
void dsc_reg_set_pps_8_9_picture_width(u32 encoder_id, u32 width)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS08_11);
	reg &= ~DSC_PPS08_9_PICTURE_WIDHT_MASK;
	reg |= DSC_PPS08_9_PICTURE_WIDHT(width);
	dsc_write(encoder_id, DSC_PPS08_11, reg);
}
void dsc_reg_set_pps_10_11_slice_height(u32 encoder_id, u32 height)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS08_11);
	reg &= ~(DSC_PPS10_11_SLICE_HEIGHT_MASK);
	reg |= DSC_PPS10_11_SLICE_HEIGHT(height);
	dsc_write(encoder_id, DSC_PPS08_11, reg);
}
void dsc_reg_set_pps_12_13_slice_width(u32 encoder_id, u32 width)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS12_15);
	reg &= ~DSC_PPS12_13_SLICE_WIDTH_MASK;
	reg |= DSC_PPS12_13_SLICE_WIDTH(width);
	dsc_write(encoder_id, DSC_PPS12_15, reg);
}
void dsc_reg_set_pps_14_15_chunk_size(u32 encoder_id, u32 slice_width)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS12_15);
	reg &= ~DSC_PPS14_15_CHUNK_SIZE_MASK;
	reg |= DSC_PPS14_15_CHUNK_SIZE(slice_width);
	dsc_write(encoder_id, DSC_PPS12_15, reg);
}

void dsc_reg_set_pps_16_17_init_xmit_delay(u32 dsc_id)
{
	u32 val;

	val = DSC_PPS16_17_INIT_XMIT_DELAY(DSC_INIT_XMIT_DELAY);
	dsc_write_mask(dsc_id, DSC_PPS16_19, val, DSC_PPS16_17_INIT_XMIT_DELAY_MASK);
}

void dsc_reg_set_pps_18_19_init_dec_delay(u32 dsc_id, u32 slice_width)
{
	int rbs_min, hrd_delay, groups_per_line, init_dec_delay;
	u32 val;

	groups_per_line = (slice_width + 2) / 3;
	rbs_min = 6144 + groups_per_line * 12;
	hrd_delay = (int)(CEIL(rbs_min / 8));
	init_dec_delay = hrd_delay - 512;

	val = DSC_PPS16_19_INIT_DEC_DELAY(init_dec_delay);
	dsc_write_mask(dsc_id, DSC_PPS16_19, val, DSC_PPS16_19_INIT_DEC_DELAY_MASK);
}

void dsc_reg_set_pps_21_initial_scale_value(u32 encoder_id)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS20_23);
	reg &= ~DSC_PPS21_INIT_SCALE_VALUE_MASK;
	reg |= DSC_PPS21_INIT_SCALE_VALUE(DSC_INIT_SCALE_VALUE);
	dsc_write(encoder_id, DSC_PPS20_23, reg);
}

void dsc_reg_set_pps_22_23_scale_increment_interval(u32 encoder_id, u32 slice_width, u32 slice_height)
{
	u32 reg;
	u32 uPPS22_ScaleIncrementInterval;
	u32 uPPS28_NflBpgOffset;
	u32 uPPS30_SliceBpgOffset;
	u32 uPPS34_FinalOffset;
	u32 uFinalScale;
	u32 uGroupsTotal;
	u32 uNumExtraMuxBits;
	u32 chunk_size;
	u32 val;

#if 1
	val = (DSC_FIRST_LINE_BPG_OFFSET*2048) / (slice_height-1);
	uPPS28_NflBpgOffset = (u32)CEIL(val);
#else
	uPPS28_NflBpgOffset = (u32)CEIL((double)((DSC_FIRST_LINE_BPG_OFFSET*2048)/(slice_height-1)));
#endif
	uPPS28_NflBpgOffset += 1;

	chunk_size = slice_width;
	uNumExtraMuxBits = dsc_reg_get_num_extra_mux_bits(slice_height, chunk_size);
	uGroupsTotal = ((slice_width+2)/3)*slice_height;
	uPPS30_SliceBpgOffset = (u32)CEIL(2048*(DSC_RC_MODE_SIZE-DSC_INIT_OFFSET+uNumExtraMuxBits)/uGroupsTotal);
	uPPS30_SliceBpgOffset += 1;

	uNumExtraMuxBits = dsc_reg_get_num_extra_mux_bits(slice_height, chunk_size);
	uPPS34_FinalOffset = DSC_RC_MODE_SIZE-((DSC_INIT_TRANSMIT_DELAY*DSC_BIT_PER_PIXEL+8)>>4)+uNumExtraMuxBits;

	uFinalScale	= 8*(DSC_RC_MODE_SIZE/(DSC_RC_MODE_SIZE-uPPS34_FinalOffset));

	uPPS22_ScaleIncrementInterval = (u32)(2048*uPPS34_FinalOffset/((uFinalScale-9)*(uPPS28_NflBpgOffset+uPPS30_SliceBpgOffset)));
	//decon_info("\nuPPS22_ScaleIncrementInterval=2048x(%d)/((%d-9x)(%d+%d))=(%d)", uPPS34_FinalOffset, uFinalScale, uPPS28_NflBpgOffset, uPPS30_SliceBpgOffset, uPPS22_ScaleIncrementInterval);

	reg  =  dsc_read(encoder_id, DSC_PPS20_23);
	reg &= ~DSC_PPS22_23_SCALE_INCREMENT_INTERVAL_MASK;
	reg |= DSC_PPS22_23_SCALE_INCREMENT_INTERVAL(uPPS22_ScaleIncrementInterval);
	dsc_write(encoder_id, DSC_PPS20_23, reg);
}

void dsc_reg_set_pps_24_25_scale_decrement_interval(u32 encoder_id, u32 slice_width)
{
	u32 reg;
	u32 uGroupPerLine;
	u32 uPPS24_ScaleDecrementInterval;

	uGroupPerLine = ((slice_width+2)/3);
	uPPS24_ScaleDecrementInterval = (uGroupPerLine/(DSC_INIT_SCALE_VALUE-8));

	reg  =  dsc_read(encoder_id, DSC_PPS24_27);
	reg &= ~DSC_PPS24_25_SCALE_DECREMENT_INTERVAL_MASK;
	reg |= DSC_PPS24_25_SCALE_DECREMENT_INTERVAL(uPPS24_ScaleDecrementInterval);
	dsc_write(encoder_id, DSC_PPS24_27, reg);
}

void dsc_reg_set_pps_27_first_line_bpg_offset(u32 encoder_id)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, DSC_PPS24_27);
	reg &= ~DSC_PPS27_FIRST_LINE_BPG_OFFSET_MASK;
	reg |= DSC_PPS27_FIRST_LINE_BPG_OFFSET(DSC_FIRST_LINE_BPG_OFFSET);
	dsc_write(encoder_id, DSC_PPS24_27, reg);
}
void dsc_reg_set_pps_28_29_nfl_bpg_offset(u32 encoder_id, u32 slice_height)												/*Fractional bits*/
{
	u32 reg;
	u32 uPPS28_uNflBpgOffset;

	uPPS28_uNflBpgOffset = (u32)CEIL(((DSC_FIRST_LINE_BPG_OFFSET*2048)/(slice_height-1)));
	uPPS28_uNflBpgOffset += 1;
	//decon_info("\nuPPS28_uNflBpgOffset=((%d*2048)/(%d-1))=%d", uPPS27_FirstLineBpgOffset, slice_height, uPPS28_uNflBpgOffset);

	reg  =  dsc_read(encoder_id, DSC_PPS28_31);
	reg &= ~DSC_PPS28_29_NOT_FIRST_LINE_BPG_OFFSET_MASK;
	reg |= DSC_PPS28_29_NOT_FIRST_LINE_BPG_OFFSET(uPPS28_uNflBpgOffset);
	dsc_write(encoder_id, DSC_PPS28_31, reg);
}
void dsc_reg_set_pps_30_31_slice_bpg_offset(u32 encoder_id, u32 slice_width, u32 slice_height, u32 chunk_size)			/*Fractional bits*/
{
	u32 reg;
	u32 uPPS30_SliceBpgOffset;
	u32 uNumExtraMuxBits;
	u32 uGroupsTotal;

	uNumExtraMuxBits = dsc_reg_get_num_extra_mux_bits(slice_height, chunk_size);
	uGroupsTotal = ((slice_width+2)/3)*slice_height;
	uPPS30_SliceBpgOffset = (u32)CEIL(2048*(DSC_RC_MODE_SIZE-DSC_INIT_OFFSET+uNumExtraMuxBits)/uGroupsTotal);
	uPPS30_SliceBpgOffset += 1;
	//decon_info("\nuNumExtraMuxBits = %d", uNumExtraMuxBits);
	//decon_info("\nuPPS30_SliceBpgOffset=2048*(%d-%d+%d)/(%d)=(%d)", uPPS38_RCModelSize, uPPS32_InitialOffset, uNumExtraMuxBits, uGroupsTotal, uPPS30_SliceBpgOffset);

	reg  =  dsc_read(encoder_id, (DSC_PPS28_31));
	reg &= ~DSC_PPS30_31_SLICE_BPG_OFFSET_MASK;
	reg |= DSC_PPS30_31_SLICE_BPG_OFFSET(uPPS30_SliceBpgOffset);
	dsc_write(encoder_id, (DSC_PPS28_31), reg);
}
void dsc_reg_set_pps_32_33_initial_offset(u32 encoder_id)
{
	u32 reg;

	reg  =  dsc_read(encoder_id, (DSC_PPS32_35));
	reg &= ~DSC_PPS32_33_INIT_OFFSET_MASK;
	reg |= DSC_PPS32_33_INIT_OFFSET(DSC_INIT_OFFSET);
	dsc_write(encoder_id, (DSC_PPS32_35), reg);
}
void dsc_reg_set_pps_34_35_final_offset(u32 encoder_id, u32 chunk_size, u32 slice_height)
{
	u32 reg;
	u32 uPPS34_FinalOffset;
	u32 uNumExtraMuxBits;

	uNumExtraMuxBits = dsc_reg_get_num_extra_mux_bits(slice_height, chunk_size);
	//decon_info("\nuNumExtraMuxBits = %d", uNumExtraMuxBits);
	uPPS34_FinalOffset = DSC_RC_MODE_SIZE-((DSC_INIT_TRANSMIT_DELAY*DSC_BIT_PER_PIXEL+8)>>4)+uNumExtraMuxBits;

	reg  =  dsc_read(encoder_id, (DSC_PPS32_35));
	reg &= ~DSC_PPS34_35_FINAL_OFFSET_MASK;
	reg |= DSC_PPS34_35_FINAL_OFFSET(uPPS34_FinalOffset);
	dsc_write(encoder_id, (DSC_PPS32_35), reg);
}

u32 dsc_get_compressed_slice_width(u32 x_resol, u32 dscc_en, u32 ds_en)
{
	u32 slice_width;
	u32 width_eff;
	u32 slice_width_byte_unit, comp_slice_width_byte_unit;
	u32 comp_slice_width_pixel_unit;
	u32 overlap_w = 0;
	u32 comp_slice_w = 0;
	u32 i, j;

	/* check if two encoders are used */
	if (dscc_en)
		width_eff = (x_resol >> 1) + overlap_w;
	else
		width_eff = x_resol + overlap_w;

	/* check if dual slice is enabled */
	if (ds_en)
		slice_width = width_eff >> 1;
	else
		slice_width = width_eff;

	/* 3bytes per pixel */
	slice_width_byte_unit = slice_width * 3;
	/* integer value, /3 for 1/3 compression */
	comp_slice_width_byte_unit = slice_width_byte_unit / 3;
	/* integer value, /3 for pixel unit */
	comp_slice_width_pixel_unit = comp_slice_width_byte_unit / 3;

	i = comp_slice_width_byte_unit % 3;
	j = comp_slice_width_pixel_unit % 2;

	if ( i == 0 && j == 0) {
		comp_slice_w = comp_slice_width_pixel_unit;
	} else if (i == 0 && j != 0) {
		comp_slice_w = comp_slice_width_pixel_unit + 1;
	} else if (i != 0) {
		while (1) {
			comp_slice_width_pixel_unit++;
			j = comp_slice_width_pixel_unit % 2;
			if (j == 0)
				break;
		}
		comp_slice_w = comp_slice_width_pixel_unit;
	}

	return comp_slice_w;

}

void dsc_reg_set_encoder(u32 encoder_id, struct decon_lcd *lcd_info)
{
	u32 dual_slice = 0;
	u32 slice_mode_change = 0;
	int dsi_cnt = 1;

	if (lcd_info->dsc_slice_num == lcd_info->dsc_cnt *2) dual_slice = 1;
	slice_mode_change = ((lcd_info->dsc_slice_num == lcd_info->dsc_cnt) != (lcd_info->dsc_slice_num == dsi_cnt));
	decon_dbg("dual slice(%d), slice_mode_change(%d)\n", dual_slice, slice_mode_change);

	dsc_reg_set_swap(encoder_id, 0x0, 0x0, 0x0);
	dsc_reg_set_flatness_det_th(encoder_id, 0x2);
	dsc_reg_set_auto_clock_gating(encoder_id, 1);
	dsc_reg_set_dual_slice_mode(encoder_id, dual_slice);
	dsc_reg_set_encoder_bypass(encoder_id, 0);
	dsc_reg_set_slice_mode_change(encoder_id, slice_mode_change);

	dsc_reg_set_pps_0_3_dsc_version(encoder_id, 0x11);
	dsc_reg_set_pps_16_17_init_xmit_delay(encoder_id);
	dsc_reg_set_pps_21_initial_scale_value(encoder_id);
	dsc_reg_set_pps_27_first_line_bpg_offset(encoder_id);
	dsc_reg_set_pps_32_33_initial_offset(encoder_id);
}

void dsc_reg_set_pps_size(u32 encoder_id, struct decon_lcd *lcd_info)
{
	u32 picture_w, picture_h;
	u32 slice_w, slice_h;
	u32 chunk_size;
	int dsc_slice_pixels;

	picture_h = lcd_info->yres;
	slice_w = lcd_info->xres / lcd_info->dsc_slice_num;
	slice_h = (lcd_info->dsc_slice_num == 4) ? 64 : 32;
	chunk_size = slice_w;

	if (lcd_info->dsc_cnt == 1) {
		picture_w = lcd_info->xres;
	} else if (lcd_info->dsc_cnt == 2) {
		picture_w = lcd_info->xres >> 1;
	} else {
		picture_w = lcd_info->xres;
		decon_err("DSC max count is %d\n", lcd_info->dsc_cnt);
	}

	dsc_slice_pixels = 15000 / (lcd_info->xres /lcd_info->dsc_slice_num);
	if( dsc_slice_pixels > 64 ) slice_h = 128;
	else if( dsc_slice_pixels > 32 ) slice_h = 64;
	else slice_h = 32;

	decon_dbg("DSC%d: picture w(%d) h(%d)\n", encoder_id, picture_w, picture_h);
	decon_dbg("slice w(%d) h(%d), chunk(%d)\n",
			slice_w, slice_h, chunk_size);

	dsc_reg_set_input_pixel_count(encoder_id, picture_w, picture_h);
	dsc_reg_set_comp_pixel_count(encoder_id);

	dsc_reg_set_pps_6_7_picture_height(encoder_id, picture_h);
	dsc_reg_set_pps_8_9_picture_width(encoder_id, picture_w);
	dsc_reg_set_pps_10_11_slice_height(encoder_id, slice_h);
	dsc_reg_set_pps_12_13_slice_width(encoder_id, slice_w);
	dsc_reg_set_pps_14_15_chunk_size(encoder_id, slice_w);
	dsc_reg_set_pps_18_19_init_dec_delay(encoder_id, slice_w);
	dsc_reg_set_pps_22_23_scale_increment_interval(encoder_id, slice_w, slice_h);
	dsc_reg_set_pps_24_25_scale_decrement_interval(encoder_id, slice_w);
	dsc_reg_set_pps_28_29_nfl_bpg_offset(encoder_id, slice_h);
	dsc_reg_set_pps_30_31_slice_bpg_offset(encoder_id, slice_w, slice_h, chunk_size);
	dsc_reg_set_pps_34_35_final_offset(encoder_id, chunk_size, slice_h);
}

void decon_reg_config_dsc_size(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	u32 width = 0;
	u32 fifo_w = 0;
	u32 dsc_id = id;
	u32 dual_slice = 0;
	u32 slice_mode_change = 0;
	int dsi_cnt = 1;

	if (lcd_info->dsc_slice_num == lcd_info->dsc_cnt *2) dual_slice = 1;
	slice_mode_change = ((lcd_info->dsc_slice_num == lcd_info->dsc_cnt) != (lcd_info->dsc_slice_num == dsi_cnt));
	decon_dbg("dual slice(%d), slice_mode_change(%d)\n", dual_slice, slice_mode_change);

	width = dsc_get_compressed_slice_width(lcd_info->xres, slice_mode_change, dual_slice) * 2;

	fifo_w = width;
	/* DSC(pps) -> Splitter -> FF_FIFO -> DISPIF */
	if (lcd_info->dsc_cnt == 2) {
		/* Case of 1-Encoder, Decon_F -> DSC0, Decon_S -> DSC1 */
		dsc_id = DECON_DSC_ENC_1;
		dsc_reg_set_input_pixel_count(dsc_id, lcd_info->xres >> 1, lcd_info->yres);
		dsc_reg_set_comp_pixel_count(dsc_id);
		dsc_reg_set_pps_6_7_picture_height(dsc_id, lcd_info->yres);
		dsc_id = DECON_DSC_ENC_0;
	}

	dsc_reg_set_pps_6_7_picture_height(dsc_id, lcd_info->yres);

	decon_reg_set_splitter(id, dsi_mode, width, lcd_info->yres);
	if (lcd_info->dsc_cnt == 2) {
		fifo_w = width / 2;
		decon_reg_set_frame_fifo_size(id, DSI_MODE_DUAL_DSI,
				fifo_w, lcd_info->yres);
	} else {
		decon_reg_set_frame_fifo_size(id, DSI_MODE_SINGLE,
				fifo_w, lcd_info->yres);
	}
	decon_reg_set_dispif_size(id, 0, width, lcd_info->yres);
}

int dsc_reg_init(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info)
{
	u32 w, h;
	u32 dsc_id = id;

	if (lcd_info->dsc_cnt == 2) {
		/* Case of 1-Encoder, Decon_F -> DSC0, Decon_S -> DSC1 */
		dsc_id = DECON_DSC_ENC_1;
		dsc_reg_set_encoder(dsc_id, lcd_info);
		dsc_reg_set_pps_size(dsc_id, lcd_info);
		dsc_id = DECON_DSC_ENC_0;
	}

	dsc_reg_set_encoder(dsc_id, lcd_info);
	dsc_reg_set_pps_size(dsc_id, lcd_info);
	decon_reg_config_dsc_size(id, dsi_mode, lcd_info);

	decon_reg_get_splitter_size(id, &w, &h);
	decon_dbg("SPLITTER size: w(%d), h(%d)\n", w, h);
	decon_reg_get_frame_fifo_size(id, 0, &w, &h);
	decon_dbg("FIFO0 size: w(%d), h(%d)\n", w, h);
	decon_reg_get_frame_fifo_size(id, 1, &w, &h);
	decon_dbg("FIFO1 size: w(%d), h(%d)\n", w, h);
	decon_reg_get_dispif_size(id, 0, &w, &h);
	decon_dbg("DISPIF0 size: w(%d), h(%d)\n", w, h);

	return 0;
}

/***************** CAL APIs implementation *******************/
int decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;

	/* DECON does not need to start, if DECON is already
	 * running(enabled in LCD_ON_UBOOT) */
	if (decon_reg_get_run_status(id)) {
		decon_info("decon_reg_init already called by BOOTLOADER\n");
		decon_reg_init_probe(id, dsi_idx, p);
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		return -EBUSY;
	}
	/* Configure a DISP_SS */
	decon_reg_set_disp_ss_cfg(id, p->disp_ss_regs, dsi_idx, psr);

	decon_reg_reset(id);

	decon_reg_set_clkgate_mode(id, 1);

	if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
		/* (denom, num) : VCLK=VCLK_SRC * (num+1)/(denom+1) */
		decon_reg_set_vclk_divider(id, 2, 1);
	else
		/* (denom, num) : VCLK=VCLK_SRC * (num+1)/(denom+1) */
		decon_reg_set_vclk_divider(id, 1, 1);

	/* (denom, num) : ECLK=ECLK_SRC * (num+1)/(denom+1) */
	decon_reg_set_eclk_divider(id, 1, 1);

	decon_reg_set_sram_share(id, 0);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_image_size(id, psr->dsi_mode, lcd_info);

	if (id == 2) {
		/* Set a TRIG mode */
		decon_reg_configure_trigger(id, psr->trig_mode);

		/*
		 * Interrupt of DECON-T should be set to video mode,
		 * because of malfunction of I80 frame done interrupt.
		 */
		psr->psr_mode = DECON_VIDEO_MODE;
		decon_reg_set_int(id, psr, 1);
		decon_reg_set_underrun_scheme(id, DECON_VCLK_NOT_AFFECTED);

		/* RGB order -> frame fifo size -> splitter -> porch values -> mic configuration -> freerun mode --> stop DECON */
		decon_reg_configure_lcd(id, psr->dsi_mode, lcd_info);
	} else {
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			decon_reg_set_underrun_scheme(id, DECON_VCLK_HOLD_ONLY);		/* guided by designer to change from DECON_VCLK_RUNNING_VDEN_DISABLE */
		else
			decon_reg_set_underrun_scheme(id, DECON_VCLK_HOLD_ONLY);

		/* RGB order -> frame fifo size -> splitter -> porch values -> mic configuration -> freerun mode --> stop DECON */
		decon_reg_configure_lcd(id, psr->dsi_mode, lcd_info);

		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
			decon_reg_configure_trigger(id, psr->trig_mode);
			decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
		}
	}

	/* FIXME: DECON_T dedicated to PRE_WB */
	if (p->psr.out_type == DECON_OUT_WB)
		decon_reg_set_data_path(id, DATAPATH_PREWB_ONLY, ENHANCEPATH_ENHANCE_ALL_OFF);

	/* asserted interrupt should be cleared before initializing decon hw */
	decon_reg_clear_int_all(id);

	return 0;
}


void decon_reg_init_probe(u32 id, u32 dsi_idx, struct decon_param *p)
{
	struct decon_lcd *lcd_info = p->lcd_info;
	struct decon_mode_info *psr = &p->psr;
	enum decon_data_path d_path;
	enum decon_enhance_path e_path;

	decon_reg_set_clkgate_mode(id, 1);

	if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
		decon_reg_set_vclk_divider(id, 2, 1);	/* (denom, num) : VCLK=VCLK_SRC * (num+1)/(denom+1) */
	else
		decon_reg_set_vclk_divider(id, 1, 1);	/* (denom, num) : VCLK=VCLK_SRC * (num+1)/(denom+1) */

	decon_reg_set_eclk_divider(id, 1, 1);	/* (denom, num) : ECLK=ECLK_SRC * (num+1)/(denom+1) */

	decon_reg_set_sram_share(id, 0);

	decon_reg_set_operation_mode(id, psr->psr_mode);

	decon_reg_set_blender_bg_image_size(id, psr->dsi_mode, lcd_info);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_underrun_scheme(id, DECON_VCLK_HOLD_ONLY);		/* guided by designer to change from DECON_VCLK_RUNNING_VDEN_DISABLE */
	else
		decon_reg_set_underrun_scheme(id, DECON_VCLK_HOLD_ONLY);

	/*
	* ->
	* same as decon_reg_configure_lcd(...) function
	* except using decon_reg_update_req_global(id) instead of decon_reg_direct_on_off(id, 0)
	*/
	decon_reg_set_rgb_order(id, DECON_RGB);

	decon_reg_set_dispif_porch(id, 0, lcd_info);

	if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
		decon_reg_set_dispif_porch(id, 1, lcd_info);

	if (lcd_info->mic_enabled) {
		if (id != 0)
			decon_err("\n   [ERROR!!!] decon.%d doesn't support MIC\n", id);
		decon_reg_config_mic(id, psr->dsi_mode, lcd_info);

		if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_MIC_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	} else if (lcd_info->dsc_enabled) {
		if (lcd_info->dsc_cnt == 1)
			decon_reg_set_data_path(id,
					DATAPATH_ENC0_SPLITTERBYPASS_U0FF_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else if (lcd_info->dsc_cnt == 2)
			decon_reg_set_data_path(id,
					DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP0,
					ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_err("not supported data path for %d DSC of decon%d",
					lcd_info->dsc_cnt, id);

		dsc_reg_init(id, psr->dsi_mode, lcd_info);
	} else {
		decon_reg_set_splitter(id, psr->dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_frame_fifo_size(id, psr->dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_dispif_size(id, 0, lcd_info->xres, lcd_info->yres);
		if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, lcd_info->xres, lcd_info->yres);

		if (psr->dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTER_U0FFU1FF_DISP0DISP1, ENHANCEPATH_ENHANCE_ALL_OFF);
		else
			decon_reg_set_data_path(id, DATAPATH_NOCOMP_SPLITTERBYPASS_U0FF_DISP0, ENHANCEPATH_ENHANCE_ALL_OFF);
	}

	decon_reg_set_vclk_freerun(id, 1);
	decon_reg_update_req_global(id);
	/*
	* <-
	*/

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
		decon_reg_configure_trigger(id, psr->trig_mode);
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	decon_reg_get_data_path(id, &d_path, &e_path);
	decon_dbg("%s: decon%d, enhance path(0x%x), data path(0x%x)\n",
			__func__, id, e_path, d_path);
}


int decon_reg_start(u32 id, struct decon_mode_info *psr)
{
	int ret = 0;
	decon_reg_direct_on_off(id, 1);

	decon_reg_update_req_global(id);

	/* DECON goes to run-status as soon as request shadow update without HW_TE */
	ret = decon_reg_wait_run_status_timeout(id, 20 * 1000);

	/* wait until run-status, then trigger */
	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
	return ret;
}

void decon_reg_set_partial_update(u32 id, enum decon_dsi_mode dsi_mode,
		struct decon_lcd *lcd_info)
{
	decon_reg_set_blender_bg_image_size(id, dsi_mode, lcd_info);
	decon_reg_set_dispif_porch(id, 0, lcd_info);

	if (dsi_mode == DSI_MODE_DUAL_DSI)
		decon_reg_set_dispif_porch(id, 1, lcd_info);

	if (lcd_info->mic_enabled) {
		if (id != 0)
			decon_err("\n   [ERROR!!!] decon.%d doesn't support MIC\n", id);
		/* set compression size */
		decon_reg_set_comp_size(id, lcd_info->mic_ratio, dsi_mode, lcd_info);
	} else if (lcd_info->dsc_enabled) {
		decon_reg_config_dsc_size(id, dsi_mode, lcd_info);
	} else {
		decon_reg_set_splitter(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_frame_fifo_size(id, dsi_mode, lcd_info->xres, lcd_info->yres);
		decon_reg_set_dispif_size(id, 0, lcd_info->xres, lcd_info->yres);
		if (dsi_mode == DSI_MODE_DUAL_DSI)
			decon_reg_set_dispif_size(id, 1, lcd_info->xres, lcd_info->yres);
	}
}

int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr)
{
	int ret = 0;

#if 0		/* method which I recommends */
	decon_reg_per_frame_off(id);
	decon_reg_update_req_global(id);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE) {
		if (psr->trig_mode == DECON_SW_TRIG) {
			/* s/w trigger will work in case vertical status is not active in codition TRIG_AUTO_MASK_EN is enabled */
			ret = decon_reg_wait_idle_status_timeout(id, 20 * 1000);	/* timeout : 20ms */
			if (ret)
				goto err;
		}
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
	}

	/* timeout : 20ms */
	ret = decon_reg_wait_run_is_off_timeout(id, 20 * 1000);
	if (ret)
		goto err;

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);

	decon_reg_per_frame_off(id);	/* UM guide : clear ENVID_F again in case of per-frame stop (because of Q-channel??) */

#else		/* method which SWSOL wants */
	if ((psr->psr_mode == DECON_MIPI_COMMAND_MODE) &&
			(psr->trig_mode == DECON_HW_TRIG)) {
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);
	}

	/* timeout : 50ms */
	/* TODO: dual DSI scenario */
	ret = decon_reg_wait_linecnt_is_zero_timeout(id, dsi_idx, 50 * 1000);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE || ret < 0)
		decon_reg_direct_on_off(id, 0);
	else
		decon_reg_per_frame_off(id);

	decon_reg_update_req_global(id);

	/* timeout : 20ms */
	ret = decon_reg_wait_run_is_off_timeout(id, 20 * 1000);
	if (ret)
		goto err;
#endif

err:
	ret = decon_reg_reset(id);

	return ret;
}

void decon_reg_release_resource(u32 id, struct decon_mode_info *psr)
{
	decon_reg_per_frame_off(id);
	decon_reg_update_req_global(id);
	decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}

void decon_reg_release_resource_instantly(u32 id)
{
	decon_reg_direct_on_off(id, 0);
	decon_reg_update_req_global(id);
	decon_reg_wait_run_is_off_timeout(id, 20 * 1000);
}

void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en)
{
	u32 val;

	decon_reg_clear_int_all(id);

#if 0
	if (en) {
		val = INTERRUPT_INT_EN | INTERRUPT_FIFO_LEVEL_INT_EN | INTERRUPT_RESOURCE_CONFLICT_INT_EN | INTERRUPT_DPU0_INT_EN | INTERRUPT_DPU1_INT_EN;
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			val |= INTERRUPT_FRAME_DONE_INT_EN;
		else
			val |= INTERRUPT_DISPIF_VSTATUS_INT_EN | INTERRUPT_DISPIF_VSTATUS_VSA;
		decon_write_mask(id, INTERRUPT_ENABLE, val, ~0);
	} else {
		decon_write_mask(id, INTERRUPT_ENABLE, 0, INTERRUPT_INT_EN);
	}

#else	/* OS always use Vstatus Interrupt irrespective of operating mode */
	if (en) {
		val = INTERRUPT_INT_EN | INTERRUPT_FIFO_LEVEL_INT_EN | INTERRUPT_RESOURCE_CONFLICT_INT_EN | INTERRUPT_DPU0_INT_EN | INTERRUPT_DPU1_INT_EN;
		if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
			val |= INTERRUPT_FRAME_DONE_INT_EN;
			val |= INTERRUPT_DISPIF_VSTATUS_INT_EN | INTERRUPT_DISPIF_VSTATUS_VSA;
		decon_write_mask(id, INTERRUPT_ENABLE, val, ~0);
	} else {
		decon_write_mask(id, INTERRUPT_ENABLE, 0, INTERRUPT_INT_EN);
	}
#endif
}

void decon_reg_set_winmap(u32 id, u32 idx, u32 color, u32 en)
{
	u32 val = en ? WIN_CONTROL_MAPCOLOR_EN_F : 0;

	/* Enable */
	decon_write_mask(id, WIN_CONTROL(idx), val, WIN_CONTROL_MAPCOLOR_EN_MASK);

	/* Color Set */
	val = WIN_COLORMAP_MAPCOLOR_F(color);
	decon_write_mask(id, WIN_COLORMAP(idx), val, WIN_COLORMAP_MAPCOLOR_MASK);
}

void decon_reg_set_window_control(u32 id, int win_idx,
		struct decon_window_regs *regs, u32 winmap_en)
{
	decon_write(id, WIN_CONTROL(win_idx), regs->wincon);
	decon_write(id, WIN_START_POSITION(win_idx), regs->start_pos);
	decon_write(id, WIN_END_POSITION(win_idx), regs->end_pos);
	decon_write(id, WIN_START_TIME_CONTROL(win_idx), regs->start_time);
	decon_write(id, WIN_PIXEL_COUNT(win_idx), regs->pixel_count);

	decon_reg_set_winmap(id, win_idx, regs->colormap, winmap_en);
	decon_reg_config_win_channel(id, win_idx, regs->type);

	decon_dbg("%s: regs->type(%d)\n", __func__, regs->type);
}


void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr)
{
	decon_reg_update_req_global(id);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_ENABLE);
}


int decon_reg_wait_update_done_and_mask(u32 id, struct decon_mode_info *psr, u32 timeout)
{
	int result;

	result = decon_reg_wait_for_update_timeout(id, timeout);

	if (psr->psr_mode == DECON_MIPI_COMMAND_MODE)
		decon_reg_set_trigger(id, psr, DECON_TRIG_DISABLE);

	return result;
}

int decon_reg_get_interrupt_and_clear(u32 id)
{
	u32 val = decon_read(id, INTERRUPT_PENDING);

	if (val & INTERRUPT_FIFO_LEVEL_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_FIFO_LEVEL_INT_EN);

	if (val & INTERRUPT_FRAME_DONE_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_FRAME_DONE_INT_EN);

	if (val & INTERRUPT_ODD_FRAME_START_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_ODD_FRAME_START_INT_EN);

	if (val & INTERRUPT_EVEN_FRAME_START_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_EVEN_FRAME_START_INT_EN);

	if (val & INTERRUPT_RESOURCE_CONFLICT_INT_EN) {
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_RESOURCE_CONFLICT_INT_EN);
		decon_warn("decon%d RESOURCE_OCCUPANCY_INFO_0: 0x%x, INFO_1: 0x%x\n",
			id,
			decon_read(id, RESOURCE_OCCUPANCY_INFO_0),
			decon_read(id, RESOURCE_OCCUPANCY_INFO_1));
		decon_warn("decon%d RESOURCE_SEL_0: 0x%x, SEL_1: 0x%x, INDUCER: 0x%x\n",
			id,
			decon_read(id, RESOURCE_SEL_0),
			decon_read(id, RESOURCE_SEL_1),
			decon_read(id, RESOURCE_CONFLICTION_INDUCER));
	}

	if (val & INTERRUPT_DISPIF_VSTATUS_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_DISPIF_VSTATUS_INT_EN);

	if (val & INTERRUPT_DPU0_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_DPU0_INT_EN);

	if (val & INTERRUPT_DPU1_INT_EN)
		decon_write(id, INTERRUPT_PENDING, INTERRUPT_DPU1_INT_EN);

	return val;
}

/******************  OS Only ******************/
int decon_reg_is_win_enabled(u32 id, int win_idx)
{
	if (decon_read(id, WIN_CONTROL(win_idx)) & WIN_CONTROL_EN_F)
		return 1;

	return 0;
}

u32 decon_reg_get_width(u32 id, int dsi_mode)
{
	int disp_idx = (!id) ? id : (id + 1);
	u32 val = 0;

	val = decon_read(id, DISPIF_SIZE_CONTROL_0(disp_idx));
	return (val >> DISPIF_WIDTH_START_POS) & DISPIF_WIDTH_MASK;
}

u32 decon_reg_get_height(u32 id, int dsi_mode)
{
	int disp_idx = (!id) ? id : (id + 1);
	u32 val = 0;

	val = decon_read(id, DISPIF_SIZE_CONTROL_0(disp_idx));
	return (val >> DISPIF_HEIGHT_START_POS) & DISPIF_WIDTH_MASK;
}

const signed long decon_clocks_table[][CLK_ID_MAX] = {
	/* VCLK,  ECLK,  ACLK,  PCLK,  DISP_PLL,  resolution,           MIC_ratio, DSC count */
	{    -1,   400,   400,    66,        -1, 3840 * 2160,	  MIC_COMP_BYPASS,         0},
	{    -1,   400,   400,    66,        -1, 3840 * 2160,	  MIC_COMP_BYPASS,         1},
	{    71,   168,   400,    66,        71, 1080 * 1920,     MIC_COMP_BYPASS,         0},
	{    63,   168,   400,    66,        63, 1440 * 2560,  MIC_COMP_RATIO_1_2,         0},
	{  41.7, 137.5,   400,    66,      62.5, 1440 * 2560,  MIC_COMP_RATIO_1_3,         0},
	{   141, 137.5,   400,    66,       141, 1440 * 2560,     MIC_COMP_BYPASS,         0},
	{    42,   337,   400,    66,        42, 1440 * 2560,     MIC_COMP_BYPASS,         1},
    {    42,   168,   400,    66,        42, 1440 * 2560,     MIC_COMP_BYPASS,         2},
    {   10.5,  168,   400,    66,      10.5,  720 * 1280,     MIC_COMP_BYPASS,         2},  /* HD */
    {    24,   168,   400,    66,        24, 1080 * 1920,     MIC_COMP_BYPASS,         2},  /* FHD */
    {    63,   224,   400,    66,        63, 1600 * 2560,  	  MIC_COMP_BYPASS,         2},  /* wqxga */
};

void decon_reg_get_clock_ratio(struct decon_clocks *clks, struct decon_lcd *lcd_info)
{
	int i = sizeof(decon_clocks_table) / sizeof(decon_clocks_table[0]) - 1;

	/* set reset value */
	clks->decon[CLK_ID_VCLK] = decon_clocks_table[0][CLK_ID_VCLK];
	clks->decon[CLK_ID_ECLK] = decon_clocks_table[0][CLK_ID_ECLK];
	clks->decon[CLK_ID_ACLK] = decon_clocks_table[0][CLK_ID_ACLK];
	clks->decon[CLK_ID_PCLK] = decon_clocks_table[0][CLK_ID_PCLK];
	clks->decon[CLK_ID_DPLL] = decon_clocks_table[0][CLK_ID_DPLL];

	for (; i >= 0; i--) {
#ifdef CONFIG_FB_DSU
		// temporary, all clock become WQHD. code doesn't have FHD/HD clock
		if (decon_clocks_table[i][CLK_ID_RESOLUTION] != 1440 * 2560) {
			continue;
		}
#else
		if (decon_clocks_table[i][CLK_ID_RESOLUTION]
				!= lcd_info->xres * lcd_info->yres) {
			continue;
		}
#endif
		if (!lcd_info->mic_enabled && !lcd_info->dsc_enabled) {
			if (decon_clocks_table[i][CLK_ID_MIC_RATIO]
					!= MIC_COMP_BYPASS)
				continue;
		}

		if (lcd_info->mic_enabled) {
			if (decon_clocks_table[i][CLK_ID_MIC_RATIO]
					!= lcd_info->mic_ratio)
				continue;
		}

		if (lcd_info->dsc_enabled) {
			if (decon_clocks_table[i][CLK_ID_DSC_RATIO]
					!= lcd_info->dsc_cnt)
				continue;
		}

		clks->decon[CLK_ID_VCLK] = decon_clocks_table[i][CLK_ID_VCLK];
		clks->decon[CLK_ID_ECLK] = decon_clocks_table[i][CLK_ID_ECLK];
		clks->decon[CLK_ID_ACLK] = decon_clocks_table[i][CLK_ID_ACLK];
		clks->decon[CLK_ID_PCLK] = decon_clocks_table[i][CLK_ID_PCLK];
		clks->decon[CLK_ID_DPLL] = decon_clocks_table[i][CLK_ID_DPLL];
		break;
	}

	decon_dbg("%s: VCLK %ld ECLK %ld ACLK %ld PCLK %ld DPLL %ld\n",
		__func__,
		clks->decon[CLK_ID_VCLK],
		clks->decon[CLK_ID_ECLK],
		clks->decon[CLK_ID_ACLK],
		clks->decon[CLK_ID_PCLK],
		clks->decon[CLK_ID_DPLL]);
}
