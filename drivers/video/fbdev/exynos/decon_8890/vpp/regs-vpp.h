/* linux/drivers/video/decon_2.0/regs-vpp.h
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Register definition file for Samsung vpp driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef VPP_REGS_H_
#define VPP_REGS_H_

/* IDMA_VG(R)x_ENABLE */
#define VG_ENABLE				(0x00)

#define VG_ENABLE_OP_STATUS			(1 << 2)
#define VG_ENABLE_SFR_UPDATE_FORCE		(1 << 4)
#define VG_ENABLE_INT_CLOCK_GATE_EN		(1 << 8)
#define VG_ENABLE_SRAM_CLOCK_GATE_EN		(1 << 9)
#define VG_ENABLE_SFR_CLOCK_GATE_EN		(1 << 10)
#define VG_ENABLE_SCALER_CLOCK_GATE_EN		(1 << 11)
#define VG_ENABLE_RT_PATH_EN			(1 << 16)
#define VG_ENABLE_RT_PATH_EN_S			(1 << 17)
#define VG_ENABLE_SRESET			(1 << 24)
#define VG_ENABLE_VSD_SRESET_MASK		(1 << 25)

/* IDMA_VG(R)x_IRQ */
#define VG_IRQ					(0x04)

#define VG_IRQ_ENABLE				(1 << 0)
#define VG_IRQ_FRAMEDONE_MASK			(1 << 1)
#define VG_IRQ_DEADLOCK_STATUS_MASK		(1 << 2)
#define VG_IRQ_READ_SLAVE_ERROR_MASK		(1 << 4)
#define VG_IRQ_HW_RESET_DONE_MASK		(1 << 5)
#define VG_IRQ_FRAMEDONE			(1 << 16)
#define VG_IRQ_DEADLOCK_STATUS			(1 << 17)
#define VG_IRQ_READ_SLAVE_ERROR			(1 << 19)
#define VG_IRQ_HW_RESET_DONE			(1 << 20)

/* IDMA_VG(R)x_IN_CON */
#define VG_IN_CON				(0x08)

#define VG_IN_CON_BLOCKING_FEATURE_EN		(1 << 3)
#define VG_IN_CON_CHROMINANCE_STRIDE_EN		(1 << 4)
#define VG_IN_CON_SCAN_MODE_MASK		(3 << 5)
#define VG_IN_CON_SCAN_MODE_PRO_TO_PRO		(1 << 5)
#define VG_IN_CON_SCAN_MODE_INT_TO_PRO		(2 << 5)
#define VG_IN_CON_SCAN_MODE_INT_TO_INT		(3 << 5)
#define VG_IN_CON_IN_ROTATION_MASK		(7 << 8)
#define VG_IN_CON_IN_ROTATION_X_FLIP		(1 << 8)
#define VG_IN_CON_IN_ROTATION_Y_FLIP		(2 << 8)
#define VG_IN_CON_IN_ROTATION_180		(3 << 8)
#define VG_IN_CON_IN_ROTATION_90		(4 << 8)
#define VG_IN_CON_IN_ROTATION_90_X_FLIP		(5 << 8)
#define VG_IN_CON_IN_ROTATION_90_Y_FLIP		(6 << 8)
#define VG_IN_CON_IN_ROTATION_270		(7 << 8)
#define VG_IN_CON_IMG_FORMAT_MASK		(31 << 11)
#define VG_IN_CON_IMG_FORMAT_ARGB8888		(0 << 11)
#define VG_IN_CON_IMG_FORMAT_ABGR8888		(1 << 11)
#define VG_IN_CON_IMG_FORMAT_RGBA8888		(2 << 11)
#define VG_IN_CON_IMG_FORMAT_BGRA8888		(3 << 11)
#define VG_IN_CON_IMG_FORMAT_XRGB8888		(4 << 11)
#define VG_IN_CON_IMG_FORMAT_XBGR8888		(5 << 11)
#define VG_IN_CON_IMG_FORMAT_RGBX8888		(6 << 11)
#define VG_IN_CON_IMG_FORMAT_BGRX8888		(7 << 11)
#define VG_IN_CON_IMG_FORMAT_RGB565		(8 << 11)
#define VG_IN_CON_IMG_FORMAT_YUV422_2P		(20 << 11)
#define VG_IN_CON_IMG_FORMAT_YVU422_2P		(21 << 11)
#define VG_IN_CON_IMG_FORMAT_YUV422_3P		(22 << 11)
#define VG_IN_CON_IMG_FORMAT_YUV420_2P		(24 << 11)
#define VG_IN_CON_IMG_FORMAT_YVU420_2P		(25 << 11)
#define VG_IN_CON_IMG_FORMAT_YUV420_3P		(26 << 11)
#define VG_IN_CON_IN_IC_MAX_DEFAULT		(0x10 << 19)
#define VG_IN_CON_IN_IC_MAX			(0x1F << 19)
#define VG_IN_CON_BURST_LENGTH_MASK		(0xF << 24)
#define VG_IN_CON_BURST_LENGTH_0		(0 << 24)
#define VG_IN_CON_BURST_LENGTH_1		(1 << 24)
#define VG_IN_CON_BURST_LENGTH_2		(2 << 24)
#define VG_IN_CON_BURST_LENGTH_3		(3 << 24)
#define VG_IN_CON_IN_AFBC_EN			(1 << 7)

/* IDMA_VG(R)x_OUT_CON */
#define VG_OUT_CON				(0x0C)

#define VG_OUT_CON_RGB_TYPE_MASK		(3 << 17)
#define VG_OUT_CON_RGB_TYPE_601_NARROW		(0 << 17)
#define VG_OUT_CON_RGB_TYPE_601_WIDE		(1 << 17)
#define VG_OUT_CON_RGB_TYPE_709_NARROW		(2 << 17)
#define VG_OUT_CON_RGB_TYPE_709_WIDE		(3 << 17)
#define VG_OUT_CON_FRAME_ALPHA_MASK		(0xFF << 24)
#define VG_OUT_CON_FRAME_ALPHA(x)		(x << 24)

/* IDMA_VG(R)x_SRC_SIZE */
#define VG_SRC_SIZE				(0x10)

#define VG_SRC_SIZE_HEIGHT_MASK			(0x1FFF << 16)
#define VG_SRC_SIZE_HEIGHT(x)			((x) << 16)
#define VG_SRC_SIZE_WIDTH_MASK			(0x1FFF << 0)
#define VG_SRC_SIZE_WIDTH(x)			((x) << 0)

/* IDMA_VG(R)x_SRC_OFFSET */
#define VG_SRC_OFFSET				(0x14)

#define VG_SRC_OFFSET_Y_MASK			(0x1FFF << 16)
#define VG_SRC_OFFSET_Y(x)			((x) << 16)
#define VG_SRC_OFFSET_X_MASK			(0x1FFF << 0)
#define VG_SRC_OFFSET_X(x)			((x) << 0)

/* IDMA_VG(R)x_IMG_SIZE */
#define VG_IMG_SIZE				(0x18)

#define VG_IMG_SIZE_HEIGHT_MASK			(0x1FFF << 16)
#define VG_IMG_SIZE_HEIGHT(x)			((x) << 16)
#define VG_IMG_SIZE_WIDTH_MASK			(0x1FFF << 0)
#define VG_IMG_SIZE_WIDTH(x)			((x) << 0)

/* IDMA_x_PINGPONG_UPDATE */
#define VG_PINGPONG_UPDATE			(0x1C)
#define VG_ADDR_PINGPONG_UPDATE			(1 << 0)

/* IDMA_VG(R)x_CHROMINANCE_STRIDE */
#define VG_CHROMINANCE_STRIDE			(0x20)

#define VG_CHROMINANCE_STRIDE_MASK		(0x1FFF << 0)
#define VG_CHROMINANCE_STRIDE_SIZE(x)		((x) << 0)

/* IDMA_VG(R)x_BLK_OFFSET */
#define VG_BLK_OFFSET				(0x24)

#define VG_BLK_OFFSET_Y_MASK			(0x1FFF << 16)
#define VG_BLK_OFFSET_Y(x)			((x) << 16)
#define VG_BLK_OFFSET_X_MASK			(0x1FFF << 0)
#define VG_BLK_OFFSET_X(x)			((x) << 0)

/* IDMA_VG(R)x_BLK_SIZE	*/
#define VG_BLK_SIZE				(0x28)

#define VG_BLK_SIZE_HEIGHT_MASK			(0x1FFF << 16)
#define VG_BLK_SIZE_HEIGHT(x)			((x) << 16)
#define VG_BLK_SIZE_WIDTH_MASK			(0x1FFF << 0)
#define VG_BLK_SIZE_WIDTH(x)			((x) << 0)

/* IDMA_VG(R)x_SCALED_SIZE */
#define VG_SCALED_SIZE				(0x2C)

#define VG_SCALED_SIZE_HEIGHT_MASK		(0x1FFF << 16)
#define VG_SCALED_SIZE_HEIGHT(x)		((x) << 16)
#define VG_SCALED_SIZE_WIDTH_MASK		(0x1FFF << 0)
#define VG_SCALED_SIZE_WIDTH(x)			((x) << 0)

/* IDMA_VG(R)x_H_RATIO */
#define VG_H_RATIO				(0x44)

#define VG_H_RATIO_MASK				(0xFFFFFF << 0)
#define VG_H_RATIO_VALUE(x)			((x) << 0)

/* IDMA_VG(R)x_V_RATIO */
#define VG_V_RATIO				(0x48)

#define VG_V_RATIO_MASK				(0xFFFFFF << 0)
#define VG_V_RATIO_VALUE(x)			((x) << 0)

/* IDMA_VG(R)x LOOKUP TABLE */
#define VG_QOS_LUT07_00				(0x60)
#define VG_QOS_LUT15_08				(0x64)

/* IDMA_VG(R)x BASE ADDRESS CONTROL */
#define VG_BASE_ADDR_CON			(0x70)

#define VG_BASE_ADDR_CON_MASK(n)		(1 << (x))
#define VG_BASE_ADDR_CON_PINGPONG_MASK		(3 << 16)
#define VG_BASE_ADDR_CON_PINGPONG(n)		((x) << 16)
#define VG_BASE_ADDR_CON_CURRENT(n)		((x) >> 24)
#define VG_BASE_ADDR_CON_CURRENT_INDEX_INIT	(1 << 31)

/* IDMA_VG(R)x BASE ADDRESS */
#define VG_BASE_ADDR_Y(n)			(0x74 + (n) * 0x4)
#define VG_BASE_ADDR_Y_EVEN(n)			(0x84 + (n) * 0x4)
#define VG_BASE_ADDR_CB(n)			(0x94 + (n) * 0x4)
#define VG_BASE_ADDR_CB_EVEN(n)			(0xA4 + (n) * 0x4)

/* shadow SFR of base address */
#define VG_SHA_BASE_ADDR_Y			0xB74
#define VG_SHA_BASE_ADDR_CB			0xB94

/* IDMA_VG(R)x scaling filter */
#define VG_H_COEF(n, s, x)	(0x290 + (n) * 0x4 + (s) * 0x24 + (x) * 0x200)
#define VG_V_COEF(n, s, x)	(0x200 + (n) * 0x4 + (s) * 0x24 + (x) * 0x200)

/* IDMA_VG(R)x Y(R) start H position */
#define VG_YHPOSITION0				(0x5B0)

/* IDMA_VG(R)x Y(R) start V position */
#define VG_YVPOSITION0				(0x5B4)

/* IDMA_VG(R)x CB(G)/CR(B) H position */
#define VG_CHPOSITION0				(0x5B8)

/* IDMA_VG(R)x CB(G)/CR(B) V position */
#define VG_CVPOSITION0				(0x5BC)

/* IDMA_VG(R)x Y(R) start H position for odd frame */
#define VG_YHPOSITION1				(0x5C0)

/* IDMA_VG(R)x Y(R) start V position for odd frame */
#define VG_YVPOSITION1				(0x5C4)

/* IDMA_VG(R)x CB(G)/CR(B) H position for odd frame */
#define VG_CHPOSITION1				(0x5C8)

/* IDMA_VG(R)x CB(G)/CR(B) V position for odd frame */
#define VG_CVPOSITION1				(0x5CC)

#define VG_POSITION_F_MASK			(0xFFFFF << 0)

/* IDMA_VG(R)x Deadlock Counter Number */
#define VG_DEADLOCK_NUM				(0xA00)

/* IDMA_VG(R)x Sharpness Enhancer Control */
#define VG_SHE_CON				(0xA0C)

#define VG_SHE_CON_SHARP_PATTERN_MASK		(1 << 3)
#define VG_SHE_CON_SHARP_PATTERN_ENABLE		(0 << 3)
#define VG_SHE_CON_SHARP_EFFECT_MASK		(7 << 0)
#define VG_SHE_CON_SHARP_EFFECT_MAX		(7 << 0)
#define VG_SHE_CON_SHARP_EN			(1 << 4)
#define VG_SHE_CON_SHARP_BYPASS			(1 << 5)

/* IDMA_VG(R)x SMART Interface Pixel Number */
#define VG_SMART_IF_PIXEL_NUM			(0xA48)

/* IDMA_VG(R)x Dynamic clock gating */
#define VG_DYNAMIC_GATING_ENABLE		(0xA54)

/* VPP_DEBUG_SFR */
#define VPP_DBG_ENABLE_SFR				(0xC04)
#define VPP_DBG_WRITE_SFR				(0xC00)
#define VPP_DBG_READ_SFR				(0xC10)

#endif
