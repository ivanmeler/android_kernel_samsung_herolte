/* linux/drivers/video/exynos/decon/vpp/vpp_drv.c
 *
 * Copyright (c) 2014 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Samsung EXYNOS5 SoC series VPP driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published
 * by the Free Software Foundation, either version 2 of the License,
 * or (at your option) any later version.
 */

#include <linux/module.h>
#include <linux/clk-provider.h>
#include <linux/clk.h>
#include <linux/of.h>
#include <linux/exynos_iovmm.h>
#include <linux/smc.h>
#include <linux/export.h>
#include <linux/videodev2_exynos_media.h>

#include "vpp.h"
#include "vpp_common.h"
#include "../decon_helper.h"

/*
 * Gscaler constraints
 * This is base of without rotation.
 */

#define CONFIG_MACH_VELOCE8890

#define check_align(width, height, align_w, align_h)\
	(IS_ALIGNED(width, align_w) && IS_ALIGNED(height, align_h))
#define is_err_irq(irq) ((irq == VG_IRQ_DEADLOCK_STATUS) ||\
			(irq == VG_IRQ_READ_SLAVE_ERROR))

#define MIF_LV1			(2912000/2)
#define INT_LV7			(400000)
#define FRAMEDONE_TIMEOUT	msecs_to_jiffies(30)

#define MEM_FAULT_VPP_MASTER            0
#define MEM_FAULT_VPP_CFW               1
#define MEM_FAULT_PROT_EXCEPT_0         2
#define MEM_FAULT_PROT_EXCEPT_1         3
#define MEM_FAULT_PROT_EXCEPT_2         4
#define MEM_FAULT_PROT_EXCEPT_3         5

struct vpp_dev *vpp0_for_decon;
EXPORT_SYMBOL(vpp0_for_decon);
struct vpp_dev *vpp1_for_decon;
EXPORT_SYMBOL(vpp1_for_decon);
struct vpp_dev *vpp2_for_decon;
EXPORT_SYMBOL(vpp2_for_decon);
struct vpp_dev *vpp3_for_decon;
EXPORT_SYMBOL(vpp3_for_decon);
struct vpp_dev *vpp4_for_decon;
EXPORT_SYMBOL(vpp4_for_decon);
struct vpp_dev *vpp5_for_decon;
EXPORT_SYMBOL(vpp5_for_decon);
struct vpp_dev *vpp6_for_decon;
EXPORT_SYMBOL(vpp6_for_decon);
struct vpp_dev *vpp7_for_decon;
EXPORT_SYMBOL(vpp7_for_decon);
struct vpp_dev *vpp8_for_decon;
EXPORT_SYMBOL(vpp8_for_decon);

static void vpp_dump_cfw_register(void)
{
	u32 smc_val;
	/* FIXME */
	return;
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_VPP_MASTER, 0, 0);
	pr_err("=== vpp_master:0x%x\n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_VPP_CFW, 0, 0);
	pr_err("=== vpp_cfw:0x%x\n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_0, 0, 0);
	pr_err("=== vpp_except_0:0x%x\n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_1, 0, 0);
	pr_err("=== vpp_except_1:0x%x\n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_2, 0, 0);
	pr_err("=== vpp_except_2:0x%x\n", smc_val);
	smc_val = exynos_smc(0x810000DE, MEM_FAULT_PROT_EXCEPT_3, 0, 0);
	pr_err("=== vpp_except_3:0x%x\n", smc_val);
}

static void vpp_dump_registers(struct vpp_dev *vpp)
{
	vpp_dump_cfw_register();
	dev_info(DEV, "=== VPP%d SFR DUMP ===\n", vpp->id);
	dev_info(DEV, "start count : %d, done count : %d\n",
			vpp->start_count, vpp->done_count);

	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs, 0x90, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0xA00, 0x8, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0xA48, 0x10, false);
	print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			vpp->regs + 0xB00, 0x100, false);
}

void vpp_op_timer_handler(unsigned long arg)
{
	struct vpp_dev *vpp = (struct vpp_dev *)arg;
	struct decon_device *decon = get_decon_drvdata(0);

	if(decon->ignore_vsync)
	{
		dev_info(DEV, "VPP[%d] irq hasn't been occured but ignore_vsync is true skip", vpp->id);
		return;
	}

	vpp_dump_registers(vpp);

	dev_info(DEV, "VPP[%d] irq hasn't been occured", vpp->id);
}

static int vpp_wait_for_framedone(struct vpp_dev *vpp)
{
	int done_cnt;
	int ret;

	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		done_cnt = vpp->done_count;
		dev_dbg(DEV, "%s (%d, %d)\n", __func__,
				done_cnt, vpp->done_count);
		ret = wait_event_interruptible_timeout(vpp->framedone_wq,
				(done_cnt != vpp->done_count),
				FRAMEDONE_TIMEOUT);
		if (ret == 0) {
			dev_dbg(DEV, "timeout of frame done(st:%d, %d, do:%d)\n",
				vpp->start_count, done_cnt, vpp->done_count);
			return -ETIMEDOUT;
		}
	}
	return 0;
}

static void vpp_separate_fraction_value(struct vpp_dev *vpp,
		int *integer, u32 *fract_val)
{
	struct decon_win_config *config = vpp->config;
	if (is_vpp_rgb32(config) || is_yuv420(config)) {
		/*
		 * [30:15] : fraction val, [14:0] : integer val.
		 */
		*fract_val = (*integer >> 15) << 4;
		if (*fract_val & ~VG_POSITION_F_MASK) {
			dev_warn(DEV, "%d is unsupported value",
					*fract_val);
			*fract_val &= VG_POSITION_F_MASK;
		}

		*integer = (*integer & 0x7fff);
	} else
		dev_err(DEV, "0x%x format did not support fraction\n",
				config->format);
}

static void vpp_set_initial_phase(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct vpp_fraction *fr = &vpp->fract_val;

	if (is_fraction(src->x)) {
		vpp_separate_fraction_value(vpp, &src->x, &fr->y_x);
		fr->c_x = is_yuv(config) ? fr->y_x / 2 : fr->y_x;
	}

	if (is_fraction(src->y)) {
		vpp_separate_fraction_value(vpp, &src->y, &fr->y_y);
		fr->c_y = is_yuv(config) ? fr->y_y / 2 : fr->y_y;
	}

	if (is_fraction(src->w)) {
		vpp_separate_fraction_value(vpp, &src->w, &fr->w);
		src->w++;
	}

	if (is_fraction(src->h)) {
		vpp_separate_fraction_value(vpp, &src->h, &fr->h);
		src->h++;
	}
}

static int vpp_check_size(struct vpp_dev *vpp, struct vpp_img_format *vi)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct vpp_size_constraints vc;

	vpp_constraints_params(&vc, vi);

	if ((!check_align(src->x, src->y, vc.src_mul_x, vc.src_mul_y)) ||
	   (!check_align(src->f_w, src->f_h, vc.src_mul_w, vc.src_mul_h)) ||
	   (!check_align(src->w, src->h, vc.img_mul_w, vc.img_mul_h)) ||
	   (!check_align(dst->w, dst->h, vc.sca_mul_w, vc.sca_mul_h))) {
		dev_err(DEV, "Alignment error\n");
		goto err;
	}

	if (src->w > vc.src_w_max || src->w < vc.src_w_min ||
		src->h > vc.src_h_max || src->h < vc.src_h_min) {
		dev_err(DEV, "Unsupported source size\n");
		goto err;
	}

	if (dst->w > vc.sca_w_max || dst->w < vc.sca_w_min ||
		dst->h > vc.sca_h_max || dst->h < vc.sca_h_min) {
		dev_err(DEV, "Unsupported dest size\n");
		goto err;
	}

	return 0;
err:
	dev_err(DEV, "offset x : %d, offset y: %d\n", src->x, src->y);
	dev_err(DEV, "src_mul_x : %d, src_mul_y : %d\n", vc.src_mul_x, vc.src_mul_y);
	dev_err(DEV, "src f_w : %d, src f_h: %d\n", src->f_w, src->f_h);
	dev_err(DEV, "src_mul_w : %d, src_mul_h : %d\n", vc.src_mul_w, vc.src_mul_h);
	dev_err(DEV, "src w : %d, src h: %d\n", src->w, src->h);
	dev_err(DEV, "img_mul_w : %d, img_mul_h : %d\n", vc.img_mul_w, vc.img_mul_h);
	dev_err(DEV, "dst w : %d, dst h: %d\n", dst->w, dst->h);
	dev_err(DEV, "sca_mul_w : %d, sca_mul_h : %d\n", vc.sca_mul_w, vc.sca_mul_h);
	dev_err(DEV, "rotation : %d, color_format : %d\n",
				config->vpp_parm.rot, config->format);

	return -EINVAL;
}

static int vpp_check_scale_ratio(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	u32 sc_up_max_w;
	u32 sc_up_max_h;
	u32 sc_down_min_w;
	u32 sc_down_min_h;

	if (is_rotation(config)) {
		sc_up_max_w = config->dst.h << 1;
		sc_up_max_h = config->dst.w << 1;
		sc_down_min_w = config->src.h >> 3;
		sc_down_min_h = config->src.w >> 3;
	} else {
		sc_up_max_w = config->dst.w << 1;
		sc_up_max_h = config->dst.h << 1;
		sc_down_min_w = config->src.w >> 3;
		sc_down_min_h = config->src.h >> 3;
	}

	if (src->w > sc_up_max_w || src->h > sc_up_max_h) {
		dev_err(DEV, "Unsupported max(2x) scale ratio\n");
		goto err;
	}

	if (dst->w < sc_down_min_w || dst->h < sc_down_min_h) {
		dev_err(DEV, "Unsupported min(1/8x) scale ratio\n");
		goto err;
	}

	return 0;
err:
	dev_err(DEV, "src w : %d, src h: %d\n", src->w, src->h);
	dev_err(DEV, "dst w : %d, dst h: %d\n", dst->w, dst->h);
	return -EINVAL;
}

static int vpp_set_scale_info(struct vpp_dev *vpp, struct vpp_size_param *p)
{
	struct decon_win_config *config = vpp->config;

	if (vpp_check_scale_ratio(vpp))
		return -EINVAL;
	vpp_reg_set_scale_ratio(vpp->id, p, is_rotation(config));

	return 0;
}

static int vpp_check_rotation(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	enum vpp_rotate rot = config->vpp_parm.rot;
	if (!is_vgr(vpp) && (rot > VPP_ROT_NORMAL)) {
		dev_err(DEV, "vpp-%d can't rotate\n", vpp->id);
		return -EINVAL;
	}

	return 0;
}

static int vpp_clk_enable(struct vpp_dev *vpp)
{
	int ret;

	ret = clk_enable(vpp->res.gate);
	if (ret) {
		dev_err(DEV, "Failed res.gate clk enable\n");
		return ret;
	}

	return ret;
}

static void vpp_clk_disable(struct vpp_dev *vpp)
{
	clk_disable(vpp->res.gate);
}

static int vpp_init(struct vpp_dev *vpp)
{
	int ret = 0;

	if (vpp->id == 0 || vpp->id == 2) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G0, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp0: %d\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 1 || vpp->id == 3) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G1, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp0: %d)\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 4 || vpp->id == 6) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_VGR0, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp1: %d\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 5 || vpp->id == 7 || vpp->id == 8) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G3, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp1: %d)\n", ret);
			return -EBUSY;
		}
	}

	ret = vpp_clk_enable(vpp);
	if (ret)
		return ret;

	vpp_reg_init(vpp->id);
	vpp->h_ratio = vpp->v_ratio = 0;
	vpp->fract_val.y_x = vpp->fract_val.y_y = 0;
	vpp->fract_val.c_x = vpp->fract_val.c_y = 0;

	vpp->start_count = 0;
	vpp->done_count = 0;

	vpp->prev_read_order = SYSMMU_PBUFCFG_ASCENDING;

	set_bit(VPP_POWER_ON, &vpp->state);

	return 0;
}

static int vpp_deinit(struct vpp_dev *vpp, bool do_sw_reset)
{
	clear_bit(VPP_POWER_ON, &vpp->state);

	vpp_reg_deinit(vpp->id, do_sw_reset);
	vpp_clk_disable(vpp);

	return 0;
}

static bool vpp_check_block_mode(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	u32 b_w = config->block_area.w;
	u32 b_h = config->block_area.h;

	if (config->vpp_parm.rot != VPP_ROT_NORMAL)
		return false;
	if (is_scaling(vpp))
		return false;
	if (!is_rgb(config))
		return false;
	if (b_w < BLK_WIDTH_MIN || b_h < BLK_HEIGHT_MIN)
		return false;

	return true;
}

static int vpp_set_read_order(struct vpp_dev *vpp)
{
	int ret = 0;
	u32 cur_read_order;

	switch (vpp->config->vpp_parm.rot) {
	case VPP_ROT_NORMAL:
	case VPP_ROT_YFLIP:
	case VPP_ROT_90_YFLIP:
	case VPP_ROT_270:
		cur_read_order = SYSMMU_PBUFCFG_ASCENDING;
		break;
	case VPP_ROT_XFLIP:
	case VPP_ROT_180:
	case VPP_ROT_90:
	case VPP_ROT_90_XFLIP:
		cur_read_order = SYSMMU_PBUFCFG_DESCENDING;
		break;
	default:
		BUG();
	}

	if (cur_read_order != vpp->prev_read_order) {
		u32 ipoption[vpp->pbuf_num];
		int i;

		if (cur_read_order == SYSMMU_PBUFCFG_ASCENDING) {
			ipoption[0] = SYSMMU_PBUFCFG_ASCENDING_INPUT;
			ipoption[1] = SYSMMU_PBUFCFG_ASCENDING_INPUT;
		} else {
			ipoption[0] = SYSMMU_PBUFCFG_DESCENDING_INPUT;
			ipoption[1] = SYSMMU_PBUFCFG_DESCENDING_INPUT;
		}

		for (i = 2; i < vpp->pbuf_num; i++)
			ipoption[i] = SYSMMU_PBUFCFG_ASCENDING_INPUT;

		ret = sysmmu_set_prefetch_buffer_property(&vpp->pdev->dev,
				vpp->pbuf_num, 0, ipoption, NULL);
	}

	vpp->prev_read_order = cur_read_order;

	return ret;
}

void vpp_split_single_plane(struct decon_win_config *config, struct vpp_size_param *p)
{
	switch(config->format) {
	case DECON_PIXEL_FORMAT_NV12N:
		p->addr1 = NV12N_CBCR_BASE(p->addr0, p->src_fw, p->src_fh);
		break;
	case DECON_PIXEL_FORMAT_NV12N_10B:
		p->addr1 = NV12N_10B_CBCR_BASE(p->addr0, p->src_fw, p->src_fh);
	default:
		break;
	}
}

static int vpp_set_config(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct vpp_size_param p;
	struct vpp_img_format vi;
	int ret = -EINVAL;
	unsigned long flags;

	if (test_bit(VPP_STOPPING, &vpp->state)) {
		dev_warn(DEV, "vpp is ongoing stop(%d)\n", vpp->id);
		return 0;
	}

	if (!test_bit(VPP_RUNNING, &vpp->state)) {
		dev_dbg(DEV, "vpp start(%d)\n", vpp->id);
		ret = pm_runtime_get_sync(DEV);
		if (ret < 0) {
			dev_err(DEV, "Failed runtime_get(), %d\n", ret);
			return ret;
		}
		spin_lock_irqsave(&vpp->slock, flags);
		ret = vpp_init(vpp);
		if (ret < 0) {
			dev_err(DEV, "Failed to initiailze clk\n");
			spin_unlock_irqrestore(&vpp->slock, flags);
			pm_runtime_put_sync(DEV);
			return ret;
		}
		/* The state need to be set here to handle err case */
		set_bit(VPP_RUNNING, &vpp->state);
		spin_unlock_irqrestore(&vpp->slock, flags);
		enable_irq(vpp->irq);
	}

	vpp_reg_wait_pingpong_clear(vpp->id);

	vpp_set_initial_phase(vpp);
	vpp_to_scale_params(vpp, &p);
	ret = vpp_set_scale_info(vpp, &p);
	if (ret)
		goto err;
	vpp->h_ratio = p.vpp_h_ratio;
	vpp->v_ratio = p.vpp_v_ratio;

	vpp_select_format(vpp, &vi);
	vpp_reg_set_rgb_type(vpp->id, config->vpp_parm.eq_mode);

	if (config->compression && is_rotation(config)) {
		dev_err(DEV,"Unsupported rotation when using afbc enable");
		return -EINVAL;
	}

	ret = vpp_reg_set_in_format(vpp->id, &vi);
	if (ret)
		goto err;

	ret = vpp_check_size(vpp, &vi);
	if (ret)
		goto err;

	ret = vpp_check_rotation(vpp);
	if (ret)
		goto err;

	vpp_reg_set_in_size(vpp->id, &p);

	config->src.w = p.src_w;
	config->src.h = p.src_h;
	vpp_reg_set_out_size(vpp->id, config->dst.w, config->dst.h);

	vpp_reg_set_rotation(vpp->id, &p);
	if (is_vgr(vpp)) {
		ret = vpp_set_read_order(vpp);
		if (ret)
			goto err;
	}

	vpp_split_single_plane(config, &p);
	vpp_reg_set_in_buf_addr(vpp->id, &p, &vi);
	vpp_reg_set_smart_if_pix_num(vpp->id, config->dst.w, config->dst.h);

	if (vpp_check_block_mode(vpp))
		vpp_reg_set_in_block_size(vpp->id, true, &p);
	else
		vpp_reg_set_in_block_size(vpp->id, false, &p);

	vpp->op_timer.expires = (jiffies + 1 * HZ);
	mod_timer(&vpp->op_timer, vpp->op_timer.expires);

	vpp->start_count++;

	DISP_SS_EVENT_LOG(DISP_EVT_VPP_WINCON, vpp->sd, ktime_set(0, 0));
	return 0;
err:
	dev_err(DEV, "failed to set config\n");
	return ret;
}

static int vpp_tui_protection(struct v4l2_subdev *sd, int enable)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);
	int ret;

	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		dev_err(DEV, "VPP is not ready for TUI (%ld)\n", vpp->state);
		return -EBUSY;
	}

	if (enable)
		ret = vpp_clk_enable(vpp);
	else
		vpp_clk_disable(vpp);

	return ret;
}

static long vpp_subdev_ioctl(struct v4l2_subdev *sd,
				unsigned int cmd, void *arg)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);
	int ret = 0;
	unsigned long flags;
	unsigned long state = (unsigned long)arg;
	bool need_reset;
	BUG_ON(!vpp);

	mutex_lock(&vpp->mlock);
	switch (cmd) {
	case VPP_WIN_CONFIG:
		vpp->config = (struct decon_win_config *)arg;
		ret = vpp_set_config(vpp);
		if (ret)
			dev_err(DEV, "Failed vpp-%d configuration\n",
					vpp->id);
		break;

	case VPP_STOP:
		if (!test_bit(VPP_RUNNING, &vpp->state)) {
			dev_warn(DEV, "vpp-%d is already stopped\n",
					vpp->id);
			goto err;
		}
		set_bit(VPP_STOPPING, &vpp->state);
		if (state != VPP_STOP_ERR) {
			ret = vpp_reg_wait_op_status(vpp->id);
			if (ret < 0) {
				dev_err(DEV, "%s : vpp-%d is working\n",
						__func__, vpp->id);
				goto err;
			}
		}
		need_reset = (state > 0) ? true : false;
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_STOP, vpp->sd, ktime_set(0, 0));
		clear_bit(VPP_RUNNING, &vpp->state);
		disable_irq(vpp->irq);
		spin_lock_irqsave(&vpp->slock, flags);
		del_timer(&vpp->op_timer);
		vpp_deinit(vpp, need_reset);
		spin_unlock_irqrestore(&vpp->slock, flags);
		call_bts_ops(vpp, bts_set_zero_bw, vpp);

		pm_runtime_put_sync(DEV);
		dev_dbg(DEV, "vpp stop(%d)\n", vpp->id);
		clear_bit(VPP_STOPPING, &vpp->state);
		break;

	case VPP_TUI_PROTECT:
		ret = vpp_tui_protection(sd, state);
		break;

	case VPP_GET_BTS_VAL:
		vpp->config = (struct decon_win_config *)arg;
		call_bts_ops(vpp, bts_get_bw, vpp);
		break;

	case VPP_SET_BW:
		vpp->config = (struct decon_win_config *)arg;
		call_bts_ops(vpp, bts_set_calc_bw, vpp);
		break;

	case VPP_SET_ROT_MIF:
		vpp->config = (struct decon_win_config *)arg;
		call_bts_ops(vpp, bts_set_rot_mif, vpp);
		break;

	case VPP_DUMP:
		vpp_dump_registers(vpp);
		break;

	case VPP_WAIT_IDLE:
		if (test_bit(VPP_RUNNING, &vpp->state))
			vpp_reg_wait_idle(vpp->id);
		break;

	case VPP_WAIT_FOR_FRAMEDONE:
		ret = vpp_wait_for_framedone(vpp);
		break;

	default:
		break;
	}

err:
	mutex_unlock(&vpp->mlock);

	return ret;
}

static int vpp_sd_open(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	dev_dbg(DEV, "vpp%d is opened\n", vpp->id);

	return 0;
}

static int vpp_sd_close(struct v4l2_subdev *sd, struct v4l2_subdev_fh *fh)
{
	struct vpp_dev *vpp = v4l2_get_subdevdata(sd);

	dev_dbg(DEV, "vpp%d is closed\n", vpp->id);

	return 0;
}

static int vpp_link_setup(struct media_entity *entity,
			const struct media_pad *local,
			const struct media_pad *remote, u32 flags)
{
	return 0;
}

static const struct media_entity_operations vpp_media_ops = {
	.link_setup = vpp_link_setup,
};

static const struct v4l2_subdev_internal_ops vpp_internal_ops = {
	.open = vpp_sd_open,
	.close = vpp_sd_close,
};

static const struct v4l2_subdev_core_ops vpp_subdev_core_ops = {
	.ioctl = vpp_subdev_ioctl,
};

static struct v4l2_subdev_ops vpp_subdev_ops = {
	.core = &vpp_subdev_core_ops,
};

static int vpp_find_media_device(struct vpp_dev *vpp)
{
	struct exynos_md *md;

	md = (struct exynos_md *)module_name_to_driver_data(MDEV_MODULE_NAME);
	if (!md) {
		decon_err("failed to get output media device\n");
		return -ENODEV;
	}
	vpp->mdev = md;

	return 0;
}

static int vpp_create_subdev(struct vpp_dev *vpp)
{
	struct v4l2_subdev *sd;
	int ret;

	sd = kzalloc(sizeof(*sd), GFP_KERNEL);
	if (!sd)
		return -ENOMEM;

	v4l2_subdev_init(sd, &vpp_subdev_ops);

	vpp->pad.flags = MEDIA_PAD_FL_SOURCE;
	sd->flags = V4L2_SUBDEV_FL_HAS_DEVNODE;
	snprintf(sd->name, sizeof(sd->name), "%s.%d", "vpp-sd", vpp->id);
	sd->grp_id = vpp->id;
	ret = media_entity_init(&sd->entity, VPP_PADS_NUM,
				&vpp->pad, 0);
	if (ret) {
		dev_err(DEV, "Failed to initialize VPP media entity");
		goto error;
	}

	sd->entity.ops = &vpp_media_ops;
	sd->internal_ops = &vpp_internal_ops;

	ret = v4l2_device_register_subdev(&vpp->mdev->v4l2_dev, sd);
	if (ret) {
		media_entity_cleanup(&sd->entity);
		goto error;
	}

	vpp->mdev->vpp_sd[vpp->id] = sd;
	vpp->mdev->vpp_dev[vpp->id] = &vpp->pdev->dev;
	dev_info(DEV, "vpp_sd[%d] = %08lx\n", vpp->id,
			(ulong)vpp->mdev->vpp_sd[vpp->id]);

	vpp->sd = sd;
	v4l2_set_subdevdata(sd, vpp);

	return 0;
error:
	kfree(sd);
	return ret;
}

static int vpp_resume(struct device *dev)
{
	return 0;
}

static int vpp_suspend(struct device *dev)
{
	return 0;
}

static irqreturn_t vpp_irq_handler(int irq, void *priv)
{
	struct vpp_dev *vpp = priv;
	int vpp_irq = 0;

	DISP_SS_EVENT_START();
	spin_lock(&vpp->slock);
	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		vpp_irq = vpp_reg_get_irq_status(vpp->id);
		vpp_reg_set_clear_irq(vpp->id, vpp_irq);

		if (is_err_irq(vpp_irq)) {
			dev_err(DEV, "Error interrupt (0x%x)\n", vpp_irq);
			vpp_dump_registers(vpp);
			exynos_sysmmu_show_status(&vpp->pdev->dev);
			goto err;
		}
	}

	if (vpp_irq & VG_IRQ_FRAMEDONE) {
		vpp->done_count++;
		wake_up_interruptible_all(&vpp->framedone_wq);
		DISP_SS_EVENT_LOG(DISP_EVT_VPP_FRAMEDONE, vpp->sd, start);
	}

	dev_dbg(DEV, "irq status : 0x%x\n", vpp_irq);
err:
	del_timer(&vpp->op_timer);
	spin_unlock(&vpp->slock);

	return IRQ_HANDLED;
}
static void vpp_clk_info(struct vpp_dev *vpp)
{
#ifndef CONFIG_MACH_VELOCE8890
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.gate),
				clk_get_rate(vpp->res.gate) / MHZ);
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.pclk_vpp),
				clk_get_rate(vpp->res.pclk_vpp) / MHZ);
	dev_info(DEV, "%s: %ld Mhz\n", __clk_get_name(vpp->res.lh_vpp),
				clk_get_rate(vpp->res.lh_vpp) / MHZ);
#endif
}

static int vpp_clk_get(struct vpp_dev *vpp)
{
	struct device *dev = &vpp->pdev->dev;
	int ret;

	vpp->res.gate = devm_clk_get(dev, "vpp_clk");
	if (IS_ERR(vpp->res.gate)) {
		dev_err(dev, "vpp-%d clock get failed\n", vpp->id);
		return PTR_ERR(vpp->res.gate);
	}

	ret = clk_prepare(vpp->res.gate);
	if (ret < 0) {
		dev_err(dev, "vpp-%d clock prepare failed\n", vpp->id);
		return ret;
	}
	return ret;
}

static void vpp_clk_put(struct vpp_dev *vpp)
{
	clk_unprepare(vpp->res.gate);
	clk_put(vpp->res.gate);
}

int vpp_sysmmu_fault_handler(struct iommu_domain *domain,
	struct device *dev, unsigned long iova, int flags, void *token)
{
	struct vpp_dev *vpp = dev_get_drvdata(dev);

	if (test_bit(VPP_POWER_ON, &vpp->state)) {
		dev_info(DEV, "vpp%d sysmmu fault handler\n", vpp->id);
		vpp_dump_registers(vpp);
	}

	return 0;
}

static void vpp_config_id(struct vpp_dev *vpp)
{
	switch (vpp->id) {
	case 0:
		vpp0_for_decon = vpp;
		break;
	case 1:
		vpp1_for_decon = vpp;
		break;
	case 2:
		vpp2_for_decon = vpp;
		break;
	case 3:
		vpp3_for_decon = vpp;
		break;
	case 4:
		vpp4_for_decon = vpp;
		break;
	case 5:
		vpp5_for_decon = vpp;
		break;
	case 6:
		vpp6_for_decon = vpp;
		break;
	case 7:
		vpp7_for_decon = vpp;
		break;
	case 8:
		vpp8_for_decon = vpp;
		break;
	default:
		dev_err(DEV, "Failed to find vpp id(%d)\n", vpp->id);
	}
}

static int vpp_probe(struct platform_device *pdev)
{
	struct device *dev = &pdev->dev;
	struct vpp_dev *vpp;
	struct resource *res;
	int irq;
	int vpp_irq = 0;
	int ret = 0;

	vpp = devm_kzalloc(dev, sizeof(*vpp), GFP_KERNEL);
	if (!vpp) {
		dev_err(dev, "Failed to allocate local vpp mem\n");
		return -ENOMEM;
	}

	vpp->id = of_alias_get_id(dev->of_node, "vpp");

	pr_info("###%s:VPP%d probe : start\n", __func__, vpp->id);
	of_property_read_u32(dev->of_node, "#ar-id-num", &vpp->pbuf_num);

	vpp->pdev = pdev;

	vpp_config_id(vpp);

	if (vpp->id == 0 || vpp->id == 2) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G0, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp0: %d\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 1 || vpp->id == 3) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G1, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp0: %d)\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 4 || vpp->id == 6) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_VGR0, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp1: %d\n", ret);
			return -EBUSY;
		}
	}

	if (vpp->id == 5 || vpp->id == 7 || vpp->id == 8) {
		ret = exynos_smc(MC_FC_SET_CFW_PROT,
				MC_FC_DRM_SET_CFW_PROT, PROT_G3, 0);
		if (ret != 2) {
			vpp_err("smc call fail for vpp1: %d)\n", ret);
			return -EBUSY;
		}
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	vpp->regs = devm_ioremap_resource(dev, res);
	if (!vpp->regs) {
		dev_err(DEV, "Failed to map registers\n");
		ret = -EADDRNOTAVAIL;
		return ret;
	}

	irq = platform_get_irq(pdev, 0);
	if (irq < 0) {
		dev_err(DEV, "Failed to get IRQ resource\n");
		return irq;
	}

	pm_runtime_enable(dev);

	ret = pm_runtime_get_sync(DEV);
	if (ret < 0) {
		dev_err(DEV, "Failed runtime_get(), %d\n", ret);
		return ret;
	}

	ret = vpp_clk_enable(vpp);
	if (ret)
		return ret;

	vpp_irq = vpp_reg_get_irq_status(vpp->id);
	vpp_reg_set_clear_irq(vpp->id, vpp_irq);

	ret = vpp_reg_wait_op_status(vpp->id);
	if (ret < 0) {
		dev_err(dev, "%s : vpp-%d is working\n",
				__func__, vpp->id);
		return ret;
	}

	vpp_clk_disable(vpp);

	pm_runtime_put_sync(DEV);

	spin_lock_init(&vpp->slock);

	ret = devm_request_irq(dev, irq, vpp_irq_handler,
				0, pdev->name, vpp);
	if (ret) {
		dev_err(DEV, "Failed to install irq\n");
		return ret;
	}

	vpp->irq = irq;
	disable_irq(vpp->irq);

	ret = vpp_find_media_device(vpp);
	if (ret) {
		dev_err(DEV, "Failed find media device\n");
		return ret;
	}

	ret = vpp_create_subdev(vpp);
	if (ret) {
		dev_err(DEV, "Failed create sub-device\n");
		return ret;
	}

	ret = vpp_clk_get(vpp);
	if (ret) {
		dev_err(DEV, "Failed to get clk\n");
		return ret;
	}

	vpp_clk_info(vpp);

	init_waitqueue_head(&vpp->stop_queue);
	init_waitqueue_head(&vpp->framedone_wq);

	platform_set_drvdata(pdev, vpp);

	ret = iovmm_activate(dev);
	if (ret < 0) {
		dev_err(DEV, "failed to reactivate vmm\n");
		return ret;
	}

	setup_timer(&vpp->op_timer, vpp_op_timer_handler,
			(unsigned long)vpp);

	vpp->bts_ops = &decon_bts_control;

	pm_qos_add_request(&vpp->vpp_mif_qos, PM_QOS_BUS_THROUGHPUT, 0);

	mutex_init(&vpp->mlock);

	iovmm_set_fault_handler(dev, vpp_sysmmu_fault_handler, NULL);

	dev_info(DEV, "VPP%d is probed successfully\n", vpp->id);

	return 0;
}

static int vpp_remove(struct platform_device *pdev)
{
	struct vpp_dev *vpp =
		(struct vpp_dev *)platform_get_drvdata(pdev);

	iovmm_deactivate(&vpp->pdev->dev);
	vpp_clk_put(vpp);

	pm_qos_remove_request(&vpp->vpp_mif_qos);

	dev_info(DEV, "%s driver unloaded\n", pdev->name);

	return 0;
}

static const struct of_device_id vpp_device_table[] = {
	{
		.compatible = "samsung,exynos7-vpp",
	},
	{},
};

static const struct dev_pm_ops vpp_pm_ops = {
	.suspend		= vpp_suspend,
	.resume			= vpp_resume,
};

static struct platform_driver vpp_driver __refdata = {
	.probe		= vpp_probe,
	.remove		= vpp_remove,
	.driver = {
		.name	= "exynos-vpp",
		.owner	= THIS_MODULE,
		.pm	= &vpp_pm_ops,
		.of_match_table = of_match_ptr(vpp_device_table),
	}
};

static int vpp_register(void)
{
	return platform_driver_register(&vpp_driver);
}

device_initcall_sync(vpp_register);

MODULE_AUTHOR("Sungchun, Kang <sungchun.kang@samsung.com>");
MODULE_DESCRIPTION("Samsung EXYNOS Soc VPP driver");
MODULE_LICENSE("GPL");
