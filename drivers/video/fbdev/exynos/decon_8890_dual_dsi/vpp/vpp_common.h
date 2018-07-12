/*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Header file for Exynos VPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#ifndef ___SAMSUNG_VPP_COMMON_H__
#define ___SAMSUNG_VPP_COMMON_H__

#define YUV_SRC_OFFSET_MULTIPLE	2
#define YUV_SRC_SIZE_MULTIPLE	2
#define YUV_SRC_SIZE_MUL_HEIGHT	1
#define YUV_SRC_WIDTH_MAX	65534
#define YUV_SRC_HEIGHT_MAX	8190
#define YUV_SRC_WIDTH_MIN	32
#define YUV_SRC_HEIGHT_MIN	16
#define RGB_SRC_OFFSET_MULTIPLE	1
#define RGB_SRC_SIZE_MULTIPLE	1
#define RGB_SRC_WIDTH_MAX	65535
#define RGB_SRC_HEIGHT_MAX	8191
#define RGB_SRC_WIDTH_MIN	16
#define RGB_SRC_HEIGHT_MIN	8

#define ROT1_RGB_SRC_WIDTH_MIN	32
#define ROT1_RGB_SRC_HEIGHT_MIN	16
#define ROT1_YUV_SRC_WIDTH_MIN	64
#define ROT1_YUV_SRC_HEIGHT_MIN	32
#define ROT2_RGB_SRC_WIDTH_MIN	16
#define ROT2_RGB_SRC_HEIGHT_MIN	32
#define ROT2_YUV_SRC_WIDTH_MIN	32
#define ROT2_YUV_SRC_HEIGHT_MIN	64

#define YUV_IMG_SIZE_MULTIPLE	2
#define RGB_IMG_SIZE_MULTIPLE	1
#define IMG_WIDTH_MAX		4096
#define IMG_HEIGHT_MAX		4096
#define RGB_IMG_WIDTH_MIN	16
#define RGB_IMG_HEIGHT_MIN	8
#define YUV_IMG_WIDTH_MIN	32
#define YUV_IMG_HEIGHT_MIN	16

#define IMG_SIZE_MULTIPLE	2

#define ROT1_RGB_IMG_WIDTH_MIN	32
#define ROT1_RGB_IMG_HEIGHT_MIN	16
#define ROT1_YUV_IMG_WIDTH_MIN	64
#define ROT1_YUV_IMG_HEIGHT_MIN	32
#define ROT2_RGB_IMG_WIDTH_MIN	16
#define ROT2_RGB_IMG_HEIGHT_MIN	32
#define ROT2_YUV_IMG_WIDTH_MIN	32
#define ROT2_YUV_IMG_HEIGHT_MIN	64

#define ROT3_RGB_IMG_WIDTH_MAX	4096
#define ROT3_RGB_IMG_HEIGHT_MAX	4096
#define ROT3_YUV_IMG_WIDTH_MAX	4096
#define ROT3_YUV_IMG_HEIGHT_MAX	4096
#define ROT4_RGB_IMG_WIDTH_MAX	2048
#define ROT4_RGB_IMG_HEIGHT_MAX	2048
#define ROT4_YUV_IMG_WIDTH_MAX	2048
#define ROT4_YUV_IMG_HEIGHT_MAX	2048

#define BLK_WIDTH_MAX		4096
#define BLK_HEIGHT_MAX		4096
#define BLK_WIDTH_MIN		144
#define BLK_HEIGHT_MIN		10
#define ROT3_RGB_BLK_WIDTH_MAX	4096
#define ROT3_RGB_BLK_HEIGHT_MAX	4096
#define ROT3_RGB_BLK_WIDTH_MIN	128
#define ROT3_RGB_BLK_HEIGHT_MIN	16
#define ROT4_RGB_BLK_WIDTH_MAX	2048
#define ROT4_RGB_BLK_HEIGHT_MAX	2048

#define SCALED_SIZE_MULTIPLE	1
#define SCALED_WIDTH_MAX	4096
#define SCALED_HEIGHT_MAX	4096
#define SCALED_WIDTH_MIN	16
#define SCALED_HEIGHT_MIN	8

#define PRE_RGB_WIDTH		1
#define PRE_RGB_HEIGHT		1
#define PRE_YUV_WIDTH		2
#define PRE_YUV_HEIGHT		2
#define PRE_ROT1_YUV_HEIGHT	1

#define SRC_SIZE_MULTIPLE	2
#define SRC_ROT1_MUL_Y		1
#define SRC_ROT2_MUL_Y		2

#define DST_SIZE_MULTIPLE	1
#define DST_SIZE_WIDTH_MIN	16
#define DST_SIZE_WIDTH_MAX	8191
#define DST_SIZE_HEIGHT_MIN	8
#define DST_SIZE_HEIGHT_MAX	8191
#define DST_OFFSET_MULTIPLE	1
#define DST_IMGAGE_MULTIPLE	1
#define DST_IMG_WIDTH_MIN	16
#define DST_IMG_HEIGHT_MIN	8
#define DST_IMG_MAX		4096

struct vpp_size_constraints {
	u32		src_mul_w;
	u32		src_mul_h;
	u32		src_w_min;
	u32		src_w_max;
	u32		src_h_min;
	u32		src_h_max;
	u32		img_mul_w;
	u32		img_mul_h;
	u32		img_w_min;
	u32		img_w_max;
	u32		img_h_min;
	u32		img_h_max;
	u32		blk_w_min;
	u32		blk_w_max;
	u32		blk_h_min;
	u32		blk_h_max;
	u32		blk_mul_w;
	u32		blk_mul_h;
	u32		src_mul_x;
	u32		src_mul_y;
	u32		sca_w_min;
	u32		sca_w_max;
	u32		sca_h_min;
	u32		sca_h_max;
	u32		sca_mul_w;
	u32		sca_mul_h;
	u32		dst_mul_w;
	u32		dst_mul_h;
	u32		dst_w_min;
	u32		dst_w_max;
	u32		dst_h_min;
	u32		dst_h_max;
	u32		dst_mul_x;
	u32		dst_mul_y;
};

struct vpp_img_format {
	u32		vgr;
	u32		normal;
	u32		rot;
	u32		scale;
	u32		format;
	u32		afbc_en;
	u32		yuv;
	u32		yuv422;
	u32		yuv420;
	u32		wb;
};

struct vpp_size_param {
	u32		src_x;
	u32		src_y;
	u32		src_fw;
	u32		src_fh;
	u32		src_w;
	u32		src_h;
	u32		dst_w;
	u32		dst_h;
	u32		fr_w;
	u32		fr_h;
	u32		fr_yx;
	u32		fr_yy;
	u32		fr_cx;
	u32		fr_cy;
	u32		vpp_h_ratio;
	u32		vpp_v_ratio;
	u32		rot;
	u32		block_w;
	u32		block_h;
	u32		block_x;
	u32		block_y;
	u64		addr0;
	u64		addr1;
	u64		addr2;
};

/* CAL APIs list */
void vpp_reg_set_realtime_path(u32 id);
void vpp_reg_set_framedone_irq(u32 id, u32 enable);
void vpp_reg_set_deadlock_irq(u32 id, u32 enable);
void vpp_reg_set_read_slave_err_irq(u32 id, u32 enable);
void vpp_reg_set_hw_reset_done_mask(u32 id, u32 enable);
void vpp_reg_set_lookup_table(u32 id);
void vpp_reg_set_enable_interrupt(u32 id);
void vpp_reg_set_rgb_type(u32 id, u32 type);
void vpp_reg_set_dynamic_clock_gating(u32 id);
void vpp_reg_set_plane_alpha_fixed(u32 id);

/* CAL raw functions list */
int vpp_reg_set_sw_reset(u32 id);
void vpp_reg_set_in_size(u32 id, struct vpp_size_param *p);
void vpp_reg_set_out_size(u32 id, u32 dst_w, u32 dst_h);
void vpp_reg_set_scale_ratio(u32 id, struct vpp_size_param *p, u32 rot_en);
int vpp_reg_set_in_format(u32 id, struct vpp_img_format *vi);
void vpp_reg_set_in_block_size(u32 id, u32 enable, struct vpp_size_param *p);
void vpp_reg_set_in_afbc_en(u32 id, u32 enable);
void vpp_reg_set_in_buf_addr(u32 id, struct vpp_size_param *p, struct vpp_img_format *vi);
void vpp_reg_set_smart_if_pix_num(u32 id, u32 dst_w, u32 dst_h);
void vpp_reg_set_sfr_update_force(u32 id);
int vpp_reg_wait_op_status(u32 id);
int vpp_reg_set_rotation(u32 id, struct vpp_size_param *p);
void vpp_reg_set_plane_alpha(u32 id, u32 plane_alpha);
void vpp_reg_wait_idle(u32 id);
int vpp_reg_get_irq_status(u32 id);
void vpp_reg_set_clear_irq(u32 id, u32 irq);
void vpp_constraints_params(struct vpp_size_constraints *vc,
					struct vpp_img_format *vi);
void vpp_reg_init(u32 id);
void vpp_reg_deinit(u32 id, u32 reset_en);
void vpp_reg_wait_pingpong_clear(u32 id);
#endif /* ___SAMSUNG_VPP_COMMON_H__ */
