/*
 * Samsung EXYNOS FIMC-IS (Imaging Subsystem) driver
 *
 * Copyright (C) 2014 Samsung Electronics Co., Ltd.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "fimc-is-hw-api-mcscaler.h"
#include "sfr/fimc-is-sfr-mcsc-v120.h"

struct scaler_v_coef scc_v_coef[7] = {
	/* x8/8 */
	{
		{0, -4, -6, -8, -8, -8, -8, -7, -6},
		{128, 127, 124, 118, 111, 102, 92, 81, 70},
		{0, 5, 11, 19, 27, 37, 48, 59, 70},
		{0, 0, -1, -1, -2, -3, -4, -5, -6},
	},
	/* x7/8 */
	{
		{8, 4, 1, -2, -3, -5, -5, -5, -5},
		{112, 111, 109, 105, 100, 93, 86, 77, 69},
		{8, 14, 20, 27, 34, 43, 51, 60, 69},
		{0, -1, -2, -2, -3, -3, -4, -4, -5},
	},
	/* x6/8 */
	{
		{16, 12, 8, 5, 2, 0, -1, -2, -2},
		{96, 97, 96, 93, 89, 84, 79, 73, 66},
		{16, 21, 26, 32, 39, 46, 53, 59, 66},
		{0, -2, -2, -2, -2, -2, -3, -2, -2},
	},
	/* x5/8 */
	{
		{22, 18, 14, 11, 8, 6, 4, 2, 1},
		{84, 85, 84, 82, 79, 76, 72, 68, 63},
		{22, 26, 31, 36, 42, 47, 52, 58, 63},
		{0, -1, -1, -1, -1, -1, 0, 0, 1},
	},
	/* x4/8 */
	{
		{26, 22, 19, 16, 13, 10, 8, 6, 5},
		{76, 76, 75, 73, 71, 69, 66, 63, 59},
		{26, 30, 34, 38, 43, 47, 51, 55, 59},
		{0, 0, 0, 1, 1, 2, 3, 4, 5},
	},
	/* x3/8 */
	{
		{29, 26, 23, 20, 17, 15, 12, 10, 8},
		{70, 68, 67, 66, 65, 63, 61, 58, 56},
		{29, 32, 36, 39, 43, 46, 50, 53, 56},
		{0, 2, 2, 3, 3, 4, 5, 7, 8},
	},
	/* x2/8 */
	{
		{32, 28, 25, 22, 19, 17, 15, 13, 11},
		{64, 63, 62, 62, 61, 59, 58, 55, 53},
		{32, 34, 37, 40, 43, 46, 48, 51, 53},
		{0, 3, 4, 4, 5, 6, 7, 9, 11},
	}
};

struct scaler_h_coef scc_h_coef[7] = {
	/* x8/8 */
	{
		{0, -1, -1, -1, -1, -1, -2, -1, -1},
		{0, 2, 4, 5, 6, 6, 7, 6, 6},
		{0, -6, -12, -15, -18, -20, -21, -20, -20},
		{128, 127, 125, 120, 114, 107, 99, 89, 79},
		{0, 7, 16, 25, 35, 46, 57, 68, 79},
		{0, -2, -5, -8, -10, -13, -16, -18, -20},
		{0, 1, 1, 2, 3, 4, 5, 5, 6},
		{0, 0, 0, 0, -1, -1, -1, -1, -1},
	},
	/* x7/8 */
	{
		{3, 2, 2, 1, 1, 1, 0, 0, 0},
		{-8, -6, -4, -2, -1, 1, 2, 3, 3},
		{14, 7, 1, -3, -7, -11, -13, -15, -16},
		{111, 112, 110, 106, 103, 97, 91, 85, 77},
		{13, 21, 28, 36, 44, 53, 61, 69, 77},
		{-8, -10, -12, -13, -15, -16, -16, -17, -16},
		{3, 3, 4, 4, 4, 4, 4, 4, 3},
		{0, -1, -1, -1, -1, -1, -1, -1, 0},
	},
	/* x6/8 */
	{
		{2, 2, 2, 2, 2, 2, 2, 1, 1},
		{-11, -10, -9, -8, -7, -5, -4, -3, -2},
		{25, 19, 14, 10, 5, 1, -2, -5, -7},
		{96, 96, 94, 92, 90, 86, 82, 77, 72},
		{25, 31, 37, 43, 49, 55, 61, 67, 72},
		{-11, -12, -12, -12, -12, -12, -11, -9, -7},
		{2, 2, 2, 1, 1, 0, -1, -1, -2},
		{0, 0, 0, 0, 0, 1, 1, 1, 1},
	},
	/* x5/8 */
	{
		{-1, -1, 0, 0, 0, 0, 1, 1, 1},
		{-8, -8, -8, -8, -8, -7, -7, -6, -6},
		{33, 28, 24, 20, 16, 13, 10, 6, 4},
		{80, 80, 79, 78, 76, 74, 71, 68, 65},
		{33, 37, 41, 46, 50, 54, 58, 62, 65},
		{-8, -7, -7, -6, -4, -3, -1, 1, 4},
		{-1, -2, -2, -3, -3, -4, -5, -5, -6},
		{0, 1, 1, 1, 1, 1, 1, 1, 1},
	},
	/* x4/8 */
	{
		{-3, -3, -2, -2, -2, -2, -1, -1, -1},
		{0, -1, -2, -3, -3, -3, -4, -4, -4},
		{35, 32, 29, 27, 24, 21, 19, 16, 14},
		{64, 64, 63, 63, 61, 60, 59, 57, 55},
		{35, 38, 41, 43, 46, 49, 51, 53, 55},
		{0, 1, 2, 4, 6, 7, 9, 12, 14},
		{-3, -3, -3, -4, -4, -4, -4, -4, -4},
		{0, 0, 0, 0, 0, 0, -1, -1, -1},
	},
	/* x3/8 */
	{
		{-1, -1, -1, -1, -1, -1, -1, -1, -1},
		{8, 7, 6, 5, 4, 3, 2, 2, 1},
		{33, 31, 30, 28, 26, 24, 23, 21, 19},
		{48, 49, 49, 48, 48, 47, 47, 45, 45},
		{33, 35, 36, 38, 39, 41, 42, 43, 45},
		{8, 9, 10, 12, 13, 15, 16, 18, 19},
		{-1, -1, -1, -1, 0, 0, 0, 1, 1},
		{0, -1, -1, -1, -1, -1, -1, -1, -1},
	},
	/* x2/8 */
	{
		{2, 2, 2, 2, 1, 1, 1, 1, 1},
		{13, 12, 11, 10, 10, 9, 8, 7, 6},
		{30, 29, 28, 26, 26, 24, 24, 22, 21},
		{38, 38, 38, 38, 37, 37, 37, 36, 36},
		{30, 30, 31, 32, 33, 34, 34, 35, 36},
		{13, 14, 15, 16, 17, 18, 19, 20, 21},
		{2, 3, 3, 4, 4, 5, 5, 6, 6},
		{0, 0, 0, 0, 0, 0, 0, 1, 1},
	}
};

void fimc_is_scaler_start(void __iomem *base_addr)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0], &mcsc_fields[MCSC_F_SCALER_ENABLE_0], 1);
}

void fimc_is_scaler_stop(void __iomem *base_addr)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_GCTRL_0], &mcsc_fields[MCSC_F_SCALER_ENABLE_0], 0);
}

u32 fimc_is_scaler_sw_reset(void __iomem *base_addr)
{
	u32 reset_count = 0;

	/* request scaler reset */
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SC_RESET_CTRL_GLOBAL], &mcsc_fields[MCSC_F_SW_RESET_GLOBAL], 1);

	/* wait reset complete */
	do {
		reset_count++;
		if (reset_count > 10000)
			return reset_count;
	} while (fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_RESET_STATUS], &mcsc_fields[MCSC_F_SW_RESET_GLOBAL_STATUS]) != 0);

	return 0;
}

void fimc_is_scaler_clear_intr_all(void __iomem *base_addr)
{
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<13);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<12);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<11);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<10);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<9);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<8);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<7);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<6);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<5);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<4);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<3);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<1);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], 1<<0);
}

void fimc_is_scaler_disable_intr(void __iomem *base_addr)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_SCALER_OVERFLOW_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_OUT_STALL_BLOCKING_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_INPUT_VERTICAL_UNF_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_INPUT_VERTICAL_OVF_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_UNF_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_INPUT_HORIZONTAL_OVF_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_INPUT_PROTOCOL_ERR_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_SHADOW_TRIGGER_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_SHADOW_HW_TRIGGER_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_CORE_FINISH_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_WDMA_FINISH_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_FRAME_START_INT_0_MASK], 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], &mcsc_fields[MCSC_F_FRAME_END_INT_0_MASK], 1);
}

void fimc_is_scaler_mask_intr(void __iomem *base_addr, u32 intr_mask)
{
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0], intr_mask);
}

void fimc_is_scaler_set_stop_req_post_en_ctrl(void __iomem *base_addr, u32 value)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_STOP_REQ_POST_EN_CTRL_0], &mcsc_fields[MCSC_F_STOP_REQ_POST_EN_CTRL_TYPE_0], value);
}

void fimc_is_scaler_get_input_status(void __iomem *base_addr, u32 *hl, u32 *vl)
{
	*hl = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INPUT_STATUS_0], &mcsc_fields[MCSC_F_CUR_HORIZONTAL_CNT_0]);
	*vl = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_SCALER_INPUT_STATUS_0], &mcsc_fields[MCSC_F_CUR_VERTICAL_CNT_0]);
}

void fimc_is_scaler_set_poly_scaler_enable(void __iomem *base_addr, u32 enable)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_ENABLE], enable);
}

void fimc_is_scaler_set_poly_img_size(void __iomem *base_addr, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_IMG_HSIZE], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_IMG_VSIZE], height);
}

void fimc_is_scaler_get_poly_img_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_IMG_HSIZE]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_IMG_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_IMG_VSIZE]);
}

void fimc_is_scaler_set_poly_src_size(void __iomem *base_addr, u32 pos_x, u32 pos_y, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_POS], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HPOS], pos_x);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_POS], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VPOS], pos_y);

	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HSIZE], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VSIZE], height);
}

void fimc_is_scaler_get_poly_src_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_HSIZE]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_SRC_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_SRC_VSIZE]);
}

void fimc_is_scaler_set_poly_dst_size(void __iomem *base_addr, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_HSIZE], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_VSIZE], height);
}

void fimc_is_scaler_get_poly_dst_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_HSIZE]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_DST_SIZE], &mcsc_fields[MCSC_F_POLY_SC0_P0_DST_VSIZE]);
}

void fimc_is_scaler_set_post_scaler_enable(void __iomem *base_addr, u32 enable)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_ENABLE], enable);
}

void fimc_is_scaler_set_post_img_size(void __iomem *base_addr, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_HSIZE], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_VSIZE], height);
}

void fimc_is_scaler_get_post_img_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_HSIZE]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_IMG_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_IMG_VSIZE]);
}

void fimc_is_scaler_set_post_dst_size(void __iomem *base_addr, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_HSIZE], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_VSIZE], height);
}

void fimc_is_scaler_get_post_dst_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_HSIZE]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_DST_SIZE], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_DST_VSIZE]);
}

void fimc_is_scaler_set_poly_scaling_ratio(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_H_RATIO], &mcsc_fields[MCSC_F_POLY_SC0_P0_H_RATIO], hratio);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_V_RATIO], &mcsc_fields[MCSC_F_POLY_SC0_P0_V_RATIO], vratio);
}

void fimc_is_scaler_set_h_init_phase_offset(void __iomem *base_addr, u32 h_offset)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_H_INIT_PHASE_OFFSET], &mcsc_fields[MCSC_F_POLY_SC0_P0_H_INIT_PHASE_OFFSET], h_offset);
}

void fimc_is_scaler_set_v_init_phase_offset(void __iomem *base_addr, u32 v_offset)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_V_INIT_PHASE_OFFSET], &mcsc_fields[MCSC_F_POLY_SC0_P0_V_INIT_PHASE_OFFSET], v_offset);
}

void fimc_is_scaler_set_post_scaling_ratio(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_H_RATIO], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_H_RATIO], hratio);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_POST_SC_V_RATIO], &mcsc_fields[MCSC_F_POST_CHAIN0_POST_SC_V_RATIO], vratio);
}

void fimc_is_scaler_set_poly_scaler_coef(void __iomem *base_addr, u32 hratio, u32 vratio)
{
	u32 h_coef;
	u32 v_coef;

	/* adjust H coef */
	if (hratio <= RATIO_X8_8)
		h_coef = 0;
	else if (hratio > RATIO_X8_8 && hratio <= RATIO_X7_8)
		h_coef = 1;
	else if (hratio > RATIO_X7_8 && hratio <= RATIO_X6_8)
		h_coef = 2;
	else if (hratio > RATIO_X6_8 && hratio <= RATIO_X5_8)
		h_coef = 3;
	else if (hratio > RATIO_X5_8 && hratio <= RATIO_X4_8)
		h_coef = 4;
	else if (hratio > RATIO_X4_8 && hratio <= RATIO_X3_8)
		h_coef = 5;
	else if (hratio > RATIO_X3_8 && hratio <= RATIO_X2_8)
		h_coef = 6;
	else
		return;

	/* adjust V coef */
	if (vratio <= RATIO_X8_8)
		v_coef = 0;
	else if (vratio > RATIO_X8_8 && vratio <= RATIO_X7_8)
		v_coef = 1;
	else if (vratio > RATIO_X7_8 && vratio <= RATIO_X6_8)
		v_coef = 2;
	else if (vratio > RATIO_X6_8 && vratio <= RATIO_X5_8)
		v_coef = 3;
	else if (vratio > RATIO_X5_8 && vratio <= RATIO_X4_8)
		v_coef = 4;
	else if (vratio > RATIO_X4_8 && vratio <= RATIO_X3_8)
		v_coef = 5;
	else if (vratio > RATIO_X3_8 && vratio <= RATIO_X2_8)
		v_coef = 6;
	else
		return;

	/* set coefficient for horizontal/vertical scaling filter */
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_COEFF_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_H_COEFF_SEL], h_coef);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POLY_SC0_P0_COEFF_CTRL], &mcsc_fields[MCSC_F_POLY_SC0_P0_V_COEFF_SEL], v_coef);
}

void fimc_is_scaler_set_dma_size(void __iomem *base_addr, u32 width, u32 height)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_WIDTH], &mcsc_fields[MCSC_F_WDMA0_WIDTH], width);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_HEIGHT], &mcsc_fields[MCSC_F_WDMA0_HEIGHT], height);
}

void fimc_is_scaler_get_dma_size(void __iomem *base_addr, u32 *width, u32 *height)
{
	*width = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_WIDTH], &mcsc_fields[MCSC_F_WDMA0_WIDTH]);
	*height = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_HEIGHT], &mcsc_fields[MCSC_F_WDMA0_HEIGHT]);
}

void fimc_is_scaler_set_stride_size(void __iomem *base_addr, u32 y_stride, u32 uv_stride)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE], &mcsc_fields[MCSC_F_WDMA0_Y_STRIDE], y_stride);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE], &mcsc_fields[MCSC_F_WDMA0_C_STRIDE], uv_stride);
}

void fimc_is_scaler_get_stride_size(void __iomem *base_addr, u32 *y_stride, u32 *uv_stride)
{
	*y_stride = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE], &mcsc_fields[MCSC_F_WDMA0_Y_STRIDE]);
	*uv_stride = fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_STRIDE], &mcsc_fields[MCSC_F_WDMA0_C_STRIDE]);
}

void fimc_is_scaler_set_flip_mode(void __iomem *base_addr, u32 flip)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_FLIP_CONTROL], &mcsc_fields[MCSC_F_WDMA0_FLIP_CONTROL], flip);
}

void fimc_is_scaler_set_swap_mode(void __iomem *base_addr, u32 swap)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_SWAP_TABLE], &mcsc_fields[MCSC_F_WDMA0_SWAP_TABLE], swap);
}

void fimc_is_scaler_set_direct_out_path(void __iomem *base_addr, u32 direct_format, bool direct_out_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_OTF_OUT_FORMAT], &mcsc_fields[MCSC_F_POST_CHAIN0_OTF_OUT_FORMAT], direct_format);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_OTF_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_OTF_OUT_ENABLE], (u32)direct_out_en);
}

void fimc_is_scaler_set_dma_out_path(void __iomem *base_addr, u32 dma_out_format, bool dma_out_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE], (u32)dma_out_en);
	if (dma_out_en == 1)
		fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_DATA_FORMAT], &mcsc_fields[MCSC_F_WDMA0_DATA_FORMAT], dma_out_format);
}

void fimc_is_scaler_set_dma_enable(void __iomem *base_addr, bool dma_out_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE], (u32)dma_out_en);
}

void fimc_is_scaler_set_dma_out_frame_seq(void __iomem *base_addr, u32 frame_seq)
{
	if (!(frame_seq <= 0xffffffff))
		return;

	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN1], (frame_seq & 0x01) >> 0);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN2], (frame_seq & 0x02) >> 1);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN3], (frame_seq & 0x04) >> 2);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN4], (frame_seq & 0x08) >> 3);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN5], (frame_seq & 0x10) >> 4);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN6], (frame_seq & 0x20) >> 5);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN7], (frame_seq & 0x40) >> 6);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR_EN], &mcsc_fields[MCSC_F_WDMA0_BASE_ADDR_EN8], (frame_seq & 0x80) >> 7);
}

void fimc_is_scaler_set_dma_out_addr(void __iomem *base_addr, u32 y_addr, u32 cb_addr, u32 cr_addr, int buf_index)
{
	/* DMA Y, Cb and Cr address setting for matched index regs */
	switch (buf_index) {
	case 0:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR01], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR11], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR21], cr_addr);
		break;
	case 1:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR02], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR12], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR22], cr_addr);
		break;
	case 2:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR03], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR13], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR23], cr_addr);
		break;
	case 3:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR04], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR14], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR24], cr_addr);
		break;
	case 4:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR05], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR15], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR25], cr_addr);
		break;
	case 5:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR06], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR16], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR26], cr_addr);
		break;
	case 6:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR07], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR17], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR27], cr_addr);
		break;
	case 7:
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR08], y_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR18], cb_addr);
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR28], cr_addr);
		break;
	default:
		break;
	}
}

void fimc_is_scaler_clear_dma_out_addr(void __iomem *base_addr)
{
	/* DMA Y address clear */
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR01], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR02], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR03], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR04], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR05], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR06], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR07], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR08], 0x0);

	/* DMA CB address clear */
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR11], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR12], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR13], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR14], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR15], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR16], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR17], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR18], 0x0);

	/* DMA CR address clear */
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR21], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR22], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR23], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR24], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR25], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR26], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR27], 0x0);
	fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_WDMA0_BASE_ADDR28], 0x0);
}

void fimc_is_scaler_set_420_conversion(void __iomem *base_addr, u32 conv420_weight, bool conv420_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_CONV420_WEIGHT], &mcsc_fields[MCSC_F_POST_CHAIN0_CONV420_WEIGHT], conv420_weight);
	if (conv420_en == true)
		fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_CONV420_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_CONV420_ENABLE], 1);
	else
		fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_CONV420_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_CONV420_ENABLE], 0);
}

void fimc_is_scaler_set_dma_out_img_fmt(void __iomem *base_addr, u32 dma_foramt)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_DATA_FORMAT], &mcsc_fields[MCSC_F_WDMA0_DATA_FORMAT], dma_foramt);
}

int fimc_is_scaler_get_dma_out_img_fmt(void __iomem *base_addr)
{
	return fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_WDMA0_DATA_FORMAT], &mcsc_fields[MCSC_F_WDMA0_DATA_FORMAT]);
}

void fimc_is_scaler_set_dither(void __iomem *base_addr, bool dither_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_DITHER_0_CTRL], &mcsc_fields[MCSC_F_DITHER_0_ON], dither_en);
}

/*
void fimc_is_scaler_set_dma_mo8_sel(void __iomem *base_addr, u32 dma_mo8_flag)
{
	fimc_is_hw_set_field(base_addr, fimc_is_scc_field.MO_8_SEL, dma_mo8_flag);
}
*/

void fimc_is_scaler_set_bchs_enable(void __iomem *base_addr, bool bchs_en)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_ENABLE], bchs_en);
}

/* brightness/contrast control */
void fimc_is_scaler_set_b_c(void __iomem *base_addr, u32 y_offset, u32 y_gain)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_BC], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_YOFFSET], y_offset);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_BC], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_YGAIN], y_gain);
}

/* hue/saturation control */
void fimc_is_scaler_set_h_s(void __iomem *base_addr, u32 c_gain00, u32 c_gain01, u32 c_gain10, u32 c_gain11)
{
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS1], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_00], c_gain00);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS1], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_01], c_gain01);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS2], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_10], c_gain10);
	fimc_is_hw_set_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_BCHS_HS2], &mcsc_fields[MCSC_F_POST_CHAIN0_BCHS_C_GAIN_11], c_gain11);
}

void fimc_is_scaler_clear_intr_src(void __iomem *base_addr, u32 status)
{
	if (status & (1 << INTR_MC_SCALER_OVERFLOW))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 13);

	if (status & (1 << INTR_MC_SCALER_OUTSTALL))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 12);

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_UNF))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 11);

	if (status & (1 << INTR_MC_SCALER_INPUT_VERTICAL_OVF))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 10);

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_UNF))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 9);

	if (status & (1 << INTR_MC_SCALER_INPUT_HORIZONTAL_OVF))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 8);

	if (status & (1 << INTR_MC_SCALER_INPUT_PROTOCOL_ERR))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 7);

	if (status & (1 << INTR_MC_SCALER_SHADOW_TRIGGER))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 6);

	if (status & (1 << INTR_MC_SCALER_SHADOW_HW_TRIGGER))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 5);

	if (status & (1 << INTR_MC_SCALER_CORE_FINISH))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 4);

	if (status & (1 << INTR_MC_SCALER_WDMA_FINISH))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 3);

	if (status & (1 << INTR_MC_SCALER_FRAME_START))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 1);

	if (status & (1 << INTR_MC_SCALER_FRAME_END))
		fimc_is_hw_set_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0], (u32)1 << 0);
}

u32 fimc_is_scaler_get_otf_out_enable(void __iomem *base_addr)
{
	return fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_OTF_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_OTF_OUT_ENABLE]);
}

u32 fimc_is_scaler_get_dma_enable(void __iomem *base_addr)
{
	return fimc_is_hw_get_field(base_addr, &mcsc_regs[MCSC_R_POST_CHAIN0_DMA_OUT_CTRL], &mcsc_fields[MCSC_F_POST_CHAIN0_DMA_OUT_ENABLE]);
}

u32 fimc_is_scaler_get_intr_mask(void __iomem *base_addr)
{
	return fimc_is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_MASK_0]);
}

u32 fimc_is_scaler_get_intr_status(void __iomem *base_addr)
{
	return fimc_is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_INTERRUPT_0]);
}

u32 fimc_is_scaler_get_version(void __iomem *base_addr)
{
	return fimc_is_hw_get_reg(base_addr, &mcsc_regs[MCSC_R_SCALER_VERSION]);
}
