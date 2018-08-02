/* linux/drivers/video/exynos/decon/vpp_core.h
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * header file for Samsung EXYNOS5 SoC series VPP driver

 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef VPP_H_
#define VPP_H_

#include <linux/delay.h>
#include <linux/sched.h>
#include <linux/spinlock.h>
#include <linux/types.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/pm_runtime.h>
#include <linux/pm_qos.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/exynos_iovmm.h>
#include <media/exynos_mc.h>
#include <soc/samsung/bts.h>

#include "../decon.h"
#include "regs-vpp.h"
#include "vpp_common.h"

#define VPP_PADS_NUM	1
#define DEV		(&vpp->pdev->dev)

#define is_rotation(config) (config->vpp_parm.rot >= VPP_ROT_90)
#define is_normal(config) (VPP_ROT_NORMAL)
#define is_yuv(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) \
			&& (config->format < DECON_PIXEL_FORMAT_MAX))
#define is_yuv422(config) ((config->format >= DECON_PIXEL_FORMAT_NV16) \
			&& (config->format <= DECON_PIXEL_FORMAT_YVU422_3P))
#define is_yuv420(config) ((config->format >= DECON_PIXEL_FORMAT_NV12) \
			&& (config->format <= DECON_PIXEL_FORMAT_YVU420M))
#define is_rgb(config) ((config->format >= DECON_PIXEL_FORMAT_ARGB_8888) \
			&& (config->format <= DECON_PIXEL_FORMAT_RGB_565))
#define is_rgb16(config) ((config->format == DECON_PIXEL_FORMAT_RGB_565))
#define is_vpp_rgb32(config) ((config->format >= DECON_PIXEL_FORMAT_ARGB_8888) \
		&& (config->format <= DECON_PIXEL_FORMAT_BGRX_8888))
#define is_ayv12(config) (config->format == DECON_PIXEL_FORMAT_YVU420)
#define is_fraction(x) ((x) >> 15)
#define is_vpp0_series(vpp) ((vpp->id == 0 || vpp->id == 1 \
				|| vpp->id == 2 || vpp->id == 3))
#define is_vgr(vpp) ((vpp->id == 6) || (vpp->id == 7))
#define is_vgr1(vpp) (vpp->id == 7)
#define is_g(vpp) ((vpp->id == 0) || (vpp->id == 1) \
				|| (vpp->id == 4) || (vpp->id == 5))
#define is_wb(vpp) (vpp->id == 8)
#define is_scaling(vpp) ((vpp->h_ratio != (1 << 20)) \
				|| (vpp->v_ratio != (1 << 20)))
#define is_scale_down(vpp) ((vpp->h_ratio > (1 << 20)) \
				|| (vpp->v_ratio > (1 << 20)))

#define vpp_err(fmt, ...)					\
	do {							\
		pr_err(pr_fmt(fmt), ##__VA_ARGS__);		\
		exynos_ss_printk(fmt, ##__VA_ARGS__);		\
	} while (0)

#define vpp_info(fmt, ...)					\
	pr_info(pr_fmt(fmt), ##__VA_ARGS__);		\

#define vpp_dbg(fmt, ...)					\
	pr_debug(pr_fmt(fmt), ##__VA_ARGS__);		\

enum vpp_dev_state {
	VPP_RUNNING,
	VPP_POWER_ON,
	VPP_STOPPING,
};

struct vpp_resources {
	struct clk *gate;
};

struct vpp_fraction {
	u32 y_x;
	u32 y_y;
	u32 c_x;
	u32 c_y;
	u32 w;
	u32 h;
};

struct vpp_minlock_entry {
	bool rotation;
	u32 scalefactor;
	u32 dvfsfreq;
};

struct vpp_minlock_table {
	struct list_head node;
	u16 width;
	u16 height;
	int num_entries;
	struct vpp_minlock_entry entries[0];
};

struct vpp_dev {
	int				id;
	struct platform_device		*pdev;
	spinlock_t			slock;
	struct media_pad		pad;
	struct exynos_md		*mdev;
	struct v4l2_subdev		*sd;
	void __iomem			*regs;
	unsigned long			state;
	struct clk			*gate_clk;
	struct vpp_resources		res;
	wait_queue_head_t		stop_queue;
	wait_queue_head_t		framedone_wq;
	struct timer_list		op_timer;
	u32				start_count;
	u32				done_count;
	struct decon_win_config		*config;
	struct pm_qos_request		*vpp_disp_qos;
	struct pm_qos_request		*vpp_int_qos;
	struct pm_qos_request		vpp_mif_qos;
	u32				h_ratio;
	u32				v_ratio;
	struct vpp_fraction		fract_val;
	struct mutex			mlock;
	unsigned int			irq;
	u32				prev_read_order;
	u32				pbuf_num;
	u64				disp_cur_bw;
	u64				cur_bw;
	u64				prev_bw;
	u32				sc_w;
	u32				sc_h;
	struct decon_bts		*bts_ops;
#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
	struct bts_vpp_info		bts_info;
	/* current each VPP's DISP INT level */
	u64				cur_disp;
	u64				shw_disp;
#endif
	bool				afbc_re;
};

extern struct vpp_dev *vpp0_for_decon;
extern struct vpp_dev *vpp1_for_decon;
extern struct vpp_dev *vpp2_for_decon;
extern struct vpp_dev *vpp3_for_decon;
extern struct vpp_dev *vpp4_for_decon;
extern struct vpp_dev *vpp5_for_decon;
extern struct vpp_dev *vpp6_for_decon;
extern struct vpp_dev *vpp7_for_decon;
extern struct vpp_dev *vpp8_for_decon;

static inline struct vpp_dev *get_vpp_drvdata(u32 id)
{
	switch (id) {
	case 0:
		return vpp0_for_decon;
	case 1:
		return vpp1_for_decon;
	case 2:
		return vpp2_for_decon;
	case 3:
		return vpp3_for_decon;
	case 4:
		return vpp4_for_decon;
	case 5:
		return vpp5_for_decon;
	case 6:
		return vpp6_for_decon;
	case 7:
		return vpp7_for_decon;
	case 8:
		return vpp8_for_decon;
	default:
		return NULL;
	}
}

static inline u32 vpp_read(u32 id, u32 reg_id)
{
	struct vpp_dev *vpp = get_vpp_drvdata(id);
	return readl(vpp->regs + reg_id);
}

static inline u32 vpp_read_mask(u32 id, u32 reg_id, u32 mask)
{
	u32 val = vpp_read(id, reg_id);
	val &= (~mask);
	return val;
}

static inline void vpp_write(u32 id, u32 reg_id, u32 val)
{
	struct vpp_dev *vpp = get_vpp_drvdata(id);
	writel(val, vpp->regs + reg_id);
}

static inline void vpp_write_mask(u32 id, u32 reg_id, u32 val, u32 mask)
{
	struct vpp_dev *vpp = get_vpp_drvdata(id);
	u32 old = vpp_read(id, reg_id);

	val = (val & mask) | (old & ~mask);
	writel(val, vpp->regs + reg_id);
}

static inline void vpp_select_format(struct vpp_dev *vpp,
					struct vpp_img_format *vi)
{
	struct decon_win_config *config = vpp->config;

	vi->vgr = is_vgr(vpp);
	vi->normal = is_normal(vpp);
	vi->rot = is_rotation(config);
	vi->scale = is_scaling(vpp);
	vi->format = config->format;
	vi->afbc_en = config->compression;
	vi->yuv = is_yuv(config);
	vi->yuv422 = is_yuv422(config);
	vi->yuv420 = is_yuv420(config);
	vi->wb = is_wb(vpp);
}

static inline void vpp_to_scale_params(struct vpp_dev *vpp,
					struct vpp_size_param *p)
{
	struct decon_win_config *config = vpp->config;
	struct vpp_params *vpp_parm = &vpp->config->vpp_parm;
	struct vpp_fraction *fr = &vpp->fract_val;

	p->src_x = config->src.x;
	p->src_y = config->src.y;
	p->src_w = config->src.w;
	p->src_h = config->src.h;
	p->fr_w = fr->w;
	p->fr_h = fr->h;
	p->dst_w = config->dst.w;
	p->dst_h = config->dst.h;
	p->vpp_h_ratio = vpp->h_ratio;
	p->vpp_v_ratio = vpp->v_ratio;
	p->src_fw = config->src.f_w;
	p->src_fh = config->src.f_h;
	p->fr_yx = vpp->fract_val.y_x;
	p->fr_yy = vpp->fract_val.y_y;
	p->fr_cx = vpp->fract_val.c_x;
	p->fr_cy = vpp->fract_val.c_y;
	p->rot = config->vpp_parm.rot;
	p->block_x = config->block_area.x;
	p->block_y = config->block_area.y;
	p->block_w = config->block_area.w;
	p->block_h = config->block_area.h;
	p->addr0 = vpp_parm->addr[0];
	p->addr1 = vpp_parm->addr[1];
	p->addr2 = vpp_parm->addr[2];

}
#endif
