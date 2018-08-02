/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_macros.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_MACROS_H
#define __S5P_MFC_MACROS_H __FILE__

#include "s5p_mfc_reg.h"

#define mb_width(x_size)		((x_size + 15) / 16)
#define mb_height(y_size)		((y_size + 15) / 16)
#define s5p_mfc_dec_mv_size(x, y)	(mb_width(x) * (((mb_height(y)+1)/2)*2) * 64 + 512)

/*
   Note that lcu_width and lcu_height are defined as follows :
   lcu_width = (frame_width + lcu_size - 1)/lcu_size
   lcu_height = (frame_height + lcu_size - 1)/lcu_size.
   (lcu_size is 32(encoder) or 64(decoder))
*/
#define dec_lcu_width(x_size)		((x_size + 63) / 64)
#define enc_lcu_width(x_size)		((x_size + 31) / 32)
#define dec_lcu_height(y_size)		((y_size + 63) / 64)
#define enc_lcu_height(y_size)		((y_size + 31) / 32)
#define s5p_mfc_dec_hevc_mv_size(x, y)	(dec_lcu_width(x) * dec_lcu_height(y) * 256 + 512)

/* Definition */
#define FRAME_DELTA_DEFAULT		1
#define TIGHT_CBR_MAX			10
#define I_LIMIT_CBR_MAX			5

/* Scratch buffer size for MFC v6.1 */
#define DEC_V61_H264_SCRATCH_SIZE(x, y)				\
		((x * 128) + 65536)
#define DEC_V61_MPEG4_SCRATCH_SIZE(x, y)			\
		((x) * ((y) * 64 + 144) +			\
		 ((2048 + 15) / 16 * (y) * 64) +		\
		 ((2048 + 15) / 16 * 256 + 8320))
#define DEC_V61_VC1_SCRATCH_SIZE(x, y)				\
		(2096 * ((x) + (y) + 1))
#define DEC_V61_MPEG2_SCRATCH_SIZE(x, y)	0
#define DEC_V61_H263_SCRATCH_SIZE(x, y)				\
		((x) * 400)
#define DEC_V61_VP8_SCRATCH_SIZE(x, y)				\
		((x) * 32 + (y) * 128 + 34816)
#define ENC_V61_H264_SCRATCH_SIZE(x, y)				\
		(((x) * 64) + (((x) + 1) * 16) + (4096 * 16))
#define ENC_V61_MPEG4_SCRATCH_SIZE(x, y)			\
		(((x) * 16) + (((x) + 1) * 16))

/* Scratch buffer size for MFC v6.5 */
#define DEC_V65_H264_SCRATCH_SIZE(x, y)				\
		((x * 192) + 64)
#define DEC_V65_MPEG4_SCRATCH_SIZE(x, y)			\
		(((x) * 144) + ((y) * 8192) + 49216 + 1048576)
#define DEC_V65_VC1_SCRATCH_SIZE(x, y)				\
		(2096 * ((x) + (y) + 1))
#define DEC_V65_MPEG2_SCRATCH_SIZE(x, y)	0
#define DEC_V65_H263_SCRATCH_SIZE(x, y)				\
		((x) * 400)
#define DEC_V65_VP8_SCRATCH_SIZE(x, y)				\
		((x) * 32 + (y) * 128 +				\
		 (((x) + 1) / 2) * 64 + 2112)
#define ENC_V65_H264_SCRATCH_SIZE(x, y)				\
		(((x) * 48) + (((x) + 1) / 2 * 128) + 144)
#define ENC_V65_MPEG4_SCRATCH_SIZE(x, y)			\
		(((x) * 32) + 16)
#define ENC_V70_VP8_SCRATCH_SIZE(x, y)				\
		(((x) * 48) + (((x) + 1) / 2 * 128) + 144 +	\
		 8192 + (((x) * 16) * (((y) * 16) * 3 / 2) * 4))

/* Additional scratch buffer size for MFC v7.x */
#define DEC_V72_ADD_SIZE_0(x)					\
		(((x) * 16 * 72 * 2) + 256)

/* Encoder buffer size for common */
#define ENC_TMV_SIZE(x, y)					\
		(((x) + 1) * ((y) + 3) * 8)
#define ENC_ME_SIZE(f_x, f_y, mb_x, mb_y)			\
		((((((f_x) + 127) / 64) * 16) *			\
		((((f_y) + 63) / 64) * 16)) +			\
		((((mb_x) * (mb_y) + 31) / 32) * 16))

/* Encoder buffer size for hevc */
#define ENC_HEVC_ME_SIZE(x, y)				\
			((((x * 32 / 8 + 63) / 64 * 64) * ((y * 8) + 64)) + (x * y * 32))

/* Encoder buffer size for MFC v10.0 */
#define ENC_V100_H264_ME_SIZE(x, y)				\
	(((x + 3) * (y + 3) * 8) + ((((x * y) + 63) / 64) * 32) + (((y * 64) + 1280) * (x + 7) / 8))
#define ENC_V100_MPEG4_ME_SIZE(x, y)				\
	(((x + 3) * (y + 3) * 8) + ((((x * y) + 127) / 128) * 16) + (((y * 64) + 1280) * (x + 7) / 8))
#define ENC_V100_VP8_ME_SIZE(x, y)				\
	(((x + 3) * (y + 3) * 8) + (((y * 64) + 1280) * (x + 7) / 8))
#define ENC_V100_VP9_ME_SIZE(x, y)				\
	((((x * 2) + 3) * ((y * 2) + 3) * 128) + (((y * 256) + 1280) * (x+1) / 2))
#define ENC_V100_HEVC_ME_SIZE(x, y)				\
	(((x + 3) * (y + 3) * 32) + (((y * 128) + 1280) * (x + 3) / 4))

#define NUM_MPEG4_LF_BUF		2

/* Scratch buffer size for MFC v8.0 */
#define DEC_V80_H264_SCRATCH_SIZE(x, y)				\
		((x * 704) + 2176)
#define DEC_V80_MPEG4_SCRATCH_SIZE(x, y)			\
		(((x) * 592) + ((y) * 4096) + (2*1048576))
#define DEC_V80_VP8_SCRATCH_SIZE(x, y)				\
		((((x) * 576) + ((y) * 128) + 4128))
#define ENC_V80_H264_SCRATCH_SIZE(x, y)				\
		(((x) * 592) + 2336)
#define ENC_V80_VP8_SCRATCH_SIZE(x, y)				\
		(((x) * 576) + 10512 +	\
		 (((x) * 16) * (((y) * 16) * 3 / 2) * 4))

#define R2H_BIT(x)	(((x) > 0) ? (1 << ((x) - 1)) : 0)

static inline unsigned int r2h_bits(int cmd)
{
	unsigned int mask = R2H_BIT(cmd);

	if (cmd == S5P_FIMV_R2H_CMD_FRAME_DONE_RET)
		mask |= (R2H_BIT(S5P_FIMV_R2H_CMD_FIELD_DONE_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_COMPLETE_SEQ_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_SLICE_DONE_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_ENC_BUFFER_FULL_RET));
	/* FIXME: Temporal mask for S3D SEI processing */
	else if (cmd == S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET)
		mask |= (R2H_BIT(S5P_FIMV_R2H_CMD_FIELD_DONE_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_SLICE_DONE_RET) |
			 R2H_BIT(S5P_FIMV_R2H_CMD_FRAME_DONE_RET));

	return (mask |= R2H_BIT(S5P_FIMV_R2H_CMD_ERR_RET));
}

#endif /* __S5P_MFC_MACROS_H */
