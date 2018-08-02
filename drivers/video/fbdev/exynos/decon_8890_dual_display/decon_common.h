/*
 * decon_common.h
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos DECON driver for exynos8890
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_DECON_COMMON_H__
#define ___SAMSUNG_DECON_COMMON_H__

#include "./panels/decon_lcd.h"

#define MAX_DECON_WIN		8
#define MAX_VPP_SUBDEV		9
#define SHADOW_UPDATE_TIMEOUT	(300 * 1000) /* 300ms */
#define CEIL(x)			((x-(u32)(x) > 0 ? (u32)(x+1) : (u32)(x)))

#define DSC_INIT_XMIT_DELAY	0x200

enum decon_trig_mode {
	DECON_HW_TRIG = 0,
	DECON_SW_TRIG
};

enum decon_output_type {
	DECON_OUT_DSI = 0,
	DECON_OUT_EDP,
	DECON_OUT_HDMI,
	DECON_OUT_WB,
	DECON_OUT_TUI
};

enum decon_dsi_mode {
	DSI_MODE_SINGLE = 0,
	DSI_MODE_DUAL_DSI,
	DSI_MODE_DUAL_DISPLAY,
	DSI_MODE_NONE
};

enum decon_hold_scheme {
	/*  should be set to this value in case of DSIM video mode */
	DECON_VCLK_HOLD_ONLY		= 0x00,
	/*  should be set to this value in case of DSIM command mode */
	DECON_VCLK_RUNNING_VDEN_DISABLE = 0x01,
	DECON_VCLK_HOLD_VDEN_DISABLE	= 0x02,
	/*  should be set to this value in case of HDMI, eDP */
	DECON_VCLK_NOT_AFFECTED		= 0x03,
};

enum decon_rgb_order {
	DECON_RGB = 0x0,
	DECON_GBR = 0x1,
	DECON_BRG = 0x2,
	DECON_BGR = 0x4,
	DECON_RBG = 0x5,
	DECON_GRB = 0x6,
};

/* Porter Duff Compositing Operators */
enum decon_win_func {
	PD_FUNC_CLEAR			= 0x0,
	PD_FUNC_COPY			= 0x1,
	PD_FUNC_DESTINATION		= 0x2,
	PD_FUNC_SOURCE_OVER		= 0x3,
	PD_FUNC_DESTINATION_OVER	= 0x4,
	PD_FUNC_SOURCE_IN		= 0x5,
	PD_FUNC_DESTINATION_IN		= 0x6,
	PD_FUNC_SOURCE_OUT		= 0x7,
	PD_FUNC_DESTINATION_OUT		= 0x8,
	PD_FUNC_SOURCE_A_TOP		= 0x9,
	PD_FUNC_DESTINATION_A_TOP	= 0xa,
	PD_FUNC_XOR			= 0xb,
	PD_FUNC_PLUS			= 0xc,
	PD_FUNC_LEGACY			= 0xd,
	PD_FUNC_LEGACY2			= 0xe,
};

enum decon_win_alpha_sel {
	/*  for plane blending */
	W_ALPHA_SEL_F_ALPAH0 = 0,
	W_ALPHA_SEL_F_ALPAH1 = 1,
	/*  for pixel blending */
	W_ALPHA_SEL_F_BYAEN = 2,
	/*  for multiplied alpha */
	W_ALPHA_SEL_F_BYMUL = 4,
};

enum decon_set_trig {
	DECON_TRIG_DISABLE = 0,
	DECON_TRIG_ENABLE
};

enum decon_idma_type {
	IDMA_G0 = 0,	/* Dedicated to WIN7 */
	IDMA_G1,
	IDMA_VG0,
	IDMA_VG1,
	IDMA_G2,
	IDMA_G3,
	IDMA_VGR0,
	IDMA_VGR1,
	ODMA_WB,
	IDMA_G0_S,
};

struct decon_mode_info {
	enum decon_psr_mode psr_mode;
	enum decon_trig_mode trig_mode;
	enum decon_output_type out_type;
	enum decon_dsi_mode dsi_mode;
};

struct decon_param {
	struct decon_mode_info psr;
	struct decon_lcd *lcd_info;
	u32 nr_windows;
	void __iomem *disp_ss_regs;
};


struct decon_window_regs {
	u32 wincon;
	u32 start_pos;
	u32 end_pos;
	u32 colormap;
	u32 start_time;
	u32 pixel_count;

	/* OS used */
	u32 whole_w;
	u32 whole_h;
	u32 offset_x;
	u32 offset_y;
	u32 winmap_state;

	enum decon_idma_type type;
};

/* To find a proper CLOCK ratio */
enum decon_clk_id {
	CLK_ID_VCLK = 0,
	CLK_ID_ECLK,
	CLK_ID_ACLK,
	CLK_ID_PCLK,
	CLK_ID_DPLL, /* DISP_PLL */
	CLK_ID_RESOLUTION,
	CLK_ID_MIC_RATIO,
	CLK_ID_DSC_RATIO,
	CLK_ID_MAX,
};

struct decon_clocks {
	signed long decon[CLK_ID_DPLL + 1];
};

enum decon_data_path {
	/* BLENDER-OUT -> No Comp -> SPLITTER(bypass) -> u0_FF -> u0_DISPIF */
	DATAPATH_NOCOMP_SPLITTERBYPASS_U0FF_DISP0		= 0x01,
	/* BLENDER-OUT -> No Comp -> SPLITTER(bypass) -> u1_FF -> u1_DISPIF */
	DATAPATH_NOCOMP_SPLITTERBYPASS_U1FF_DISP1		= 0x02,
	/* BLENDER-OUT -> No Comp -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_NOCOMP_SPLITTER_U0FFU1FF_DISP0DISP1		= 0x03,
	/* BLENDER-OUT -> MIC -> SPLITTER(bypass) -> u0_FF -> u0_DISPIF */
	DATAPATH_MIC_SPLITTERBYPASS_U0FF_DISP0			= 0x09,
	/* BLENDER-OUT -> MIC -> SPLITTER(bypass) -> u1_FF -> u1_DISPIF */
	DATAPATH_MIC_SPLITTERBYPASS_U1FF_DISP1			= 0x0A,
	/* BLENDER-OUT -> MIC -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_MIC_SPLITTER_U0FFU1FF_DISP0DISP1		= 0x0B,
	/* BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u_MERGER -> u0_DISPIF */
	DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP0		= 0x11,
	/* BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u_MERGER -> u1_DISPIF */
	DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP1		= 0x12,
	/* BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_DISP0DISP1		= 0x13,
	/* BLENDER-OUT -> ENC0 -> SPLITTER(bypass) -> u0_FF u0_DISPIF */
	DATAPATH_ENC0_SPLITTERBYPASS_U0FF_DISP0			= 0x21,
	/* BLENDER-OUT -> ENC0 -> SPLITTER(bypass) -> u1_FF u1_DISPIF */
	DATAPATH_ENC0_SPLITTERBYPASS_U1FF_DISP1			= 0x22,
	/* BLENDER-OUT -> ENC0 -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_ENC0_SPLITTER_U0FFU1FF_DISP0DISP1		= 0x23,
	/* BLENDER-OUT -> No Comp -> SPLITTER(bypass) -> u0_FF -> POST-WB */
	DATAPATH_NOCOMP_SPLITTERBYPASS_U0FF_POSTWB		= 0x04,
	/* BLENDER-OUT -> MIC -> SPLITTER(bypass) -> u0_FF -> POST-WB */
	DATAPATH_MIC_SPLITTERBYPASS_U0FF_POSTWB			= 0x0C,
	/* BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u_MERGER -> POST-WB */
	DATAPATH_DSCC_ENC0ENC1_U0FFU1FF_MERGER_POSTWB		= 0x14,
	/* BLENDER-OUT -> ENC0 -> SPLITTER(bypass) -> u0_FF -> POST-WB */
	DATAPATH_ENC0_SPLITTERBYPASS_U0FF_POSTWB		= 0x24,
	/* u0_DISPIF (mapcolor mode) */
	DATAPATH_DISP0_COLORMAP					= 0x41,
	/* u1_DISPIF (mapcolor mode) */
	DATAPATH_DISP1_COLORMAP					= 0x42,
	/* u0_DISPIF/u1_DISPIF (mapcolor mode) */
	DATAPATH_DISP0DISP1_ColorMap				= 0x43,

	/* PRE-WB & BLENDER-OUT -> No Comp -> SPLITTER(bypass) -> u0_FF -> u0_DISPIF */
	DATAPATH_PREWB_NOCOMP_SPLITTERBYPASS_U0FF_DISP0		= 0x81,
	/* PRE-WB & BLENDER-OUT -> No Comp -> SPLITTER(bypass) -> u1_FF -> u1_DISPIF */
	DATAPATH_PREWB_NOCOMP_SPLITTERBYPASS_U1FF_DISP1		= 0x82,
	/* PRE-WB & BLENDER-OUT -> No Comp -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_PREWB_NOCOMP_SPLITTER_U0FFU1FF_DISP0DISP1	= 0x83,
	/* PRE-WB & BLENDER-OUT -> MIC -> SPLITTER(bypass) -> u0_FF -> u0_DISPIF */
	DATAPATH_PREWB_MIC_SPLITTERBYPASS_U0FF_DISP0		= 0x89,
	/* PRE-WB & BLENDER-OUT -> MIC -> SPLITTER(bypass) -> u1_FF -> u1_DISPIF */
	DATAPATH_PREWB_MIC_SPLITTERBYPASS_U1FF_DISP1		= 0x8A,
	/* PRE-WB & BLENDER-OUT -> MIC -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_PREWB_MIC_SPLITTER_U0FFU1FF_DISP0DISP1		= 0x8B,
	/* PRE-WB & BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u_MERGER -> u0_DISPIF */
	DATAPATH_PREWB_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP0	= 0x91,
	/* PRE-WB & BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u_MERGER -> u1_DISPIF */
	DATAPATH_PREWB_DSCC_ENC0ENC1_U0FFU1FF_MERGER_DISP1	= 0x92,
	/* PRE-WB & BLENDER-OUT -> DSCC,ENC0/1 -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_PREWB_DSCC_ENC0ENC1_U0FFU1FF_DISP0DISP1	= 0x93,
	/* PRE-WB & BLENDER-OUT -> ENC0 -> SPLITTER(bypass) -> u0_FF -> u0_DISPIF */
	DATAPATH_PREWB_ENC0_SPLITTERBYPASS_U0FF_DISP0		= 0xA1,
	/* PRE-WB & BLENDER-OUT -> ENC0 -> SPLITTER(bypass) -> u1_FF -> u1_DISPIF */
	DATAPATH_PREWB_ENC0_SPLITTERBYPASS_U1FF_DISP1		= 0xA2,
	/* PRE-WB & BLENDER-OUT -> ENC0 -> SPLITTER(split) -> u0_FF/u1_FF -> u0_DISPIF/u1_DISPIF */
	DATAPATH_PREWB_ENC0_SPLITTER_U0FFU1FF_DISP0DISP1	= 0xA3,
	/* PRE-WB only */
	DATAPATH_PREWB_ONLY					= 0xC0,
};

enum decon_s_data_path {
	/*  BLENDER-OUT -> No Comp -> u0_FF -> u0_DISPIF */
	DECONS_DATAPATH_NOCOMP_U0FF_DISP0			= 0x01,
	/*  BLENDER-OUT -> No Comp -> u0_FF -> POST_WB */
	DECONS_DATAPATH_NOCOMP_U0FF_POSTWB			= 0x04,
	/*  BLENDER-OUT -> ENC1 -> u0_FF --> u0_DISPIF */
	DECONS_DATAPATH_ENC0_U0FF_DISP0				= 0x21,
	/*  BLENDER-OUT -> ENC1 -> u0_FF -> POST_WB */
	DECONS_DATAPATH_ENC0_U0FF_POSTWB			= 0x24,
	/*  u0_DISPIF (mapcolor mode) */
	DECONS_DATAPATH_DISP0_COLORMAP				= 0x41,
	/*  PRE_WB & BLENDER-OUT -> No Comp -> u0_FF -> u0_DISPIF */
	DECONS_DATAPATH_PREWB_NOCOMP_U0FF_DISP0			= 0x81,
	/*  PRE_WB & BLENDER-OUT -> ENC1 -> u0_FF -> u0_DISPIF */
	DECONS_DATAPATH_PREWB_ENC0_U0FF_DISP0			= 0xA1,
	/*  PRE_WB only */
	DECONS_DATAPATH_PREWB_ONLY				= 0xC0,
};

enum decon_enhance_path {
	ENHANCEPATH_ENHANCE_ALL_OFF	= 0x0,
	ENHANCEPATH_DITHER_ON		= 0x1,
	ENHANCEPATH_DPU_ON		= 0x2,
	ENHANCEPATH_DPU_DITHER_ON	= 0x3,
	ENHANCEPATH_MDNIE_ON		= 0x4,
	ENHANCEPATH_MDNIE_DITHER_ON	= 0x5,
};

enum decon_dsc_id {
	DECON_DSC_ENC_0	= 0x0,
	DECON_DSC_ENC_1,
};

/* CAL APIs list */
int decon_reg_init(u32 id, u32 dsi_idx, struct decon_param *p);
void decon_reg_init_probe(u32 id, u32 dsi_idx, struct decon_param *p);
int decon_reg_start(u32 id, struct decon_mode_info *psr);
int decon_reg_stop(u32 id, u32 dsi_idx, struct decon_mode_info *psr);
void decon_reg_release_resource(u32 id, struct decon_mode_info *psr);
void decon_reg_release_resource_instantly(u32 id);
void decon_reg_set_int(u32 id, struct decon_mode_info *psr, u32 en);
void decon_reg_set_window_control(u32 id, int win_idx, struct decon_window_regs *regs, u32 winmap_en);
void decon_reg_update_req_and_unmask(u32 id, struct decon_mode_info *psr);
int decon_reg_wait_update_done_and_mask(u32 id, struct decon_mode_info *psr, u32 timeout);
void decon_reg_set_trigger(u32 id, struct decon_mode_info *psr,
			enum decon_set_trig en);
int decon_reg_wait_for_update_timeout(u32 id, unsigned long timeout);
int decon_reg_wait_for_window_update_timeout(u32 id, u32 win_idx, unsigned long timeout);
int decon_reg_get_interrupt_and_clear(u32 id);

/* DSC related functions */
int dsc_reg_init(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);

/* CAL raw functions list */
void decon_reg_set_disp_ss_cfg(u32 id, void __iomem *disp_ss_regs, u32 dsi_idx, struct decon_mode_info *psr);
int decon_reg_reset(u32 id);
void decon_reg_set_clkgate_mode(u32 id, u32 en);
void decon_reg_set_vclk_freerun(u32 id, u32 en);
void decon_reg_set_vclk_divider(u32 id, u32 denom, u32 num);
void decon_reg_set_eclk_divider(u32 id, u32 denom, u32 num);
void decon_reg_set_sram_share(u32 id, u32 en);
void decon_reg_set_data_path(u32 id, enum decon_data_path data_path, enum decon_enhance_path enhance_path);
void decon_reg_get_data_path(u32 id, enum decon_data_path *data_path, enum decon_enhance_path *enhance_path);
void decon_reg_set_operation_mode(u32 id, enum decon_psr_mode mode);
void decon_reg_set_blender_bg_image_size(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);
void decon_reg_get_blender_bg_image_size(u32 id, u32 *p_width, u32 *p_height);
void decon_reg_set_underrun_scheme(u32 id, enum decon_hold_scheme mode);
void decon_reg_set_rgb_order(u32 id, enum decon_rgb_order order);
void decon_reg_set_frame_fifo_size(u32 id, enum decon_dsi_mode dsi_mode, u32 width, u32 height);
void decon_reg_set_splitter(u32 id, enum decon_dsi_mode dsi_mode, u32 width, u32 height);
void decon_reg_set_dispif_porch(u32 id, int dsi_idx, struct decon_lcd *lcd_info);
void decon_reg_set_dispif_size(u32 id, int dsi_idx, u32 width, u32 height);
void decon_reg_get_dispif_size(u32 id, int dsi_idx, u32 *p_width, u32 *p_height);
void decon_reg_set_comp_size(u32 id, enum decon_mic_comp_ratio cr, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);
void decon_reg_config_mic(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);
void decon_reg_update_req_global(u32 id);
void decon_reg_update_req_window(u32 id, u32 win_idx);
void decon_reg_all_win_shadow_update_req(u32 id);
void decon_reg_direct_on_off(u32 id, u32 en);
void decon_reg_per_frame_off(u32 id);
void decon_reg_configure_lcd(u32 id, enum decon_dsi_mode dsi_mode, struct decon_lcd *lcd_info);
void decon_reg_configure_trigger(u32 id, enum decon_trig_mode mode);
u32 decon_reg_get_linecnt(u32 id, int dispif_idx);
int decon_reg_wait_linecnt_is_zero_timeout(u32 id, int dsi_idx, unsigned long timeout);
u32 decon_reg_get_run_status(u32 id);
int decon_reg_wait_run_status_timeout(u32 id, unsigned long timeout);
void decon_reg_clear_int_all(u32 id);
u32 decon_get_win_channel(u32 id, u32 win_idx, enum decon_idma_type type);
void decon_reg_config_win_channel(u32 id, u32 win_idx, enum decon_idma_type type);
int decon_reg_is_win_enabled(u32 id, int win_idx);
u32 decon_reg_get_width(u32 id, int dsi_mode);
u32 decon_reg_get_height(u32 id, int dsi_mode);
void decon_reg_get_clock_ratio(struct decon_clocks *clks, struct decon_lcd *lcd_info);
void decon_reg_set_partial_update(u32 id, enum decon_dsi_mode dsi_mode,
		struct decon_lcd *lcd_info);

#endif /* ___SAMSUNG_DECON_COMMON_H__ */
