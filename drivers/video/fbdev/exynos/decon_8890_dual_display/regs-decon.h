/*
 * drivers/video/exynos_8890/decon/regs-decon.h
 *
 * Register definition file for Samsung DECON driver
 *
 * Copyright (c) 2014 Samsung Electronics
 * Sewoon Park <seuni.park@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/


#ifndef _REGS_DISP_SS_H
#define _REGS_DISP_SS_H

#define DISP_CFG					0x0000
#define DISP_CFG_DP_PATH_CFG0_EN			(0x1 << 20)
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF_MASK(_v)	(0x3 << (9 + (_v) * 4))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF0(_v)		(0x0 << (9 + (_v) * 4))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF1(_v)		(0x1 << (9 + (_v) * 4))
#define DISP_CFG_DSIM_PATH_CFG1_DISP_IF2(_v)		(0x2 << (9 + (_v) * 4))
#define DISP_CFG_DSIM_PATH_CFG0_EN(_v)			(0x1 << (8 + (_v) * 4))
#define DISP_CFG_DSIM_PATH_CFG0_MASK(_v)		(0x1 << (8 + (_v) * 4))
#define DISP_CFG_SYNC_MODE1_TE(_v)			((_v) << 2)
#define DISP_CFG_SYNC_MODE1_MASK			(0x3 << 2)
#define DISP_CFG_SYNC_MODE0_TE(_v)			((_v) << 0)
#define DISP_CFG_SYNC_MODE0_MASK			(0x3 << 0)

#define DISP_CFG_SYNC_MODE1_TE_F			(0x0 << 2)
#define DISP_CFG_SYNC_MODE1_TE_S			(0x1 << 2)
#define DISP_CFG_SYNC_MODE1_TE_T			(0x2 << 2)
#define DISP_CFG_SYNC_MODE1_MASK			(0x3 << 2)
#define DISP_CFG_SYNC_MODE0_TE_F			(0x0 << 0)
#define DISP_CFG_SYNC_MODE0_TE_S			(0x1 << 0)
#define DISP_CFG_SYNC_MODE0_TE_T			(0x2 << 0)
#define DISP_CFG_SYNC_MODE0_MASK			(0x3 << 0)

#endif /* _REGS_DISP_SS_H */


#ifndef _REGS_DECON_H
#define _REGS_DECON_H

/*
 *	IP			start_offset	end_offset
 *=================================================
 *	DECON_F			0x0000		0x007f
 *	DECON_F/DISPIF0,1	0x0080		0x00F4
 *	DECON_F			0x00F8		0x0FFF
 *-------------------------------------------------
 *	MIC			0x1000		0x1FFF
 *-------------------------------------------------
 *	DSC_ENC0		0x2000		0x2FFF
 *	DSC_ENC1		0x3000		0x3FFF
 *-------------------------------------------------
 *	DECON_S/DISPIF2		0x5000		0x5FFF
 *	DECON_T/DISPIF3		0x6000		0x6FFF
 *-------------------------------------------------
 *-------------------------------------------------
 *	SHD_DECON_F		0x7000		0x7FFF
 *-------------------------------------------------
 *	SHD_MIC			0x8000		0x8FFF
 *-------------------------------------------------
 *	SHD_DSC_ENC0		0x9000		0x9FFF
 *	SHD_DSC_ENC1		0xA000		0xAFFF
 *-------------------------------------------------
 *	SHD_DISPIF2		0xB000		0xBFFF
 *	SHD_DISPIF3		0xC000		0xCFFF
 *-------------------------------------------------
 *	mDNIe			0xD000		0xDFFF
 *-------------------------------------------------
 *	DPU			0xE000		0xFFFF
*/


/*
 * DECON_F registers
 * ->
 * updated by SHADOW_REG_UPDATE_REQ[31] : SHADOW_REG_UPDATE_REQ
 *	(0x0000~0x011C, 0x0230~0x209C, Dither/MIC/DSC)
*/

#define GLOBAL_CONTROL					0x0000
#define GLOBAL_CONTROL_SRESET				(1 << 28)
#define GLOBAL_CONTROL_AUTOMATIC_MAPCOLOR_ENTER_EN_F	(1 << 12)
#define GLOBAL_CONTROL_OPERATION_MODE_F			(1 << 8)
#define GLOBAL_CONTROL_OPERATION_MODE_RGBIF_F		(0 << 8)
#define GLOBAL_CONTROL_OPERATION_MODE_I80IF_F		(1 << 8)
#define GLOBAL_CONTROL_URGENT_STATUS			(1 << 6)
#define GLOBAL_CONTROL_IDLE_STATUS			(1 << 5)
#define GLOBAL_CONTROL_RUN_STATUS			(1 << 4)
#define GLOBAL_CONTROL_DECON_EN				(1 << 1)
#define GLOBAL_CONTROL_DECON_EN_F			(1 << 0)

#define AUTOMATIC_MAPCOLOR_PERIOD_CONTROL		0x0004
#define AUTOMATIC_MAPCOLOR_PERIOD_F(_v)			((_v) << 0)

#define RESOURCE_OCCUPANCY_INFO_0			0x0010
#define RESOURCE_OCCUPANCY_INFO_1			0x0014
#define RESOURCE_SEL_0					0x0018/* DECon_F only */
#define RESOURCE_SEL_1					0x001C/* DECon_F only */
#define RESOURCE_CONFLICTION_INDUCER			0x0020

#define SRAM_SHARE_ENABLE				0x0030/* DECon_F only */
#define SRAM_SHARE_ENABLE_DSC_F				(1 << 4)
#define SRAM_SHARE_ENABLE_F				(1 << 0)

#define INTERRUPT_ENABLE				0x0040
#define INTERRUPT_DPU1_INT_EN				(1 << 29)
#define INTERRUPT_DPU0_INT_EN				(1 << 28)
#define INTERRUPT_DISPIF_VSTATUS_INT_EN			(1 << 24)
#define INTERRUPT_DISPIF_VSTATUS_VBP			(0 << 20)
#define INTERRUPT_DISPIF_VSTATUS_VSA			(1 << 20)
#define INTERRUPT_DISPIF_VSTATUS_VACTIVE		(2 << 20)
#define INTERRUPT_DISPIF_VSTATUS_VFP			(3 << 20)
#define INTERRUPT_RESOURCE_CONFLICT_INT_EN		(1 << 18)
#define INTERRUPT_EVEN_FRAME_START_INT_EN		(1 << 17)
#define INTERRUPT_ODD_FRAME_START_INT_EN		(1 << 16)
#define INTERRUPT_FRAME_DONE_INT_EN			(1 << 12)
#define INTERRUPT_FIFO_LEVEL_INT_EN			(1 << 8)
#define INTERRUPT_INT_EN				(1 << 0)

#define UNDER_RUN_CYCLE_THRESHOLD			0x0044
#define INTERRUPT_PENDING				0x0048

#define SHADOW_REG_UPDATE_REQ				0x0060
#define SHADOW_REG_UPDATE_REQ_GLOBAL			(1 << 31)
#define SHADOW_REG_UPDATE_REQ_DPU			(1 << 28)
#define SHADOW_REG_UPDATE_REQ_MDNIE			(1 << 24)
#define SHADOW_REG_UPDATE_REQ_WIN(_win)			(1 << (_win))
#define SHADOW_REG_UPDATE_REQ_FOR_DECON_F		(0xff)
#define SHADOW_REG_UPDATE_REQ_FOR_DECON_T		(0xf)

#define HW_SW_TRIG_CONTROL				0x0070
#define HW_SW_TRIG_CONTROL_TRIG_AUTO_MASK_TRIG		(1 << 12)
/* 1 : s/w trigger */
#define HW_SW_TRIG_CONTROL_SW_TRIG			(1 << 8)
/* 0 : unmask, 1 : mask */
#define HW_SW_TRIG_CONTROL_HW_TRIG_MASK			(1 << 5)
/* 0 : s/w trigger, 1 : h/w trigger */
#define HW_SW_TRIG_CONTROL_HW_TRIG_EN			(1 << 4)

#define DISPIF0_DISPIF1_CONTROL				0x0080/* DECon_F only */
#define DISPIF0_DISPIF1_CONTROL_FREE_RUN_EN		(1 << 4)
#define DISPIF0_DISPIF1_CONTROL_UNDERRUN_SCHEME_F(_v)	((_v) << 12)
#define DISPIF0_DISPIF1_CONTROL_UNDERRUN_SCHEME_MASK	(0x3 << 12)
#define DISPIF0_DISPIF1_CONTROL_OUT_RGB_ORDER_F(_v)	((_v) << 8)
#define DISPIF0_DISPIF1_CONTROL_OUT_RGB_ORDER_MASK	(0x7 << 8)

#define DISPIF0_MAPCOLOR				0x0084/* DECon_F only */
#define DISPIF1_MAPCOLOR				0x0088/* DECon_F only */

#define DISPIF_LINE_COUNT				0x008C/* DECon_F only */
#define DISPIF_LINE_COUNT_DISPIF1_SHIFT			16
#define DISPIF_LINE_COUNT_DISPIF1_MASK			(0xffff << 16)
#define DISPIF_LINE_COUNT_DISPIF0_SHIFT			0
#define DISPIF_LINE_COUNT_DISPIF0_MASK			(0xffff << 0)

#define DISPIF0_TIMING_CONTROL_0			0x0090/* DECon_F only */
#define DISPIF_TIMING_VBPD_F(_v)			((_v) << 16)
#define DISPIF_TIMING_VFPD_F(_v)			((_v) << 0)

#define DISPIF0_TIMING_CONTROL_1			0x0094/* DECon_F only */
#define DISPIF_TIMING_VSPD_F(_v)			((_v) << 0)

#define DISPIF0_TIMING_CONTROL_2			0x0098/* DECon_F only */
#define DISPIF_TIMING_HBPD_F(_v)			((_v) << 16)
#define DISPIF_TIMING_HFPD_F(_v)			((_v) << 0)

#define DISPIF0_TIMING_CONTROL_3			0x009C/* DECon_F only */
#define DISPIF_TIMING_HSPD_F(_v)			((_v) << 0)

#define DISPIF0_SIZE_CONTROL_0				0x00A0/* DECon_F only */
#define DISPIF_HEIGHT_F(_v)				((_v) << 16)
#define DISPIF_HEIGHT_MASK				(0x3fff << 16)
#define DISPIF_HEIGHT_GET(_v)				(((_v) >> 16) & 0x3fff)
#define DISPIF_WIDTH_F(_v)				((_v) << 0)
#define DISPIF_WIDTH_MASK				(0x3fff << 0)
#define DISPIF_WIDTH_GET(_v)				(((_v) >> 0) & 0x3fff)

/* All of DECON */
/* DISP INTERFACE OFFSET : IF0,1 = 0x0, IF2 = 0x5000, IF3 = 0x6000 */
#define IF_OFFSET(_x)			((((_x) < 2) ? 0 : 0x1000) * ((_x) + 3))
/* For TIMING VALUE : IF0,2,3 = 0x0, IF1 = 0x30 */
#define IF1_OFFSET(_x)			(((_x) == 1) ? 0x30 : 0x0)
#define DISPIF_SIZE_CONTROL_0(_if_idx) \
	(0x00A0 + IF_OFFSET(_if_idx) + IF1_OFFSET(_if_idx))
#define DISPIF_WIDTH_START_POS				(0)
#define DISPIF_HEIGHT_START_POS				(16)

#define DISPIF0_SIZE_CONTROL_1				0x00A4/* DECon_F only */
#define DISPIF0_URGENT_CONTROL				0x00A8/* DECon_F only */
#define DISPIF1_TIMING_CONTROL_0			0x00C0/* DECon_F only */
#define DISPIF1_TIMING_CONTROL_1			0x00C4/* DECon_F only */
#define DISPIF1_TIMING_CONTROL_2			0x00C8/* DECon_F only */
#define DISPIF1_TIMING_CONTROL_3			0x00CC/* DECon_F only */
#define DISPIF1_SIZE_CONTROL_0				0x00D0/* DECon_F only */
#define DISPIF1_SIZE_CONTROL_1				0x00D4/* DECon_F only */
#define DISPIF1_URGENT_CONTROL				0x00D8/* DECon_F only */

#define CLOCK_CONTROL_0					0x00F0
/* [16] 0: QACTIVE is dynamically changed by DECON h/w,
 * 1: QACTIVE is stuck to 1'b1
*/
#define CLOCK_CONTROL_0_F_MASK				(0xF31000FF)
#define CLOCK_CONTROL_0_S_MASK				(0x1300FF0F)
#define CLOCK_CONTROL_0_T_MASK				(0x1300FF0F)

#define VCLK_DIVIDER_CONTROL				0x00F4/* DECon_F only */
#define ECLK_DIVIDER_CONTROL				0x00F8

#define BLENDER_BG_IMAGE_SIZE_0				0x0110
#define BLENDER_BG_HEIGHT_F(_v)				((_v) << 16)
#define BLENDER_BG_HEIGHT_MASK				(0x3fff << 16)
#define BLENDER_BG_HEIGHT_GET(_v)			(((_v) >> 16) & 0x3fff)
#define BLENDER_BG_WIDTH_F(_v)				((_v) << 0)
#define BLENDER_BG_WIDTH_MASK				(0x3fff << 0)
#define BLENDER_BG_WIDTH_GET(_v)			(((_v) >> 0) & 0x3fff)

#define BLENDER_BG_IMAGE_SIZE_1				0x0114
#define BLENDER_BG_IMAGE_COLOR				0x0118

#define LRMERGER_MODE_CONTROL				0x011C

#define WIN_CONTROL(_win)			(0x0130 + ((_win) * 0x20))
#define WIN_CONTROL_ALPHA1_F(_v)		(((_v) & 0xFF) << 24)
#define WIN_CONTROL_ALPHA1_MASK			(0xFF << 24)
#define WIN_CONTROL_ALPHA0_F(_v)		(((_v) & 0xFF) << 16)
#define WIN_CONTROL_ALPHA0_MASK			(0xFF << 16)
#define WIN_CONTROL_CHMAP_F(_v)			(((_v) & 0x7) << 12)
#define WIN_CONTROL_CHMAP_MASK			(0x7 << 12)
#define WIN_CONTROL_FUNC_F(_v)			(((_v) & 0xF) << 8)
#define WIN_CONTROL_FUNC_MASK			(0xF << 8)
#define WIN_CONTROL_ALPHA_MUL_F			(1 << 6)
#define WIN_CONTROL_ALPHA_SEL_F(_v)		(((_v) & 0x7) << 4)
#define WIN_CONTROL_ALPHA_SEL_MASK		(0x7 << 4)
#define WIN_CONTROL_MAPCOLOR_EN_F		(1 << 1)
#define WIN_CONTROL_MAPCOLOR_EN_MASK		(1 << 1)
#define WIN_CONTROL_EN_F			(1 << 0)

#define WIN_START_POSITION(_win)		(0x0134 + ((_win) * 0x20))
#define WIN_STRPTR_Y_F(_v)			(((_v) & 0x1FFF) << 16)
#define WIN_STRPTR_X_F(_v)			(((_v) & 0x1FFF) << 0)

#define WIN_END_POSITION(_win)			(0x0138 + ((_win) * 0x20))
#define WIN_ENDPTR_Y_F(_v)			(((_v) & 0x1FFF) << 16)
#define WIN_ENDPTR_X_F(_v)			(((_v) & 0x1FFF) << 0)

#define WIN_COLORMAP(_win)			(0x013C + ((_win) * 0x20))
#define WIN_COLORMAP_MAPCOLOR_F(_v)		((_v) << 0)
#define WIN_COLORMAP_MAPCOLOR_MASK		(0xffffff << 0)

#define WIN_START_TIME_CONTROL(_win)		(0x0140 + ((_win) * 0x20))

#define WIN_PIXEL_COUNT(_win)			(0x0144 + ((_win) * 0x20))

#define DATA_PATH_CONTROL				0x0230
#define DATA_PATH_CONTROL_ENHANCE_SHIFT			8
#define DATA_PATH_CONTROL_ENHANCE_MASK			(0x7 << 8)
#define DATA_PATH_CONTROL_ENHANCE(_v)			((_v) << 8)
#define DATA_PATH_CONTROL_ENHANCE_GET(_v)		(((_v) >> 8) & 0x7)
#define DATA_PATH_CONTROL_PATH_SHIFT			0
#define DATA_PATH_CONTROL_PATH_MASK			(0xFF << 0)
#define DATA_PATH_CONTROL_PATH(_v)			((_v) << 0)
#define DATA_PATH_CONTROL_PATH_GET(_v)			(((_v) >> 0) & 0xFF)

#define SPLITTER_CONTROL_0				0x0240/* DECon_F only */

#define SPLITTER_SIZE_CONTROL_0				0x0244/* DECon_F only */
#define SPLITTER_HEIGHT_F(_v)				((_v) << 16)
#define SPLITTER_HEIGHT_MASK				(0x3fff << 16)
#define SPLITTER_HEIGHT_GET(_v)				(((_v) >> 16) & 0x3fff)
#define SPLITTER_WIDTH_F(_v)				((_v) << 0)
#define SPLITTER_WIDTH_MASK				(0x3fff << 0)
#define SPLITTER_WIDTH_GET(_v)				(((_v) >> 0) & 0x3fff)

#define SPLITTER_SIZE_CONTROL_1				0x0248/* DECon_F only */

#define FRAME_FIFO_CONTROL				0x024C

#define FRAME_FIFO_0_SIZE_CONTROL_0			0x0250
#define FRAME_FIFO_HEIGHT_F(_v)				((_v) << 16)
#define FRAME_FIFO_HEIGHT_MASK				(0x3fff << 16)
#define FRAME_FIFO_HEIGHT_GET(_v)			(((_v) >> 16) & 0x3fff)
#define FRAME_FIFO_WIDTH_F(_v)				((_v) << 0)
#define FRAME_FIFO_WIDTH_MASK				(0x3fff << 0)
#define FRAME_FIFO_WIDTH_GET(_v)			(((_v) >> 0) & 0x3fff)

#define FRAME_FIFO_0_SIZE_CONTROL_1			0x0254
#define FRAME_FIFO_1_SIZE_CONTROL_0			0x0258/* DECon_F only */
#define FRAME_FIFO_1_SIZE_CONTROL_1			0x025C/* DECon_F only */
#define FRAME_FIFO_INFO_0				0x0260
#define FRAME_FIFO_INFO_1				0x0264
#define FRAME_FIFO_INFO_2				0x0268
#define FRAME_FIFO_INFO_3				0x026C

#define SRAM_SHARE_COMP_INFO				0x0270/* DECon_F only */
#define SRAM_SHARE_ENH_INFO				0x0274/* DECon_F only */

#define CRC_DATA_0					0x0280
#define CRC_DATA_2					0x0284/* DECon_F only */
#define CRC_CONTROL					0x0288

#define FRAME_ID					0x02A0

#define DEBUG_CLOCK_OUT_SEL				0x02AC

#define DITHER_CONTROL					0x0300/* DECon_F only */


/*
* MIC registers
* ->
* 0x1004 ~
* updated by SHADOW_REG_UPDATE_REQ[31] : SHADOW_REG_UPDATE_REQ
*/

#define MIC_CONTROL					0x1004
#define MIC_DUMMY_F(_v)					((_v) << 20)
#define MIC_DUMMY_MASK					(0x1ff << 20)
/* 0 : single slice, 1: dual slice */
#define MIC_SLICE_NUM_F(_v)				((_v) << 8)
#define MIC_SLICE_NUM_MASK				(0x3 << 8)
/* 0 : {PIX0, PIX1}, 1 : {PIX1, PIX0} */
#define MIC_PIXEL_ORDER_F(_v)				((_v) << 4)
#define MIC_PIXEL_ORDER_MASK				(1 << 4)
/* 0 : 1/2, 1: 1/3 */
#define MIC_PARA_CR_CTRL_F(_v)				((_v) << 1)
#define MIC_PARA_CR_CTRL_MASK				(1 << 1)
#define MIC_PARA_CR_CTRL_SHIFT				1

#define MIC_ENC_PARAM0					0x1008
#define MIC_ENC_PARAM1					0x100C
#define MIC_ENC_PARAM2					0x1010
#define MIC_ENC_PARAM3					0x1014
#define MIC_BG_COLOR					0x1018
#define MIC_SIZE_CONTROL				0x101C
#define MIC_WIDTH_C_F(_v)				((_v) << 0)
#define MIC_WIDTH_C_MASK				(0x3fff << 0)

/*
* <-
* MIC registers
*/


/*
* DSC registers
* ->
* 0x2000 ~
* updated by SHADOW_REG_UPDATE_REQ[31] : SHADOW_REG_UPDATE_REQ
*
* @ regs-dsc.h
*
* <-
* DSC registers
*/

#define DSC0_OFFSET					0x2000
#define DSC1_OFFSET					0x3000
#define DSC_NUM_EXTRA_MUX_BIT				246
#define DSC_MIN_SLICE_SIZE				15000
#define DSC_INIT_TRANSMIT_DELAY				0x200
#define DSC_INIT_SCALE_VALUE				0x20
#define DSC_BIT_PER_PIXEL				0x80
#define DSC_FIRST_LINE_BPG_OFFSET			0xC
#define DSC_INIT_OFFSET					0x1800
#define DSC_RC_MODE_SIZE				0x2000

#define DSC_CONTROL0					0x0000
#define DSC_CONTROL0_SW_RESET				(0x1 << 28)
#define DSC_CONTROL0_BIT_SWAP_MASK			(0x1 << 10)
#define DSC_CONTROL0_BIT_SWAP(_v)			((_v) << 10)
#define DSC_CONTROL0_BYTE_SWAP_MASK			(0x1 << 9)
#define DSC_CONTROL0_BYTE_SWAP(_v)			((_v) << 9)
#define DSC_CONTROL0_WORD_SWAP_MASK			(0x1 << 8)
#define DSC_CONTROL0_WORD_SWAP(_v)			((_v) << 8)
#define DSC_CONTROL0_FLATNESS_DET_TH_F_MASK		(0xf << 4)
#define DSC_CONTROL0_FLATNESS_DET_TH_F(_v)		((_v) << 4)
#define DSC_CONTROL0_SLICE_MODE_CH_F_MASK		(0x1 << 3)
#define DSC_CONTROL0_SLICE_MODE_CH_F(_v)		((_v) << 3)
#define DSC_CONTROL0_DSC_BYPASS_F_MASK			(0x1 << 2)
#define DSC_CONTROL0_DSC_BYPASS_F(_v)			((_v) << 2)
#define DSC_CONTROL0_DSC_CG_EN_F_MASK			(0x1 << 1)
#define DSC_CONTROL0_DSC_CG_EN_F(_v)			((_v) << 1)
#define DSC_CONTROL0_DUAL_SLICE_EN_F_MASK		0x1
#define DSC_CONTROL0_DUAL_SLICE_EN_F(_v)		(_v)

#define DSC_CONTROL1					0x0004
#define DSC_CONTROL1_DSC_V_FRONT_F_MASK			(0xFF << 16)
#define DSC_CONTROL1_DSC_V_FRONT_F(_v)			((_v) << 16)
#define DSC_CONTROL1_DSC_V_SYNC_F_MASK			(0xFF << 8)
#define DSC_CONTROL1_DSC_V_SYNC_F(_v)			((_v) << 8)
#define DSC_CONTROL1_DSC_V_BACK_F_MASK			0xFF
#define DSC_CONTROL1_DSC_V_BACK_F(_v)			(_v)

#define DSC_CONTROL2					0x0008
#define DSC_CONTROL2_DSC_H_FRONT_F_MASK			(0xFF << 16)
#define DSC_CONTROL2_DSC_H_FRONT_F(_v)			((_v) << 16)
#define DSC_CONTROL2_DSC_H_SYNC_F_MASK			(0xFF << 8)
#define DSC_CONTROL2_DSC_H_SYNC_F(_v)			((_v) << 8)
#define DSC_CONTROL2_DSC_H_BACK_F_MASK			0xFF
#define DSC_CONTROL2_DSC_H_BACK_F(_v)			(_v)

#define DSC_IN_PIXEL_COUNT				0x000C
#define DSC_COMP_PIXEL_COUNT				0x0010

#define DSC_PPS00_03					0x0020
#define DSC_PPS00_03_DSC_VER(_v)			(_v << 24)
#define DSC_PPS00_03_DSC_VER_MASK			(0xFF << 24)

#define DSC_PPS04_07					0x0024
#define DSC_PPS06_07_PICTURE_HEIGHT_MASK		0xFFFF
#define DSC_PPS06_07_PICTURE_HEIGHT(_v)			(_v)

#define DSC_PPS08_11					0x0028
#define DSC_PPS08_9_PICTURE_WIDHT_MASK			(0xFFFF << 16)
#define DSC_PPS08_9_PICTURE_WIDHT(_v)			((_v) << 16)
#define DSC_PPS10_11_SLICE_HEIGHT_MASK			0xFFFF
#define DSC_PPS10_11_SLICE_HEIGHT(_v)			(_v)

#define DSC_PPS12_15					0x002C
#define DSC_PPS12_13_SLICE_WIDTH_MASK			(0xFFFF << 16)
#define DSC_PPS12_13_SLICE_WIDTH(_v)			((_v) << 16)
#define DSC_PPS14_15_CHUNK_SIZE_MASK			0xFFFF
#define DSC_PPS14_15_CHUNK_SIZE(_v)				(_v)

#define DSC_PPS16_19					0x0030
#define DSC_PPS16_17_INIT_XMIT_DELAY_MASK		(0x3FF << 16)
#define DSC_PPS16_17_INIT_XMIT_DELAY(_v)		((_v) << 16)
#define DSC_PPS16_19_INIT_DEC_DELAY_MASK		(0xFFFF << 0)
#define DSC_PPS16_19_INIT_DEC_DELAY(_v)			((_v) << 0)


#define DSC_PPS20_23					0x0034
#define DSC_PPS21_INIT_SCALE_VALUE_MASK			(0xFF << 16)
#define DSC_PPS21_INIT_SCALE_VALUE(_v)			((_v) << 16)
#define DSC_PPS22_23_SCALE_INCREMENT_INTERVAL_MASK	0xFFFF
#define DSC_PPS22_23_SCALE_INCREMENT_INTERVAL(_v)	(_v)

#define DSC_PPS24_27					0x0038
#define DSC_PPS24_25_SCALE_DECREMENT_INTERVAL_MASK	(0xFFFF << 16)
#define DSC_PPS24_25_SCALE_DECREMENT_INTERVAL(_v)	((_v) << 16)
#define DSC_PPS27_FIRST_LINE_BPG_OFFSET_MASK		0x1F
#define DSC_PPS27_FIRST_LINE_BPG_OFFSET(_v)		(_v)

#define DSC_PPS28_31					0x003C
#define DSC_PPS28_29_NOT_FIRST_LINE_BPG_OFFSET_MASK	(0xFFFF << 16)
#define DSC_PPS28_29_NOT_FIRST_LINE_BPG_OFFSET(_v)	((_v) << 16)
#define DSC_PPS30_31_SLICE_BPG_OFFSET_MASK		0xFFFF
#define DSC_PPS30_31_SLICE_BPG_OFFSET(_v)		(_v)

#define DSC_PPS32_35					0x0040
#define DSC_PPS32_33_INIT_OFFSET_MASK			(0xFFFF << 16)
#define DSC_PPS32_33_INIT_OFFSET(_v)			((_v) << 16)
#define DSC_PPS34_35_FINAL_OFFSET_MASK			0xFFFF
#define DSC_PPS34_35_FINAL_OFFSET(_v)			(_v)

#define DSC_PPS124_127					0x009C //PPS end register

/*
* DISPIF2,3 registers
* ->
*/

#define DISPIF2_CONTROL					0x5080/* DECon_F only */
#define DISPIF2_CONTROL_FREE_RUN_EN			(1 << 4)
#define DISPIF2_CONTROL_UNDERRUN_SCHEME_F(_v)		((_v) << 12)
#define DISPIF2_CONTROL_UNDERRUN_SCHEME_MASK		(0x3 << 12)
#define DISPIF2_CONTROL_OUT_RGB_ORDER_F(_v)		((_v) << 8)
#define DISPIF2_CONTROL_OUT_RGB_ORDER_MASK		(0x7 << 8)

#define DISPIF2_MAPCOLOR				0x5084/* DECon_F only */

#define DISPIF2_LINE_COUNT				0x508C
#define DISPIF2_LINE_COUNT_SHIFT			0
#define DISPIF2_LINE_COUNT_MASK				(0xffff << 0)

#define DISPIF2_TIMING_CONTROL_0			0x5090/* DECon_F only */
#define DISPIF2_TIMING_CONTROL_1			0x5094/* DECon_F only */
#define DISPIF2_TIMING_CONTROL_2			0x5098/* DECon_F only */
#define DISPIF2_TIMING_CONTROL_3			0x509C/* DECon_F only */
#define DISPIF2_SIZE_CONTROL_0				0x50A0/* DECon_F only */
#define DISPIF2_SIZE_CONTROL_1				0x50A4/* DECon_F only */

#define VCLK2_DIVIDER_CONTROL				0x50F4/* DECon_F only */

#define DISPIF3_CONTROL					0x6080/* DECon_F only */
#define DISPIF3_CONTROL_FREE_RUN_EN			(1 << 4)
#define DISPIF3_CONTROL_UNDERRUN_SCHEME_F(_v)		((_v) << 12)
#define DISPIF3_CONTROL_UNDERRUN_SCHEME_MASK		(0x3 << 12)
#define DISPIF3_CONTROL_OUT_RGB_ORDER_F(_v)		((_v) << 8)
#define DISPIF3_CONTROL_OUT_RGB_ORDER_MASK		(0x7 << 8)

#define DISPIF3_MAPCOLOR				0x6084/* DECon_F only */

#define DISPIF3_LINE_COUNT				0x608C/* DECon_F only */
#define DISPIF3_LINE_COUNT_SHIFT			0
#define DISPIF3_LINE_COUNT_MASK				(0xffff << 0)

#define DISPIF3_TIMING_CONTROL_0			0x6090/* DECon_F only */
#define DISPIF3_TIMING_CONTROL_1			0x6094/* DECon_F only */

#define DISPIF3_TIMING_CONTROL_2			0x6098/* DECon_F only */
#define DISPIF3_TIMING_CONTROL_3			0x609C/* DECon_F only */

#define DISPIF3_SIZE_CONTROL_0				0x60A0/* DECon_F only */
#define DISPIF3_SIZE_CONTROL_1				0x60A4/* DECon_F only */

#define VCLK3_DIVIDER_CONTROL				0x60F4/* DECon_F only */
#define DIVIDER_DENOM_VALUE_OF_CLK_F(_v)		((_v) << 16)
#define DIVIDER_NUM_VALUE_OF_CLK_F(_v)			((_v) << 0)

/* <-
* DISPIF2,3 registers
*/


/*
* mDNIe registers
* ->
* 0xD000 ~
*  updated by SHADOW_REG_UPDATE_REQ[24] : SHADOW_REG_UPDATE_REQ_mDNIe
*
* @ regs-mdnie.h
*
* <-
* mDNIe registers
*/


/*
* DPU registers
* ->
* 0xE000 ~
*  updated by SHADOW_REG_UPDATE_REQ[28] : SHADOW_REG_UPDATE_REQ_DPU
*
* @ regs-dpu.h
*
* <-
* DPU registers
*/

#define SHADOW_OFFSET					0x7000

#endif /* _REGS_DECON_H */
