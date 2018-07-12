/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_dec.c
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

#include "s5p_mfc_dec.h"
#include "s5p_mfc_dec_internal.h"

#include "s5p_mfc_ctrl.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_qos.h"
#include "s5p_mfc_opr_v10.h"
#include "s5p_mfc_buf.h"
#include "s5p_mfc_utils.h"

#define DEF_SRC_FMT	2
#define DEF_DST_FMT	0

#define MAX_FRAME_SIZE		(2*1024*1024)

/* Find selected format description */
static struct s5p_mfc_fmt *find_format(struct v4l2_format *f, unsigned int t)
{
	unsigned int i;

	for (i = 0; i < NUM_FORMATS; i++) {
		if (formats[i].fourcc == f->fmt.pix_mp.pixelformat &&
		    formats[i].type == t)
			return (struct s5p_mfc_fmt *)&formats[i];
	}

	return NULL;
}

static struct v4l2_queryctrl *get_ctrl(int id)
{
	int i;

	for (i = 0; i < NUM_CTRLS; ++i)
		if (id == controls[i].id)
			return &controls[i];
	return NULL;
}

/* Check whether a ctrl value if correct */
static int check_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct v4l2_queryctrl *c;

	c = get_ctrl(ctrl->id);
	if (!c)
		return -EINVAL;

	if (ctrl->value < c->minimum || ctrl->value > c->maximum
		|| (c->step != 0 && ctrl->value % c->step != 0)) {
		v4l2_err(&dev->v4l2_dev, "invalid control value\n");
		return -ERANGE;
	}

	return 0;
}

/* Check whether a context should be run on hardware */
int s5p_mfc_dec_ctx_ready(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	mfc_debug(2, "src=%d, dst=%d, ref=%d, state=%d capstat=%d\n",
		  ctx->src_queue_cnt, ctx->dst_queue_cnt, dec->ref_queue_cnt,
		  ctx->state, ctx->capture_state);
	mfc_debug(2, "wait_state = %d\n", ctx->wait_state);

	/* Skip ready check temporally */
	spin_lock_irq(&dev->condlock);
	if (test_bit(ctx->num, &dev->ctx_stop_bits)) {
		spin_unlock_irq(&dev->condlock);
		return 0;
	}
	spin_unlock_irq(&dev->condlock);

	/* Context is to parse header */
	if (ctx->src_queue_cnt >= 1 && ctx->state == MFCINST_GOT_INST)
		return 1;
	/* Context is to decode a frame */
	if (ctx->src_queue_cnt >= 1 &&
		ctx->state == MFCINST_RUNNING &&
		ctx->wait_state == WAIT_NONE &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		 (dec->is_dynamic_dpb && is_h264(ctx) && dec->ref_queue_cnt == (ctx->dpb_count + 5)) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	/* Context is to return last frame */
	if (ctx->state == MFCINST_FINISHING &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		 (dec->is_dynamic_dpb && is_h264(ctx) && dec->ref_queue_cnt == (ctx->dpb_count + 5)) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	/* Context is to set buffers */
	if (ctx->state == MFCINST_HEAD_PARSED &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1 &&
		  ctx->wait_state == WAIT_NONE) ||
		(!dec->is_dynamic_dpb &&
				ctx->capture_state == QUEUE_BUFS_MMAPED)))
		return 1;
	/* Resolution change */
	if ((ctx->state == MFCINST_RES_CHANGE_INIT ||
		ctx->state == MFCINST_RES_CHANGE_FLUSH) &&
		((dec->is_dynamic_dpb && ctx->dst_queue_cnt >= 1) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt >= ctx->dpb_count)))
		return 1;
	if (ctx->state == MFCINST_RES_CHANGE_END &&
		ctx->src_queue_cnt >= 1)
		return 1;

	mfc_debug(2, "s5p_mfc_dec_ctx_ready: ctx is not ready.\n");

	return 0;
}

static int dec_cleanup_ctx_ctrls(struct s5p_mfc_ctx *ctx)
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

static int dec_init_ctx_ctrls(struct s5p_mfc_ctx *ctx)
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

			dec_cleanup_ctx_ctrls(ctx);

			return -ENOMEM;
		}

		ctx_ctrl->type = mfc_ctrl_list[i].type;
		ctx_ctrl->id = mfc_ctrl_list[i].id;
		ctx_ctrl->addr = mfc_ctrl_list[i].addr;
		ctx_ctrl->has_new = 0;
		ctx_ctrl->val = 0;

		list_add_tail(&ctx_ctrl->list, &ctx->ctrls);

		mfc_debug(7, "Add context control id: 0x%08x, type : %d\n",
				ctx_ctrl->id, ctx_ctrl->type);
	}

	return 0;
}

static void __dec_reset_buf_ctrls(struct list_head *head)
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

static void __dec_cleanup_buf_ctrls(struct list_head *head)
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

static int dec_init_buf_ctrls(struct s5p_mfc_ctx *ctx,
	enum s5p_mfc_ctrl_type type, unsigned int index)
{
	unsigned long i;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
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

			__dec_reset_buf_ctrls(&ctx->src_ctrls[index]);

			return 0;
		}

		head = &ctx->src_ctrls[index];
	} else if (type & MFC_CTRL_TYPE_DST) {
		if (test_bit(index, &ctx->dst_ctrls_avail)) {
			mfc_debug(7, "Dest. per-buffer control is already "\
					"initialized [%d]\n", index);

			__dec_reset_buf_ctrls(&ctx->dst_ctrls[index]);

			return 0;
		}

		head = &ctx->dst_ctrls[index];
	} else {
		mfc_err("Control type mismatch. type : %d\n", type);
		return -EINVAL;
	}

	INIT_LIST_HEAD(head);

	list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
		if (!(type & ctx_ctrl->type))
			continue;

		/* find matched control configuration index */
		for (i = 0; i < NUM_CTRL_CFGS; i++) {
			if (ctx_ctrl->id == mfc_ctrl_list[i].id)
				break;
		}

		if (i == NUM_CTRL_CFGS) {
			mfc_err("Failed to find buffer control "\
					"id: 0x%08x, type: %d\n",
					ctx_ctrl->id, ctx_ctrl->type);
			continue;
		}

		buf_ctrl = kzalloc(sizeof(struct s5p_mfc_buf_ctrl), GFP_KERNEL);
		if (buf_ctrl == NULL) {
			mfc_err("Failed to allocate buffer control "\
					"id: 0x%08x, type: %d\n",
					mfc_ctrl_list[i].id,
					mfc_ctrl_list[i].type);

			__dec_cleanup_buf_ctrls(head);

			return -ENOMEM;
		}

		buf_ctrl->id = ctx_ctrl->id;
		buf_ctrl->type = ctx_ctrl->type;
		buf_ctrl->addr = ctx_ctrl->addr;

		buf_ctrl->is_volatile = mfc_ctrl_list[i].is_volatile;
		buf_ctrl->mode = mfc_ctrl_list[i].mode;
		buf_ctrl->mask = mfc_ctrl_list[i].mask;
		buf_ctrl->shft = mfc_ctrl_list[i].shft;
		buf_ctrl->flag_mode = mfc_ctrl_list[i].flag_mode;
		buf_ctrl->flag_addr = mfc_ctrl_list[i].flag_addr;
		buf_ctrl->flag_shft = mfc_ctrl_list[i].flag_shft;

		list_add_tail(&buf_ctrl->list, head);

		mfc_debug(7, "Add buffer control id: 0x%08x, type : %d\n",\
				buf_ctrl->id, buf_ctrl->type);
	}

	__dec_reset_buf_ctrls(head);

	if (type & MFC_CTRL_TYPE_SRC)
		set_bit(index, &ctx->src_ctrls_avail);
	else
		set_bit(index, &ctx->dst_ctrls_avail);

	return 0;
}

static int dec_cleanup_buf_ctrls(struct s5p_mfc_ctx *ctx,
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

	__dec_cleanup_buf_ctrls(head);

	return 0;
}

static int dec_to_buf_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	struct s5p_mfc_buf_ctrl *buf_ctrl;

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

static int dec_to_ctx_ctrls(struct s5p_mfc_ctx *ctx, struct list_head *head)
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

static int dec_set_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET) || !buf_ctrl->has_new)
			continue;

		/* read old vlaue */
		value = MFC_READL(buf_ctrl->addr);

		/* save old vlaue for recovery */
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
			dec->stored_tag = buf_ctrl->val;

		mfc_debug(8, "Set buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int dec_get_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_GET))
			continue;

		value = MFC_READL(buf_ctrl->addr);
		value = (value >> buf_ctrl->shft) & buf_ctrl->mask;

		buf_ctrl->val = value;
		buf_ctrl->has_new = 1;

		mfc_debug(8, "Get buffer control "\
				"id: 0x%08x val: %d\n",
				buf_ctrl->id, buf_ctrl->val);
	}

	return 0;
}

static int dec_recover_buf_ctrls_val(struct s5p_mfc_ctx *ctx, struct list_head *head)
{
	struct s5p_mfc_buf_ctrl *buf_ctrl;
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned int value = 0;

	list_for_each_entry(buf_ctrl, head, list) {
		if (!(buf_ctrl->type & MFC_CTRL_TYPE_SET)
			|| !buf_ctrl->is_volatile
			|| !buf_ctrl->updated)
			continue;

		value = MFC_READL(buf_ctrl->addr);
		value &= ~(buf_ctrl->mask << buf_ctrl->shft);
		value |= ((buf_ctrl->old_val & buf_ctrl->mask) << buf_ctrl->shft);
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
	}

	return 0;
}

static int dec_get_buf_update_val(struct s5p_mfc_ctx *ctx,
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

static struct s5p_mfc_codec_ops decoder_codec_ops = {
	.pre_seq_start		= NULL,
	.post_seq_start		= NULL,
	.pre_frame_start	= NULL,
	.post_frame_start	= NULL,
	.init_ctx_ctrls		= dec_init_ctx_ctrls,
	.cleanup_ctx_ctrls	= dec_cleanup_ctx_ctrls,
	.init_buf_ctrls		= dec_init_buf_ctrls,
	.cleanup_buf_ctrls	= dec_cleanup_buf_ctrls,
	.to_buf_ctrls		= dec_to_buf_ctrls,
	.to_ctx_ctrls		= dec_to_ctx_ctrls,
	.set_buf_ctrls_val	= dec_set_buf_ctrls_val,
	.get_buf_ctrls_val	= dec_get_buf_ctrls_val,
	.recover_buf_ctrls_val	= dec_recover_buf_ctrls_val,
	.get_buf_update_val	= dec_get_buf_update_val,
};

/* Query capabilities of the device */
static int vidioc_querycap(struct file *file, void *priv,
			   struct v4l2_capability *cap)
{
	strncpy(cap->driver, "MFC", sizeof(cap->driver) - 1);
	strncpy(cap->card, "decoder", sizeof(cap->card) - 1);
	cap->bus_info[0] = 0;
	cap->version = KERNEL_VERSION(1, 0, 0);
	cap->capabilities = V4L2_CAP_VIDEO_CAPTURE
			| V4L2_CAP_VIDEO_OUTPUT
			| V4L2_CAP_VIDEO_CAPTURE_MPLANE
			| V4L2_CAP_VIDEO_OUTPUT_MPLANE
			| V4L2_CAP_STREAMING;

	return 0;
}

/* Enumerate format */
static int vidioc_enum_fmt(struct v4l2_fmtdesc *f, bool mplane, bool out)
{
	struct s5p_mfc_fmt *fmt;
	int i, j = 0;

	for (i = 0; i < ARRAY_SIZE(formats); ++i) {
		if (mplane && formats[i].mem_planes == 1)
			continue;
		else if (!mplane && formats[i].mem_planes > 1)
			continue;
		if (out && formats[i].type != MFC_FMT_DEC)
			continue;
		else if (!out && formats[i].type != MFC_FMT_RAW)
			continue;

		if (j == f->index)
			break;
		++j;
	}
	if (i == ARRAY_SIZE(formats))
		return -EINVAL;
	fmt = &formats[i];
	strlcpy(f->description, fmt->name, sizeof(f->description));
	f->pixelformat = fmt->fourcc;
	return 0;
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

/* Get format */
static int vidioc_g_fmt_vid_cap_mplane(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	struct s5p_mfc_raw_info *raw;
	int i;

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}
	mfc_debug_enter();
	mfc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	if (ctx->state == MFCINST_VPS_PARSED_ONLY) {
		mfc_err("MFCINST_VPS_PARSED_ONLY !!!\n");
		return -EAGAIN;
	}
	if (ctx->state == MFCINST_GOT_INST ||
	    ctx->state == MFCINST_RES_CHANGE_FLUSH ||
	    ctx->state == MFCINST_RES_CHANGE_END) {
		/* If the MFC is parsing the header,
		 * so wait until it is finished */
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_SEQ_DONE_RET)) {
			if (ctx->state == MFCINST_VPS_PARSED_ONLY) {
				mfc_err("MFCINST_VPS_PARSED_ONLY !!!\n");
				return -EAGAIN;
			} else {
				s5p_mfc_cleanup_timeout_and_try_run(ctx);
				return -EIO;
			}
		}
	}

	if (ctx->state >= MFCINST_HEAD_PARSED &&
	    ctx->state < MFCINST_ABORT) {
		/* This is run on CAPTURE (deocde output) */

		/* only 2 plane is supported for HEVC 10bit */
		if (dec->is_10bit) {
			if (ctx->dst_fmt->mem_planes == 1) {
				ctx->dst_fmt = (struct s5p_mfc_fmt *)&formats[7];
			} else if (ctx->dst_fmt->mem_planes == 3) {
				ctx->dst_fmt = (struct s5p_mfc_fmt *)&formats[5];
				ctx->raw_buf.num_planes = 2;
			}
			mfc_info_ctx("HEVC 10bit: format is changed to %s\n",
							ctx->dst_fmt->name);
		}

		raw = &ctx->raw_buf;
		/* Width and height are set to the dimensions
		   of the movie, the buffer is bigger and
		   further processing stages should crop to this
		   rectangle. */
		s5p_mfc_dec_calc_dpb_size(ctx);

		pix_mp->width = ctx->img_width;
		pix_mp->height = ctx->img_height;
		pix_mp->num_planes = ctx->dst_fmt->mem_planes;

		if (dec->is_interlaced)
			pix_mp->field = V4L2_FIELD_INTERLACED;
		else
			pix_mp->field = V4L2_FIELD_NONE;

		/* Set pixelformat to the format in which MFC
		   outputs the decoded frame */
		pix_mp->pixelformat = ctx->dst_fmt->fourcc;
		for (i = 0; i < ctx->dst_fmt->mem_planes; i++) {
			pix_mp->plane_fmt[i].bytesperline = raw->stride[i];
			if (ctx->dst_fmt->mem_planes == 1) {
				pix_mp->plane_fmt[i].sizeimage = raw->total_plane_size;
			} else {
				if (dec->is_10bit)
					pix_mp->plane_fmt[i].sizeimage = raw->plane_size[i]
						+ raw->plane_size_2bits[i];
				else
					pix_mp->plane_fmt[i].sizeimage = raw->plane_size[i];
			}
		}
	}

	mfc_debug_leave();

	return 0;
}

static int vidioc_g_fmt_vid_out_mplane(struct file *file, void *priv,
						struct v4l2_format *f)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;

	mfc_debug_enter();

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}
	mfc_debug(2, "f->type = %d ctx->state = %d\n", f->type, ctx->state);

	/* This is run on OUTPUT
	   The buffer contains compressed image
	   so width and height have no meaning */
	pix_mp->width = 0;
	pix_mp->height = 0;
	pix_mp->field = V4L2_FIELD_NONE;
	pix_mp->plane_fmt[0].bytesperline = dec->src_buf_size;
	pix_mp->plane_fmt[0].sizeimage = (unsigned int)(dec->src_buf_size);
	pix_mp->pixelformat = ctx->src_fmt->fourcc;
	pix_mp->num_planes = ctx->src_fmt->mem_planes;

	mfc_debug_leave();

	return 0;
}

/* Try format */
static int vidioc_try_fmt(struct file *file, void *priv, struct v4l2_format *f)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_fmt *fmt;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	mfc_debug(2, "Type is %d\n", f->type);
	if (f->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		fmt = find_format(f, MFC_FMT_DEC);
		if (!fmt) {
			mfc_err_dev("Unsupported format for source.\n");
			return -EINVAL;
		}
		if (!IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_VP8) {
				mfc_err_dev("Not supported format.\n");
				return -EINVAL;
			}
		}
	} else if (f->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		fmt = find_format(f, MFC_FMT_RAW);
		if (!fmt) {
			mfc_err_dev("Unsupported format for destination.\n");
			return -EINVAL;
		}
		if (IS_MFCV6(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT) {
				mfc_err_dev("Not supported format.\n");
				return -EINVAL;
			}
		} else {
			if (fmt->fourcc != V4L2_PIX_FMT_NV12MT) {
				mfc_err_dev("Not supported format.\n");
				return -EINVAL;
			}
		}
		if (IS_MFCV8(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) {
				mfc_err_dev("Not supported format.\n");
				return -EINVAL;
			}
		}
		if (!IS_MFCv10X(dev)) {
			if (fmt->fourcc == V4L2_PIX_FMT_NV12N ||
				fmt->fourcc == V4L2_PIX_FMT_YUV420N ||
				fmt->fourcc == V4L2_PIX_FMT_NV12N_10B) {
				mfc_err_dev("Not supported single plane format.\n");
				return -EINVAL;
			}
		}
	}

	return 0;
}

/* Set format */
static int vidioc_s_fmt_vid_cap_mplane(struct file *file, void *priv,
							struct v4l2_format *f)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret = 0;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (ctx->vq_dst.streaming) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	ctx->dst_fmt = find_format(f, MFC_FMT_RAW);
	if (!ctx->dst_fmt) {
		mfc_err_ctx("Unsupported format for destination.\n");
		return -EINVAL;
	}
	ctx->raw_buf.num_planes = ctx->dst_fmt->num_planes;
	mfc_info_ctx("Dec output pixelformat : %s\n", ctx->dst_fmt->name);

	mfc_debug_leave();

	return 0;
}

static int vidioc_s_fmt_vid_out_mplane(struct file *file, void *priv,
							struct v4l2_format *f)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec;
	int ret = 0;
	struct v4l2_pix_format_mplane *pix_mp = &f->fmt.pix_mp;
	int i;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	if (ctx->vq_src.streaming) {
		v4l2_err(&dev->v4l2_dev, "%s queue busy\n", __func__);
		return -EBUSY;
	}

	ret = vidioc_try_fmt(file, priv, f);
	if (ret)
		return ret;

	ctx->src_fmt = find_format(f, MFC_FMT_DEC);
	ctx->codec_mode = ctx->src_fmt->codec_mode;
	mfc_info_ctx("Dec input codec(%d): %s\n",
			ctx->codec_mode, ctx->src_fmt->name);
	ctx->pix_format = pix_mp->pixelformat;
	if ((pix_mp->width > 0) && (pix_mp->height > 0)) {
		ctx->img_height = pix_mp->height;
		ctx->img_width = pix_mp->width;
	}
	/* As this buffer will contain compressed data, the size is set
	 * to the maximum size. */
	if (pix_mp->plane_fmt[0].sizeimage)
		dec->src_buf_size = pix_mp->plane_fmt[0].sizeimage;
	else
		dec->src_buf_size = MAX_FRAME_SIZE;
	mfc_debug(2, "sizeimage: %d\n", pix_mp->plane_fmt[0].sizeimage);
	pix_mp->plane_fmt[0].bytesperline = 0;

	/* In case of calling s_fmt twice or more */
	if (ctx->inst_no != MFC_NO_INSTANCE_SET) {
		/* Wait for hw_lock == 0 for this context */
		ret = wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));
		if (ret == 0) {
			mfc_err_ctx("Waiting for hardware to finish timed out\n");
			return -EBUSY;
		}

		s5p_mfc_change_state(ctx, MFCINST_RETURN_INST);
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_clean_ctx_int_flags(ctx);
		s5p_mfc_try_run(dev);
		/* Wait until instance is returned or timeout occured */
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET)) {
			mfc_err_ctx("Waiting for CLOSE_INSTANCE timed out\n");
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
			return -EIO;
		}
		/* Free resources */
		s5p_mfc_release_instance_buffer(ctx);
		s5p_mfc_change_state(ctx, MFCINST_INIT);
	}

	if (dec->crc_enable && (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
				ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC)) {
		/* CRC related control types should be changed by the codec mode. */
		mfc_debug(5, "ctx_ctrl is changed for H.264\n");
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			switch (ctx_ctrl->id) {
			case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_LUMA:
				ctx_ctrl->type = MFC_CTRL_TYPE_GET_DST;
				ctx_ctrl->addr = S5P_FIMV_D_DISPLAY_FIRST_PLANE_CRC;
				break;
			case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA:
				ctx_ctrl->type = MFC_CTRL_TYPE_GET_DST;
				ctx_ctrl->addr = S5P_FIMV_D_DISPLAY_SECOND_PLANE_CRC;
				break;
			case V4L2_CID_MPEG_MFC51_VIDEO_CRC_GENERATED:
				ctx_ctrl->type = MFC_CTRL_TYPE_GET_DST;
				ctx_ctrl->addr = S5P_FIMV_D_DISPLAY_STATUS;
				break;
			case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_LUMA_BOT:
			case V4L2_CID_MPEG_MFC51_VIDEO_CRC_DATA_CHROMA_BOT:
				ctx_ctrl->type = MFC_CTRL_TYPE_GET_DST;
				break;
			default:
				break;
			}
		}

		/* Reinitialize controls for source buffers */
		for (i = 0; i < MFC_MAX_BUFFERS; i++) {
			if (test_bit(i, &ctx->src_ctrls_avail)) {
				if (call_cop(ctx, cleanup_buf_ctrls, ctx,
						MFC_CTRL_TYPE_SRC, i) < 0)
					mfc_err_ctx("failed in cleanup_buf_ctrls\n");
				if (call_cop(ctx, init_buf_ctrls, ctx,
						MFC_CTRL_TYPE_SRC, i) < 0)
					mfc_err_ctx("failed in init_buf_ctrls\n");
			}
		}
	}

	ret = s5p_mfc_alloc_instance_buffer(ctx);
	if (ret) {
		mfc_err_ctx("Failed to allocate instance[%d] buffers.\n",
				ctx->num);
		return -ENOMEM;
	}

	spin_lock_irq(&dev->condlock);
	set_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
	s5p_mfc_try_run(dev);
	if (s5p_mfc_wait_for_done_ctx(ctx,
			S5P_FIMV_R2H_CMD_OPEN_INSTANCE_RET)) {
		s5p_mfc_cleanup_timeout_and_try_run(ctx);
		s5p_mfc_release_instance_buffer(ctx);
		return -EIO;
	}
	mfc_debug(2, "Got instance number: %d\n", ctx->inst_no);

	mfc_debug_leave();

	return 0;
}

/* Reqeust buffers */
static int vidioc_reqbufs(struct file *file, void *priv,
		struct v4l2_requestbuffers *reqbufs)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec;
	int ret = 0;
	void *alloc_ctx;

	mfc_debug_enter();
	mfc_debug(2, "Memory type: %d\n", reqbufs->memory);

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (ctx->is_drm)
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		else
			alloc_ctx = ctx->dev->alloc_ctx;

		/* Can only request buffers after
		   an instance has been opened.*/
		if ((ctx->state == MFCINST_GOT_INST) ||
			(ctx->state == MFCINST_VPS_PARSED_ONLY)) {
			if (reqbufs->count == 0) {
				mfc_debug(2, "Freeing buffers.\n");
				ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
				ctx->output_state = QUEUE_FREE;
				return ret;
			}

			/* Decoding */
			if (ctx->output_state != QUEUE_FREE) {
				mfc_err_ctx("Bufs have already been requested.\n");
				return -EINVAL;
			}

			if (ctx->cacheable & MFCMASK_SRC_CACHE)
				s5p_mfc_mem_set_cacheable(alloc_ctx, true);

			ret = vb2_reqbufs(&ctx->vq_src, reqbufs);
			if (ret) {
				mfc_err_ctx("vb2_reqbufs on output failed.\n");
				s5p_mfc_mem_set_cacheable(alloc_ctx, false);
				return ret;
			}

			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			ctx->output_state = QUEUE_BUFS_REQUESTED;
		}
	} else if (reqbufs->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (reqbufs->count == 0) {
			mfc_debug(2, "Freeing buffers.\n");
			ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
			s5p_mfc_release_codec_buffers(ctx);
			dec->dpb_queue_cnt = 0;
			ctx->capture_state = QUEUE_FREE;
			return ret;
		}

		dec->dst_memtype = reqbufs->memory;

		if (ctx->is_drm) {
			alloc_ctx = ctx->dev->alloc_ctx_drm;
		} else {
			alloc_ctx = ctx->dev->alloc_ctx;
		}

		if (ctx->capture_state != QUEUE_FREE) {
			mfc_err_ctx("Bufs have already been requested.\n");
			return -EINVAL;
		}

		if (ctx->cacheable & MFCMASK_DST_CACHE)
			s5p_mfc_mem_set_cacheable(alloc_ctx, true);

		ret = vb2_reqbufs(&ctx->vq_dst, reqbufs);
		if (ret) {
			mfc_err_ctx("vb2_reqbufs on capture failed.\n");
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return ret;
		}

		if (reqbufs->count < ctx->dpb_count) {
			mfc_err_ctx("Not enough buffers allocated.\n");
			reqbufs->count = 0;
			vb2_reqbufs(&ctx->vq_dst, reqbufs);
			s5p_mfc_mem_set_cacheable(alloc_ctx, false);
			return -ENOMEM;
		}

		s5p_mfc_mem_set_cacheable(alloc_ctx, false);
		ctx->capture_state = QUEUE_BUFS_REQUESTED;

		dec->total_dpb_count = reqbufs->count;

		ret = s5p_mfc_alloc_codec_buffers(ctx);
		if (ret) {
			mfc_err_ctx("Failed to allocate decoding buffers.\n");
			reqbufs->count = 0;
			vb2_reqbufs(&ctx->vq_dst, reqbufs);
			return -ENOMEM;
		}

		if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
			if (dec->dpb_queue_cnt == dec->total_dpb_count) {
				ctx->capture_state = QUEUE_BUFS_MMAPED;
			} else {
				mfc_err_ctx("Not all buffers passed to buf_init.\n");
				reqbufs->count = 0;
				vb2_reqbufs(&ctx->vq_dst, reqbufs);
				s5p_mfc_release_codec_buffers(ctx);
				return -ENOMEM;
			}
		}

		if (s5p_mfc_dec_ctx_ready(ctx)) {
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}

		s5p_mfc_try_run(dev);

		if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
			if (s5p_mfc_wait_for_done_ctx(ctx,
					S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET)) {
				s5p_mfc_cleanup_timeout_and_try_run(ctx);
				return -EIO;
			}
		}
	}

	mfc_debug_leave();

	return ret;
}

/* Query buffer */
static int vidioc_querybuf(struct file *file, void *priv,
						   struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	int ret;
	int i;

	mfc_debug_enter();

	if (buf->memory != V4L2_MEMORY_MMAP) {
		mfc_err("Only mmaped buffers can be used.\n");
		return -EINVAL;
	}

	mfc_debug(2, "State: %d, buf->type: %d\n", ctx->state, buf->type);
	if (ctx->state == MFCINST_GOT_INST &&
			buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_src, buf);
	} else if (ctx->state == MFCINST_RUNNING &&
			buf->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = vb2_querybuf(&ctx->vq_dst, buf);
		for (i = 0; i < buf->length; i++)
			buf->m.planes[i].m.mem_offset += DST_QUEUE_OFF_BASE;
	} else {
		mfc_err("vidioc_querybuf called in an inappropriate state.\n");
		ret = -EINVAL;
	}
	mfc_debug_leave();
	return ret;
}

extern int no_order;
/* Queue a buffer */
static int vidioc_qbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int ret = -EINVAL;

	mfc_debug_enter();

	mfc_debug(2, "Enqueued buf: %d, (type = %d)\n", buf->index, buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on QBUF after unrecoverable error.\n");
		return -EIO;
	}

	if (V4L2_TYPE_IS_MULTIPLANAR(buf->type) && !buf->length) {
		mfc_err_ctx("multiplanar but length is zero\n");
		return -EIO;
	}

	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		if (buf->m.planes[0].bytesused > ctx->vq_src.plane_sizes[0]) {
			mfc_err_ctx("data size (%d) must be less than "
					"plane size(%d)\n",
					buf->m.planes[0].bytesused,
					ctx->vq_src.plane_sizes[0]);
			return -EIO;
		}

		if (no_order && dec->is_dts_mode) {
			mfc_debug(7, "timestamp: %ld %ld\n",
					buf->timestamp.tv_sec,
					buf->timestamp.tv_usec);
			mfc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
			ctx->last_framerate = (ctx->qos_ratio *
						get_framerate(&buf->timestamp,
						&ctx->last_timestamp)) / 100;

			memcpy(&ctx->last_timestamp, &buf->timestamp,
				sizeof(struct timeval));
		}
		if (ctx->last_framerate != 0 &&
				ctx->last_framerate != ctx->framerate) {
			mfc_debug(2, "fps changed: %d -> %d\n",
					ctx->framerate, ctx->last_framerate);
			ctx->framerate = ctx->last_framerate;
			s5p_mfc_qos_on(ctx);
		}
		mfc_debug(2, "Src input size = %d\n", buf->m.planes[0].bytesused);
		ret = vb2_qbuf(&ctx->vq_src, buf);
	} else {
		ret = vb2_qbuf(&ctx->vq_dst, buf);
		mfc_debug(2, "End of enqueue(%d) : %d\n", buf->index, ret);
	}

	mfc_debug_leave();
	return ret;
}

/* Dequeue a buffer */
static int vidioc_dqbuf(struct file *file, void *priv, struct v4l2_buffer *buf)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct dec_dpb_ref_info *dstBuf, *srcBuf;
	int ret;
	int ncount = 0;

	mfc_debug_enter();
	mfc_debug(2, "Addr: %p %p %p Type: %d\n", &ctx->vq_src, buf, buf->m.planes,
								buf->type);
	if (ctx->state == MFCINST_ERROR) {
		mfc_err_ctx("Call on DQBUF after unrecoverable error.\n");
		return -EIO;
	}
	if (buf->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = vb2_dqbuf(&ctx->vq_src, buf, file->f_flags & O_NONBLOCK);
	} else {
		ret = vb2_dqbuf(&ctx->vq_dst, buf, file->f_flags & O_NONBLOCK);

		/* Memcpy from dec->ref_info to shared memory */
		if (buf->index >= MFC_MAX_DPBS) {
			mfc_err_ctx("buffer index[%d] range over\n", buf->index);
			return -EINVAL;
		}

		srcBuf = &dec->ref_info[buf->index];
		for (ncount = 0; ncount < MFC_MAX_DPBS; ncount++) {
			if (srcBuf->dpb[ncount].fd[0] == MFC_INFO_INIT_FD)
				break;
			mfc_debug(2, "DQ index[%d] Released FD = %d\n",
					buf->index, srcBuf->dpb[ncount].fd[0]);
		}

		if ((dec->is_dynamic_dpb) && (dec->sh_handle.virt != NULL)) {
			dstBuf = (struct dec_dpb_ref_info *)
					dec->sh_handle.virt + buf->index;
			memcpy(dstBuf, srcBuf, sizeof(struct dec_dpb_ref_info));
			dstBuf->index = buf->index;
		}
	}
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
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = vb2_streamon(&ctx->vq_dst, type);

		if (!ret)
			s5p_mfc_qos_on(ctx);
	} else {
		mfc_err_ctx("unknown v4l2 buffer type\n");
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
	int ret = -EINVAL;

	mfc_debug_enter();

	if (type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ctx->last_framerate = 0;
		memset(&ctx->last_timestamp, 0, sizeof(struct timeval));
		ret = vb2_streamoff(&ctx->vq_src, type);
	} else if (type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		ret = vb2_streamoff(&ctx->vq_dst, type);
		if (!ret)
			s5p_mfc_qos_off(ctx);
	} else {
		mfc_err_ctx("unknown v4l2 buffer type\n");
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

static int dec_ext_info(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int val = 0;

	if (IS_MFCv7X(dev) && !IS_MFCv78(dev))
		val |= DEC_SET_DUAL_DPB;
	if (FW_HAS_DYNAMIC_DPB(dev))
		val |= DEC_SET_DYNAMIC_DPB;
	if (FW_HAS_LAST_DISP_INFO(dev))
		val |= DEC_SET_LAST_FRAME_INFO;
	if (FW_SUPPORT_SKYPE(dev))
		val |= DEC_SET_SKYPE_FLAG;

	return val;
}

/* Get ctrl */
static int get_ctrl_val(struct s5p_mfc_ctx *ctx, struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int found = 0;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		ctrl->value = dec->loop_filter_mpeg4;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY:
		ctrl->value = dec->display_delay;
		break;
	case V4L2_CID_CACHEABLE:
		ctrl->value = ctx->cacheable;
		break;
	case V4L2_CID_MIN_BUFFERS_FOR_CAPTURE:
		if (ctx->state >= MFCINST_HEAD_PARSED &&
		    ctx->state < MFCINST_ABORT) {
			ctrl->value = ctx->dpb_count;
			break;
		} else if (ctx->state != MFCINST_INIT) {
			v4l2_err(&dev->v4l2_dev, "Decoding not initialised.\n");
			return -EINVAL;
		}

		/* Should wait for the header to be parsed */
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_SEQ_DONE_RET)) {
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
			return -EIO;
		}

		if (ctx->state >= MFCINST_HEAD_PARSED &&
		    ctx->state < MFCINST_ABORT) {
			ctrl->value = ctx->dpb_count;
		} else {
			v4l2_err(&dev->v4l2_dev,
					 "Decoding not initialised.\n");
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:
		ctrl->value = dec->slice_enable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PACKED_PB:
		ctrl->value = dec->is_packedpb;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CRC_ENABLE:
		ctrl->value = dec->crc_enable;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CHECK_STATE:
		if (ctx->is_dpb_realloc && ctx->state == MFCINST_HEAD_PARSED)
			ctrl->value = MFCSTATE_DEC_S3D_REALLOC;
		else if (ctx->state == MFCINST_RES_CHANGE_FLUSH
				|| ctx->state == MFCINST_RES_CHANGE_END
				|| ctx->state == MFCINST_HEAD_PARSED)
			ctrl->value = MFCSTATE_DEC_RES_DETECT;
		else if (ctx->state == MFCINST_FINISHING)
			ctrl->value = MFCSTATE_DEC_TERMINATING;
		else
			ctrl->value = MFCSTATE_PROCESSING;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		ctrl->value = dec->sei_parse;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_I_FRAME_DECODING:
		ctrl->value = dec->idr_decoding;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE:
		ctrl->value = ctx->framerate;
		break;
	case V4L2_CID_MPEG_MFC_GET_VERSION_INFO:
#if defined(CONFIG_SOC_EXYNOS7580)
		ctrl->value = 0x78D;
#else
		ctrl->value = mfc_version(dev);
#endif
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		ctrl->value = ctx->qos_ratio;
		break;
	case V4L2_CID_MPEG_MFC_SET_DYNAMIC_DPB_MODE:
		ctrl->value = dec->is_dynamic_dpb;
		break;
	case V4L2_CID_MPEG_MFC_GET_EXT_INFO:
		ctrl->value = dec_ext_info(ctx);
		break;
	case V4L2_CID_MPEG_MFC_GET_10BIT_INFO:
		ctrl->value = dec->is_10bit;
		break;
	case V4L2_CID_MPEG_MFC_GET_DRIVER_INFO:
		ctrl->value = MFC_DRIVER_INFO;
		break;
	default:
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
			v4l2_err(&dev->v4l2_dev, "Invalid control 0x%08x\n",
					ctrl->id);
			return -EINVAL;
		}
		break;
	}

	mfc_debug_leave();

	return 0;
}

/* Get a ctrl */
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

static int process_user_shared_handle(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	int ret = 0;

	dec->sh_handle.ion_handle =
		ion_import_dma_buf(dev->mfc_ion_client, dec->sh_handle.fd);
	if (IS_ERR(dec->sh_handle.ion_handle)) {
		mfc_err_ctx("Failed to import fd\n");
		ret = PTR_ERR(dec->sh_handle.ion_handle);
		goto import_dma_fail;
	}

	dec->sh_handle.virt =
		ion_map_kernel(dev->mfc_ion_client, dec->sh_handle.ion_handle);
	if (dec->sh_handle.virt == NULL) {
		mfc_err_ctx("Failed to get kernel virtual address\n");
		ret = -EINVAL;
		goto map_kernel_fail;
	}

	mfc_debug(2, "User Handle: fd = %d, virt = 0x%p\n",
				dec->sh_handle.fd, dec->sh_handle.virt);

	return 0;

map_kernel_fail:
	ion_free(dev->mfc_ion_client, dec->sh_handle.ion_handle);

import_dma_fail:
	return ret;
}

int dec_cleanup_user_shared_handle(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;

	if (dec->sh_handle.fd == -1)
		return 0;

	if (dec->sh_handle.virt)
		ion_unmap_kernel(dev->mfc_ion_client,
					dec->sh_handle.ion_handle);

	ion_free(dev->mfc_ion_client, dec->sh_handle.ion_handle);

	return 0;
}

/* Set a ctrl */
static int vidioc_s_ctrl(struct file *file, void *priv,
			 struct v4l2_control *ctrl)
{
	struct s5p_mfc_dev *dev = video_drvdata(file);
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_ctx_ctrl *ctx_ctrl;
	int ret = 0;
	int found = 0;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	ret = check_ctrl_val(ctx, ctrl);
	if (ret)
		return ret;

	switch (ctrl->id) {
	case V4L2_CID_MPEG_VIDEO_DECODER_MPEG4_DEBLOCK_FILTER:
		dec->loop_filter_mpeg4 = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_DECODER_H264_DISPLAY_DELAY:
		dec->display_delay = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_SLICE_INTERFACE:
		dec->slice_enable = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_PACKED_PB:
		dec->is_packedpb = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_CRC_ENABLE:
		dec->crc_enable = ctrl->value;
		break;
	case V4L2_CID_CACHEABLE:
		if (ctrl->value)
			ctx->cacheable |= ctrl->value;
		else
			ctx->cacheable = 0;
		break;
	case V4L2_CID_MPEG_VIDEO_H264_SEI_FRAME_PACKING:
		dec->sei_parse = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_I_FRAME_DECODING:
		dec->idr_decoding = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC51_VIDEO_FRAME_RATE:
		if (ctx->framerate != ctrl->value) {
			ctx->framerate = ctrl->value;
			s5p_mfc_qos_on(ctx);
		}
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_IMMEDIATE_DISPLAY:
		dec->immediate_display = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_DECODING_TIMESTAMP_MODE:
		dec->is_dts_mode = ctrl->value;
		break;
	case V4L2_CID_MPEG_VIDEO_DECODER_WAIT_DECODING_START:
		ctx->wait_state = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_SET_DUAL_DPB_MODE:
		mfc_err("not supported CID: 0x%x\n",ctrl->id);
		break;
	case V4L2_CID_MPEG_VIDEO_QOS_RATIO:
		if (ctrl->value > 150)
			ctrl->value = 1000;
		mfc_info_ctx("set %d qos_ratio.\n", ctrl->value);
		ctx->qos_ratio = ctrl->value;
		break;
	case V4L2_CID_MPEG_MFC_SET_DYNAMIC_DPB_MODE:
		if (FW_HAS_DYNAMIC_DPB(dev))
			dec->is_dynamic_dpb = ctrl->value;
		else
			dec->is_dynamic_dpb = 0;
		break;
	case V4L2_CID_MPEG_MFC_SET_USER_SHARED_HANDLE:
		dec->sh_handle.fd = ctrl->value;
		if (process_user_shared_handle(ctx)) {
			dec->sh_handle.fd = -1;
			return -EINVAL;
		}
		break;
	case V4L2_CID_MPEG_MFC_SET_BUF_PROCESS_TYPE:
		ctx->buf_process_type = ctrl->value;
		break;
	default:
		list_for_each_entry(ctx_ctrl, &ctx->ctrls, list) {
			if (!(ctx_ctrl->type & MFC_CTRL_TYPE_SET))
				continue;

			if (ctx_ctrl->id == ctrl->id) {
				ctx_ctrl->has_new = 1;
				ctx_ctrl->val = ctrl->value;

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
	}

	mfc_debug_leave();

	return 0;
}

void s5p_mfc_dec_store_crop_info(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	u32 left, right, top, bottom;

	left = MFC_READL(S5P_FIMV_D_DISPLAY_CROP_INFO1);
	right = left >> S5P_FIMV_SHARED_CROP_RIGHT_SHIFT;
	left = left & S5P_FIMV_SHARED_CROP_LEFT_MASK;
	top = MFC_READL(S5P_FIMV_D_DISPLAY_CROP_INFO2);
	bottom = top >> S5P_FIMV_SHARED_CROP_BOTTOM_SHIFT;
	top = top & S5P_FIMV_SHARED_CROP_TOP_MASK;

	dec->cr_left = left;
	dec->cr_right = right;
	dec->cr_top = top;
	dec->cr_bot = bottom;
}

#define ready_to_get_crop(ctx)			\
	((ctx->state == MFCINST_HEAD_PARSED) ||	\
	 (ctx->state == MFCINST_RUNNING) ||	\
	 (ctx->state == MFCINST_FINISHING) ||	\
	 (ctx->state == MFCINST_FINISHED))
/* Get cropping information */
static int vidioc_g_crop(struct file *file, void *priv,
		struct v4l2_crop *cr)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dec *dec = ctx->dec_priv;

	mfc_debug_enter();

	if (!ready_to_get_crop(ctx)) {
		mfc_debug(2, "ready to get crop failed\n");
		return -EINVAL;
	}

	if (ctx->src_fmt->fourcc == V4L2_PIX_FMT_H264 ||
			ctx->src_fmt->fourcc == V4L2_PIX_FMT_HEVC) {
		cr->c.left = dec->cr_left;
		cr->c.top = dec->cr_top;
		cr->c.width = ctx->img_width - dec->cr_left - dec->cr_right;
		cr->c.height = ctx->img_height - dec->cr_top - dec->cr_bot;
		mfc_debug(2, "Cropping info [h264]: l=%d t=%d "	\
			"w=%d h=%d (r=%d b=%d fw=%d fh=%d)\n",
			dec->cr_left, dec->cr_top, cr->c.width, cr->c.height,
			dec->cr_right, dec->cr_bot,
			ctx->buf_width, ctx->buf_height);
	} else {
		cr->c.left = 0;
		cr->c.top = 0;
		cr->c.width = ctx->img_width;
		cr->c.height = ctx->img_height;
		mfc_debug(2, "Cropping info: w=%d h=%d fw=%d "
			"fh=%d\n", cr->c.width,	cr->c.height, ctx->buf_width,
							ctx->buf_height);
	}
	mfc_debug_leave();
	return 0;
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

/* v4l2_ioctl_ops */
static const struct v4l2_ioctl_ops s5p_mfc_dec_ioctl_ops = {
	.vidioc_querycap = vidioc_querycap,
	.vidioc_enum_fmt_vid_cap = vidioc_enum_fmt_vid_cap,
	.vidioc_enum_fmt_vid_cap_mplane = vidioc_enum_fmt_vid_cap_mplane,
	.vidioc_enum_fmt_vid_out = vidioc_enum_fmt_vid_out,
	.vidioc_enum_fmt_vid_out_mplane = vidioc_enum_fmt_vid_out_mplane,
	.vidioc_g_fmt_vid_cap_mplane = vidioc_g_fmt_vid_cap_mplane,
	.vidioc_g_fmt_vid_out_mplane = vidioc_g_fmt_vid_out_mplane,
	.vidioc_try_fmt_vid_cap_mplane = vidioc_try_fmt,
	.vidioc_try_fmt_vid_out_mplane = vidioc_try_fmt,
	.vidioc_s_fmt_vid_cap_mplane = vidioc_s_fmt_vid_cap_mplane,
	.vidioc_s_fmt_vid_out_mplane = vidioc_s_fmt_vid_out_mplane,
	.vidioc_reqbufs = vidioc_reqbufs,
	.vidioc_querybuf = vidioc_querybuf,
	.vidioc_qbuf = vidioc_qbuf,
	.vidioc_dqbuf = vidioc_dqbuf,
	.vidioc_streamon = vidioc_streamon,
	.vidioc_streamoff = vidioc_streamoff,
	.vidioc_queryctrl = vidioc_queryctrl,
	.vidioc_g_ctrl = vidioc_g_ctrl,
	.vidioc_s_ctrl = vidioc_s_ctrl,
	.vidioc_g_crop = vidioc_g_crop,
	.vidioc_g_ext_ctrls = vidioc_g_ext_ctrls,
};

static int s5p_mfc_queue_setup(struct vb2_queue *vq,
				const struct v4l2_format *fmt,
				unsigned int *buf_count, unsigned int *plane_count,
				unsigned int psize[], void *allocators[])
{
	struct s5p_mfc_ctx *ctx;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_raw_info *raw;
	void *alloc_ctx;
	void *capture_ctx;
	int i;

	mfc_debug_enter();

	if (!vq) {
		mfc_err("no vb2_queue info\n");
		return -EINVAL;
	}

	ctx = vq->drv_priv;
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	raw = &ctx->raw_buf;
	alloc_ctx = ctx->dev->alloc_ctx;

	/* Video output for decoding (source)
	 * this can be set after getting an instance */
	if (ctx->state == MFCINST_GOT_INST &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mfc_debug(2, "setting for VIDEO output\n");
		/* A single plane is required for input */
		*plane_count = 1;
		if (*buf_count < 1)
			*buf_count = 1;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;
	/* Video capture for decoding (destination)
	 * this can be set after the header was parsed */
	} else if (ctx->state == MFCINST_HEAD_PARSED &&
		   vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		mfc_debug(2, "setting for VIDEO capture\n");
		/* Output plane count is different by the pixel format */
		*plane_count = ctx->dst_fmt->mem_planes;
		/* Setup buffer count */
		if (*buf_count < ctx->dpb_count)
			*buf_count = ctx->dpb_count;
		if (*buf_count > MFC_MAX_BUFFERS)
			*buf_count = MFC_MAX_BUFFERS;
	} else {
		mfc_err_ctx("State seems invalid. State = %d, vq->type = %d\n",
							ctx->state, vq->type);
		return -EINVAL;
	}
	mfc_debug(2, "buffer count=%d, plane count=%d type=0x%x\n",
					*buf_count, *plane_count, vq->type);

	if (ctx->state == MFCINST_HEAD_PARSED &&
	    vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (ctx->is_drm)
			capture_ctx = ctx->dev->alloc_ctx_drm;
		else
			capture_ctx = alloc_ctx;

		if (ctx->dst_fmt->mem_planes == 1) {
			psize[0] = raw->total_plane_size;
			allocators[0] = capture_ctx;
		} else {
			for (i = 0; i < ctx->dst_fmt->num_planes; i++) {
				psize[i] = raw->plane_size[i];
				allocators[i] = capture_ctx;
			}
		}

	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE &&
		   ctx->state == MFCINST_GOT_INST) {
		psize[0] = dec->src_buf_size;

		if (ctx->is_drm)
			allocators[0] = ctx->dev->alloc_ctx_drm;
		else
			allocators[0] = alloc_ctx;

	} else {
		mfc_err_ctx("Currently only decoding is supported. Decoding not initalised.\n");
		return -EINVAL;
	}

	mfc_debug(2, "plane=0, size=%d\n", psize[0]);
	mfc_debug(2, "plane=1, size=%d\n", psize[1]);

	mfc_debug_leave();

	return 0;
}

static void s5p_mfc_unlock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mutex_unlock(&dev->mfc_mutex);
}

static void s5p_mfc_lock(struct vb2_queue *q)
{
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dev *dev;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mutex_lock(&dev->mfc_mutex);
}

static int s5p_mfc_buf_init(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	dma_addr_t start_raw;
	int i, ret;
	unsigned long flags;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (!dec->is_dynamic_dpb &&
				(ctx->capture_state == QUEUE_BUFS_MMAPED)) {
			mfc_debug_leave();
			return 0;
		}
		ret = check_vb_with_fmt(ctx->dst_fmt, vb);
		if (ret < 0)
			return ret;

		start_raw = s5p_mfc_mem_plane_addr(ctx, vb, 0);
		if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12N) {
			buf->planes.raw[0] = start_raw;
			buf->planes.raw[1] = NV12N_CBCR_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
		} else if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12N_10B) {
			buf->planes.raw[0] = start_raw;
			buf->planes.raw[1] = NV12N_10B_CBCR_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
		} else if (ctx->dst_fmt->fourcc == V4L2_PIX_FMT_YUV420N) {
			buf->planes.raw[0] = start_raw;
			buf->planes.raw[1] = YUV420N_CB_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
			buf->planes.raw[2] = YUV420N_CR_BASE(start_raw,
							ctx->img_width,
							ctx->img_height);
		} else {
			for (i = 0; i < ctx->dst_fmt->mem_planes; i++)
				buf->planes.raw[i] = s5p_mfc_mem_plane_addr(ctx, vb, i);
		}

		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &dec->dpb_queue);
		dec->dpb_queue_cnt++;
		spin_unlock_irqrestore(&dev->irqlock, flags);

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_DST,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		ret = check_vb_with_fmt(ctx->src_fmt, vb);
		if (ret < 0)
			return ret;

		buf->planes.stream = s5p_mfc_mem_plane_addr(ctx, vb, 0);

		if (call_cop(ctx, init_buf_ctrls, ctx, MFC_CTRL_TYPE_SRC,
					vb->v4l2_buf.index) < 0)
			mfc_err_ctx("failed in init_buf_ctrls\n");
	} else {
		mfc_err_ctx("s5p_mfc_buf_init: unknown queue type.\n");
		return -EINVAL;
	}

	mfc_debug_leave();

	return 0;
}

static int s5p_mfc_buf_prepare(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_raw_info *raw;
	unsigned int index = vb->v4l2_buf.index;
	int i;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return -EINVAL;
	}
	if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		raw = &ctx->raw_buf;
		/* check the size per plane */
		if (ctx->dst_fmt->mem_planes == 1) {
			mfc_debug(2, "Plane size = %lu, dst size:%d\n",
					vb2_plane_size(vb, 0),
					raw->total_plane_size);
			if (vb2_plane_size(vb, 0) < raw->total_plane_size) {
				mfc_err_ctx("Capture plane is too small\n");
				return -EINVAL;
			}
		} else {
			for (i = 0; i < ctx->dst_fmt->mem_planes; i++) {
				mfc_debug(2, "Plane[%d] size: %lu, dst[%d] size: %d\n",
						i, vb2_plane_size(vb, i),
						i, raw->plane_size[i]);
				if (vb2_plane_size(vb, i) < raw->plane_size[i]) {
					mfc_err_ctx("Capture plane[%d] is too small\n", i);
					return -EINVAL;
				}
			}
		}
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		mfc_debug(2, "Plane size: %lu, ctx->dec_src_buf_size: %zu\n",
				vb2_plane_size(vb, 0), dec->src_buf_size);

		if (vb2_plane_size(vb, 0) < dec->src_buf_size) {
			mfc_err_ctx("Plane buffer (OUTPUT) is too small.\n");
			return -EINVAL;
		}

		if (call_cop(ctx, to_buf_ctrls, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in to_buf_ctrls\n");
	}

	s5p_mfc_mem_prepare(vb);

	return 0;
}

static void s5p_mfc_buf_finish(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	unsigned int index = vb->v4l2_buf.index;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

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
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

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
	struct s5p_mfc_dev *dev;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (ctx->state == MFCINST_FINISHING || ctx->state == MFCINST_FINISHED)
		s5p_mfc_change_state(ctx, MFCINST_RUNNING);

	/* If context is ready then dev = work->data;schedule it to run */
	if (s5p_mfc_dec_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}

	s5p_mfc_try_run(dev);

	return 0;
}

void cleanup_assigned_dpb(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	int i;

	dec = ctx->dec_priv;

	for (i = 0; i < MFC_MAX_DPBS; i++)
		dec->assigned_dpb[i] = NULL;
}

static void cleanup_assigned_fd(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	int i;

	dec = ctx->dec_priv;

	for (i = 0; i < MFC_MAX_DPBS; i++)
		dec->assigned_fd[i] = MFC_INFO_INIT_FD;
}

#define need_to_wait_frame_start(ctx)		\
	(((ctx->state == MFCINST_FINISHING) ||	\
	  (ctx->state == MFCINST_RUNNING)) &&	\
	 test_bit(ctx->num, &ctx->dev->hw_lock))
#define need_to_dpb_flush(ctx)		\
	((ctx->state == MFCINST_FINISHING) ||	\
	  (ctx->state == MFCINST_RUNNING))
static void s5p_mfc_stop_streaming(struct vb2_queue *q)
{
	unsigned long flags;
	struct s5p_mfc_ctx *ctx = q->drv_priv;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_dev *dev;
	int index = 0;
	int ret = 0;
	int prev_state;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	mfc_info_ctx("dec stop_streaming is called, hw_lock : %d, type : %d\n",
				test_bit(ctx->num, &dev->hw_lock), q->type);
	MFC_TRACE_CTX("** DEC streamoff(type:%d)\n", q->type);

	spin_lock_irq(&dev->condlock);
	set_bit(ctx->num, &dev->ctx_stop_bits);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If a H/W operation is in progress, wait for it complete */
	if (test_bit(ctx->num, &dev->hw_lock)) {
		ret = wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));
		if (ret == 0)
			mfc_err_ctx("wait for event failed\n");
	}

	spin_lock_irqsave(&dev->irqlock, flags);

	if (q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		if (dec->is_dynamic_dpb) {
			cleanup_assigned_fd(ctx);
			s5p_mfc_cleanup_queue(&dec->ref_queue);
			INIT_LIST_HEAD(&dec->ref_queue);
			dec->ref_queue_cnt = 0;
			dec->dynamic_used = 0;
			dec->err_reuse_flag = 0;
		}

		s5p_mfc_cleanup_queue(&ctx->dst_queue);
		INIT_LIST_HEAD(&ctx->dst_queue);
		ctx->dst_queue_cnt = 0;
		ctx->is_dpb_realloc = 0;
		dec->dpb_flush = 1;
		dec->dpb_status = 0;

		INIT_LIST_HEAD(&dec->dpb_queue);
		dec->dpb_queue_cnt = 0;
		dec->consumed = 0;
		dec->remained_size = 0;
		dec->y_addr_for_pb = 0;

		if (ctx->is_drm && ctx->raw_protect_flag) {
			struct s5p_mfc_buf *dst_buf;
			int i;

			mfc_debug(2, "raw_protect_flag(%#lx) will be released\n",
					ctx->raw_protect_flag);
			for (i = 0; i < MFC_MAX_DPBS; i++) {
				if (test_bit(i, &ctx->raw_protect_flag)) {
					dst_buf = dec->assigned_dpb[i];
					if (s5p_mfc_raw_buf_prot(ctx, dst_buf, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(i, &ctx->raw_protect_flag);
					mfc_debug(2, "[%d] dec dst buf un-prot_flag: %#lx\n",
							i, ctx->raw_protect_flag);
				}
			}
			cleanup_assigned_dpb(ctx);
		}

		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->dst_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__dec_reset_buf_ctrls(&ctx->dst_ctrls[index]);
			index++;
		}
		if (ctx->wait_state == WAIT_INITBUF_DONE ||
					ctx->wait_state == WAIT_DECODING) {
			ctx->wait_state = WAIT_NONE;
			mfc_debug(2, "Decoding can be started now\n");
		}
	} else if (q->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		while (!list_empty(&ctx->src_queue)) {
			struct s5p_mfc_buf *src_buf;
			int index, csd, condition = 0;

			src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
			index = src_buf->vb.v4l2_buf.index;
			csd = src_buf->vb.v4l2_buf.reserved2 & FLAG_CSD ? 1 : 0;

			if (csd) {
				spin_unlock_irqrestore(&dev->irqlock, flags);
				s5p_mfc_clean_ctx_int_flags(ctx);
				if (need_to_special_parsing(ctx)) {
					s5p_mfc_change_state(ctx, MFCINST_SPECIAL_PARSING);
					condition = S5P_FIMV_R2H_CMD_SEQ_DONE_RET;
					mfc_info_ctx("try to special parsing! (before NAL_START)\n");
				} else if (need_to_special_parsing_nal(ctx)) {
					s5p_mfc_change_state(ctx, MFCINST_SPECIAL_PARSING_NAL);
					condition = S5P_FIMV_R2H_CMD_FRAME_DONE_RET;
					mfc_info_ctx("try to special parsing! (after NAL_START)\n");
				} else {
					mfc_info_ctx("can't parsing CSD!, state = %d\n", ctx->state);
				}
				if (condition) {
					spin_lock_irq(&dev->condlock);
					set_bit(ctx->num, &dev->ctx_work_bits);
					spin_unlock_irq(&dev->condlock);
					s5p_mfc_try_run(dev);					
					if (s5p_mfc_wait_for_done_ctx(ctx, condition)) {
						mfc_err_ctx("special parsing time out\n");
						s5p_mfc_cleanup_timeout_and_try_run(ctx);
					}
				}
				spin_lock_irqsave(&dev->irqlock, flags);
			}
			if (ctx->is_drm && test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, src_buf, false))
					mfc_err_ctx("failed to CFW_UNPROT\n");
				else
					clear_bit(index, &ctx->stream_protect_flag);
				mfc_debug(2, "[%d] dec src buf un-prot flag: %#lx\n",
						index, ctx->stream_protect_flag);
			}
			vb2_set_plane_payload(&src_buf->vb, 0, 0);
			vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_ERROR);
			list_del(&src_buf->list);
		}

		INIT_LIST_HEAD(&ctx->src_queue);
		ctx->src_queue_cnt = 0;

		while (index < MFC_MAX_BUFFERS) {
			index = find_next_bit(&ctx->src_ctrls_avail,
					MFC_MAX_BUFFERS, index);
			if (index < MFC_MAX_BUFFERS)
				__dec_reset_buf_ctrls(&ctx->src_ctrls[index]);
			index++;
		}
	}

	if (ctx->state == MFCINST_FINISHING)
		s5p_mfc_change_state(ctx, MFCINST_RUNNING);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (IS_MFCV6(dev) && q->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE &&
			need_to_dpb_flush(ctx)) {
		prev_state = ctx->state;
		s5p_mfc_change_state(ctx, MFCINST_DPB_FLUSHING);
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_clean_ctx_int_flags(ctx);
		mfc_info_ctx("try to DPB flush\n");
		s5p_mfc_try_run(dev);
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_DPB_FLUSH_RET))
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
		s5p_mfc_change_state(ctx, prev_state);
	}

	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_stop_bits);
	spin_unlock_irq(&dev->condlock);

	mfc_debug(2, "buffer cleanup & flush is done in stop_streaming, type : %d\n", q->type);

	if (s5p_mfc_dec_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}
	spin_lock_irq(&dev->condlock);
	if (dev->ctx_work_bits)
		queue_work(dev->sched_wq, &dev->sched_work);
	spin_unlock_irq(&dev->condlock);

}

#define mfc_need_to_fill_dpb(ctx, dec, index)						\
				((dec->is_dynamic_dpb) && (!dec->dynamic_ref_filled)	\
				 && (!ctx->is_drm) && (index == 0))

static void s5p_mfc_buf_queue(struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned long flags;
	struct s5p_mfc_buf *buf = vb_to_mfc_buf(vb);
	struct s5p_mfc_buf *dpb_buf, *tmp_buf;
	int wait_flag = 0;
	int remove_flag = 0;
	int index, i;
	int skip_add = 0;
	unsigned char *stream_vir = NULL;

	mfc_debug_enter();
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}
	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	if (vq->type == V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE) {
		buf->used = 0;
		buf->consumed = 0;
		mfc_debug(2, "Src queue: %p\n", &ctx->src_queue);
		mfc_debug(2, "Adding to src: %p (0x%08llx, 0x%08llx)\n", vb,
				s5p_mfc_mem_plane_addr(ctx, vb, 0),
				buf->planes.stream);
		if (ctx->state < MFCINST_HEAD_PARSED && !ctx->is_drm) {
			stream_vir = vb2_plane_vaddr(vb, 0);
			s5p_mfc_mem_inv_vb(vb, 1);
		}
		spin_lock_irqsave(&dev->irqlock, flags);
		list_add_tail(&buf->list, &ctx->src_queue);
		ctx->src_queue_cnt++;
		buf->vir_addr = stream_vir;
		spin_unlock_irqrestore(&dev->irqlock, flags);
	} else if (vq->type == V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE) {
		buf->used = 0;
		buf->already = 0;
		index = vb->v4l2_buf.index;
		mfc_debug(2, "Dst queue: %p\n", &ctx->dst_queue);
		mfc_debug(2, "Adding to dst: %p (0x%08llx)\n", vb,
				s5p_mfc_mem_plane_addr(ctx, vb, 0));
		for (i = 0; i < ctx->dst_fmt->num_planes; i++)
			mfc_debug(2, "dec dst plane[%d]: %08llx\n",
					i, buf->planes.raw[i]);
		mfc_debug(2, "ADDING Flag before: %lx (%d)\n",
					dec->dpb_status, index);
		/* Mark destination as available for use by MFC */
		spin_lock_irqsave(&dev->irqlock, flags);
		if (!list_empty(&dec->dpb_queue)) {
			remove_flag = 0;
			list_for_each_entry_safe(dpb_buf, tmp_buf, &dec->dpb_queue, list) {
				if (dpb_buf == buf) {
					list_del(&dpb_buf->list);
					remove_flag = 1;
					break;
				}
			}
			if (remove_flag == 0) {
				mfc_err_ctx("Can't find buf(0x%08llx)\n",
						buf->planes.raw[0]);
				spin_unlock_irqrestore(&dev->irqlock, flags);
				return;
			}
		}
		if (dec->is_dynamic_dpb) {
			dec->assigned_fd[index] = vb->v4l2_planes[0].m.fd;
			mfc_debug(2, "Assigned FD[%d] = %d\n", index,
						dec->assigned_fd[index]);
			if (dec->dynamic_used & (1 << index)) {
				/* This buffer is already referenced */
				mfc_debug(2, "Already ref[%d], fd = %d\n",
						index, dec->assigned_fd[index]);
				if (is_h264(ctx)) {
					mfc_debug(2, "Add to DPB list\n");
					buf->already = 1;
				} else {
					list_add_tail(&buf->list, &dec->ref_queue);
					dec->ref_queue_cnt++;
					skip_add = 1;
				}
			}
		} else {
			set_bit(index, &dec->dpb_status);
			mfc_debug(2, "ADDING Flag after: %lx\n",
							dec->dpb_status);
		}
		if (!skip_add) {
			list_add_tail(&buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;
		}
		spin_unlock_irqrestore(&dev->irqlock, flags);
		if (mfc_need_to_fill_dpb(ctx, dec, index)) {
			mfc_fill_dynamic_dpb(ctx, vb);
			dec->dynamic_ref_filled = 1;
		}
		if ((dec->dst_memtype == V4L2_MEMORY_USERPTR || dec->dst_memtype == V4L2_MEMORY_DMABUF) &&
				ctx->dst_queue_cnt == dec->total_dpb_count)
			ctx->capture_state = QUEUE_BUFS_MMAPED;
	} else {
		mfc_err_ctx("Unsupported buffer type (%d)\n", vq->type);
	}

	if (s5p_mfc_dec_ctx_ready(ctx)) {
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
		if (ctx->state == MFCINST_HEAD_PARSED &&
			ctx->capture_state == QUEUE_BUFS_MMAPED)
			wait_flag = 1;
	}
	s5p_mfc_try_run(dev);
	if (wait_flag) {
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET))
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
	}

	mfc_debug_leave();
}

static struct vb2_ops s5p_mfc_dec_qops = {
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

const struct v4l2_ioctl_ops *get_dec_v4l2_ioctl_ops(void)
{
	return &s5p_mfc_dec_ioctl_ops;
}

int s5p_mfc_init_dec_ctx(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	int ret = 0;
	int i;

	dec = kzalloc(sizeof(struct s5p_mfc_dec), GFP_KERNEL);
	if (!dec) {
		mfc_err("failed to allocate decoder private data\n");
		return -ENOMEM;
	}
	ctx->dec_priv = dec;

	ctx->inst_no = MFC_NO_INSTANCE_SET;

	INIT_LIST_HEAD(&ctx->src_queue);
	INIT_LIST_HEAD(&ctx->dst_queue);
	ctx->src_queue_cnt = 0;
	ctx->dst_queue_cnt = 0;

	for (i = 0; i < MFC_MAX_BUFFERS; i++) {
		INIT_LIST_HEAD(&ctx->src_ctrls[i]);
		INIT_LIST_HEAD(&ctx->dst_ctrls[i]);
	}
	ctx->src_ctrls_avail = 0;
	ctx->dst_ctrls_avail = 0;

	ctx->capture_state = QUEUE_FREE;
	ctx->output_state = QUEUE_FREE;

	s5p_mfc_change_state(ctx, MFCINST_INIT);
	ctx->type = MFCINST_DECODER;
	ctx->c_ops = &decoder_codec_ops;
	ctx->src_fmt = &formats[DEF_SRC_FMT];
	ctx->dst_fmt = &formats[DEF_DST_FMT];

	ctx->framerate = DEC_MAX_FPS;
	ctx->qos_ratio = 100;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&ctx->qos_list);
#endif
	INIT_LIST_HEAD(&ctx->ts_list);

	INIT_LIST_HEAD(&dec->dpb_queue);
	dec->dpb_queue_cnt = 0;
	INIT_LIST_HEAD(&dec->ref_queue);
	dec->ref_queue_cnt = 0;

	dec->display_delay = -1;
	dec->is_packedpb = 0;
	dec->is_interlaced = 0;
	dec->immediate_display = 0;
	dec->is_dts_mode = 0;
	dec->tiled_buf_cnt = 0;
	dec->err_reuse_flag = 0;

	dec->is_dynamic_dpb = 0;
	dec->dynamic_used = 0;
	cleanup_assigned_fd(ctx);
	cleanup_assigned_dpb(ctx);
	dec->sh_handle.fd = -1;
	dec->ref_info = kzalloc(
		(sizeof(struct dec_dpb_ref_info) * MFC_MAX_DPBS), GFP_KERNEL);
	if (!dec->ref_info) {
		mfc_err("failed to allocate decoder information data\n");
		return -ENOMEM;
	}
	for (i = 0; i < MFC_MAX_BUFFERS; i++)
		dec->ref_info[i].dpb[0].fd[0] = MFC_INFO_INIT_FD;

	dec->profile = -1;

	/* Init videobuf2 queue for OUTPUT */
	ctx->vq_src.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
	ctx->vq_src.drv_priv = ctx;
	ctx->vq_src.buf_struct_size = sizeof(struct s5p_mfc_buf);
	ctx->vq_src.io_modes = VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	ctx->vq_src.ops = &s5p_mfc_dec_qops;
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
	ctx->vq_dst.ops = &s5p_mfc_dec_qops;
	ctx->vq_dst.mem_ops = s5p_mfc_mem_ops();
	ctx->vq_dst.timestamp_flags = V4L2_BUF_FLAG_TIMESTAMP_COPY;
	ret = vb2_queue_init(&ctx->vq_dst);
	if (ret) {
		mfc_err("Failed to initialize videobuf2 queue(capture)\n");
		return ret;
	}

	/* For MFC 6.x */
	dec->remained = 0;

	return ret;
}
