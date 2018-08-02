 /*
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * Core file for Samsung EXYNOS DECON driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include "vpp/vpp.h"
#include "decon.h"
#include "decon_helper.h"

#include <soc/samsung/bts.h>
#include <media/v4l2-subdev.h>

#define MULTI_FACTOR 		(1 << 10)
#define VPP_MAX 		9
#define HALF_MIC 		2
#define PIX_PER_CLK 		2
#define ROTATION 		4
#define NOT_ROT 		1
#if !defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
#define RGB_FACTOR 		4
#define YUV_FACTOR 		4
#endif
 /* If use float factor, DATA format factor should be multiflied by 2
 * And then, should be divided using FORMAT_FACTOR at the end of bw_eq
 */
#define FORMAT_FACTOR 		2
#define DISP_FACTOR 		(105 * KHZ / 100)

#define ENUM_OFFSET 		2

#if defined(CONFIG_PM_DEVFREQ) && !defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
void bts_int_mif_bw(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk = (clk_get_rate(decon->res.vclk_leaf) * PIX_PER_CLK / MHZ);
	/* TODO, parse mic factor automatically at dt */
	u32 mic_factor = HALF_MIC; /* 1/2 MIC */
	u64 s_ratio_h, s_ratio_v = 0;
	/* bpl is multiplied 2, it should be divied at end of eq. */
	u8 bpl = 0;
	u8 rot_factor = is_rotation(config) ? ROTATION : NOT_ROT;
	bool is_rotated = is_rotation(config) ? true : false;

	if (is_rgb(config))
		bpl = RGB_FACTOR;
	else if (is_yuv(config))
		bpl = YUV_FACTOR;
	else
		bpl = RGB_FACTOR;

	if (is_rotated) {
		s_ratio_h = MULTI_FACTOR * src->h / dst->w;
		s_ratio_v = MULTI_FACTOR * src->w / dst->h;
	} else {
		s_ratio_h = MULTI_FACTOR * src->w / dst->w;
		s_ratio_v = MULTI_FACTOR * src->h / dst->h;
	}

	/* BW = (VCLK * MIC_factor * Data format * ScaleRatio_H * ScaleRatio_V * RotationFactor) */
	vpp->cur_bw = vclk * mic_factor * bpl * s_ratio_h * s_ratio_v
		* rot_factor * KHZ / (MULTI_FACTOR * MULTI_FACTOR);
}

void bts_disp_bw(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	struct decon_device *decon = get_decon_drvdata(0);
	u32 vclk = (clk_get_rate(decon->res.vclk_leaf) * PIX_PER_CLK / MHZ);
	/* TODO, parse mic factor automatically at dt */
	u32 mic_factor = HALF_MIC; /* 1/2 MIC */
	/* TODO, parse lcd width automatically at dt */
	u32 lcd_width = 1440;
	u64 s_ratio_h, s_ratio_v = 0;

	u32 src_w = is_rotation(config) ? src->h : src->w;
	u32 src_h = is_rotation(config) ? src->w : src->h;

	/* ACLK_DISP_400 [MHz] = (VCLK * MIC_factor * ScaleRatio_H * ScaleRatio_V * disp_factor) / 2 * (DST_width / LCD_width) */
	s_ratio_h = (src_w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * src_w / dst->w;
	s_ratio_v = (src_h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * src_h / dst->h;

	vpp->disp_cur_bw = vclk * mic_factor * s_ratio_h * s_ratio_v
		* DISP_FACTOR * (MULTI_FACTOR * dst->w / lcd_width)
		/ (MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR) / 2;
}

void bts_mif_lock(struct vpp_dev *vpp)
{
	struct decon_win_config *config = vpp->config;

	if (is_rotation(config)) {
		pm_qos_update_request(&vpp->vpp_mif_qos, 1144000);
	} else {
		pm_qos_update_request(&vpp->vpp_mif_qos, 0);
	}
}

void bts_send_bw(struct vpp_dev *vpp, bool enable)
{
	struct decon_win_config *config = vpp->config;
	u8 vpp_type;
	enum vpp_bw_type bw_type;

	if (is_rotation(config)) {
		if (config->src.w * config->src.h >= FULLHD_SRC)
			bw_type = BW_FULLHD_ROT;
		else
			bw_type = BW_ROT;
	} else {
		bw_type = BW_DEFAULT;
	}

	vpp_type = vpp->id + ENUM_OFFSET;

	if (enable) {
		exynos_update_media_scenario(vpp_type, vpp->cur_bw, bw_type);
	} else {
		exynos_update_media_scenario(vpp_type, 0, BW_DEFAULT);
		vpp->prev_bw = vpp->cur_bw = 0;
	}
}

void bts_add_request(struct decon_device *decon)
{
	pm_qos_add_request(&decon->mif_qos, PM_QOS_BUS_THROUGHPUT, 0);
	pm_qos_add_request(&decon->disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
	pm_qos_add_request(&decon->int_qos, PM_QOS_DEVICE_THROUGHPUT, 0);
}

void bts_send_init(struct decon_device *decon)
{
	pm_qos_update_request(&decon->mif_qos, 546000);
}

void bts_send_release(struct decon_device *decon)
{
	pm_qos_update_request(&decon->mif_qos, 0);
	pm_qos_update_request(&decon->disp_qos, 0);
	pm_qos_update_request(&decon->int_qos, 0);
	decon->disp_cur = decon->disp_prev = 0;
}

void bts_remove_request(struct decon_device *decon)
{
	pm_qos_remove_request(&decon->mif_qos);
	pm_qos_remove_request(&decon->disp_qos);
	pm_qos_remove_request(&decon->int_qos);
}

void bts_calc_bw(struct vpp_dev *vpp)
{
	bts_disp_bw(vpp);
	bts_int_mif_bw(vpp);
}

void bts_send_zero_bw(struct vpp_dev *vpp)
{
	bts_send_bw(vpp, false);
}

void bts_send_calc_bw(struct vpp_dev *vpp)
{
	bts_send_bw(vpp, true);
}
#else
void bts_add_request(struct decon_device *decon){ return; }
void bts_send_init(struct decon_device *decon){ return; }
void bts_send_release(struct decon_device *decon){ return; }
void bts_remove_request(struct decon_device *decon){ return; }

void bts_calc_bw(struct vpp_dev *vpp){ return; }
void bts_send_calc_bw(struct vpp_dev *vpp){ return; }
void bts_send_zero_bw(struct vpp_dev *vpp){ return; }
void bts_mif_lock(struct vpp_dev *vpp){ return; }
#endif

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
static struct vpp_dev *get_running_vpp_dev(struct decon_device *decon, int id)
{
	struct vpp_dev *vpp = NULL;
	struct v4l2_subdev *sd = decon->mdev->vpp_sd[id];

	if (sd) {
		vpp = v4l2_get_subdevdata(sd);
		if (test_bit(VPP_RUNNING, &vpp->state))
			return vpp;
	}

	return NULL;
}

static void update_req_vpp_bts_info(struct decon_device *decon)
{
	int id;
	struct vpp_dev *vpp = NULL;

	for (id = 0; id < MAX_VPP_SUBDEV; id++) {
		struct v4l2_subdev *sd = decon->mdev->vpp_sd[id];
		BUG_ON(!sd);
		if (decon->vpp_usage_bitmask & (1 << id)) {
			vpp = v4l2_get_subdevdata(sd);
			vpp->bts_info.shw_cur_bw = vpp->bts_info.cur_bw;
			vpp->bts_info.shw_peak_bw = vpp->bts_info.peak_bw;
			vpp->shw_disp = vpp->cur_disp;
		}
	}
}

/*
*  VPP0(G0), VPP1(G1), VPP2(VG0), VPP3(VG1),
*  VPP4(G2), VPP5(G3), VPP6(VGR0), VPP7(VGR1), VPP8(WB)
*  vpp_bus_bw[0] = DISP0_0_BW = G0 + VG0;
*  vpp_bus_bw[1] = DISP0_1_BW = G1 + VG1;
*  vpp_bus_bw[2] = DISP1_0_BW = G2 + VGR0;
*  vpp_bus_bw[3] = DISP1_1_BW = G3 + VGR1 + WB;
*/
static enum vpp_port_num bts_get_port_number(enum decon_idma_type type)
{
	enum vpp_port_num port_num = VPP_PORT_NUM0;
	switch (type) {
	case IDMA_G0:
	case IDMA_VG0:
		port_num = VPP_PORT_NUM0;
		break;
	case IDMA_G1:
	case IDMA_VG1:
		port_num = VPP_PORT_NUM1;
		break;
	case IDMA_G2:
	case IDMA_VGR0:
		port_num = VPP_PORT_NUM2;
		break;
	case IDMA_G3:
	case IDMA_VGR1:
	case ODMA_WB:
		port_num = VPP_PORT_NUM3;
		break;
	default:
		break;
	}
	return port_num;
}

static void bts2_calc_disp(struct decon_device *decon, struct vpp_dev *vpp,
		struct decon_win_config *config)
{
	struct decon_frame *src = &config->src;
	struct decon_frame *dst = &config->dst;
	u64 s_ratio_h, s_ratio_v;
	u32 src_w = is_rotation(config) ? src->h : src->w;
	u32 src_h = is_rotation(config) ? src->w : src->h;
	u32 lcd_width = decon->lcd_info->xres;

	s_ratio_h = (src_w <= dst->w) ? MULTI_FACTOR : MULTI_FACTOR * src_w / dst->w;
	s_ratio_v = (src_h <= dst->h) ? MULTI_FACTOR : MULTI_FACTOR * src_h / dst->h;

	vpp->cur_disp = decon->vclk_factor * decon->mic_factor * s_ratio_h * s_ratio_v
		* DISP_FACTOR * (MULTI_FACTOR * dst->w / lcd_width)
		/ (MULTI_FACTOR * MULTI_FACTOR * MULTI_FACTOR) / 2;

	decon_bts("VPP_INFO_%d: src w(%d) h(%d), dst w(%d) h(%d)\n",
			vpp->id, src_w, src_h, dst->w, dst->h);
}

static struct bts_vpp_info *bts_get_bts_info
	(struct decon_device *decon, int id, u64 *disp_bw)
{
	int i;
	struct vpp_dev *vpp = get_running_vpp_dev(decon, id);

	if (!vpp) return NULL;

	/* Finding a local connected vpp */
	if (decon->vpp_usage_bitmask & (1 << id)) {
		struct decon_win_config *config = vpp->config;

		bts2_calc_disp(decon, vpp, config);
		*disp_bw = vpp->cur_disp;

		vpp->bts_info.src_w = config->src.w;
		vpp->bts_info.src_h = config->src.h;
		vpp->bts_info.dst_w = config->dst.w;
		vpp->bts_info.dst_h = config->dst.h;
		vpp->bts_info.bpp = decon_get_bpp(config->format);
		vpp->bts_info.is_rotation = is_rotation(config);
		vpp->bts_info.mic = decon->mic_factor;
		vpp->bts_info.pix_per_clk = DECON_PIX_PER_CLK;
		vpp->bts_info.vclk = decon->vclk_factor;
		exynos_bw_calc(vpp->id, &vpp->bts_info.hw);

		return &vpp->bts_info;
	}

	/* Finding a remote connected vpp */
	for (i = 0; i < NUM_DECON_IPS; i++) {
		struct decon_device *other = get_decon_drvdata(i);

		if (other->state == DECON_STATE_OFF || other->id == decon->id)
			continue;

		if (other->vpp_usage_bitmask & (1 << id)) {
			*disp_bw = vpp->shw_disp;
			return &vpp->bts_info;
		}
	}

	return NULL;
}

void bts_rot_scenario_set(struct vpp_dev *vpp, u32 is_after)
{
	u32 rot = 0;

	if (IS_ERR_OR_NULL(vpp) || !is_vgr(vpp))
		return;

	rot = is_rotation(vpp->config) && test_bit(VPP_RUNNING, &vpp->state);
	if ((is_after && !rot) || (!is_after && rot)) {
		bts_ext_scenario_set(vpp->id, TYPE_ROTATION, rot);
		decon_bts("SCEN_SET: vpp-%d, rot(%d)\n", vpp->id, rot);
	}
}

static DEFINE_MUTEX(bts_lock);
void bts2_calc_bw(struct decon_device *decon)
{
	int i, num;
	struct bts_vpp_info *bts;
	u32 peak_bw[VPP_PORT_MAX], mem_sum, bus_sum;
	u64 disp_bw[VPP_PORT_MAX], disp_sum;

	mutex_lock(&bts_lock);
	mem_sum = bus_sum = disp_sum = 0;
	for (i = 0; i < MAX_VPP_SUBDEV; i++) {
		u32 mem_bw = 0, bus_bw = 0; u64 disp_ch_bw = 0;

		num = bts_get_port_number(i);
		bts = bts_get_bts_info(decon, i, &disp_ch_bw);
		if (bts) {
			if (decon->vpp_usage_bitmask & (1 << i)) {
				mem_bw = bts->cur_bw;
				bus_bw = bts->peak_bw;
			} else { /* Remote connected VPP */
				mem_bw = bts->shw_cur_bw;
				bus_bw = bts->shw_peak_bw;
			}
			decon_bts("VPP_BW_%d_%d: mem(%d), bus(%d), disp(%llu)\n",
					num, i, mem_bw, bus_bw, disp_ch_bw);
		}
		/* Get maximum bus & disp bandwidth */
		switch (i) {
		case IDMA_G0:
		case IDMA_G1:
		case IDMA_G2:
		case IDMA_G3:
			peak_bw[num] = bus_bw;
			disp_bw[num] = disp_ch_bw;
			break;
		default:
			peak_bw[num] += bus_bw;
			disp_bw[num] += disp_ch_bw;
			bus_sum  = max(bus_sum, peak_bw[num]);
			disp_sum = max(disp_sum, disp_bw[num]);
			break;
		}
		mem_sum += mem_bw;
	}
	decon->total_bw = mem_sum;
	decon->max_peak_bw = bus_sum;
	decon->max_disp_ch = disp_sum;

	for (i = 0; i < VPP_PORT_MAX; i++)
		if (peak_bw[i] != 0 && disp_bw[i] != 0)
			decon_bts("Port_BW_%d: bus(%d), disp(%llu)\n",
					i, peak_bw[i], disp_bw[i]);
	decon_bts("Result_BW: mem(%d), bus(%d), disp(%llu)\n",
			mem_sum, bus_sum, disp_sum);
	mutex_unlock(&bts_lock);
}

void bts2_update_bw(struct decon_device *decon, u32 is_after)
{
	int i;
	struct vpp_dev *vpp;
	u32 mem_bw, bus_bw; u64 disp_bw;
	struct decon_device *deconf = get_decon_drvdata(0);

	for (i = IDMA_VGR0; i < ODMA_WB; i++) {
		vpp = get_running_vpp_dev(decon, i);
		bts_rot_scenario_set(vpp, is_after);
	}

	if (is_after) { /* after DECON h/w configuration */
		bts_update_winlayer(decon->num_of_win);
		mem_bw  = min(decon->total_bw, deconf->prev_total_bw);
		bus_bw  = min(decon->max_peak_bw, deconf->prev_max_peak_bw);
		disp_bw = min(decon->max_disp_ch, deconf->prev_max_disp_ch);

	} else {
		bts_update_winlayer(decon->prev_num_of_win);
		mem_bw  = max(decon->total_bw, deconf->prev_total_bw);
		bus_bw  = max(decon->max_peak_bw, deconf->prev_max_peak_bw);
		disp_bw = max(decon->max_disp_ch, deconf->prev_max_disp_ch);
	}

	mutex_lock(&bts_lock);
	if (mem_bw != deconf->prev_total_bw ||
			bus_bw != deconf->prev_max_peak_bw) {
		deconf->prev_total_bw = mem_bw;
		deconf->prev_max_peak_bw = bus_bw;
		exynos_update_bw(0, mem_bw, bus_bw); /* don't care 1st param */
		decon_bts("UPDATE_BW(%s): MIF(%d), INT(%d)\n",
				is_after? "AF" : "BE", mem_bw, bus_bw);
	}

	if (disp_bw != deconf->prev_max_disp_ch) {
		deconf->prev_max_disp_ch = disp_bw;
		pm_qos_update_request(&decon->disp_qos, disp_bw);
		decon_bts("UPDATE_BW(%s): DISP(%llu)\n",
				is_after? "AF" : "BE", disp_bw);
	}
	/* update latest bandwidth value after H/W applied */
	if (is_after || decon->pdata->trig_mode == DECON_SW_TRIG)
		update_req_vpp_bts_info(decon);

	decon_bts("------ %s Updated(%s,0x%x)-----\n", dev_name(decon->dev),
			is_after? "AF" : "BE", decon->vpp_usage_bitmask);
	mutex_unlock(&bts_lock);
}

void bts2_release_bw(struct decon_device *decon)
{
	bts2_calc_bw(decon);
	bts2_update_bw(decon, 1);

	mutex_lock(&bts_lock);
	decon->prev_max_disp_ch = 0;
	decon->prev_total_bw = 0;
	decon->prev_max_peak_bw = 0;
	mutex_unlock(&bts_lock);
}

void bts2_release_vpp(struct vpp_dev *vpp)
{
	if (IS_ERR_OR_NULL(vpp))
		return;

	bts_rot_scenario_set(vpp, 1);

	mutex_lock(&bts_lock);
	vpp->bts_info.shw_cur_bw  = vpp->bts_info.cur_bw  = 0;
	vpp->bts_info.shw_peak_bw = vpp->bts_info.peak_bw = 0;
	vpp->shw_disp = vpp->cur_disp = 0;
	mutex_unlock(&bts_lock);
}

void bts2_init(struct decon_device *decon)
{
	if (decon->lcd_info != NULL) {
		if (decon->lcd_info->mic_ratio == MIC_COMP_RATIO_1_2)
			decon->mic_factor = 2;
		else if (decon->lcd_info->mic_ratio == MIC_COMP_RATIO_1_3)
			decon->mic_factor = 3;
		else
			decon->mic_factor = 3;

		if (decon->lcd_info->dsc_enabled)
			decon->mic_factor = 3;	/* 1/3 compression */
	} else {
		decon->mic_factor = 3;
	}

	decon_info("mic factor(%d), pixel per clock(%d)\n",
			decon->mic_factor, DECON_PIX_PER_CLK);

	pm_qos_add_request(&decon->disp_qos, PM_QOS_DISPLAY_THROUGHPUT, 0);
}

void bts2_deinit(struct decon_device *decon)
{
	decon_info("%s +\n", __func__);
	pm_qos_remove_request(&decon->disp_qos);
	decon_info("%s -\n", __func__);
}
#endif

struct decon_init_bts decon_init_bts_control = {
	.bts_add		= bts_add_request,
	.bts_set_init		= bts_send_init,
	.bts_release_init	= bts_send_release,
	.bts_remove		= bts_remove_request,
};

struct decon_bts decon_bts_control = {
	.bts_get_bw		= bts_calc_bw,
	.bts_set_calc_bw	= bts_send_calc_bw,
	.bts_set_zero_bw	= bts_send_zero_bw,
	.bts_set_rot_mif	= bts_mif_lock,
};

#if defined(CONFIG_EXYNOS8890_BTS_OPTIMIZATION)
struct decon_bts2 decon_bts2_control = {
	.bts_init		= bts2_init,
	.bts_calc_bw		= bts2_calc_bw,
	.bts_update_bw		= bts2_update_bw,
	.bts_release_bw		= bts2_release_bw,
	.bts_release_vpp	= bts2_release_vpp,
	.bts_deinit		= bts2_deinit,
};
#endif
