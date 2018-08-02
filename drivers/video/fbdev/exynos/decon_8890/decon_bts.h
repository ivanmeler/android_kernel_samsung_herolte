/* linux/drivers/video/exynos/decon/decon_bts.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * header file for Samsung EXYNOS5 SoC series DECON driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef DECON_BTS_H_
#define DECON_BTS_H_

#include <linux/clk-provider.h>
#include <linux/clk.h>

#include "decon.h"
#include "vpp/vpp.h"

#define call_bts_ops(q, op, args...)				\
	(((q)->bts_ops->op) ? ((q)->bts_ops->op(args)) : 0)

#define call_bts2_ops(q, op, args...)				\
	(((q)->bts2_ops->op) ? ((q)->bts2_ops->op(args)) : 0)

#define call_init_ops(q, op, args...)				\
		(((q)->bts_init_ops->op) ? ((q)->bts_init_ops->op(args)) : 0)

extern struct decon_bts decon_bts_control;
extern struct decon_init_bts decon_init_bts_control;
extern struct decon_bts2 decon_bts2_control;

struct decon_init_bts {
	void	(*bts_add)(struct decon_device *decon);
	void	(*bts_set_init)(struct decon_device *decon);
	void	(*bts_release_init)(struct decon_device *decon);
	void	(*bts_remove)(struct decon_device *decon);
};

struct decon_bts {
	void	(*bts_get_bw)(struct vpp_dev *vpp);
	void	(*bts_set_calc_bw)(struct vpp_dev *vpp);
	void	(*bts_set_zero_bw)(struct vpp_dev *vpp);
	void	(*bts_set_rot_mif)(struct vpp_dev *vpp);
};

struct decon_bts2 {
	void (*bts_calc_bw)(struct struct decon_device *decon);
	void (*bts_update_bw)(struct decon_device *decon);
};

/* You should sync between enum init value of enum 'VPP0 = 2' and bts_media_type in bts driver */
enum disp_media_type {
	VPP0 = 2,
	VPP1,
	VPP2,
	VPP3,
	VPP4,
	VPP5,
	VPP6,
	VPP7,
	VPP8,
};

#define is_rotation(config) (config->vpp_parm.rot >= VPP_ROT_90)
#define is_yuv(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) && (config->format < DECON_PIXEL_FORMAT_MAX))
#define is_rgb(config) ((config->format >= DECON_PIXEL_FORMAT_ARGB_8888) && (config->format <= DECON_PIXEL_FORMAT_RGB_565))
#define is_scaling(vpp) ((vpp->h_ratio != (1 << 20)) || (vpp->v_ratio != (1 << 20)))
#define is_scale_down(vpp) ((vpp->h_ratio > (1 << 20)) || (vpp->v_ratio > (1 << 20)))

#endif
