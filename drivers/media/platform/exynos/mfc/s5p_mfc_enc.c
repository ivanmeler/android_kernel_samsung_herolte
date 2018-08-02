/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_enc.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include <media/videobuf2-core.h>
#include <linux/v4l2-controls.h>

#include "s5p_mfc_enc.h"
#include "s5p_mfc_enc_internal.h"

#include "s5p_mfc_ctrl.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_qos.h"
#include "s5p_mfc_opr_v10.h"
#include "s5p_mfc_buf.h"
#include "s5p_mfc_utils.h"
#include "s5p_mfc_nal_q.h"

#define DEF_SRC_FMT	1
#define DEF_DST_FMT	2

#define MFC_ENC_AVG_FPS_MODE

static struct s5p_mfc_fmt *find_format(struct v4l2_format *f, unsigned int t)
{
	unsigned long i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat &&
		    formats[i].type == t)
			return (struct s5p_mfc_fmt *)&formats[i];
	}

	return NULL;
}

static struct v4l2_queryctrl *get_ctrl(int id)
{
	unsigned long i;

	for (i = 0; i < NUM_CTRLS; ++i)
		if (id == controls[i].id)
			return &controls[i];
	return NULL;
}

static int check_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct v4l2_queryctrl *c;

	c = get_ctrl(ctrl->id);
	if (!c)
		return -EINVAL;
	if (ctrl->id == V4L2_CID_MPEG_VIDEO_GOP_SIZE
	    && ctrl->value > c->maximum) {
		mfc_info_ctx("GOP_SIZE is changed to max(%d -> %d)\n",
                                ctrl->value, c->maximum);
		ctrl->value = c->maximum;
	}
	if (ctrl->id == V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER) {
		if ((ctrl->value & ~(1 << 16)) < c->minimum || (ctrl->value & ~(1 << 16)) > c->maximum
		    || (c->step != 0 && (ctrl->value & ~(1 << 16)) % c->step != 0)) {
			v4l2_err(&dev->v4l2_dev, "Invalid control value\n");
			return -ERANGE;
		} else {
			return 0;
		}
	}
	if (ctrl->value < c->minimum || ctrl->value > c->maximum
	    || (c->step != 0 && ctrl->value % c->step != 0)) {
		v4l2_err(&dev->v4l2_dev, "Invalid control value\n");
		return -ERANGE;
	}
	if (!FW_HAS_ENC_SPSPPS_CTRL(dev) && ctrl->id ==	\
			V4L2_CID_MPEG_VIDEO_H264_PREPEND_SPSPPS_TO_IDR) {
		mfc_err_ctx("Not support feature(0x%x) for F/W\n", ctrl->id);
		return -ERANGE;
	}
	if (!FW_HAS_ROI_CONTROL(dev) && ctrl->id == \
			V4L2_CID_MPEG_VIDEO_ROI_CONTROL) {
		mfc_err_ctx("Not support feature(0x%x) for F/W\n", ctrl->id);
		return -ERANGE;
	}

	return 0;
}

static int enc_ctrl_read_cst(struct s5p_mfc_ctx *ctx,
		struct s5p_mfc_buf_ctrl *buf_ctrl)
{
	int ret;
	struct s5p_mfc_enc *enc = ctx->enc_priv;

	switch (buf_ctrl->id) {
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
		ret = !enc->in_slice;
		break;
	default:
		mfc_err_ctx("not support custom per-buffer control\n");
		ret = -EINVAL;
		break;
	}

	return ret;
}

int s5p_mfc_enc_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	mfc_debug(2, "src=%d, dst=%d, src_q=%d, dst_q=%d, ref=%d, state=%d\n",
		  ctx->src_queue_cnt, ctx->dst_queue_cnt,
		  ctx->src_queue_cnt_nal_q, ctx->dst_queue_cnt_nal_q,
		  enc->ref_queue_cnt, ctx->state);

	/* Skip ready check temporally */
	spin_lock_irq(&dev->condlock);
	if (test_bit(ctx->num, &dev->ctx_stop_bits)) {
		spin_unlock_irq(&dev->condlock);
		return 0;
	}
	spin_unlock_irq(&dev->condlock);

	/* context is ready to make header */
	if (ctx->state == MFCINST_GOT_INST && ctx->dst_queue_cnt >= 1) {
		if (p->seq_hdr_mode == \
			V4L2_MPEG_VIDEO_HEADER_MODE_AT_THE_READY) {
			if (ctx->src_queue_cnt >= 1)
				return 1;
		} else {
			return 1;
		}
	}
	/* context is ready to allocate DPB */
	if (ctx->dst_queue_cnt >= 1 && ctx->state == MFCINST_HEAD_PARSED)
		return 1;
	/* context is ready to encode a frame */
	if (ctx->state == MFCINST_RUNNING &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;
	/* context is ready to encode a frame in case of B frame */
	if (ctx->state == MFCINST_RUNNING_NO_OUTPUT &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;
	/* context is ready to encode a frame for NAL_ABORT command */
	if (ctx->state == MFCINST_ABORT_INST &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;
	/* context is ready to encode remain frames */
	if (ctx->state == MFCINST_FINISHING &&
		ctx->src_queue_cnt >= 1 && ctx->dst_queue_cnt >= 1)
		return 1;

	mfc_debug(2, "ctx is not ready.\n");

	return 0;
}

static inline int h264_profile(struct s5p_mfc_ctx *ctx, int profile)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret = 0;

	switch (profile) {
	case V4L2_MPEG_VIDEO_H264_PROFILE_MAIN:
		ret = S5P_FIMV_ENC_PROFILE_H264_MAIN;
		break;
	case V4L2_MPEG_VIDEO_H264_PROFILE_HIGH:
		ret = S5P_FIMV_ENC_PROFILE_H264_HIGH;
		break;
	case V4L2_MPEG_VIDEO_H264_PROFILE_BASELINE:
		ret =S5P_FIMV_ENC_PROFILE_H264_BASELINE;
		break;
	case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_BASELINE:
		if (IS_MFCV6(dev))
			ret = S5P_FIMV_ENC_PROFILE_H264_CONSTRAINED_BASELINE;
		else
			ret = -EINVAL;
		break;
	case V4L2_MPEG_VIDEO_H264_PROFILE_CONSTRAINED_HIGH:
		if (IS_MFCV6(dev))
			ret = S5P_FIMV_ENC_PROFILE_H264_CONSTRAINED_HIGH;
		else
			ret = -EINVAL;
		break;
	default:
		ret = -EINVAL;
	}

	return ret;
}

static int enc_cleanup_ctx_ctrls(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;

	while (!list_empty(&ctx->ctrls)) {
		ctx_ctrl = list_entry((&ctx->ctrls)->next,
				      struct s5p_mfc_ctx_ctrl, list);

		mfc_debug(7, "Cleanup context control "\
				"id: 0x%08x, type: %d\n",
				ctx_ctrl->id, ctx_ctrl->type);

		list_del(&ctx_ctrl->list);
		kfree(ctx_ctrl);
	}

	INIT_LIST_HEAD(&ctx->ctrls);

	return 0;
}

static int enc_get_buf_update_val(struct s5p_mfc_ctx *ctx,
			struct list_head *head, unsigned int id, int value)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if ((buf_ctrl->id == id)) {
			buf_ctrl->val = value;
			mfc_debug(5, "++id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
			break;
		}
	}

	return 0;
}

static int enc_init_ctx_ctrls(struct s5p_mfc_ctx *ctx)
{
	unsigned long i;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;

	INIT_LIST_HEAD(&ctx->ctrls);

	for (i = 0; i < NUM_CTRL_CFGS; i++) {
		ctx_ctrl = kzalloc(sizeof(struct s5p_mfc_ctx_ctrl), GFP_KERNEL);
		if (ctx_ctrl == NULL) {
			mfc_err("Failed to allocate context control "\
					"id: 0x%08x, type: %d\n",
					mfc_ctrl_list[i].id,
					mfc_ctrl_list[i].type);

			enc_cleanup_ctx_ctrls(ctx);

			return -ENOMEM;
		}

		ctx_ctrl->type = mfc_ctrl_list[i].type;
		ctx_ctrl->id = mfc_ctrl_list[i].id;
		ctx_ctrl->has_new = 0;
		ctx_ctrl->val = 0;

		list_add_tail(&ctx_ctrl->list, &ctx->ctrls);

		mfc_debug(7, "Add context control id: 0x%08x, type : %d\n",
				ctx_ctrl->id, ctx_ctrl->type);
	}

	return 0;
}

static void __enc_reset_buf_ctrls(struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		mfc_debug(8, "Reset buffer control value "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		buf_ctrl->has_new = 0;
		buf_ctrl->val = 0;
		buf_ctrl->old_val = 0;
		buf_ctrl->updated = 0;
	}
}

static void __enc_cleanup_buf_ctrls(struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	while (!list_empty(head)) {
		buf_ctrl = list_entry(head->next,
				struct s5p_mfc_buf_ctrl, list);

		mfc_debug(7, "Cleanup buffer control "\
				"id: 0x%08x, type: %d\n",
				buf_ctrl->id, buf_ctrl->type);

		list_del(&buf_ctrl->list);
		kfree(buf_ctrl);
	}

	INIT_LIST_HEAD(head);
}

static int enc_init_buf_ctrls(struct s5p_mfc_ctx *ctx,
	enum s5p_mfc_ctrl_type type, unsigned int index)
{
	unsigned long i;
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (test_bit(index, &ctx->src_ctrls_avail)) {
			mfc_debug(7, "Source per-buffer control is already "\
					"initialized [%d]\n", index);

			__enc_reset_buf_ctrls(&ctx->src_ctrls[index]);

			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (test_bit(index, &ctx->dst_ctrls_avail)) {
			mfc_debug(7, "Dest. per-buffer control is already "\
					"initialized [%d]\n", index);

			__enc_reset_buf_ctrls(&ctx->dst_ctrls[index]);

			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	INIT_LIST_HEAD(head);

	for (i = 0; i < NUM_CTRL_CFGS; i++) {
		if (!(type & mfc_ctrl_list[i].type))
			continue;

		buf_ctrl = kzalloc(sizeof(struct s5p_mfc_buf_ctrl), GFP_KERNEL);
		if (buf_ctrl == NULL) {
			mfc_err("Failed to allocate buffer control "\
					"id: 0x%08x, type: %d\n",
					mfc_ctrl_list[i].id,
					mfc_ctrl_list[i].type);

			__enc_cleanup_buf_ctrls(head);

			return -ENOMEM;
		}

		buf_ctrl->type = mfc_ctrl_list[i].type;
		buf_ctrl->id = mfc_ctrl_list[i].id;
		buf_ctrl->is_volatile = mfc_ctrl_list[i].is_volatile;
		buf_ctrl->mode = mfc_ctrl_list[i].mode;
		buf_ctrl->addr = mfc_ctrl_list[i].addr;
		buf_ctrl->mask = mfc_ctrl_list[i].mask;
		buf_ctrl->shft = mfc_ctrl_list[i].shft;
		buf_ctrl->flag_mode = mfc_ctrl_list[i].flag_mode;
		buf_ctrl->flag_addr = mfc_ctrl_list[i].flag_addr;
		buf_ctrl->flag_shft = mfc_ctrl_list[i].flag_shft;
		if (buf_ctrl->mode == MFC_CTRL_MODE_CST) {
			buf_ctrl->read_cst = mfc_ctrl_list[i].read_cst;
			buf_ctrl->write_cst = mfc_ctrl_list[i].write_cst;
		}

		list_add_tail(&buf_ctrl->list, head);

		mfc_debug(7, "Add buffer control id: 0x%08x, type : %d\n",
				buf_ctrl->id, buf_ctrl->type);
	}

	__enc_reset_buf_ctrls(head);

	if (type & MFC_CTRL_TYPE_SRC)
		set_bit(index, &ctx->src_ctrls_avail);
	else
		set_bit(index, &ctx->dst_ctrls_avail);

	return 0;
}

static int enc_cleanup_buf_ctrls(struct s5p_mfc_ctx *ctx,
	enum s5p_mfc_ctrl_type type, unsigned int index)
{
	struct list_head *head;

	if (index >= MFC_MAX_BUFFERS) {
		mfc_err("Per-buffer control index is out of range\n");
		return -EINVAL;
	}

	if (type & MFC_CTRL_TYPE_SRC) {
		if (!(test_and_clear_bit(index, &ctx->src_ctrls_avail))) {
			mfc_debug(7, "Source per-buffer control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (!(test_and_clear_bit(index, &ctx->dst_ctrls_avail))) {
			mfc_debug(7, "Dest. per-buffer Control is "\
					"not available [%d]\n", index);
			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	__enc_cleanup_buf_ctrls(head);

	return 0;
}

static int enc_to_buf_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int index = 0;
	unsigned int reg = 0;

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET) || !ctx_ctrl->has_new)
			continue;

		list_for_each_entry(buf_ctrl, head, list) {
			if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (buf_ctrl->id == ctx_ctrl->id) {
				buf_ctrl->has_new = 1;
				buf_ctrl->val = ctx_ctrl->val;
				if (buf_ctrl->is_volatile)
					buf_ctrl->updated = 0;

				ctx_ctrl->has_new = 0;
				if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_ROI_CONTROL) {
					index = enc->roi_index;
					if (enc->roi_info[index].enable) {
						enc->roi_index =
							(index + 1) % MFC_MAX_EXTRA_BUF;
						reg |= enc->roi_info[index].enable;
						reg &= ~(0xFF << 8);
						reg |= (enc->roi_info[index].lower_qp << 8);
						reg &= ~(0xFFFF << 16);
						reg |= (enc->roi_info[index].upper_qp << 16);
					}
					buf_ctrl->val = reg;
					buf_ctrl->old_val2 = index;
				}
				break;
			}
		}
	}

	list_for_each_entry(buf_ctrl, head, list) {
		if (buf_ctrl->has_new)
			mfc_debug(8, "Updated buffer control "\
					"id: 0x%08x val: %d\n",
					buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int enc_to_ctx_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	struct s5p_mfc_buf_ctrl *buf_ctrl;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET) || !buf_ctrl->has_new)
			continue;

		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == buf_ctrl->id) {
				if (ctx_ctrl->has_new)
					mfc_debug(8,
					"Overwrite context control "\
					"value id: 0x%08x, val: %d\n",
						ctx_ctrl->id, ctx_ctrl->val);

				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = buf_ctrl->val;

				buf_ctrl->has_new = 0;
			}
		}
	}

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (ctx_ctrl->has_new)
			mfc_debug(8, "Updated context control "\
					"id: 0x%08x val: %d\n",
					ctx_ctrl->id, ctx_ctrl->val);
	}

	return 0;
}

static int enc_set_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0, value2 = 0;
	struct temporal_layer_info temporal_LC;
	unsigned int i;
	struct s5p_mfc_enc_params *p = &enc->params;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;

		/* read old vlaue */
		value = MFC_READL(buf_ctrl->addr);

		/* save old value for recovery */
		if (buf_ctrl->is_volatile)
			buf_ctrl->old_val = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		/* write new value */
		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft);
		MFC_WRITEL(value, buf_ctrl->addr);

		/* set change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = MFC_READL(buf_ctrl->flag_addr);
			value |= (1 << buf_ctrl->flag_shft);
			MFC_WRITEL(value, buf_ctrl->flag_addr);
		}

		buf_ctrl->has_new = 0;
		buf_ctrl->updated = 1;

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG)
			enc->stored_tag = buf_ctrl->val;

		if (buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH) {

			memcpy(&temporal_LC,
				enc->sh_handle_svc.virt, sizeof(struct temporal_layer_info));

			if(((temporal_LC.temporal_layer_count & 0x7) < 1) ||
				((temporal_LC.temporal_layer_count > 3) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC)) ||
				((temporal_LC.temporal_layer_count > 3) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_VP9_ENC))) {
				/* clear NUM_T_LAYER_CHANGE */
				value = MFC_READL(buf_ctrl->flag_addr);
				value &= ~(1 << 10);
				MFC_WRITEL(value, buf_ctrl->flag_addr);
				mfc_err_ctx("Temporal SVC: layer count is invalid : %d\n",
						temporal_LC.temporal_layer_count);
				goto invalid_layer_count;
			}

			if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
				p->codec.h264.num_hier_layer = temporal_LC.temporal_layer_count & 0x7;

			/* enable RC_BIT_RATE_CHANGE */
			value = MFC_READL(buf_ctrl->flag_addr);
			if (temporal_LC.temporal_layer_bitrate[0] > 0)
				/* set RC_BIT_RATE_CHANGE */
				value |= (1 << 2);
			else
				/* clear RC_BIT_RATE_CHANGE */
				value &= ~(1 << 2);
			MFC_WRITEL(value, buf_ctrl->flag_addr);

			mfc_debug(3, "Temporal SVC: layer count %d, E_PARAM_CHANGE %#x\n",
					temporal_LC.temporal_layer_count & 0x7, value);

			value = MFC_READL(S5P_FIMV_E_NUM_T_LAYER);
			buf_ctrl->old_val2 = value;
			value &= ~(0x7);
			value |= (temporal_LC.temporal_layer_count & 0x7);
			MFC_WRITEL(value, S5P_FIMV_E_NUM_T_LAYER);
			mfc_debug(3, "Temporal SVC: E_NUM_T_LAYER %#x\n", value);
			for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
				mfc_debug(3, "Temporal SVC: layer bitrate[%d] %d\n",
					i, temporal_LC.temporal_layer_bitrate[i]);
				MFC_WRITEL(temporal_LC.temporal_layer_bitrate[i],
						buf_ctrl->addr + i * 4);
			}

			/* priority change */
			if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC) {
				value = 0;
				value2 = 0;
				for (i = 0; i < (p->codec.h264.num_hier_layer & 0x07); i++) {
					if (i <= 4)
						value |= ((p->codec.h264.base_priority & 0x3F) + i) << (6 * i);
					else
						value2 |= ((p->codec.h264.base_priority & 0x3F) + i) << (6 * (i - 5));
				}
				MFC_WRITEL(value, S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
				MFC_WRITEL(value2, S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
				mfc_debug(3, "Temporal SVC: EXTENSION0 %#x, EXTENSION1 %#x\n",
						value, value2);

				value = MFC_READL(buf_ctrl->flag_addr);
				value |= (1 << 12);
				MFC_WRITEL(value, buf_ctrl->flag_addr);
				mfc_debug(3, "Temporal SVC: E_PARAM_CHANGE %#x\n", value);
			}
		}

		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_MARK_LTR) {
			value = MFC_READL(S5P_FIMV_E_H264_NAL_CONTROL);
			buf_ctrl->old_val2 = (value >> 8) & 0x7;
			value &= ~(0x7 << 8);
			value |= (buf_ctrl->val & 0x7) << 8;
			MFC_WRITEL(value, S5P_FIMV_E_H264_NAL_CONTROL);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_USE_LTR) {
			value = MFC_READL(S5P_FIMV_E_H264_NAL_CONTROL);
			buf_ctrl->old_val2 = (value >> 11) & 0x7;
			value &= ~(0x7 << 11);
			value |= (buf_ctrl->val & 0x7) << 11;
			MFC_WRITEL(value, S5P_FIMV_E_H264_NAL_CONTROL);
		}

		if ((buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH) && FW_HAS_GOP2(dev)) {
			value = MFC_READL(S5P_FIMV_E_GOP_CONFIG2);
			buf_ctrl->old_val |= (value << 16) & 0x3FFF0000;
			value &= ~(0x3FFF);
			value |= (buf_ctrl->val >> 16) & 0x3FFF;
			MFC_WRITEL(value, S5P_FIMV_E_GOP_CONFIG2);
		}

		/* PROFILE & LEVEL have to be set up together */
		if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
			value = MFC_READL(S5P_FIMV_E_PICTURE_PROFILE);
			buf_ctrl->old_val |= (value & 0x000F) << 8;
			value &= ~(0x000F);
			value |= p->codec.h264.profile & 0x000F;
			MFC_WRITEL(value, S5P_FIMV_E_PICTURE_PROFILE);
			p->codec.h264.level = buf_ctrl->val;
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_PROFILE) {
			value = MFC_READL(S5P_FIMV_E_PICTURE_PROFILE);
			buf_ctrl->old_val |= value & 0xFF00;
			value &= ~(0x00FF << 8);
			value |= (p->codec.h264.level << 8) & 0xFF00;
			MFC_WRITEL(value, S5P_FIMV_E_PICTURE_PROFILE);
			p->codec.h264.profile = buf_ctrl->val;
		}

		/* temproral layer priority */
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY) {
			value = MFC_READL(S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
			buf_ctrl->old_val |= value & 0x3FFFFFC0;
			value &= ~(0x3FFFFFC0);
			value2 = MFC_READL(S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
			buf_ctrl->old_val2 = value2 & 0x0FFF;
			value2 &= ~(0x0FFF);
			for (i = 0; i < (p->codec.h264.num_hier_layer & 0x07); i++) {
				if (i <= 4)
					value |= ((buf_ctrl->val & 0x3F) + i) << (6 * i);
				else
					value2 |= ((buf_ctrl->val & 0x3F) + i) << (6 * (i - 5));
			}
			MFC_WRITEL(value, S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
			MFC_WRITEL(value2, S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
			p->codec.h264.base_priority = buf_ctrl->val;
			mfc_debug(3, "Temporal SVC: EXTENSION0 %#x, EXTENSION1 %#x\n",
					value, value2);
		}
		/* per buffer QP setting change */
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_CONFIG_QP)
			p->config_qp = buf_ctrl->val;

		/* set the ROI buffer DVA */
		if ((buf_ctrl->id == V4L2_CID_MPEG_VIDEO_ROI_CONTROL) &&
				FW_HAS_ROI_CONTROL(dev))
			MFC_WRITEL(enc->roi_buf[buf_ctrl->old_val2].ofs,
						S5P_FIMV_E_ROI_BUFFER_ADDR);

invalid_layer_count:
		mfc_debug(8, "Set buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	if (!p->rc_frame && !p->rc_mb && p->dynamic_qp) {
		value = MFC_READL(S5P_FIMV_E_FIXED_PICTURE_QP);
		value &= ~(0xFF000000);
		value |= (p->config_qp & 0xFF) << 24;
		MFC_WRITEL(value, S5P_FIMV_E_FIXED_PICTURE_QP);
	}

	return 0;
}

static int enc_get_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = MFC_READL(buf_ctrl->addr);
		else if (buf_ctrl->mode == MFC_CTRL_MODE_CST)
			value = call_bop(buf_ctrl, read_cst, ctx, buf_ctrl);

		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		mfc_debug(8, "Get buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int enc_set_buf_ctrls_val_nal_q(struct s5p_mfc_ctx *ctx,
			struct list_head *head, EncoderInputStr *pInStr)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct temporal_layer_info temporal_LC;
	unsigned int i, param_change;
	struct s5p_mfc_enc_params *p = &enc->params;

	mfc_debug_enter();

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;
		param_change = 0;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			pInStr->PictureTag &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureTag |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			enc->stored_tag = buf_ctrl->val;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
			pInStr->FrameInsertion &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->FrameInsertion |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH:
			pInStr->GopConfig &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->GopConfig |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			if (FW_HAS_GOP2(dev)) {
				pInStr->GopConfig2 &= ~(0x3FFF);
				pInStr->GopConfig2 |= (buf_ctrl->val >> 16) & 0x3FFF;
			}
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH:
			pInStr->RcFrameRate &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcFrameRate |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH:
			pInStr->RcBitRate &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcBitRate |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP:
		case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
			pInStr->RcQpBound &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->RcQpBound |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH:
		case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH:
		case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH:
			memcpy(&temporal_LC,
				enc->sh_handle_svc.virt, sizeof(struct temporal_layer_info));

			if (((temporal_LC.temporal_layer_count & 0x7) < 1) ||
				((temporal_LC.temporal_layer_count > 3) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC)) ||
				((temporal_LC.temporal_layer_count > 3) &&
				(ctx->codec_mode == S5P_FIMV_CODEC_VP9_ENC))) {
				/* claer NUM_T_LAYER_CHANGE */
				mfc_err_ctx("Temporal SVC: layer count(%d) is invalid\n",
						temporal_LC.temporal_layer_count);
				return 0;
			}

			if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH) {
				p->codec.h264.num_hier_layer =
					temporal_LC.temporal_layer_count & 0x7;
				p->codec.h264.hier_ref_type =
					(temporal_LC.temporal_layer_count >> 16) & 0x1;
			}

			/* enable RC_BIT_RATE_CHANGE */
			if (temporal_LC.temporal_layer_bitrate[0] > 0)
				pInStr->ParamChange |= (1 << 2);
			else
				pInStr->ParamChange &= ~(1 << 2);

			/* enalbe NUM_T_LAYER_CHANGE */
			if (temporal_LC.temporal_layer_count & 0x7)
				pInStr->ParamChange |= (1 << 10);
			else
				pInStr->ParamChange &= ~(1 << 10);
			mfc_debug(3, "Temporal SVC layer count %d\n",
					temporal_LC.temporal_layer_count & 0x7);

			pInStr->NumTLayer &= ~(0x7);
			pInStr->NumTLayer |= (temporal_LC.temporal_layer_count & 0x7);
			for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
				mfc_debug(3, "Temporal SVC: layer bitrate[%d] %d\n",
					i, temporal_LC.temporal_layer_bitrate[i]);
				pInStr->HierarchicalBitRateLayer[i] =
					temporal_LC.temporal_layer_bitrate[i];
			}

			/* priority change */
			if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC) {
				for (i = 0; i < (temporal_LC.temporal_layer_count & 0x7); i++) {
					if (i <= 4)
						pInStr->H264HDSvcExtension0 |=
							((p->codec.h264.base_priority & 0x3f) + i) << (6 * i);
					else
						pInStr->H264HDSvcExtension1 |=
							((p->codec.h264.base_priority & 0x3f) + i) << (6 * (i - 5));
				}
				mfc_debug(3, "NAL-Q: Temporal SVC: EXTENSION0 %#x, EXTENSION1 %#x\n",
						pInStr->H264HDSvcExtension0, pInStr->H264HDSvcExtension1);

				pInStr->ParamChange |= (1 << 12);
			}
			break;
		case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER:
			pInStr->NumTLayer &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->NumTLayer |= (buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			p->codec.h264.num_hier_layer = buf_ctrl->val & 0x7;
			p->codec.h264.hier_ref_type = (buf_ctrl->val >> 16) & 0x1;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
			pInStr->PictureProfile &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureProfile |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->PictureProfile &= ~(0xf);
			pInStr->PictureProfile |= p->codec.h264.profile & 0xf;
			p->codec.h264.level = buf_ctrl->val;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
			pInStr->PictureProfile &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->PictureProfile |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			pInStr->PictureProfile &= ~(0xff << 8);
			pInStr->PictureProfile |= (p->codec.h264.level << 8) & 0xff00;
			p->codec.h264.profile = buf_ctrl->val;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC_H264_MARK_LTR:
		case V4L2_CID_MPEG_MFC_H264_USE_LTR:
			pInStr->H264NalControl &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->H264NalControl |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			break;
		case V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY:
			for (i = 0; i < (p->codec.h264.num_hier_layer & 0x7); i++)
				if (i <= 4)
					pInStr->H264HDSvcExtension0 |=
						((buf_ctrl->val & 0x3f) + i) << (6 * i);
				else
					pInStr->H264HDSvcExtension1 |=
						((buf_ctrl->val & 0x3f) + i) << (6 * (i - 5));
			p->codec.h264.base_priority = buf_ctrl->val;
			param_change = 1;
			break;
		case V4L2_CID_MPEG_MFC_CONFIG_QP:
			pInStr->FixedPictureQp &= ~(buf_ctrl->mask << buf_ctrl->shft);
			pInStr->FixedPictureQp |=
				(buf_ctrl->val & buf_ctrl->mask) << buf_ctrl->shft;
			p->config_qp = buf_ctrl->val;
			break;
		}

		if (param_change)
			pInStr->ParamChange |= (1 << buf_ctrl->flag_shft);

		buf_ctrl->updated = 1;
	}

	if (!p->rc_frame && !p->rc_mb) {
		pInStr->FixedPictureQp &= ~(0xFF000000);
		pInStr->FixedPictureQp |= (p->config_qp & 0xFF) << 24;
	}
	mfc_debug_leave();

	return 0;
}

static int enc_get_buf_ctrls_val_nal_q(struct s5p_mfc_ctx *ctx,
			struct list_head *head, EncoderOutputStr *pOutStr)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	unsigned int value = 0;

	mfc_debug_enter();
	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;
		switch (buf_ctrl->id) {
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
			value = pOutStr->PictureTag;
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR:
			value = pOutStr->EncodedSourceAddr[0];
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR:
			value = pOutStr->EncodedSourceAddr[1];
			break;
		case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
			value = !enc->in_slice;
			break;
		}
		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		mfc_debug(8, "Get buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}
	mfc_debug_leave();

	return 0;
}

static int enc_recover_buf_ctrls_val(struct s5p_mfc_ctx *ctx,
						struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
			|| !buf_ctrl->is_volatile
			|| !buf_ctrl->updated)
			continue;

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			value = MFC_READL(buf_ctrl->addr);

		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->old_val & buf_ctrl->mask)
							<< buf_ctrl->shft);

		if (buf_ctrl->mode == MFC_CTRL_MODE_SFR)
			MFC_WRITEL(value, buf_ctrl->addr);

		/* clear change flag bit */
		if (buf_ctrl->flag_mode == MFC_CTRL_MODE_SFR) {
			value = MFC_READL(buf_ctrl->flag_addr);
			value &= ~(1 << buf_ctrl->flag_shft);
			MFC_WRITEL(value, buf_ctrl->flag_addr);
		}

		mfc_debug(8, "Recover buffer control "\
				"id: 0x%08x old val: %d\n",
				buf_ctrl->id, buf_ctrl->old_val);
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH && FW_HAS_GOP2(dev)) {
			value = MFC_READL(S5P_FIMV_E_GOP_CONFIG2);
			value &= ~(0x3FFF);
			value |= (buf_ctrl->old_val >> 16) & 0x3FFF;
			MFC_WRITEL(value, S5P_FIMV_E_GOP_CONFIG2);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_LEVEL) {
			value = MFC_READL(S5P_FIMV_E_PICTURE_PROFILE);
			value &= ~(0x000F);
			value |= (buf_ctrl->old_val >> 8) & 0x000F;
			MFC_WRITEL(value, S5P_FIMV_E_PICTURE_PROFILE);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_PROFILE) {
			value = MFC_READL(S5P_FIMV_E_PICTURE_PROFILE);
			value &= ~(0xFF00);
			value |= buf_ctrl->old_val & 0xFF00;
			MFC_WRITEL(value, S5P_FIMV_E_PICTURE_PROFILE);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY) {
			MFC_WRITEL(buf_ctrl->old_val, S5P_FIMV_E_H264_HD_SVC_EXTENSION_0);
			MFC_WRITEL(buf_ctrl->old_val2, S5P_FIMV_E_H264_HD_SVC_EXTENSION_1);
		}
		if (buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH ||
			buf_ctrl->id
			== V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH) {
			MFC_WRITEL(buf_ctrl->old_val2, S5P_FIMV_E_NUM_T_LAYER);
			/* clear RC_BIT_RATE_CHANGE */
			value = MFC_READL(buf_ctrl->flag_addr);
			value &= ~(1 << 2);
			MFC_WRITEL(value, buf_ctrl->flag_addr);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_MARK_LTR) {
			value = MFC_READL(S5P_FIMV_E_H264_NAL_CONTROL);
			value &= ~(0x7 << 8);
			value |= (buf_ctrl->old_val2 & 0x7) << 8;
			MFC_WRITEL(value, S5P_FIMV_E_H264_NAL_CONTROL);
		}
		if (buf_ctrl->id == V4L2_CID_MPEG_MFC_H264_USE_LTR) {
			value = MFC_READL(S5P_FIMV_E_H264_NAL_CONTROL);
			value &= ~(0x7 << 11);
			value |= (buf_ctrl->old_val2 & 0x7) << 11;
			MFC_WRITEL(value, S5P_FIMV_E_H264_NAL_CONTROL);
		}
	}

	return 0;
}

static void cleanup_ref_queue(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_buf *mb_entry;
	int i;

	/* move buffers in ref queue to src queue */
	while (!list_empty(&enc->ref_queue)) {
		mb_entry = list_entry((&enc->ref_queue)->next, struct s5p_mfc_buf, list);

		mfc_debug(2, "enc ref addr: 0x%08llx",
			s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, 0));
		for (i = 0; i < ctx->raw_buf.num_planes; i++)
			mfc_debug(2, "ref plane[%d] addr: 0x%08llx",
					i, mb_entry->planes.raw[i]);

		list_del(&mb_entry->list);
		enc->ref_queue_cnt--;

		list_add_tail(&mb_entry->list, &ctx->src_queue);
		ctx->src_queue_cnt++;
	}

	mfc_debug(2, "enc src count: %d, enc ref count: %d\n",
		  ctx->src_queue_cnt, enc->ref_queue_cnt);

	INIT_LIST_HEAD(&enc->ref_queue);
	enc->ref_queue_cnt = 0;
}

static int enc_pre_seq_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf *dst_mb;
	unsigned long flags;

	spin_lock_irqsave(&dev->irqlock, flags);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	s5p_mfc_set_enc_stream_buffer(ctx, dst_mb);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	return 0;
}

static int enc_post_seq_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_buf *dst_mb;
	unsigned long flags;

	mfc_debug(2, "seq header size: %d", s5p_mfc_get_enc_strm_size());

	if ((p->seq_hdr_mode == V4L2_MPEG_VIDEO_HEADER_MODE_SEPARATE) ||
	    (p->seq_hdr_mode == V4L2_MPEG_VIDEO_HEADER_MODE_AT_THE_READY)) {
		spin_lock_irqsave(&dev->irqlock, flags);

		dst_mb = list_entry(ctx->dst_queue.next,
				struct s5p_mfc_buf, list);
		list_del(&dst_mb->list);
		ctx->dst_queue_cnt--;

		vb2_set_plane_payload(&dst_mb->vb, 0, s5p_mfc_get_enc_strm_size());
		vb2_buffer_done(&dst_mb->vb, VB2_BUF_STATE_DONE);

		/* encoder dst buffer CFW UNPROT */
		if (ctx->is_drm) {
			int index = dst_mb->vb.v4l2_buf.index;

			if (test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, dst_mb, false))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					clear_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] enc dst buf un-prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}

		spin_unlock_irqrestore(&dev->irqlock, flags);
	}

	if (IS_MFCV6(dev))
		s5p_mfc_change_state(ctx, MFCINST_HEAD_PARSED); /* for INIT_BUFFER cmd */
	else {
		s5p_mfc_change_state(ctx, MFCINST_RUNNING);

		if (s5p_mfc_enc_ctx_ready(ctx)) {
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}
		queue_work(dev->sched_wq, &dev->sched_work);
	}
	if (IS_MFCV6(dev))
		ctx->dpb_count = s5p_mfc_get_enc_dpb_count();
	if (FW_HAS_E_MIN_SCRATCH_BUF(dev))
		ctx->scratch_buf_size = s5p_mfc_get_enc_scratch_size();

	return 0;
}

static int enc_pre_frame_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_buf *dst_mb;
	struct s5p_mfc_buf *src_mb;
	struct s5p_mfc_raw_info *raw;
	unsigned long flags;
	dma_addr_t src_addr[3] = { 0, 0, 0 };
	unsigned int i;

	raw = &ctx->raw_buf;
	spin_lock_irqsave(&dev->irqlock, flags);

	src_mb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);

	for (i = 0; i < raw->num_planes; i++) {
		src_addr[i] = src_mb->planes.raw[i];
		mfc_debug(2, "enc src[%d] addr: 0x%08llx\n", i, src_addr[i]);
	}
	if (src_mb->planes.raw[0] != s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, 0))
		mfc_err_ctx("enc src yaddr: 0x%08llx != vb2 yaddr: 0x%08llx\n",
				src_mb->planes.raw[i],
				s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, i));

	s5p_mfc_set_enc_frame_buffer(ctx, &src_addr[0], raw->num_planes);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	s5p_mfc_set_enc_stream_buffer(ctx, dst_mb);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	return 0;
}

static void enc_change_resolution(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int reg = 0;

	/*
	 * Right after the first NAL_START finished with the new resolution,
	 * We need to reset the fields
	 */
	if (ctx->enc_res_change) {
		/* clear resolution change bits */
		reg = MFC_READL(S5P_FIMV_E_PARAM_CHANGE);
		reg &= ~(0x7 << 6);
		MFC_WRITEL(reg, S5P_FIMV_E_PARAM_CHANGE);

		ctx->enc_res_change_state = ctx->enc_res_change;
		ctx->enc_res_change = 0;
	}

	if (ctx->enc_res_change_state == 1) { /* resolution swap */
		ctx->enc_res_change_state = 0;
	} else if (ctx->enc_res_change_state == 2) { /* resolution change */
		reg = MFC_READL(S5P_FIMV_E_NAL_DONE_INFO);
		reg = (reg & (0x3 << 4)) >> 4;

		/*
		 * Encoding resolution status
		 * 0: Normal encoding
		 * 1: Resolution Change for B-frame
		 *    (Encode with previous resolution)
		 * 2: Resolution Change for B-frame
		 *    (Last encoding with previous resolution)
		 * 3: Resolution Change for only P-frame
		 *    (No encode, as all frames with previous resolution are encoded)
		 */
		mfc_debug(2, "Encoding Resolution Status : %d\n", reg);

		if (reg == 2 || reg == 3) {
			s5p_mfc_release_codec_buffers(ctx);
			/* for INIT_BUFFER cmd */
			s5p_mfc_change_state(ctx, MFCINST_HEAD_PARSED);

			ctx->enc_res_change_state = 0;
			ctx->min_scratch_buf_size =
				MFC_READL(S5P_FIMV_E_MIN_SCRATCH_BUFFER_SIZE);
			mfc_debug(2, "S5P_FIMV_E_MIN_SCRATCH_BUFFER_SIZE = 0x%x\n",
					(unsigned int)ctx->min_scratch_buf_size);
			if (reg == 3)
				ctx->enc_res_change_re_input = 1;
		}
	}
}

static int enc_post_frame_start(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_buf *mb_entry, *next_entry;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t enc_addr[3] = { 0, 0, 0 };
	dma_addr_t mb_addr[3] = { 0, 0, 0 };
	int slice_type, i;
	unsigned int strm_size;
	unsigned int pic_count;
	unsigned long flags;
	unsigned int index;

	slice_type = s5p_mfc_get_enc_slice_type();
	strm_size = s5p_mfc_get_enc_strm_size();
	pic_count = s5p_mfc_get_enc_pic_count();

	mfc_debug(2, "encoded slice type: %d\n", slice_type);
	mfc_debug(2, "encoded stream size: %d\n", strm_size);
	mfc_debug(2, "display order: %d\n", pic_count);

	if (enc->buf_full) {
		s5p_mfc_change_state(ctx, MFCINST_ABORT_INST);
		return 0;
	}

	if (ctx->enc_res_change || ctx->enc_res_change_state)
		enc_change_resolution(ctx);

	/* set encoded frame type */
	enc->frame_type = slice_type;
	raw = &ctx->raw_buf;

	spin_lock_irqsave(&dev->irqlock, flags);

	ctx->sequence++;
	if (strm_size > 0 || ctx->state == MFCINST_FINISHING
			  || ctx->state == MFCINST_RUNNING_BUF_FULL) {
		/* at least one more dest. buffers exist always  */
		mb_entry = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);

		mb_entry->vb.v4l2_buf.flags &=
			~(V4L2_BUF_FLAG_KEYFRAME |
			  V4L2_BUF_FLAG_PFRAME |
			  V4L2_BUF_FLAG_BFRAME);

		switch (slice_type) {
		case S5P_FIMV_ENCODED_TYPE_I:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
			break;
		case S5P_FIMV_ENCODED_TYPE_P:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_PFRAME;
			break;
		case S5P_FIMV_ENCODED_TYPE_B:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_BFRAME;
			break;
		default:
			mb_entry->vb.v4l2_buf.flags |=
				V4L2_BUF_FLAG_KEYFRAME;
			break;
		}
		mfc_debug(2, "Slice type : %d\n", mb_entry->vb.v4l2_buf.flags);

		list_del(&mb_entry->list);
		ctx->dst_queue_cnt--;
		vb2_set_plane_payload(&mb_entry->vb, 0, strm_size);

		index = mb_entry->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err_ctx("failed in get_buf_ctrls_val\n");

		if (strm_size == 0 && ctx->state == MFCINST_FINISHING)
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				enc->stored_tag);

		vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);

		/* encoder dst buffer CFW UNPROT */
		if (ctx->is_drm) {
			if (test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, mb_entry, false))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					clear_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] enc dst buf un-prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}
	}

	if (enc->in_slice) {
		if (ctx->dst_queue_cnt == 0) {
			spin_lock_irq(&dev->condlock);
			clear_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		spin_unlock_irqrestore(&dev->irqlock, flags);

		return 0;
	}

	if (!ctx->enc_res_change_re_input && slice_type >= 0 &&
			ctx->state != MFCINST_FINISHING) {
		if (ctx->state == MFCINST_RUNNING_NO_OUTPUT ||
			ctx->state == MFCINST_RUNNING_BUF_FULL)
			s5p_mfc_change_state(ctx, MFCINST_RUNNING);

		s5p_mfc_get_enc_frame_buffer(ctx, &enc_addr[0], raw->num_planes);

		for (i = 0; i < raw->num_planes; i++)
			mfc_debug(2, "encoded[%d] addr: 0x%08llx\n",
						i, enc_addr[i]);

		list_for_each_entry_safe(mb_entry, next_entry,
						&ctx->src_queue, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = mb_entry->planes.raw[i];
				mfc_debug(2, "enc src[%d] addr: 0x%08llx\n",
						i, mb_addr[i]);
			}
			if (mb_entry->planes.raw[0] != s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, 0))
				mfc_err_ctx("enc src yaddr: 0x%08llx != vb2 yaddr: 0x%08llx\n",
						mb_entry->planes.raw[i],
						s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, i));

			if (enc_addr[0] == mb_addr[0]) {
				index = mb_entry->vb.v4l2_buf.index;
				if (call_cop(ctx, recover_buf_ctrls_val, ctx,
						&ctx->src_ctrls[index]) < 0)
					mfc_err_ctx("failed in recover_buf_ctrls_val\n");

				list_del(&mb_entry->list);
				ctx->src_queue_cnt--;

				vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);

				/* encoder src buffer CFW UNPROT */
				if (ctx->is_drm) {
					if (test_bit(index, &ctx->raw_protect_flag)) {
						if (s5p_mfc_raw_buf_prot(ctx, mb_entry, false))
							mfc_err_ctx("failed to CFW_PROT\n");
						else
							clear_bit(index, &ctx->raw_protect_flag);
					}
					mfc_debug(2, "[%d] enc src buf un-prot_flag: %#lx\n",
							index, ctx->raw_protect_flag);
				}
				break;
			}
		}

		list_for_each_entry(mb_entry, &enc->ref_queue, list) {
			for (i = 0; i < raw->num_planes; i++) {
				mb_addr[i] = mb_entry->planes.raw[i];
				mfc_debug(2, "enc ref[%d] addr: 0x%08llx\n",
							i, mb_addr[i]);
			}
			if (mb_entry->planes.raw[0] != s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, 0))
				mfc_err_ctx("enc ref yaddr: 0x%08llx != vb2 yaddr: 0x%08llx\n",
						mb_entry->planes.raw[i],
						s5p_mfc_mem_plane_addr(ctx, &mb_entry->vb, i));

			if (enc_addr[0] == mb_addr[0]) {
				list_del(&mb_entry->list);
				enc->ref_queue_cnt--;

				vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);

				/* encoder src buffer CFW UNPROT */
				if (ctx->is_drm) {
					index = mb_entry->vb.v4l2_buf.index;
					if (test_bit(index, &ctx->raw_protect_flag)) {
						if (s5p_mfc_raw_buf_prot(ctx, mb_entry, false))
							mfc_err_ctx("failed to CFW_PROT\n");
						else
							clear_bit(index, &ctx->raw_protect_flag);
					}
					mfc_debug(2, "[%d] enc src buf un-prot_flag: %#lx\n",
							index, ctx->raw_protect_flag);
				}
				break;
			}
		}
	} else if (ctx->state == MFCINST_FINISHING) {
		mb_entry = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
		list_del(&mb_entry->list);
		ctx->src_queue_cnt--;

		vb2_buffer_done(&mb_entry->vb, VB2_BUF_STATE_DONE);

		/* encoder src buffer CFW UNPROT */
		if (ctx->is_drm) {
			index = mb_entry->vb.v4l2_buf.index;
			if (test_bit(index, &ctx->raw_protect_flag)) {
				if (s5p_mfc_raw_buf_prot(ctx, mb_entry, false))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					clear_bit(index, &ctx->raw_protect_flag);
			}
			mfc_debug(2, "[%d] enc src buf un-prot_flag: %#lx\n",
					index, ctx->raw_protect_flag);
		}
	}

	if (ctx->enc_res_change_re_input)
		ctx->enc_res_change_re_input = 0;

	if ((ctx->src_queue_cnt > 0) &&
		((ctx->state == MFCINST_RUNNING) ||
		 (ctx->state == MFCINST_RUNNING_NO_OUTPUT) ||
		 (ctx->state == MFCINST_RUNNING_BUF_FULL))) {
		mb_entry = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);

		if (mb_entry->used) {
			list_del(&mb_entry->list);
			ctx->src_queue_cnt--;

			list_add_tail(&mb_entry->list, &enc->ref_queue);
			enc->ref_queue_cnt++;
		}
		/* slice_type = 4 && strm_size = 0, skipped enable
		   should be considered */
		if ((slice_type == -1) && (strm_size == 0))
			s5p_mfc_change_state(ctx, MFCINST_RUNNING_NO_OUTPUT);

		mfc_debug(2, "slice_type: %d, ctx->state: %d\n", slice_type, ctx->state);
		mfc_debug(2, "enc src count: %d, enc ref count: %d\n",
			  ctx->src_queue_cnt, enc->ref_queue_cnt);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	return 0;
}

static struct s5p_mfc_codec_ops encoder_codec_ops = {
	.get_buf_update_val	= enc_get_buf_update_val,
	.init_ctx_ctrls		= enc_init_ctx_ctrls,
	.cleanup_ctx_ctrls	= enc_cleanup_ctx_ctrls,
	.init_buf_ctrls		= enc_init_buf_ctrls,
	.cleanup_buf_ctrls	= enc_cleanup_buf_ctrls,
	.to_buf_ctrls		= enc_to_buf_ctrls,
	.to_ctx_ctrls		= enc_to_ctx_ctrls,
	.set_buf_ctrls_val	= enc_set_buf_ctrls_val,
	.get_buf_ctrls_val	= enc_get_buf_ctrls_val,
	.recover_buf_ctrls_val	= enc_recover_buf_ctrls_val,
	.pre_seq_start		= enc_pre_seq_start,
	.post_seq_start		= enc_post_seq_start,
	.pre_frame_start	= enc_pre_frame_start,
	.post_frame_start	= enc_post_frame_start,
	.set_buf_ctrls_val_nal_q	= enc_set_buf_ctrls_val_nal_q,
	.get_buf_ctrls_val_nal_q	= enc_get_buf_ctrls_val_nal_q,
};

/* Query capabilities of the device */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, "MFC", sizeof(cap->driver) - 1);
	strncpy(cap->card, "encoder", sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE
			| V4L2_CAP_STREAMING;

	return 0;
}

static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool mplane, bool out)
{
	struct s5p_mfc_fmt *fmt;
	unsigned long i;
	int j = 0;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (mplane && formats[i].mem_planes == 1)
			continue;
		else if (!mplane && formats[i].mem_planes > 1)
			continue;
		if (out && formats[i].type != MFC_FMT_RAW)
			continue;
		else if (!out && formats[i].type != MFC_FMT_ENC)
			continue;

		if (j == f->index) {
			fmt = &formats[i];
			strlcpy(f->description, fmt->name,
				sizeof(f->description));
			f->pixelformat = fmt->fourcc;

			return 0;
		}

		++j;
	}

	return -EINVAL;
}

static int vidioc_enum_fmt_vid_cap(struct file *file, void *pirv,
				   struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, false);
}

static int vidioc_enum_fmt_vid_cap_mplane(struct file *file, void *pirv,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, false);
}

static int vidioc_enum_fmt_vid_out(struct file *file, void *prov,
				   struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, false, true);
}

static int vidioc_enum_fmt_vid_out_mplane(struct file *file, void *prov,
					  struct v4l2_fmtdesc *f)
{
	return vidioc_enum_fmt(f, true, true);
}

static int vidioc_g_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	struct s5p_mfc_raw_info *raw;
	int i;

	mfc_debug_enter();

	mfc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		/* This is run on output (encoder dest) */
		pix_fmt_mp->width = 0;
		pix_fmt_mp->height = 0;
		pix_fmt_mp->field = V4L2_FIELD_NONE;
		pix_fmt_mp->pixelformat = ctx->dst_fmt->fourcc;
		pix_fmt_mp->num_planes = ctx->dst_fmt->mem_planes;

		pix_fmt_mp->plane_fmt[0].bytesperline = enc->dst_buf_size;
		pix_fmt_mp->plane_fmt[0].sizeimage = (unsigned int)(enc->dst_buf_size);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		/* This is run on capture (encoder src) */
		raw = &ctx->raw_buf;

		pix_fmt_mp->width = ctx->img_width;
		pix_fmt_mp->height = ctx->img_height;
		pix_fmt_mp->field = V4L2_FIELD_NONE;
		pix_fmt_mp->pixelformat = ctx->src_fmt->fourcc;
		pix_fmt_mp->num_planes = ctx->src_fmt->mem_planes;
		for (i = 0; i < ctx->src_fmt->mem_planes; i++) {
			pix_fmt_mp->plane_fmt[i].bytesperline = raw->stride[i];
			pix_fmt_mp->plane_fmt[i].sizeimage = raw->plane_size[i];
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return 0;
}

static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, MFC_FMT_ENC);
		if (!fmt) {
			mfc_err("failed to try capture format\n");
			return -EINVAL;
		}

		if (pix_fmt_mp->plane_fmt[0].sizeimage == 0) {
			mfc_err("must be set encoding dst size\n");
			return -EINVAL;
		}

		pix_fmt_mp->plane_fmt[0].bytesperline =
			pix_fmt_mp->plane_fmt[0].sizeimage;
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, MFC_FMT_RAW);
		if (!fmt) {
			mfc_err("failed to try output format\n");
			return -EINVAL;
		}

		if (fmt->mem_planes != pix_fmt_mp->num_planes) {
			mfc_err("plane number is different (%d != %d)\n",
				fmt->mem_planes, pix_fmt_mp->num_planes);
			return -EINVAL;
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	return 0;
}

static int vidioc_s_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_fmt *fmt;
	struct v4l2_pix_format_mplane *pix_fmt_mp = &f->fmt.pix_mp;
	int ret = 0;

	mfc_debug_enter();

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->vq_dst.streaming) {
			v4l2_err(&dev->v4l2_dev, "%s dst queue busy\n", __func__);
			ret = -EBUSY;
			goto out;
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (ctx->vq_src.streaming) {
			v4l2_err(&dev->v4l2_dev, "%s src queue busy\n", __func__);
			ret = -EBUSY;
			goto out;
		}
	}

	if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, MFC_FMT_ENC);
		if (!fmt) {
			mfc_err_ctx("failed to set capture format\n");
			return -EINVAL;
		}
		s5p_mfc_change_state(ctx, MFCINST_INIT);

		ctx->dst_fmt = fmt;
		ctx->codec_mode = ctx->dst_fmt->codec_mode;
		mfc_info_ctx("Enc output codec(%d) : %s\n",
				ctx->dst_fmt->codec_mode, ctx->dst_fmt->name);

		enc->dst_buf_size = pix_fmt_mp->plane_fmt[0].sizeimage;
		pix_fmt_mp->plane_fmt[0].bytesperline = 0;

		ctx->capture_state = QUEUE_FREE;

		ret = s5p_mfc_alloc_instance_buffer(ctx);
		if (ret) {
			mfc_err_ctx("Failed to allocate instance[%d] buffers.\n",
					ctx->num);
			return -ENOMEM;
		}
		if (FW_HAS_ROI_CONTROL(dev)) {
			ret = s5p_mfc_alloc_enc_roi_buffer(ctx);
			if (ret) {
				mfc_err_ctx("Failed to allocate ROI buffers.\n");
				return -ENOMEM;
			}
		}

		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_try_run(dev);
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_OPEN_INSTANCE_RET)) {
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
			s5p_mfc_release_instance_buffer(ctx);
			ret = -EIO;
			goto out;
		}
		mfc_debug(2, "Got instance number: %d\n", ctx->inst_no);
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, MFC_FMT_RAW);
		if (!fmt) {
			mfc_err_ctx("failed to set output format\n");
			return -EINVAL;
		}
		if (!IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) {
				mfc_err_ctx("Not supported format.\n");
				return -EINVAL;
			}
		} else if (IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT) {
				mfc_err_ctx("Not supported format.\n");
				return -EINVAL;
			}
		}

		if (fmt->mem_planes != pix_fmt_mp->num_planes) {
			mfc_err_ctx("plane number is different (%d != %d)\n",
				fmt->mem_planes, pix_fmt_mp->num_planes);
			ret = -EINVAL;
			goto out;
		}

		ctx->src_fmt = fmt;
		ctx->raw_buf.num_planes = ctx->src_fmt->num_planes;
		ctx->img_width = pix_fmt_mp->width;
		ctx->img_height = pix_fmt_mp->height;
		ctx->buf_stride = pix_fmt_mp->plane_fmt[0].bytesperline;

		if ((ctx->state == MFCINST_RUNNING)
			&& (((ctx->old_img_width != 0) && (ctx->old_img_width != ctx->img_width))
				|| ((ctx->old_img_height != 0) && (ctx->old_img_height != ctx->img_height)))) {
			ctx->enc_drc_flag = 1;
		}

		mfc_info_ctx("Enc input pixelformat : %s\n", ctx->src_fmt->name);
		mfc_info_ctx("fmt - w: %d, h: %d, stride: %d\n",
			pix_fmt_mp->width, pix_fmt_mp->height, ctx->buf_stride);

		s5p_mfc_enc_calc_src_size(ctx);

		ctx->output_state = QUEUE_FREE;
	} else {
		mfc_err_ctx("invalid buf type\n");
		return -EINVAL;
	}
out:
	mfc_debug_leave();
	return ret;
}

static int vidioc_reqbufs(struct file *file, void *priv,
					  struct v4l2_requestbuffers *reqbufs)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;
	void *alloc_ctx;

	mfc_debug_enter();

	mfc_debug(2, "type: %d\n", reqbufs->memory);

	if ((reqbufs->memory != V4L2_MEMORY_MMAP) &&
		(reqbufs->memory != V4L2_MEMORY_USERPTR) &&
		(reqbufs->memory != V4L2_MEMORY_DMABUF))
		return -EINVAL;

	if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (reqbufs->count == 0) {
			mfc_debug(2, "Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
			ctx->capture_state = QUEUE_FREE;
			return ret;
		}

		if (ctx->is_drm)
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		else
			alloc_ctx = ctx->dev->alloc_ctx;

		if (ctx->capture_state != QUEUE_FREE) {
			mfc_err_ctx("invalid capture state: %d\n", ctx->capture_state);
			return -EINVAL;
		}

		if (ctx->cacheable & MFCMASK_DST_CACHE)
			s5p_mfc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
		if (ret) {
			mfc_err_ctx("error in vb2_reqbufs() for E(D)\n");
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		s5p_mfc_mem_set_cacheable(alloc_ctx, false);
		ctx->capture_state = QUEUE_BUFS_REQUESTED;

		if (!IS_MFCV6(dev)) {
			ret = s5p_mfc_alloc_codec_buffers(ctx);
			if (ret) {
				mfc_err_ctx("Failed to allocate encoding buffers.\n");
				reqbufs->count = 0;
				vb2_reqbufs(&ctx->vq_dst, reqbufs);
				return -ENOMEM;
			}
		}
	} else if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (reqbufs->count == 0) {
			mfc_debug(2, "Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
			ctx->output_state = QUEUE_FREE;
			return ret;
		}

		if (ctx->is_drm) {
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		} else {
			alloc_ctx = ctx->dev->alloc_ctx;
		}

		if (ctx->output_state != QUEUE_FREE) {
			mfc_err_ctx("invalid output state: %d\n", ctx->output_state);
			return -EINVAL;
		}

		if (ctx->cacheable & MFCMASK_SRC_CACHE)
			s5p_mfc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
		if (ret) {
			mfc_err_ctx("error in vb2_reqbufs() for E(S)\n");
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		s5p_mfc_mem_set_cacheable(alloc_ctx, false);
		ctx->output_state = QUEUE_BUFS_REQUESTED;
	} else {
		mfc_err_ctx("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}

static int vidioc_querybuf(struct file *file, void *priv,
						   struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();

	mfc_debug(2, "type: %d\n", buf->memory);

	if ((buf->memory != V4L2_MEMORY_MMAP) &&
		(buf->memory != V4L2_MEMORY_USERPTR) &&
		(buf->memory != V4L2_MEMORY_DMABUF))
		return -EINVAL;

	mfc_debug(2, "state: %d, buf->type: %d\n", ctx->state, buf->type);

	if (buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->state != MFCINST_GOT_INST) {
			mfc_err("invalid context state: %d\n", ctx->state);
			return -EINVAL;
		}

		ret = vb2_querybuf(&ctx->vq_dst, buf);
		if (ret != 0) {
			mfc_err("error in vb2_querybuf() for E(D)\n");
			return ret;
		}
		buf->m.planes[0].m.mem_offset += DST_QUEUE_OFF_BASE;

	} else if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_src, buf);
		if (ret != 0) {
			mfc_err("error in vb2_querybuf() for E(S)\n");
			return ret;
		}
	} else {
		mfc_err("invalid buf type\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return ret;
}

/* Queue a buffer */
static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = -EINVAL;

	mfc_debug_enter();

	mfc_debug(2, "Enqueued buf: %d (type = %d)\n", buf->index, buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on QBUF after unrecoverable error.\n");
		return -EIO;
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(buf->type) && !buf->length) {
		mfc_err_ctx("multiplanar but length is zero\n");
		return -EIO;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_qbuf(&ctx->vq_src, buf);
		if (!ret) {
			mfc_debug(7, "timestamp: %ld %ld\n",
					buf->timestamp.tv_sec,
					buf->timestamp.tv_usec);
			mfc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
			ctx->last_framerate = (ctx->qos_ratio *
						get_framerate(&buf->timestamp,
						&ctx->last_timestamp)) / 100;
			memcpy(&ctx->last_timestamp, &buf->timestamp,
				sizeof(struct timeval));

#ifdef MFC_ENC_AVG_FPS_MODE
			if (ctx->src_queue_cnt > 0) {
				if (ctx->frame_count > ENC_AVG_FRAMES &&
					ctx->framerate == ENC_MAX_FPS) {
					if (ctx->is_max_fps)
						goto out;
					else
						goto calc_again;
				}

				ctx->frame_count++;
				ctx->avg_framerate =
					(ctx->avg_framerate *
						(ctx->frame_count - 1) +
						ctx->last_framerate) /
						ctx->frame_count;

				if (ctx->frame_count < ENC_AVG_FRAMES)
					goto out;

				if (ctx->avg_framerate > ENC_HIGH_FPS) {
					if (ctx->frame_count == ENC_AVG_FRAMES) {
						mfc_debug(2, "force fps: %d\n", ENC_MAX_FPS);
						ctx->framerate = ENC_MAX_FPS;
						ctx->is_max_fps = 1;
						s5p_mfc_qos_on(ctx);
					}
					goto out;
				}
			} else {
				if (ctx->is_max_fps)
					ctx->last_framerate = ENC_MAX_FPS;
				else
					ctx->last_framerate = ENC_DEFAULT_FPS;
				mfc_debug(2, "fps set to %d\n", ctx->last_framerate);
			}
calc_again:
#endif
			if (ctx->last_framerate != 0 &&
				ctx->last_framerate != ctx->framerate) {
				mfc_debug(2, "fps changed: %d -> %d\n",
					ctx->framerate, ctx->last_framerate);
				ctx->framerate = ctx->last_framerate;
				s5p_mfc_qos_on(ctx);
			}
		}
	} else {
		ret = vb2_qbuf(&ctx->vq_dst, buf);
	}

out:
	mfc_debug_leave();
	return ret;
}

/* Dequeue a buffer */
static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret;

	mfc_debug_enter();
	mfc_debug(2, "Addr: %p %p %p Type: %d\n", &ctx->vq_src, buf, buf->m.planes,
								buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on DQBUF after unrecoverable error.\n");
		return -EIO;
	}
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE)
		ret = vb2_dqbuf(&ctx->vq_src, buf, file->f_flags & O_NONBLOCK);
	else
		ret = vb2_dqbuf(&ctx->vq_dst, buf, file->f_flags & O_NONBLOCK);
	mfc_debug_leave();
	return ret;
}

/* Stream on */
static int vidioc_streamon(struct file *file, void *priv,
			   enum v4l2_buf_type type)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = -EINVAL;

	mfc_debug_enter();
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_streamon(&ctx->vq_src, type);

		if (!ret) {
			ctx->frame_count = 0;
			ctx->is_max_fps = 0;
			ctx->avg_framerate = 0;
			s5p_mfc_qos_on(ctx);
		}
	} else {
		ret = vb2_streamon(&ctx->vq_dst, type);
	}
	mfc_debug(2, "ctx->src_queue_cnt = %d ctx->state = %d "
		  "ctx->dst_queue_cnt = %d ctx->dpb_count = %d\n",
		  ctx->src_queue_cnt, ctx->state, ctx->dst_queue_cnt,
		  ctx->dpb_count);
	mfc_debug_leave();
	return ret;
}

/* Stream off, which equals to a pause */
static int vidioc_streamoff(struct file *file, void *priv,
			    enum v4l2_buf_type type)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret;

	mfc_debug_enter();
	ret = -EINVAL;
	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->last_framerate = 0;
		memset(&ctx->last_timestamp, 0, sizeof(struct timeval));

		ctx->old_img_width = ctx->img_width;
		ctx->old_img_height = ctx->img_height;

		mfc_debug(2, "vidioc_streamoff ctx->old_img_width = %d\n", ctx->old_img_width);
		mfc_debug(2, "vidioc_streamoff ctx->old_img_height = %d\n", ctx->old_img_height);

		ret = vb2_streamoff(&ctx->vq_src, type);
		if (!ret)
			s5p_mfc_qos_off(ctx);
	} else {
		ret = vb2_streamoff(&ctx->vq_dst, type);
	}

	mfc_debug(2, "streamoff\n");
	mfc_debug_leave();

	return ret;
}

/* Query a ctrl */
static int vidioc_queryctrl(struct file *file, void *priv,
			    struct v4l2_queryctrl *qc)
{
	struct v4l2_queryctrl *c;

	c = get_ctrl(qc->id);
	if (!c)
		return -EINVAL;
	*qc = *c;
	return 0;
}

static int enc_ext_info(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int val = 0;

	if (IS_MFCv7X(dev) || IS_MFCV8(dev)) {
		if (!IS_MFCv10X(dev))
			val |= ENC_SET_RGB_INPUT;
		val |= ENC_SET_SPARE_SIZE;
	}
	if (FW_HAS_TEMPORAL_SVC_CH(dev))
		val |= ENC_SET_TEMP_SVC_CH;

	if (FW_SUPPORT_SKYPE(dev))
		val |= ENC_SET_SKYPE_FLAG;

	if (FW_HAS_ROI_CONTROL(dev))
		val |= ENC_SET_ROI_CONTROL;

	if (FW_HAS_FIXED_SLICE(dev))
		val |= ENC_SET_FIXED_SLICE;

	val |= ENC_SET_QP_BOUND_PB;

	return val;
}

static int get_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;

	switch (ctrl->id) {
	case V4L2_CID_CACHEABLE:
		ctrl->value = ctx->cacheable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_STREAM_SIZE:
		ctrl->value = enc->dst_buf_size;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_COUNT:
		ctrl->value = enc->frame_count;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TYPE:
		ctrl->value = enc->frame_type;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CHECK_STATE:
		if (ctx->state == MFCINST_RUNNING_NO_OUTPUT)
			ctrl->value = MFCSTATE_ENC_NO_OUTPUT;
		else
			ctrl->value = MFCSTATE_PROCESSING;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
	case V4L2_CID_MPEG_MFC51_VIDEO_LUMA_ADDR:
	case V4L2_CID_MPEG_MFC51_VIDEO_CHROMA_ADDR:
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_STATUS:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_GET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				if (ctx_ctrl->has_new) {
					ctx_ctrl->has_new = 0;
					ctrl->value = ctx_ctrl->val;
				} else {
					mfc_debug(8, "Control value "\
							"is not up to date: "\
							"0x%08x\n", ctrl->id);
					return -EINVAL;
				}

				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control: 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MFC_GET_VERSION_INFO:
		ctrl->value = mfc_version(dev);
		break;
	case V4L2_CID_MPEG_MFC_GET_DRIVER_INFO:
		ctrl->value = MFC_DRIVER_INFO;
		break;
	case V4L2_CID_MPEG_MFC_GET_EXTRA_BUFFER_SIZE:
		ctrl->value = mfc_linear_buf_size(mfc_version(dev));
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctrl->value = ctx->qos_ratio;
		break;
	case V4L2_CID_MPEG_MFC_GET_EXT_INFO:
		ctrl->value = enc_ext_info(ctx);
		break;
	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control: 0x%08x\n", ctrl->id);
		ret = -EINVAL;
		break;
	}

	return ret;
}

static int vidioc_g_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();
	ret = get_ctrl_val(ctx, ctrl);
	mfc_debug_leave();

	return ret;
}

static inline int h264_level(enum v4l2_mpeg_video_h264_level lvl)
{
	static unsigned int t[V4L2_MPEG_VIDEO_H264_LEVEL_5_1 + 1] = {
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_0   */ 10,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1B    */ 9,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_1   */ 11,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_2   */ 12,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_1_3   */ 13,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_0   */ 20,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_1   */ 21,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_2_2   */ 22,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_0   */ 30,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_1   */ 31,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_3_2   */ 32,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_0   */ 40,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_1   */ 41,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_4_2   */ 42,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_5_0   */ 50,
		/* V4L2_MPEG_VIDEO_H264_LEVEL_5_1   */ 51,
	};
	return t[lvl];
}

static inline int mpeg4_level(enum v4l2_mpeg_video_mpeg4_level lvl)
{
	static unsigned int t[V4L2_MPEG_VIDEO_MPEG4_LEVEL_6 + 1] = {
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_0	     */ 0,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_0B, Simple    */ 9,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_1	     */ 1,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_2	     */ 2,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_3	     */ 3,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_3B, Advanced  */ 7,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_4	     */ 4,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_5	     */ 5,
		/* V4L2_MPEG_VIDEO_MPEG4_LEVEL_6,  Simple    */ 6,
	};
	return t[lvl];
}

static inline int vui_sar_idc(enum v4l2_mpeg_video_h264_vui_sar_idc sar)
{
	static unsigned int t[V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_EXTENDED + 1] = {
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_UNSPECIFIED     */ 0,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_1x1             */ 1,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_12x11           */ 2,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_10x11           */ 3,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_16x11           */ 4,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_40x33           */ 5,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_24x11           */ 6,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_20x11           */ 7,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_32x11           */ 8,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_80x33           */ 9,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_18x11           */ 10,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_15x11           */ 11,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_64x33           */ 12,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_160x99          */ 13,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_4x3             */ 14,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_3x2             */ 15,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_2x1             */ 16,
		/* V4L2_MPEG_VIDEO_H264_VUI_SAR_IDC_EXTENDED        */ 255,
	};
	return t[sar];
}

static int set_enc_param(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	int ret = 0;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_GOP_SIZE:
		p->gop_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MODE:
		p->slice_mode =
			(enum v4l2_mpeg_video_multi_slice_mode)ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB:
		p->slice_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_BYTES:
		p->slice_bit = ctrl->value * 8;
		break;
	case V4L2_CID_MPEG_VIDEO_MULTI_SLICE_MAX_MB_ROW:
		p->slice_mb_row = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_CYCLIC_INTRA_REFRESH_MB:
		p->intra_refresh_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PADDING:
		p->pad = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PADDING_YUV:
		p->pad_luma = (ctrl->value >> 16) & 0xff;
		p->pad_cb = (ctrl->value >> 8) & 0xff;
		p->pad_cr = (ctrl->value >> 0) & 0xff;
		break;
	case V4L2_CID_MPEG_VIDEO_FRAME_RC_ENABLE:
		p->rc_frame = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_BITRATE:
		p->rc_bitrate = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_REACTION_COEFF:
		p->rc_reaction_coeff = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
		enc->force_frame_type = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VBV_SIZE:
		p->vbv_buf_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEADER_MODE:
		p->seq_hdr_mode =
			(enum v4l2_mpeg_video_header_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_SKIP_MODE:
		p->frame_skip_mode =
			(enum v4l2_mpeg_mfc51_video_frame_skip_mode)
			(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_RC_FIXED_TARGET_BIT: /* MFC5.x Only */
		p->fixed_target_bit = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_B_FRAMES:
		p->num_b_frame = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
		p->codec.h264.profile =
		h264_profile(ctx, (enum v4l2_mpeg_video_h264_profile)(ctrl->value));
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
		p->codec.h264.level =
		h264_level((enum v4l2_mpeg_video_h264_level)(ctrl->value));
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_INTERLACE:
		p->codec.h264.interlace = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_MODE:
		p->codec.h264.loop_filter_mode =
		(enum v4l2_mpeg_video_h264_loop_filter_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_ALPHA:
		p->codec.h264.loop_filter_alpha = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_LOOP_FILTER_BETA:
		p->codec.h264.loop_filter_beta = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ENTROPY_MODE:
		p->codec.h264.entropy_mode =
			(enum v4l2_mpeg_video_h264_entropy_mode)(ctrl->value);
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_NUM_REF_PIC_FOR_P:
		p->codec.h264.num_ref_pic_4p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_8X8_TRANSFORM:
		p->codec.h264._8x8_transform = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MB_RC_ENABLE:
		p->rc_mb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_RC_FRAME_RATE:
		p->codec.h264.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_FRAME_QP:
		p->codec.h264.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
		p->codec.h264.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
		p->codec.h264.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_P:
		p->codec.h264.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_P:
		p->codec.h264.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_B:
		p->codec.h264.rc_min_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_B:
		p->codec.h264.rc_max_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_DARK:
		p->codec.h264.rc_mb_dark = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_SMOOTH:
		p->codec.h264.rc_mb_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_STATIC:
		p->codec.h264.rc_mb_static = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H264_ADAPTIVE_RC_ACTIVITY:
		p->codec.h264.rc_mb_activity = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_P_FRAME_QP:
		p->codec.h264.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_B_FRAME_QP:
		p->codec.h264.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_ENABLE:
		p->codec.h264.ar_vui = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_SAR_IDC:
		p->codec.h264.ar_vui_idc =
		vui_sar_idc((enum v4l2_mpeg_video_h264_vui_sar_idc)(ctrl->value));
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_WIDTH:
		p->codec.h264.ext_sar_width = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_VUI_EXT_SAR_HEIGHT:
		p->codec.h264.ext_sar_height = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_GOP_CLOSURE:
		p->codec.h264.open_gop = ctrl->value ? 0 : 1;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_I_PERIOD:
		p->codec.h264.open_gop_size = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING:
		p->codec.h264.hier_qp_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_TYPE:
		p->codec.h264.hier_qp_type =
		(enum v4l2_mpeg_video_h264_hierarchical_coding_type)(ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER:
		p->codec.h264.num_hier_layer = ctrl->value & 0x7;
		p->codec.h264.hier_ref_type = (ctrl->value >> 16) & 0x1;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_QP:
		p->codec.h264.hier_qp_layer[(ctrl->value >> 16) & 0x7]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.h264.hier_bit_layer[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.h264.hier_bit_layer[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.h264.hier_bit_layer[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT3:
		p->codec.h264.hier_bit_layer[3] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT4:
		p->codec.h264.hier_bit_layer[4] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT5:
		p->codec.h264.hier_bit_layer[5] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_BIT6:
		p->codec.h264.hier_bit_layer[6] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		p->codec.h264.sei_gen_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_CURRENT_FRAME_0:
		p->codec.h264.sei_fp_curr_frame_0 = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FP_ARRANGEMENT_TYPE:
		p->codec.h264.sei_fp_arrangement_type = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO:
		p->codec.h264.fmo_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_MAP_TYPE:
		switch ((enum v4l2_mpeg_video_h264_fmo_map_type)(ctrl->value)) {
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_INTERLEAVED_SLICES:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_SCATTERED_SLICES:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_RASTER_SCAN:
		case V4L2_MPEG_VIDEO_H264_FMO_MAP_TYPE_WIPE_SCAN:
			p->codec.h264.fmo_slice_map_type = ctrl->value;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_SLICE_GROUP:
		p->codec.h264.fmo_slice_num_grp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_RUN_LENGTH:
		p->codec.h264.fmo_run_length[(ctrl->value >> 30) & 0x3]
			= ctrl->value & 0x3FFFFFFF;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_DIRECTION:
		p->codec.h264.fmo_sg_dir = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_FMO_CHANGE_RATE:
		p->codec.h264.fmo_sg_rate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ASO:
		p->codec.h264.aso_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_ASO_SLICE_ORDER:
		p->codec.h264.aso_slice_order[(ctrl->value >> 18) & 0x7]
			&= ~(0xFF << (((ctrl->value >> 16) & 0x3) << 3));
		p->codec.h264.aso_slice_order[(ctrl->value >> 18) & 0x7]
			|= (ctrl->value & 0xFF) << \
				(((ctrl->value >> 16) & 0x3) << 3);
		break;
	case V4L2_CID_MPEG_VIDEO_H264_PREPEND_SPSPPS_TO_IDR:
		p->codec.h264.prepend_sps_pps_to_idr = ctrl->value ? 1 : 0;
		break;
	case V4L2_CID_MPEG_MFC_H264_ENABLE_LTR:
		p->codec.h264.enable_ltr = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_H264_NUM_OF_LTR:
		p->codec.h264.num_of_ltr = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY:
		p->codec.h264.base_priority = ctrl->value;
		p->codec.h264.set_priority = 1;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_PROFILE:
		switch ((enum v4l2_mpeg_video_mpeg4_profile)(ctrl->value)) {
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_SIMPLE:
			p->codec.mpeg4.profile =
				S5P_FIMV_ENC_PROFILE_MPEG4_SIMPLE;
			break;
		case V4L2_MPEG_VIDEO_MPEG4_PROFILE_ADVANCED_SIMPLE:
			p->codec.mpeg4.profile =
			S5P_FIMV_ENC_PROFILE_MPEG4_ADVANCED_SIMPLE;
			break;
		default:
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_LEVEL:
		p->codec.mpeg4.level =
		mpeg4_level((enum v4l2_mpeg_video_mpeg4_level)(ctrl->value));
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_I_FRAME_QP:
		p->codec.mpeg4.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
		p->codec.mpeg4.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
		p->codec.mpeg4.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_P:
		p->codec.mpeg4.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_P:
		p->codec.mpeg4.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_B:
		p->codec.mpeg4.rc_min_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_B:
		p->codec.mpeg4.rc_max_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_QPEL:
		p->codec.mpeg4.quarter_pixel = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_P_FRAME_QP:
		p->codec.mpeg4.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_MPEG4_B_FRAME_QP:
		p->codec.mpeg4.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_TIME_RES:
		p->codec.mpeg4.vop_time_res = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_MPEG4_VOP_FRM_DELTA:
		p->codec.mpeg4.vop_frm_delta = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_H263_RC_FRAME_RATE:
		p->codec.mpeg4.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_I_FRAME_QP:
		p->codec.mpeg4.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
		p->codec.mpeg4.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
		p->codec.mpeg4.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP_P:
		p->codec.mpeg4.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP_P:
		p->codec.mpeg4.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H263_P_FRAME_QP:
		p->codec.mpeg4.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_VERSION:
		p->codec.vp8.vp8_version = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_RC_FRAME_RATE:
		p->codec.vp8.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
		p->codec.vp8.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
		p->codec.vp8.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP_P:
		p->codec.vp8.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP_P:
		p->codec.vp8.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_I_FRAME_QP:
		p->codec.vp8.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_P_FRAME_QP:
		p->codec.vp8.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_OF_PARTITIONS:
		p->codec.vp8.vp8_numberofpartitions = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_LEVEL:
		p->codec.vp8.vp8_filterlevel = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_FILTER_SHARPNESS:
		p->codec.vp8.vp8_filtersharpness = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_GOLDEN_FRAMESEL:
		p->codec.vp8.vp8_goldenframesel = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_GF_REFRESH_PERIOD:
		p->codec.vp8.vp8_gfrefreshperiod = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_ENABLE:
		p->codec.vp8.hier_qp_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER0:
		p->codec.vp8.hier_qp_layer[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER1:
		p->codec.vp8.hier_qp_layer[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_HIERARCHY_QP_LAYER2:
		p->codec.vp8.hier_qp_layer[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.vp8.hier_bit_layer[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.vp8.hier_bit_layer[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.vp8.hier_bit_layer[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_REF_NUMBER_FOR_PFRAMES:
		p->codec.vp8.num_refs_for_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_DISABLE_INTRA_MD4X4:
		p->codec.vp8.intra_4x4mode_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC70_VIDEO_VP8_NUM_TEMPORAL_LAYER:
		p->codec.vp8.num_hier_layer = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_VERSION:
		p->codec.vp9.vp9_version = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_RC_FRAME_RATE:
		p->codec.vp9.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP:
		p->codec.vp9.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP:
		p->codec.vp9.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP_P:
		p->codec.vp9.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP_P:
		p->codec.vp9.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_I_FRAME_QP:
		p->codec.vp9.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_P_FRAME_QP:
		p->codec.vp9.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_GOLDEN_FRAMESEL:
		p->codec.vp9.vp9_goldenframesel = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_GF_REFRESH_PERIOD:
		p->codec.vp9.vp9_gfrefreshperiod = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHY_QP_ENABLE:
		p->codec.vp9.hier_qp_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_QP:
		p->codec.vp9.hier_qp_layer[(ctrl->value >> 16) & 0x3]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.vp9.hier_bit_layer[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.vp9.hier_bit_layer[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.vp9.hier_bit_layer[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_REF_NUMBER_FOR_PFRAMES:
		p->codec.vp9.num_refs_for_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER:
		p->codec.vp9.num_hier_layer = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_MAX_PARTITION_DEPTH:
		p->codec.vp9.max_partition_depth = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_VP9_DISABLE_INTRA_PU_SPLIT:
		p->codec.vp9.intra_pu_split_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DISABLE_IVF_HEADER:
		p->codec.vp9.ivf_header = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_I_FRAME_QP:
		p->codec.hevc.rc_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_P_FRAME_QP:
		p->codec.hevc.rc_p_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_B_FRAME_QP:
		p->codec.hevc.rc_b_frame_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_RC_FRAME_RATE:
		p->codec.hevc.rc_framerate = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
		p->codec.hevc.rc_min_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
		p->codec.hevc.rc_max_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_P:
		p->codec.hevc.rc_min_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_P:
		p->codec.hevc.rc_max_qp_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_B:
		p->codec.hevc.rc_min_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_B:
		p->codec.hevc.rc_max_qp_b = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_LEVEL:
		p->codec.hevc.level = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PROFILE:
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_DARK:
		p->codec.hevc.rc_lcu_dark = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_SMOOTH:
		p->codec.hevc.rc_lcu_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_STATIC:
		p->codec.hevc.rc_lcu_static = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_ADAPTIVE_RC_ACTIVITY:
		p->codec.hevc.rc_lcu_activity = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TIER_FLAG:
		p->codec.hevc.tier_flag = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_PARTITION_DEPTH:
		p->codec.hevc.max_partition_depth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REF_NUMBER_FOR_PFRAMES:
		p->codec.hevc.num_refs_for_p = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_TYPE:
		p->codec.hevc.refreshtype = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_CONST_INTRA_PRED_ENABLE:
		p->codec.hevc.const_intra_period_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LOSSLESS_CU_ENABLE:
		p->codec.hevc.lossless_cu_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WAVEFRONT_ENABLE:
		p->codec.hevc.wavefront_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_DISABLE:
		p->codec.hevc.loopfilter_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_SLICE_BOUNDARY:
		p->codec.hevc.loopfilter_across = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LTR_ENABLE:
		p->codec.hevc.enable_ltr = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_QP_ENABLE:
		p->codec.hevc.hier_qp_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_TYPE:
		p->codec.hevc.hier_qp_type =
		(enum v4l2_mpeg_video_hevc_hierarchical_coding_type)(ctrl->value);
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER:
		p->codec.hevc.num_hier_layer = ctrl->value & 0x7;
		p->codec.hevc.hier_ref_type = (ctrl->value >> 16) & 0x1;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_QP:
		p->codec.hevc.hier_qp_layer[(ctrl->value >> 16) & 0x7]
			= ctrl->value & 0xFF;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT0:
		p->codec.hevc.hier_bit_layer[0] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT1:
		p->codec.hevc.hier_bit_layer[1] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT2:
		p->codec.hevc.hier_bit_layer[2] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT3:
		p->codec.hevc.hier_bit_layer[3] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT4:
		p->codec.hevc.hier_bit_layer[4] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT5:
		p->codec.hevc.hier_bit_layer[5] = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_BIT6:
		p->codec.hevc.hier_bit_layer[6] = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIGN_DATA_HIDING:
		p->codec.hevc.sign_data_hiding = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_GENERAL_PB_ENABLE:
		p->codec.hevc.general_pb_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_TEMPORAL_ID_ENABLE:
		p->codec.hevc.temporal_id_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STRONG_SMOTHING_FLAG:
		p->codec.hevc.strong_intra_smooth = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_INTRA_PU_SPLIT:
		p->codec.hevc.intra_pu_split_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_DISABLE_TMV_PREDICTION:
		p->codec.hevc.tmv_prediction_disable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_MAX_NUM_MERGE_MV_MINUS1:
		p->codec.hevc.max_num_merge_mv = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_WITHOUT_STARTCODE_ENABLE:
		p->codec.hevc.encoding_nostartcode_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_REFRESH_PERIOD:
		p->codec.hevc.refreshperiod = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_BETA_OFFSET_DIV2:
		p->codec.hevc.lf_beta_offset_div2 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_LF_TC_OFFSET_DIV2:
		p->codec.hevc.lf_tc_offset_div2 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_SIZE_OF_LENGTH_FIELD:
		p->codec.hevc.size_of_length_field = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_USER_REF:
		p->codec.hevc.user_ref = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC90_VIDEO_HEVC_STORE_REF:
		p->codec.hevc.store_ref = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_ROI_ENABLE:
		p->codec.hevc.roi_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_H264_VUI_RESTRICTION_ENABLE:
		p->codec.h264.vui_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_HEVC_PREPEND_SPSPPS_TO_IDR:
		p->codec.hevc.prepend_sps_pps_to_idr = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_CONFIG_QP_ENABLE:
		p->dynamic_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_CONFIG_QP:
		p->config_qp = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_TEMPORAL_SHORTTERM_MAX_LAYER:
		p->num_hier_max_layer = ctrl->value;
		break;
	default:
		v4l2_err(&dev->v4l2_dev, "Invalid control\n");
		ret = -EINVAL;
	}

	return ret;
}

static int process_user_shared_handle_enc(struct s5p_mfc_ctx *ctx,
			struct mfc_user_shared_handle *handle)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret = 0;

	handle->ion_handle =
		ion_import_dma_buf(dev->mfc_ion_client, handle->fd);
	if (IS_ERR(handle->ion_handle)) {
		mfc_err_ctx("Failed to import fd\n");
		ret = PTR_ERR(handle->ion_handle);
		goto import_dma_fail;
	}

	handle->virt =
		ion_map_kernel(dev->mfc_ion_client, handle->ion_handle);
	if (handle->virt == NULL) {
		mfc_err_ctx("Failed to get kernel virtual address\n");
		ret = -EINVAL;
		goto map_kernel_fail;
	}

	mfc_debug(2, "User Handle: fd = %d, virt = 0x%p\n",
				handle->fd, handle->virt);

	return 0;

map_kernel_fail:
	ion_free(dev->mfc_ion_client, handle->ion_handle);

import_dma_fail:
	return ret;
}


int enc_cleanup_user_shared_handle(struct s5p_mfc_ctx *ctx,
				struct mfc_user_shared_handle *handle)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	if (handle->fd == -1)
		return 0;

	if (handle->virt)
		ion_unmap_kernel(dev->mfc_ion_client,
					handle->ion_handle);

	ion_free(dev->mfc_ion_client, handle->ion_handle);

	return 0;
}


static int set_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{

	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;
	int index = 0;

	switch (ctrl->id) {
	case V4L2_CID_CACHEABLE:
		if (ctrl->value)
			ctx->cacheable |= ctrl->value;
		else
			ctx->cacheable = 0;
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctx->qos_ratio = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_H263_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_VP8_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_VP9_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_P:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_H263_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_VP8_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_VP9_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_P:
	case V4L2_CID_MPEG_VIDEO_H264_MAX_QP_B:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MAX_QP_B:
	case V4L2_CID_MPEG_VIDEO_HEVC_MAX_QP_B:
	case V4L2_CID_MPEG_VIDEO_H264_MIN_QP_B:
	case V4L2_CID_MPEG_VIDEO_MPEG4_MIN_QP_B:
	case V4L2_CID_MPEG_VIDEO_HEVC_MIN_QP_B:
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG:
	case V4L2_CID_MPEG_MFC51_VIDEO_FORCE_FRAME_TYPE:
	case V4L2_CID_MPEG_MFC51_VIDEO_I_PERIOD_CH:
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH:
	case V4L2_CID_MPEG_MFC51_VIDEO_BIT_RATE_CH:
	case V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH:
	case V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH:
	case V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH:
	case V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH:
	case V4L2_CID_MPEG_VIDEO_H264_PROFILE:
	case V4L2_CID_MPEG_VIDEO_H264_LEVEL:
	case V4L2_CID_MPEG_MFC_H264_MARK_LTR:
	case V4L2_CID_MPEG_MFC_H264_USE_LTR:
	case V4L2_CID_MPEG_MFC_H264_BASE_PRIORITY:
	case V4L2_CID_MPEG_MFC_CONFIG_QP:
	case V4L2_CID_MPEG_VIDEO_ROI_CONTROL:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = ctrl->value;
				if (ctx_ctrl->id == \
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE_CH)
					ctx_ctrl->val *= p->rc_frame_delta;

				if (((ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_H264_HIERARCHICAL_CODING_LAYER_CH) ||
					(ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_VP8_HIERARCHICAL_CODING_LAYER_CH) ||
					(ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_VP9_HIERARCHICAL_CODING_LAYER_CH) ||
					(ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_HEVC_HIERARCHICAL_CODING_LAYER_CH)) &&
					(enc->sh_handle_svc.fd == -1)) {
						enc->sh_handle_svc.fd = ctrl->value;
						if (process_user_shared_handle_enc(ctx,
									&enc->sh_handle_svc)) {
							enc->sh_handle_svc.fd = -1;
							return -EINVAL;
						}
				}
				if (ctx_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_LEVEL)
					ctx_ctrl->val = h264_level((enum v4l2_mpeg_video_h264_level)(ctrl->value));
				if (ctx_ctrl->id == V4L2_CID_MPEG_VIDEO_H264_PROFILE)
					ctx_ctrl->val = h264_profile(ctx, (enum v4l2_mpeg_video_h264_profile)(ctrl->value));
				if (ctx_ctrl->id == \
					V4L2_CID_MPEG_VIDEO_ROI_CONTROL) {
					enc->sh_handle_roi.fd = ctrl->value;
					if (process_user_shared_handle_enc(ctx,
								&enc->sh_handle_roi)) {
						enc->sh_handle_roi.fd = -1;
						return -EINVAL;
					}
					index = enc->roi_index;
					memcpy(&enc->roi_info[index],
							enc->sh_handle_roi.virt,
							sizeof(struct mfc_enc_roi_info));
					if (copy_from_user(enc->roi_buf[index].virt,
							enc->roi_info[index].addr,
							enc->roi_info[index].size))
						return -EFAULT;
				}

				found = 1;
				break;
			}
		}

		if (!found) {
			v4l2_err(&dev->v4l2_dev, "Invalid control 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	default:
		ret = set_enc_param(ctx, ctrl);
		break;
	}

	return ret;
}

static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();

	ret = check_ctrl_val(ctx, ctrl);
	if (ret != 0)
		return ret;

	ret = set_ctrl_val(ctx, ctrl);

	mfc_debug_leave();

	return ret;
}

static int vidioc_g_ext_ctrls(struct file *file, void *priv,
			      struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;

		ret = get_ctrl_val(ctx, &ctrl);
		if (ret == 0) {
			ext_ctrl->value = ctrl.value;
		} else {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static int vidioc_s_ext_ctrls(struct file *file, void *priv,
				struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;
		ctrl.value = ext_ctrl->value;

		ret = check_ctrl_val(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		ret = set_enc_param(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static int vidioc_try_ext_ctrls(struct file *file, void *priv,
				struct v4l2_ext_controls *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct v4l2_ext_control *ext_ctrl;
	struct v4l2_control ctrl;
	int i;
	int ret = 0;

	if (f->ctrl_class != V4L2_CTRL_CLASS_MPEG)
		return -EINVAL;

	for (i = 0; i < f->count; i++) {
		ext_ctrl = (f->controls + i);

		ctrl.id = ext_ctrl->id;
		ctrl.value = ext_ctrl->value;

		ret = check_ctrl_val(ctx, &ctrl);
		if (ret != 0) {
			f->error_idx = i;
			break;
		}

		mfc_debug(2, "[%d] id: 0x%08x, value: %d", i, ext_ctrl->id, ext_ctrl->value);
	}

	return ret;
}

static const struct v4l2_ioctl_ops s5p_mfc_enc_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_g_ext_ctrls = vidioc_g_ext_ctrls,
	.vidioc_s_ext_ctrls = vidioc_s_ext_ctrls,
	.vidioc_try_ext_ctrls = vidioc_try_ext_ctrls,
};

static int s5p_mfc_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *buf_count, unsigned int *plane_count,
				unsigned int psize[], void *allocators[])
{
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_raw_info *raw;
	int i;
	void *alloc_ctx = ctx->dev->alloc_ctx;
	void *output_ctx;

	mfc_debug_enter();

	if (ctx->state != MFCINST_GOT_INST &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		mfc_err_ctx("invalid state: %d\n", ctx->state);
		return -EINVAL;
	}
	if (ctx->state >= MFCINST_FINISHING &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mfc_err_ctx("invalid state: %d\n", ctx->state);
		return -EINVAL;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->dst_fmt)
			*plane_count = ctx->dst_fmt->mem_planes;
		else
			*plane_count = MFC_ENC_CAP_PLANE_COUNT;

		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;

		psize[0] = (unsigned int)(enc->dst_buf_size);
		if (ctx->is_drm)
			allocators[0] = ctx->dev->alloc_ctx_drm;
		else
			allocators[0] = alloc_ctx;
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		raw = &ctx->raw_buf;

		if (ctx->src_fmt)
			*plane_count = ctx->src_fmt->mem_planes;
		else
			*plane_count = MFC_ENC_OUT_PLANE_COUNT;

		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;

		if (ctx->is_drm) {
			output_ctx = ctx->dev->alloc_ctx_drm;
		} else {
			output_ctx = alloc_ctx;
		}

		if (*plane_count == 1) {
			psize[0] = raw->total_plane_size;
			allocators[0] = output_ctx;
		} else {
			for (i = 0; i < *plane_count; i++) {
				psize[i] = raw->plane_size[i];
				allocators[i] = output_ctx;
			}
		}
	} else {
		mfc_err_ctx("invalid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	mfc_debug(2, "buf_count: %d, plane_count: %d\n", *buf_count, *plane_count);
	for (i = 0; i < *plane_count; i++)
		mfc_debug(2, "plane[%d] size=%d\n", i, psize[i]);

	mfc_debug_leave();

	return 0;
}

static void s5p_mfc_unlock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	mutex_unlock(&dev->mfc_mutex);
}

static void s5p_mfc_lock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	mutex_lock(&dev->mfc_mutex);
}

static int s5p_mfc_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	dma_addr_t start_raw;
	int i, ret;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = check_vb_with_fmt(ctx->dst_fmt, vb);
		if (ret < 0)
			return ret;

		buf->planes.stream = s5p_mfc_mem_plane_addr(ctx, vb, 0);

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_DST,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");

	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = check_vb_with_fmt(ctx->src_fmt, vb);
		if (ret < 0)
			return ret;

		start_raw = s5p_mfc_mem_plane_addr(ctx, vb, 0);
		if (start_raw == 0) {
			mfc_err_ctx("Plane mem not allocated.\n");
			return -ENOMEM;
		}
		if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_NV12N) {
			buf->planes.raw[0] = start_raw;
			buf->planes.raw[1] = NV12N_CBCR_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
		} else if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_YUV420N) {
			buf->planes.raw[0] = start_raw;
			buf->planes.raw[1] = YUV420N_CB_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
			buf->planes.raw[2] = YUV420N_CR_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
		} else {
			for (i = 0; i < ctx->src_fmt->num_planes; i++)
				buf->planes.raw[i] = s5p_mfc_mem_plane_addr(ctx, vb, i);
		}

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_SRC,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");

	} else {
		mfc_err_ctx("inavlid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_raw_info *raw;
	unsigned int index = vb->v4l2_buf.index;
	int ret, i;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = check_vb_with_fmt(ctx->dst_fmt, vb);
		if (ret < 0)
			return ret;

		mfc_debug(2, "plane size: %lu, dst size: %zu\n",
			vb2_plane_size(vb, 0), enc->dst_buf_size);

		if (vb2_plane_size(vb, 0) < enc->dst_buf_size) {
			mfc_err_ctx("plane size is too small for capture\n");
			return -EINVAL;
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = check_vb_with_fmt(ctx->src_fmt, vb);
		if (ret < 0)
			return ret;

		raw = &ctx->raw_buf;
		if (ctx->src_fmt->mem_planes == 1) {
			mfc_debug(2, "Plane size = %lu, src size:%d\n",
					vb2_plane_size(vb, 0),
					raw->total_plane_size);
			if (vb2_plane_size(vb, 0) < raw->total_plane_size) {
				mfc_err_ctx("Output plane is too small\n");
				return -EINVAL;
			}
		} else {
			for (i = 0; i < ctx->src_fmt->mem_planes; i++) {
				mfc_debug(2, "plane[%d] size: %lu, src[%d] size: %d\n",
						i, vb2_plane_size(vb, i),
						i, raw->plane_size[i]);
				if (vb2_plane_size(vb, i) < raw->plane_size[i]) {
					mfc_err_ctx("Output plane[%d] is too smalli\n", i);
					return -EINVAL;
				}
			}
		}

		if (call_cop(ctx, to_buf_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_buf_ctrls\n");
	} else {
		mfc_err_ctx("inavlid queue type: %d\n", vq->type);
		return -EINVAL;
	}

	s5p_mfc_mem_prepare(vb);

	mfc_debug_leave();

	return 0;
}

static void s5p_mfc_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;


	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_ctx_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, to_ctx_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_ctx_ctrls\n");
	}

	s5p_mfc_mem_finish(vb);
}

static void s5p_mfc_buf_cleanup(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					MFC_CTRL_TYPE_DST, index) < 0)
			mfc_err_ctx("failed in cleanup_buf_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (call_cop(ctx, cleanup_buf_ctrls, ctx,
					MFC_CTRL_TYPE_SRC, index) < 0)
			mfc_err_ctx("failed in cleanup_buf_ctrls\n");
	} else {
		mfc_err_ctx("s5p_mfc_buf_cleanup: unknown queue type.\n");
	}

	mfc_debug_leave();
}

static int s5p_mfc_start_streaming(struct vb2_queue *q, unsigned int count)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;

	/* If context is ready then dev = work->data;schedule it to run */
	if (s5p_mfc_enc_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}

	s5p_mfc_try_run(dev);

	return 0;
}

static void s5p_mfc_stop_streaming(struct vb2_queue *q)
{
	unsigned long flags;
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	int index = 0;
	int aborted = 0;
	int ret = 0;

	mfc_info_ctx("enc stop_streaming is called, hw_lock : %d, type : %d\n",
				test_bit(ctx->num, &dev->hw_lock), q->type);
	MFC_TRACE_CTX("** ENC streamoff(type:%d)\n", q->type);

#ifdef NAL_Q_ENABLE
	nal_queue_handle *nal_q_handle = dev->nal_q_handle;
	if (nal_q_handle && nal_q_handle->nal_q_state == NAL_Q_STATE_STARTED) {
		mfc_debug(2, "NAL Q: stop NAL QUEUE\n");
		s5p_mfc_clean_dev_int_flags(dev);
		s5p_mfc_nal_q_stop(dev, nal_q_handle);
		if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_COMPLETE_QUEUE_RET)) {
			mfc_err("NAL Q: Failed to stop queue in stop streaming\n");
			s5p_mfc_clean_dev_int_flags(dev);
		}
		s5p_mfc_nal_q_cleanup_queue(dev);
		s5p_mfc_nal_q_init(dev, nal_q_handle);
		s5p_mfc_clock_off(dev);
	}
#endif

	spin_lock_irq(&dev->condlock);
	set_bit(ctx->num, &dev->ctx_stop_bits);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If a H/W operation is in progress, wait for it complete */
	 if (need_to_wait_nal_abort(ctx)) {
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_NAL_ABORT_RET))
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
		aborted = 1;
	} else if (test_bit(ctx->num, &dev->hw_lock)) {
		ret = wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));
		if (ret == 0)
			mfc_err_ctx("wait for event failed\n");
	}

	if (enc->in_slice || enc->buf_full) {
		s5p_mfc_change_state(ctx, MFCINST_ABORT_INST);
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_try_run(dev);
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_NAL_ABORT_RET))
			s5p_mfc_cleanup_timeout_and_try_run(ctx);

		enc->in_slice = 0;
		enc->buf_full = 0;
		aborted = 1;
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->is_drm && ctx->stream_protect_flag) {
			struct s5p_mfc_buf *dst_buf;
			int i;

			mfc_debug(2, "stream_protect_flag(%#lx) will be released\n",
					ctx->stream_protect_flag);
			list_for_each_entry(dst_buf, &ctx->dst_queue, list) {
				i = dst_buf->vb.v4l2_buf.index;
				if (test_bit(i, &ctx->stream_protect_flag)) {
					if (s5p_mfc_stream_buf_prot(ctx, dst_buf, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(i, &ctx->stream_protect_flag);
				}
				mfc_debug(2, "[%d] enc dst buf un-prot_flag: %#lx\n",
						i, ctx->stream_protect_flag);
			}
		}
		s5p_mfc_cleanup_queue(&ctx->dst_queue);
		INIT_LIST_HEAD(&ctx->dst_queue);
		ctx->dst_queue_cnt = 0;
		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->dst_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__enc_reset_buf_ctrls(&ctx->dst_ctrls[index]);
			index++;
		}
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		cleanup_ref_queue(ctx);
		if (ctx->is_drm && ctx->raw_protect_flag) {
			struct s5p_mfc_buf *src_buf;
			int i;

			mfc_debug(2, "raw_protect_flag(%#lx) will be released\n",
					ctx->raw_protect_flag);
			list_for_each_entry(src_buf, &ctx->src_queue, list) {
				i = src_buf->vb.v4l2_buf.index;
				if (test_bit(i, &ctx->raw_protect_flag)) {
					if (s5p_mfc_raw_buf_prot(ctx, src_buf, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(i, &ctx->raw_protect_flag);
				}
				mfc_debug(2, "[%d] enc src buf un-prot_flag: %#lx\n",
						i, ctx->raw_protect_flag);
			}
		}

		s5p_mfc_cleanup_queue(&ctx->src_queue);
		INIT_LIST_HEAD(&ctx->src_queue);
		ctx->src_queue_cnt = 0;

		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->src_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__enc_reset_buf_ctrls(&ctx->src_ctrls[index]);
			index++;
		}
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (aborted && ctx->state == MFCINST_FINISHING)
		s5p_mfc_change_state(ctx, MFCINST_RUNNING);

	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_stop_bits);
	spin_unlock_irq(&dev->condlock);

	mfc_debug(2, "buffer cleanup is done in stop_streaming, type : %d\n", q->type);

	if (s5p_mfc_enc_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}
	spin_lock_irq(&dev->condlock);
	if (dev->ctx_work_bits)
		queue_work(dev->sched_wq, &dev->sched_work);
	spin_unlock_irq(&dev->condlock);
}

static void s5p_mfc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	int i;

	mfc_debug_enter();

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = 0;
		mfc_debug(2, "dst queue: %p\n", &ctx->dst_queue);
		mfc_debug(2, "Adding to dst: %p (%08llx, %08llx)\n", vb,
				s5p_mfc_mem_plane_addr(ctx, vb, 0),
				buf->planes.stream);

		/* Mark destination as available for use by MFC */
		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &ctx->dst_queue);
		ctx->dst_queue_cnt++;
		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		buf->used = 0;
		mfc_debug(2, "src queue: %p\n", &ctx->src_queue);
		mfc_debug(2, "Adding to src: %p (0x%08llx)\n", vb,
				s5p_mfc_mem_plane_addr(ctx, vb, 0));
		for (i = 0; i < ctx->src_fmt->num_planes; i++)
			mfc_debug(2, "enc src plane[%d]: %08llx\n",
					i, buf->planes.raw[i]);

		spin_lock_irqsave(&dev->irqlock, flags);

		list_add_tail(&buf->list, &ctx->src_queue);
		ctx->src_queue_cnt++;

		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else {
		mfc_err_ctx("unsupported buffer type (%d)\n", vq->type);
	}

	if (s5p_mfc_enc_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}
	s5p_mfc_try_run(dev);

	mfc_debug_leave();
}

static struct vb2_ops s5p_mfc_enc_qops = {
	.queue_setup		= s5p_mfc_queue_setup,
	.wait_prepare		= s5p_mfc_unlock,
	.wait_finish		= s5p_mfc_lock,
	.buf_init		= s5p_mfc_buf_init,
	.buf_prepare		= s5p_mfc_buf_prepare,
	.buf_finish		= s5p_mfc_buf_finish,
	.buf_cleanup		= s5p_mfc_buf_cleanup,
	.start_streaming	= s5p_mfc_start_streaming,
	.stop_streaming		= s5p_mfc_stop_streaming,
	.buf_queue		= s5p_mfc_buf_queue,
};

const struct v4l2_ioctl_ops *get_enc_v4l2_ioctl_ops(void)
{
	return &s5p_mfc_enc_ioctl_ops;
}

int s5p_mfc_init_enc_ctx(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_enc *enc;
	int ret = 0;
	int i;

	enc = kzalloc(sizeof(struct s5p_mfc_enc), GFP_KERNEL);
	if (!enc) {
		mfc_err("failed to allocate encoder private data\n");
		return -ENOMEM;
	}
	ctx->enc_priv = enc;

	ctx->inst_no = MFC_NO_INSTANCE_SET;

	INIT_LIST_HEAD(&ctx->src_queue);
	INIT_LIST_HEAD(&ctx->dst_queue);
	INIT_LIST_HEAD(&ctx->src_queue_nal_q);
	INIT_LIST_HEAD(&ctx->dst_queue_nal_q);
	ctx->src_queue_cnt = 0;
	ctx->dst_queue_cnt = 0;
	ctx->src_queue_cnt_nal_q = 0;
	ctx->dst_queue_cnt_nal_q = 0;

	ctx->framerate = ENC_DEFAULT_FPS;
	ctx->qos_ratio = 100;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&ctx->qos_list);
#endif

	for (i = 0; i < MFC_MAX_BUFFERS; i++) {
		INIT_LIST_HEAD(&ctx->src_ctrls[i]);
		INIT_LIST_HEAD(&ctx->dst_ctrls[i]);
	}
	ctx->src_ctrls_avail = 0;
	ctx->dst_ctrls_avail = 0;

	ctx->type = MFCINST_ENCODER;
	ctx->c_ops = &encoder_codec_ops;
	ctx->src_fmt = &formats[DEF_SRC_FMT];
	ctx->dst_fmt = &formats[DEF_DST_FMT];

	INIT_LIST_HEAD(&enc->ref_queue);
	enc->ref_queue_cnt = 0;
	enc->sh_handle_svc.fd = -1;
	enc->sh_handle_roi.fd = -1;

	/* Init videobuf2 queue for OUTPUT */
	ctx->vq_src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ctx->vq_src.drv_priv = ctx;
	ctx->vq_src.buf_struct_size = sizeof(struct s5p_mfc_buf);
	ctx->vq_src.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_src.ops = &s5p_mfc_enc_qops;
	ctx->vq_src.mem_ops = s5p_mfc_mem_ops();
	ctx->vq_src.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_src);
	if (ret) {
		mfc_err("Failed to initialize videobuf2 queue(output)\n");
		return ret;
	}

	/* Init videobuf2 queue for CAPTURE */
	ctx->vq_dst.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	ctx->vq_dst.drv_priv = ctx;
	ctx->vq_dst.buf_struct_size = sizeof(struct s5p_mfc_buf);
	ctx->vq_dst.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_dst.ops = &s5p_mfc_enc_qops;
	ctx->vq_dst.mem_ops = s5p_mfc_mem_ops();
	ctx->vq_dst.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_dst);
	if (ret) {
		mfc_err("Failed to initialize videobuf2 queue(capture)\n");
		return ret;
	}

	return 0;
}
