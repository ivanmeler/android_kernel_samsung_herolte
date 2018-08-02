/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_opr_v6.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Samsung MFC (Multi Function Codec - FIMV) driver
 * This file contains hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/delay.h>
#include <linux/mm.h>
#include <linux/io.h>
#include <linux/jiffies.h>

#include <linux/firmware.h>
#include <linux/err.h>
#include <linux/sched.h>
#include <linux/dma-mapping.h>
#include <asm/cacheflush.h>

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
#include <linux/exynos_iovmm.h>
#endif
#include <soc/samsung/bts.h>
#include <soc/samsung/devfreq.h>

#include "s5p_mfc_opr_v10.h"

#include "s5p_mfc_cmd.h"
#include "s5p_mfc_enc_param.h"
#include "s5p_mfc_buf.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_intr.h"
#include "s5p_mfc_inst.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_utils.h"
#include "s5p_mfc_nal_q.h"

/* This value guarantees 375msec ~ 2sec according to MFC clock (533MHz ~ 100MHz)
 * releated with S5P_FIMV_DEC_TIMEOUT_VALUE */
#define MFC_TIMEOUT_VALUE	200000000

/* Initialize decoding */
static int s5p_mfc_init_decode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned int reg = 0, pix_val;
	int fmo_aso_ctrl = 0;

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
	mfc_debug(2, "InstNo: %d/%d\n", ctx->inst_no, S5P_FIMV_CH_SEQ_HEADER);
	mfc_debug(2, "BUFs: %08x\n", MFC_READL(S5P_FIMV_D_CPB_BUFFER_ADDR));

	reg |= (dec->idr_decoding << S5P_FIMV_D_OPT_IDR_DECODING_SHFT);
	/* FMO_ASO_CTRL - 0: Enable, 1: Disable */
	reg |= (fmo_aso_ctrl << S5P_FIMV_D_OPT_FMO_ASO_CTRL_MASK);
	/* When user sets desplay_delay to 0,
	 * It works as "display_delay enable" and delay set to 0.
	 * If user wants display_delay disable, It should be
	 * set to negative value. */
	if (dec->display_delay >= 0) {
		reg |= (0x1 << S5P_FIMV_D_OPT_DDELAY_EN_SHIFT);
		MFC_WRITEL(dec->display_delay, S5P_FIMV_D_DISPLAY_DELAY);
	}
	/* Setup loop filter, for decoding this is only valid for MPEG4 */
	if ((ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_DEC) &&
			!FW_HAS_INITBUF_LOOP_FILTER(dev)) {
		mfc_debug(2, "Set loop filter to: %d\n", dec->loop_filter_mpeg4);
		reg |= (dec->loop_filter_mpeg4 << S5P_FIMV_D_OPT_LF_CTRL_SHIFT);
	}
	if ((ctx->dst_fmt->fourcc == V4L2_PIX_FMT_NV12MT_16X16) &&
			!FW_HAS_INITBUF_TILE_MODE(dev))
		reg |= (0x1 << S5P_FIMV_D_OPT_TILE_MODE_SHIFT);

	/* VC1 RCV: Discard to parse additional header as default */
	if (ctx->codec_mode == S5P_FIMV_CODEC_VC1RCV_DEC)
		reg |= (0x1 << S5P_FIMV_D_OPT_DISCARD_RCV_HEADER);

	/* conceal control to specific color */
	if (FW_HAS_CONCEAL_CONTROL(dev))
		reg |= (0x3 << S5P_FIMV_D_OPT_CONCEAL_CONTROL);

	/* Parsing all including PPS */
	reg |= (0x1 << S5P_FIMV_D_OPT_SPECIAL_PARSING_SHIFT);

	MFC_WRITEL(reg, S5P_FIMV_D_DEC_OPTIONS);

	if (FW_HAS_CONCEAL_CONTROL(dev))
		MFC_WRITEL(MFC_CONCEAL_COLOR, S5P_FIMV_D_FORCE_PIXEL_VAL);

	if (ctx->codec_mode == S5P_FIMV_CODEC_FIMV1_DEC) {
		mfc_debug(2, "Setting FIMV1 resolution to %dx%d\n",
					ctx->img_width, ctx->img_height);
		MFC_WRITEL(ctx->img_width, S5P_FIMV_D_SET_FRAME_WIDTH);
		MFC_WRITEL(ctx->img_height, S5P_FIMV_D_SET_FRAME_HEIGHT);
	}

	switch (ctx->dst_fmt->fourcc) {
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV12N:
	case V4L2_PIX_FMT_NV12N_10B:
	case V4L2_PIX_FMT_NV12MT_16X16:
		pix_val = 0;
		break;
	case V4L2_PIX_FMT_NV21M:
		pix_val = 1;
		break;
	case V4L2_PIX_FMT_YVU420M:
		if (IS_MFCV8(dev)) {
			pix_val = 2;
		} else {
			pix_val = 0;
			mfc_err_ctx("Not supported format : YV12\n");
		}
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YUV420N:
		if (IS_MFCV8(dev)) {
			pix_val = 3;
		} else {
			pix_val = 0;
			mfc_err_ctx("Not supported format : I420\n");
		}
		break;
	default:
		pix_val = 0;
		break;
	}
	MFC_WRITEL(pix_val, S5P_FIMV_PIXEL_FORMAT);

	/* sei parse */
	reg = dec->sei_parse;
	/* Enable realloc interface if SEI is enabled */
	if (dec->sei_parse && FW_HAS_SEI_S3D_REALLOC(dev))
		reg |= (0x1 << S5P_FIMV_D_SEI_ENABLE_NEED_INIT_BUFFER_SHIFT);
	if (FW_HAS_SEI_INFO_FOR_HDR(dev)) {
		reg |= (0x1 << S5P_FIMV_D_SEI_ENABLE_CONTENT_LIGHT_SHIFT);
		reg |= (0x1 << S5P_FIMV_D_SEI_ENABLE_MASTERING_DISPLAY_SHIFT);
	}
	MFC_WRITEL(reg, S5P_FIMV_D_SEI_ENABLE);
	mfc_debug(2, "SEI available was set, 0x%x\n", MFC_READL(S5P_FIMV_D_SEI_ENABLE));

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_SEQ_HEADER, NULL);

	mfc_debug_leave();
	return 0;
}

/* Decode a single frame */
static int s5p_mfc_decode_one_frame(struct s5p_mfc_ctx *ctx, int last_frame)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;

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
	mfc_debug(2, "Setting flags to %08lx (free:%d WTF:%d)\n",
				dec->dpb_status, ctx->dst_queue_cnt,
						dec->dpb_queue_cnt);
	if (dec->is_dynamic_dpb) {
		mfc_debug(2, "Dynamic:0x%08x, Available:0x%08lx\n",
					dec->dynamic_set, dec->dpb_status);
		MFC_WRITEL(dec->dynamic_set, S5P_FIMV_D_DYNAMIC_DPB_FLAG_LOWER);
		MFC_WRITEL(0x0, S5P_FIMV_D_DYNAMIC_DPB_FLAG_UPPER);
	}
	MFC_WRITEL(dec->dpb_status, S5P_FIMV_D_AVAILABLE_DPB_FLAG_LOWER);
	MFC_WRITEL(0x0, S5P_FIMV_D_AVAILABLE_DPB_FLAG_UPPER);
	MFC_WRITEL(dec->slice_enable, S5P_FIMV_D_SLICE_IF_ENABLE);
	if (FW_HAS_INT_TIMEOUT(dev))
		MFC_WRITEL(MFC_TIMEOUT_VALUE, S5P_FIMV_DEC_TIMEOUT_VALUE);

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	/* Issue different commands to instance basing on whether it
	 * is the last frame or not. */
	switch (last_frame) {
	case 0:
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_FRAME_START, NULL);
		break;
	case 1:
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_LAST_FRAME, NULL);
		break;
	}

	mfc_debug(2, "Decoding a usual frame.\n");
	return 0;
}

static int s5p_mfc_init_encode(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	mfc_debug(2, "++\n");

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
		s5p_mfc_set_enc_params_h264(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_MPEG4_ENC)
		s5p_mfc_set_enc_params_mpeg4(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_H263_ENC)
		s5p_mfc_set_enc_params_h263(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC)
		s5p_mfc_set_enc_params_vp8(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_VP9_ENC)
		s5p_mfc_set_enc_params_vp9(ctx);
	else if (ctx->codec_mode == S5P_FIMV_CODEC_HEVC_ENC)
		s5p_mfc_set_enc_params_hevc(ctx);
	else {
		mfc_err_ctx("Unknown codec for encoding (%x).\n",
			ctx->codec_mode);
		return -EINVAL;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_SEQ_HEADER, NULL);

	mfc_debug(2, "--\n");

	return 0;
}

static int s5p_mfc_h264_set_aso_slice_order(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	struct s5p_mfc_h264_enc_params *p_264 = &p->codec.h264;
	int i;

	if (p_264->aso_enable) {
		for (i = 0; i < 8; i++)
			MFC_WRITEL(p_264->aso_slice_order[i],
				S5P_FIMV_E_H264_ASO_SLICE_ORDER_0 + i * 4);
	}
	return 0;
}

static inline void s5p_mfc_set_stride_enc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int i;

	if (IS_MFCv7X(dev) || IS_MFCV8(dev)) {
		for (i = 0; i < ctx->raw_buf.num_planes; i++) {
			MFC_WRITEL(ctx->raw_buf.stride[i],
				S5P_FIMV_E_SOURCE_FIRST_STRIDE + (i * 4));
			mfc_debug(2, "enc src[%d] stride: 0x%08lx",
				i, (unsigned long)ctx->raw_buf.stride[i]);
		}
	}
}

/*
	When the resolution is changed,
	s5p_mfc_start_change_resol_enc() should be called right before NAL_START.
	return value
	0: no resolution change
	1: resolution swap
	2: resolution change
*/
int s5p_mfc_start_change_resol_enc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_enc *enc = ctx->enc_priv;
	struct s5p_mfc_enc_params *p = &enc->params;
	unsigned int reg = 0;
	int old_img_width;
	int old_img_height;
	int new_img_width;
	int new_img_height;
	int ret = 0;

	if ((ctx->img_width == 0) || (ctx->img_height == 0)) {
		mfc_err("new_img_width = %d, new_img_height = %d\n",
			ctx->img_width, ctx->img_height);
		return 0;
	}

	old_img_width = ctx->old_img_width;
	old_img_height = ctx->old_img_height;

	new_img_width = ctx->img_width;
	new_img_height = ctx->img_height;

	if ((old_img_width == new_img_width) && (old_img_height == new_img_height)) {
		mfc_err("Resolution is not changed. new_img_width = %d, new_img_height = %d\n",
			new_img_width, new_img_height);
		return 0;
	}

	mfc_info_ctx("Resolution Change : (%d x %d) -> (%d x %d)\n",
		old_img_width, old_img_height, new_img_width, new_img_height);

	if (ctx->img_width) {
		ctx->buf_width = ALIGN(ctx->img_width, MFC_NV12M_HALIGN);
	}

	if ((old_img_width == new_img_height) && (old_img_height == new_img_width)) {
		reg = MFC_READL(S5P_FIMV_E_PARAM_CHANGE);
		reg &= ~(0x1 << 6);
		reg |= (0x1 << 6); /* resolution swap */
		MFC_WRITEL(reg, S5P_FIMV_E_PARAM_CHANGE);
		ret = 1;
	} else {
		reg = MFC_READL(S5P_FIMV_E_PARAM_CHANGE);
		reg &= ~(0x3 << 7);
		/* For now, FW does not care S5P_FIMV_E_PARAM_CHANGE is 1 or 2.
		 * It cares S5P_FIMV_E_PARAM_CHANGE is NOT zero.
		 */
		if ((old_img_width*old_img_height) < (new_img_width*new_img_height)) {
			reg |= (0x1 << 7); /* resolution increased */
			mfc_info_ctx("Resolution Increased\n");
		} else {
			reg |= (0x2 << 7); /* resolution decreased */
			mfc_info_ctx("Resolution Decreased\n");
		}
		MFC_WRITEL(reg, S5P_FIMV_E_PARAM_CHANGE);

		/** set cropped width */
		MFC_WRITEL(ctx->img_width, S5P_FIMV_E_CROPPED_FRAME_WIDTH);
		/** set cropped height */
		MFC_WRITEL(ctx->img_height, S5P_FIMV_E_CROPPED_FRAME_HEIGHT);

		/* bit rate */
		if (p->rc_frame)
			MFC_WRITEL(p->rc_bitrate, S5P_FIMV_E_RC_BIT_RATE);
		else
			MFC_WRITEL(1, S5P_FIMV_E_RC_BIT_RATE);
		ret = 2;
	}

	/** set new stride */
	s5p_mfc_set_stride_enc(ctx);

	/** set cropped offset */
	MFC_WRITEL(0x0, S5P_FIMV_E_FRAME_CROP_OFFSET);

	return ret;
}

/* Encode a single frame */
static int s5p_mfc_encode_one_frame(struct s5p_mfc_ctx *ctx, int last_frame)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	mfc_debug(2, "++\n");

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_ENC)
		s5p_mfc_h264_set_aso_slice_order(ctx);

	s5p_mfc_set_slice_mode(ctx);

	if (ctx->enc_drc_flag) {
		ctx->enc_res_change = s5p_mfc_start_change_resol_enc(ctx);
		ctx->enc_drc_flag = 0;
	}

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	/* Issue different commands to instance basing on whether it
	 * is the last frame or not. */
	switch (last_frame) {
	case 0:
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_FRAME_START, NULL);
		break;
	case 1:
		s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_LAST_FRAME, NULL);
		break;
	}

	mfc_debug(2, "--\n");

	return 0;
}

static int mfc_set_dynamic_dpb(struct s5p_mfc_ctx *ctx, struct s5p_mfc_buf *dst_vb)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_raw_info *raw = &ctx->raw_buf;
	int dst_index;
	int i;

	dst_index = dst_vb->vb.v4l2_buf.index;
	dst_vb->used = 1;
	set_bit(dst_index, &dec->dpb_status);
	dec->dynamic_set = 1 << dst_index;
	mfc_debug(2, "ADDING Flag after: %lx\n", dec->dpb_status);
	mfc_debug(2, "Dst addr [%d] = 0x%llx\n", dst_index,
			(unsigned long long)dst_vb->planes.raw[0]);

	/* decoder dst buffer CFW PROT */
	if (ctx->is_drm) {
		dec->assigned_dpb[dst_index] = dst_vb;
		if (!test_bit(dst_index, &ctx->raw_protect_flag)) {
			if (s5p_mfc_raw_buf_prot(ctx, dst_vb, true))
				mfc_err_ctx("failed to CFW_PROT\n");
			else
				set_bit(dst_index, &ctx->raw_protect_flag);
		}
		mfc_debug(2, "[%d] dec dst buf prot_flag: %#lx\n",
				dst_index, ctx->raw_protect_flag);
	}

	for (i = 0; i < raw->num_planes; i++) {
		MFC_WRITEL(raw->plane_size[i],
				S5P_FIMV_D_FIRST_PLANE_DPB_SIZE + i*4);
		MFC_WRITEL(dst_vb->planes.raw[i],
				S5P_FIMV_D_FIRST_PLANE_DPB0 + (i*0x100 + dst_index*4));
	}

	return 0;
}

static inline int s5p_mfc_run_dec_last_frames(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *temp_vb, *dst_vb;
	struct s5p_mfc_dec *dec;
	unsigned long flags;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	if ((dec->is_dynamic_dpb) && (ctx->dst_queue_cnt == 0)) {
		mfc_debug(2, "No dst buffer\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "No src buffers.\n");
		s5p_mfc_set_dec_stream_buffer(ctx, 0, 0, 0);
	} else {
		/* Get the next source buffer */
		temp_vb = list_entry(ctx->src_queue.next,
					struct s5p_mfc_buf, list);
		temp_vb->used = 1;

		/* decoder src buffer CFW PROT */
		if (ctx->is_drm) {
			int index = temp_vb->vb.v4l2_buf.index;

			if (!test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, temp_vb, true))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					set_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] dec src buf prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}

		if (dec->consumed)
			s5p_mfc_set_dec_stream_buffer(ctx, temp_vb,
					dec->consumed, dec->remained_size);
		else
			s5p_mfc_set_dec_stream_buffer(ctx, temp_vb, 0, 0);
	}

	if (dec->is_dynamic_dpb) {
		dst_vb = list_entry(ctx->dst_queue.next,
						struct s5p_mfc_buf, list);
		mfc_set_dynamic_dpb(ctx, dst_vb);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_decode_one_frame(ctx, 1);

	return 0;
}

#define is_full_DPB(ctx, total)		(((ctx)->dst_queue_cnt == 1) &&		\
					((total) >= (ctx->dpb_count + 5)))
#define is_full_refered(ctx, dec)	(((ctx)->dst_queue_cnt == 0) &&		\
					(((dec)->ref_queue_cnt) == ((ctx)->dpb_count + 5)))
/* Try to search non-referenced DPB on ref-queue */
static struct s5p_mfc_buf *search_for_DPB(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *dst_vb = NULL;
	int found = 0, temp_index, sum_dpb;

	mfc_debug(2, "Failed to find non-referenced DPB\n");

	list_for_each_entry(dst_vb, &dec->ref_queue, list) {
		temp_index = dst_vb->vb.v4l2_buf.index;
		if ((dec->dynamic_used & (1 << temp_index)) == 0) {
			found = 1;
			break;
		}
	}

	if (!found) {
		dst_vb = list_entry(ctx->dst_queue.next,
				struct s5p_mfc_buf, list);

		sum_dpb = ctx->dst_queue_cnt + dec->ref_queue_cnt;

		if (is_full_DPB(ctx, sum_dpb)) {
			mfc_debug(2, "We should use this buffer.\n");
		} else if (is_full_refered(ctx, dec)) {
			mfc_debug(2, "All buffers are referenced.\n");
			dst_vb = list_entry(dec->ref_queue.next,
					struct s5p_mfc_buf, list);

			list_del(&dst_vb->list);
			dec->ref_queue_cnt--;

			list_add_tail(&dst_vb->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;
		} else {
			list_del(&dst_vb->list);
			ctx->dst_queue_cnt--;

			list_add_tail(&dst_vb->list, &dec->ref_queue);
			dec->ref_queue_cnt++;

			mfc_debug(2, "Failed to start, ref = %d, dst = %d\n",
					dec->ref_queue_cnt, ctx->dst_queue_cnt);

			return NULL;
		}
	} else {
		list_del(&dst_vb->list);
		dec->ref_queue_cnt--;

		list_add_tail(&dst_vb->list, &ctx->dst_queue);
		ctx->dst_queue_cnt++;
	}

	return dst_vb;
}

static inline int s5p_mfc_run_dec_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *temp_vb, *dst_vb;
	struct s5p_mfc_dec *dec;
	unsigned long flags;
	int last_frame = 0;
	unsigned int index;
	unsigned int size;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	spin_lock_irqsave(&dev->irqlock, flags);

	/* Frames are being decoded */
	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "No src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}
	if (is_h264(ctx)) {
		if (dec->is_dynamic_dpb && ctx->dst_queue_cnt == 0 &&
			dec->ref_queue_cnt < (ctx->dpb_count + 5)) {
			spin_unlock_irqrestore(&dev->irqlock, flags);
			return -EAGAIN;
		}

	} else if ((dec->is_dynamic_dpb && ctx->dst_queue_cnt == 0) ||
		(!dec->is_dynamic_dpb && ctx->dst_queue_cnt < ctx->dpb_count)) {
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	/* Get the next source buffer */
	temp_vb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	temp_vb->used = 1;
	mfc_debug(2, "Temp vb: %p\n", temp_vb);
	mfc_debug(2, "Src Addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0));

	/* decoder src buffer CFW PROT */
	if (ctx->is_drm) {
		if (!dec->consumed && !temp_vb->consumed) {
			index = temp_vb->vb.v4l2_buf.index;
			if (!test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, temp_vb, true))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					set_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] dec src buf prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}
	}

	if (temp_vb->vb.v4l2_buf.reserved2 & FLAG_EMPTY_DATA)
		temp_vb->vb.v4l2_planes[0].bytesused = 0;

	if (dec->consumed) {
		s5p_mfc_set_dec_stream_buffer(ctx, temp_vb, dec->consumed, dec->remained_size);
	} else {
		if (temp_vb->consumed)
			size = temp_vb->vb.v4l2_planes[0].bytesused - temp_vb->consumed;
		else
			size = temp_vb->vb.v4l2_planes[0].bytesused;
		s5p_mfc_set_dec_stream_buffer(ctx, temp_vb, temp_vb->consumed, size);
	}

	index = temp_vb->vb.v4l2_buf.index;
	if (call_cop(ctx, set_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
		mfc_err_ctx("failed in set_buf_ctrls_val\n");

	if (dec->is_dynamic_dpb) {
		if (is_h264(ctx)) {
			int found = 0, temp_index;

			/* Try to use the non-referenced DPB on dst-queue */
			list_for_each_entry(dst_vb, &ctx->dst_queue, list) {
				temp_index = dst_vb->vb.v4l2_buf.index;
				if ((dec->dynamic_used & (1 << temp_index)) == 0) {
					found = 1;
					break;
				}
			}

			if (!found) {
				dst_vb = search_for_DPB(ctx);
				if (!dst_vb) {
					spin_unlock_irqrestore(&dev->irqlock, flags);
					return -EAGAIN;
				}
			}
		} else {
			dst_vb = list_entry(ctx->dst_queue.next,
					struct s5p_mfc_buf, list);
		}

		mfc_set_dynamic_dpb(ctx, dst_vb);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_clean_ctx_int_flags(ctx);

	if (temp_vb->vb.v4l2_buf.reserved2 & FLAG_LAST_FRAME) {
		last_frame = 1;
		mfc_debug(2, "Setting ctx->state to FINISHING\n");
		s5p_mfc_change_state(ctx, MFCINST_FINISHING);
	}
	s5p_mfc_decode_one_frame(ctx, last_frame);

	return 0;
}

static inline int s5p_mfc_run_enc_last_frames(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t src_addr[3] = { 0, 0, 0 };

	raw = &ctx->raw_buf;
	spin_lock_irqsave(&dev->irqlock, flags);

	/* Source is not used for encoding, but should exist. */
	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "no src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	if (list_empty(&ctx->dst_queue)) {
		mfc_debug(2, "no dst buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	mfc_debug(2, "Set address zero for all planes\n");
	s5p_mfc_set_enc_frame_buffer(ctx, &src_addr[0], raw->num_planes);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_mb->used = 1;

	/* encoder dst buffer CFW PROT */
	if (ctx->is_drm) {
		int index = dst_mb->vb.v4l2_buf.index;

		if (!test_bit(index, &ctx->stream_protect_flag)) {
			if (s5p_mfc_stream_buf_prot(ctx, dst_mb, true))
				mfc_err_ctx("failed to CFW_PROT\n");
			else
				set_bit(index, &ctx->stream_protect_flag);
		}
		mfc_debug(2, "[%d] enc dst buf prot_flag: %#lx\n",
				index, ctx->stream_protect_flag);
	}

	s5p_mfc_set_enc_stream_buffer(ctx, dst_mb);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_encode_one_frame(ctx, 1);

	return 0;
}

static inline int s5p_mfc_run_enc_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb;
	struct s5p_mfc_buf *src_mb;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t src_addr[3] = { 0, 0, 0 };
	unsigned int index, i;
	int last_frame = 0;

	raw = &ctx->raw_buf;
	spin_lock_irqsave(&dev->irqlock, flags);

	if (list_empty(&ctx->src_queue)) {
		mfc_debug(2, "no src buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	if (list_empty(&ctx->dst_queue)) {
		mfc_debug(2, "no dst buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	src_mb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	src_mb->used = 1;

	if (src_mb->vb.v4l2_buf.reserved2 & FLAG_LAST_FRAME) {
		last_frame = 1;
		mfc_debug(2, "Setting ctx->state to FINISHING\n");
		s5p_mfc_change_state(ctx, MFCINST_FINISHING);
	}

	for (i = 0; i < raw->num_planes; i++) {
		src_addr[i] = src_mb->planes.raw[i];
		mfc_debug(2, "enc src[%d] addr: 0x%08llx\n", i, src_addr[i]);
	}
	if (src_mb->planes.raw[0] != s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, 0))
		mfc_err_ctx("enc src yaddr: 0x%08llx != vb2 yaddr: 0x%08llx\n",
				src_mb->planes.raw[i],
				s5p_mfc_mem_plane_addr(ctx, &src_mb->vb, i));

	index = src_mb->vb.v4l2_buf.index;

	/* encoder src buffer CFW PROT */
	if (ctx->is_drm) {
		if (!test_bit(index, &ctx->raw_protect_flag)) {
			if (s5p_mfc_raw_buf_prot(ctx, src_mb, true))
				mfc_err_ctx("failed to CFW_PROT\n");
			else
				set_bit(index, &ctx->raw_protect_flag);
		}
		mfc_debug(2, "[%d] enc src buf prot_flag: %#lx\n",
				index, ctx->raw_protect_flag);
	}

	s5p_mfc_set_enc_frame_buffer(ctx, &src_addr[0], raw->num_planes);

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	dst_mb->used = 1;

	/* encoder dst buffer CFW PROT */
	if (ctx->is_drm) {
		i = dst_mb->vb.v4l2_buf.index;
		if (!test_bit(i, &ctx->stream_protect_flag)) {
			if (s5p_mfc_stream_buf_prot(ctx, dst_mb, true))
				mfc_err_ctx("failed to CFW_PROT\n");
			else
				set_bit(i, &ctx->stream_protect_flag);
		}
		mfc_debug(2, "[%d] enc dst buf prot_flag: %#lx\n",
				i, ctx->stream_protect_flag);
	}
	mfc_debug(2, "nal start : src index from src_queue:%d\n",
		src_mb->vb.v4l2_buf.index);
	mfc_debug(2, "nal start : dst index from dst_queue:%d\n",
		dst_mb->vb.v4l2_buf.index);

	s5p_mfc_set_enc_stream_buffer(ctx, dst_mb);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	if (call_cop(ctx, set_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
		mfc_err_ctx("failed in set_buf_ctrls_val\n");

	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_encode_one_frame(ctx, last_frame);

	return 0;
}

static inline int s5p_mfc_run_init_dec(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	unsigned long flags;
	struct s5p_mfc_buf *temp_vb;
	struct s5p_mfc_dec *dec = NULL;

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
	/* Initializing decoding - parsing header */
	spin_lock_irqsave(&dev->irqlock, flags);

	if (list_empty(&ctx->src_queue)) {
		spin_unlock_irqrestore(&dev->irqlock, flags);
		mfc_err("no ctx src_queue to run\n");
		return -EAGAIN;
	}

	mfc_debug(2, "Preparing to init decoding.\n");
	temp_vb = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	mfc_debug(2, "Header size: %d, (offset: %d)\n",
		temp_vb->vb.v4l2_planes[0].bytesused, temp_vb->consumed);


	if (temp_vb->consumed) {
		s5p_mfc_set_dec_stream_buffer(ctx, temp_vb, temp_vb->consumed,
			temp_vb->vb.v4l2_planes[0].bytesused - temp_vb->consumed);
	} else {
		/* decoder src buffer CFW PROT */
		if (ctx->is_drm) {
			int index = temp_vb->vb.v4l2_buf.index;

			if (!test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, temp_vb, true))
					mfc_err_ctx("failed to CFW_PROT\n");
				else
					set_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] dec src buf prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}

		s5p_mfc_set_dec_stream_buffer(ctx, temp_vb,
			0, temp_vb->vb.v4l2_planes[0].bytesused);
	}

	spin_unlock_irqrestore(&dev->irqlock, flags);
	mfc_debug(2, "Header addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &temp_vb->vb, 0));
	s5p_mfc_clean_ctx_int_flags(ctx);
	s5p_mfc_init_decode(ctx);

	return 0;
}

static inline int s5p_mfc_run_init_enc(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	unsigned long flags;
	struct s5p_mfc_buf *dst_mb;
	int ret;

	spin_lock_irqsave(&dev->irqlock, flags);

	if (list_empty(&ctx->dst_queue)) {
		mfc_debug(2, "no dst buffers.\n");
		spin_unlock_irqrestore(&dev->irqlock, flags);
		return -EAGAIN;
	}

	dst_mb = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);
	/* encoder dst buffer CFW PROT */
	if (ctx->is_drm) {
		int index = dst_mb->vb.v4l2_buf.index;

		if (!test_bit(index, &ctx->stream_protect_flag)) {
			if (s5p_mfc_stream_buf_prot(ctx, dst_mb, true))
				mfc_err_ctx("failed to CFW_PROT\n");
			else
				set_bit(index, &ctx->stream_protect_flag);
		}
		mfc_debug(2, "[%d] enc dst buf prot_flag: %#lx\n",
				index, ctx->stream_protect_flag);
	}
	s5p_mfc_set_enc_stream_buffer(ctx, dst_mb);

	spin_unlock_irqrestore(&dev->irqlock, flags);

	s5p_mfc_set_stride_enc(ctx);

	mfc_debug(2, "Header addr: 0x%08lx\n",
		(unsigned long)s5p_mfc_mem_plane_addr(ctx, &dst_mb->vb, 0));
	s5p_mfc_clean_ctx_int_flags(ctx);

	ret = s5p_mfc_init_encode(ctx);
	return ret;
}

static inline int s5p_mfc_run_init_dec_buffers(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	int ret;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return -EINVAL;
	}
	dec = ctx->dec_priv;
	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}
	/* Initializing decoding - parsing header */
	/* Header was parsed now starting processing
	 * First set the output frame buffers
	 * s5p_mfc_alloc_dec_buffers(ctx); */

	if (!dec->is_dynamic_dpb && (ctx->capture_state != QUEUE_BUFS_MMAPED)) {
		mfc_err_ctx("It seems that not all destionation buffers were "
			"mmaped.\nMFC requires that all destination are mmaped "
			"before starting processing.\n");
		return -EAGAIN;
	}

	s5p_mfc_clean_ctx_int_flags(ctx);
	ret = s5p_mfc_set_dec_frame_buffer(ctx);
	if (ret) {
		mfc_err_ctx("Failed to alloc frame mem.\n");
		s5p_mfc_change_state(ctx, MFCINST_ERROR);
	}

	if (ctx->codec_mode == S5P_FIMV_CODEC_VP9_DEC)
		dec->is_packedpb = 1;

	return ret;
}

static inline int s5p_mfc_run_init_enc_buffers(struct s5p_mfc_ctx *ctx)
{
	int ret;

	ret = s5p_mfc_alloc_codec_buffers(ctx);
	if (ret) {
		mfc_err_ctx("Failed to allocate encoding buffers.\n");
		return -ENOMEM;
	}

	/* Header was generated now starting processing
	 * First set the reference frame buffers
	 */
	if (ctx->capture_state != QUEUE_BUFS_REQUESTED) {
		mfc_err_ctx("It seems that destionation buffers were not "
			"requested.\nMFC requires that header should be generated "
			"before allocating codec buffer.\n");
		return -EAGAIN;
	}

	s5p_mfc_clean_ctx_int_flags(ctx);
	ret = s5p_mfc_set_enc_ref_buffer(ctx);
	if (ret) {
		mfc_err_ctx("Failed to alloc frame mem.\n");
		s5p_mfc_change_state(ctx, MFCINST_ERROR);
	}
	return ret;
}

static inline int s5p_mfc_abort_inst(struct s5p_mfc_ctx *ctx)
{
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

	s5p_mfc_clean_ctx_int_flags(ctx);

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_NAL_ABORT, NULL);

	return 0;
}

static inline int s5p_mfc_dec_dpb_flush(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	if (on_res_change(ctx))
		mfc_err("dpb flush on res change(state:%d)\n", ctx->state);

	s5p_mfc_clean_ctx_int_flags(ctx);

	MFC_WRITEL(ctx->inst_no, S5P_FIMV_INSTANCE_ID);
	s5p_mfc_cmd_host2risc(dev, S5P_FIMV_H2R_CMD_FLUSH, NULL);

	return 0;
}

/* Try running an operation on hardware */
void s5p_mfc_try_run(struct s5p_mfc_dev *dev)
{
	struct s5p_mfc_ctx *ctx;
	int new_ctx;
	unsigned int ret = 0;
	int need_cache_flush = 0;

#ifdef NAL_Q_ENABLE
	nal_queue_handle *nal_q_handle = dev->nal_q_handle;
	EncoderInputStr *pInStr;
#endif
	mfc_debug(1, "Try run dev: %p\n", dev);
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	/* Check whether hardware is not running */
	if (dev->hw_lock != 0) {
		spin_unlock_irq(&dev->condlock);
		/* This is perfectly ok, the scheduled ctx should wait */
		mfc_debug(1, "Couldn't lock HW.\n");
		return;
	}

	/* Choose the context to run */
	new_ctx = s5p_mfc_get_new_ctx(dev);
	if (new_ctx < 0) {
		/* No contexts to run */
		spin_unlock_irq(&dev->condlock);
		mfc_debug(1, "No ctx is scheduled to be run.\n");
		return;
	}

	ctx = dev->ctx[new_ctx];
	if (!ctx) {
		spin_unlock_irq(&dev->condlock);
		mfc_err("no mfc context to run\n");
		return;
	}

	if (test_and_set_bit(ctx->num, &dev->hw_lock) != 0) {
		spin_unlock_irq(&dev->condlock);
		mfc_err_ctx("Failed to lock hardware.\n");
		return;
	}
	spin_unlock_irq(&dev->condlock);
	if (ctx->state == MFCINST_RUNNING)
		s5p_mfc_clean_ctx_int_flags(ctx);

	mfc_debug(1, "New context: %d\n", new_ctx);
	mfc_debug(1, "Seting new context to %p\n", ctx);
	dev->curr_ctx = ctx->num;

	/* Got context to run in ctx */
	mfc_debug(1, "ctx->dst_queue_cnt=%d ctx->dpb_count=%d ctx->src_queue_cnt=%d\n",
		ctx->dst_queue_cnt, ctx->dpb_count, ctx->src_queue_cnt);
	mfc_debug(1, "ctx->state=%d\n", ctx->state);
	/* Last frame has already been sent to MFC
	 * Now obtaining frames from MFC buffer */

	/* Check if cache flush command is needed */
	if (dev->curr_ctx_drm != ctx->is_drm)
		need_cache_flush = 1;
	else
		dev->curr_ctx_drm = ctx->is_drm;

	mfc_debug(2, "need_cache_flush = %d, is_drm = %d\n", need_cache_flush, ctx->is_drm);
#ifdef NAL_Q_ENABLE
	if (nal_q_handle && nal_q_handle->nal_q_state != NAL_Q_STATE_STARTED) {
#endif
		spin_lock_irq(&dev->condlock);
		if (!dev->has_job) {
			spin_unlock_irq(&dev->condlock);
			s5p_mfc_clock_on(dev);
		} else {
			dev->has_job = false;
			spin_unlock_irq(&dev->condlock);
		}
#ifdef NAL_Q_ENABLE
	}
#endif

	if (need_cache_flush) {
		if (FW_HAS_BASE_CHANGE(dev)) {
			s5p_mfc_clean_dev_int_flags(dev);

			s5p_mfc_cmd_host2risc(dev, S5P_FIMV_CH_CACHE_FLUSH, NULL);
			if (s5p_mfc_wait_for_done_dev(dev, S5P_FIMV_R2H_CMD_CACHE_FLUSH_RET)) {
				mfc_err_ctx("Failed to flush cache\n");
			}

			s5p_mfc_init_memctrl(dev, (ctx->is_drm ? MFCBUF_DRM : MFCBUF_NORMAL));
			s5p_mfc_clock_off(dev);

			dev->curr_ctx_drm = ctx->is_drm;
			s5p_mfc_clock_on(dev);
		} else {
			dev->curr_ctx_drm = ctx->is_drm;
		}
	}

#ifdef NAL_Q_ENABLE
	if (nal_q_handle) {
		switch (nal_q_handle->nal_q_state) {
		case NAL_Q_STATE_CREATED:
			s5p_mfc_nal_q_init(dev, nal_q_handle);
		case NAL_Q_STATE_INITIALIZED:
			if (s5p_mfc_nal_q_check_single_encoder(ctx) == 1 &&
					s5p_mfc_nal_q_check_last_frame(ctx) == 0) {
				/* enable NAL QUEUE */
				pInStr = s5p_mfc_nal_q_get_free_slot_in_q(dev,
						nal_q_handle->nal_q_in_handle);
				if (!pInStr) {
					mfc_err("NAL Q: there is no available input slot\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				if (s5p_mfc_nal_q_set_in_buf(ctx, pInStr)) {
					mfc_err("NAL Q: Failed to set input queue\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				if (s5p_mfc_nal_q_queue_in_buf(dev,
						nal_q_handle->nal_q_in_handle)) {
					mfc_err("NAL Q: Failed to increase input count\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				mfc_info_ctx("NAL Q: start NAL QUEUE\n");
				s5p_mfc_nal_q_start(dev, nal_q_handle);
				spin_lock_irq(&dev->condlock);
				clear_bit(nal_q_handle->nal_q_ctx, &dev->ctx_work_bits);
				clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
				spin_unlock_irq(&dev->condlock);
				if (test_bit(nal_q_handle->nal_q_ctx, &dev->ctx_work_bits))
					queue_work(dev->sched_wq, &dev->sched_work);
				return;
			} else {
				/* NAL START */
				break;
			}
		case NAL_Q_STATE_STARTED:
			if (s5p_mfc_nal_q_check_single_encoder(ctx) == 0 ||
					s5p_mfc_nal_q_check_last_frame(ctx) == 1) {
				/* disable NAL QUEUE */
				mfc_info_ctx("NAL Q: stop NAL QUEUE\n");
				s5p_mfc_clean_dev_int_flags(dev);
				s5p_mfc_nal_q_stop(dev, nal_q_handle);
				if (s5p_mfc_wait_for_done_dev(dev,
						S5P_FIMV_R2H_CMD_COMPLETE_QUEUE_RET)) {
					mfc_err("NAL Q: Failed to stop queue.\n");
					s5p_mfc_clean_dev_int_flags(dev);
				}
				s5p_mfc_nal_q_cleanup_queue(dev);
				s5p_mfc_nal_q_init(dev, nal_q_handle);
				break;
			} else {
				/* NAL QUEUE */
				pInStr = s5p_mfc_nal_q_get_free_slot_in_q(dev,
						nal_q_handle->nal_q_in_handle);
				if (!pInStr) {
					mfc_err("NAL Q: there is no available input slot\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				if (s5p_mfc_nal_q_set_in_buf(ctx, pInStr)) {
					mfc_err("NAL Q: Failed to set input queue\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				if (s5p_mfc_nal_q_queue_in_buf(dev, nal_q_handle->nal_q_in_handle)) {
					mfc_err("NAL Q: Failed to increase input count\n");
					spin_lock_irq(&dev->condlock);
					clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
					spin_unlock_irq(&dev->condlock);
					return;
				}
				spin_lock_irq(&dev->condlock);
				clear_bit(nal_q_handle->nal_q_ctx, &dev->ctx_work_bits);
				clear_hw_bit(dev->ctx[nal_q_handle->nal_q_ctx]);
				spin_unlock_irq(&dev->condlock);
				if (test_bit(nal_q_handle->nal_q_ctx, &dev->ctx_work_bits))
					queue_work(dev->sched_wq, &dev->sched_work);
				return;
			}
		default:
			mfc_err("NAL Q: Should not be here! nal_q state : %d\n",
					nal_q_handle->nal_q_state);
			return;
		}
	}
#endif

	if (ctx->type == MFCINST_DECODER) {
		switch (ctx->state) {
		case MFCINST_FINISHING:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RUNNING:
		case MFCINST_SPECIAL_PARSING_NAL:
			ret = s5p_mfc_run_dec_frame(ctx);
			break;
		case MFCINST_INIT:
			ret = s5p_mfc_open_inst(ctx);
			break;
		case MFCINST_RETURN_INST:
			ret = s5p_mfc_close_inst(ctx);
			break;
		case MFCINST_GOT_INST:
		case MFCINST_SPECIAL_PARSING:
			ret = s5p_mfc_run_init_dec(ctx);
			break;
		case MFCINST_HEAD_PARSED:
			ret = s5p_mfc_run_init_dec_buffers(ctx);
			break;
		case MFCINST_RES_CHANGE_INIT:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RES_CHANGE_FLUSH:
			ret = s5p_mfc_run_dec_last_frames(ctx);
			break;
		case MFCINST_RES_CHANGE_END:
			mfc_debug(2, "Finished remaining frames after resolution change.\n");
			ctx->capture_state = QUEUE_FREE;
			mfc_debug(2, "Will re-init the codec`.\n");
			ret = s5p_mfc_run_init_dec(ctx);
			break;
		case MFCINST_DPB_FLUSHING:
			ret = s5p_mfc_dec_dpb_flush(ctx);
			break;
		default:
			mfc_info_ctx("can't try command(decoder try_run), state : %d\n", ctx->state);
			ret = -EAGAIN;
		}
	} else if (ctx->type == MFCINST_ENCODER) {
		switch (ctx->state) {
		case MFCINST_FINISHING:
			ret = s5p_mfc_run_enc_last_frames(ctx);
			break;
		case MFCINST_RUNNING:
		case MFCINST_RUNNING_NO_OUTPUT:
			ret = s5p_mfc_run_enc_frame(ctx);
			break;
		case MFCINST_INIT:
			ret = s5p_mfc_open_inst(ctx);
			break;
		case MFCINST_RETURN_INST:
			ret = s5p_mfc_close_inst(ctx);
			break;
		case MFCINST_GOT_INST:
			ret = s5p_mfc_run_init_enc(ctx);
			break;
		case MFCINST_HEAD_PARSED: /* Only for MFC6.x */
			ret = s5p_mfc_run_init_enc_buffers(ctx);
			break;
		case MFCINST_ABORT_INST:
			ret = s5p_mfc_abort_inst(ctx);
			break;
		default:
			mfc_info_ctx("can't try command(encoder try_run), state : %d\n", ctx->state);
			ret = -EAGAIN;
		}
	} else {
		mfc_err_ctx("invalid context type: %d\n", ctx->type);
		ret = -EAGAIN;
	}

	if (ret) {
		/* Check again the ctx condition and clear work bits
		 * if ctx is not available. */
		if (s5p_mfc_ctx_ready(ctx) == 0) {
			spin_lock_irq(&dev->condlock);
			clear_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
		}
		dev->continue_ctx = MFC_NO_INSTANCE_SET;
		/* Free hardware lock */
		if (clear_hw_bit(ctx) == 0)
			mfc_err_ctx("Failed to unlock hardware.\n");

		s5p_mfc_clock_off(dev);

		/* Trigger again if other instance's work is waiting */
		spin_lock_irq(&dev->condlock);
		if (dev->ctx_work_bits)
			queue_work(dev->sched_wq, &dev->sched_work);
		spin_unlock_irq(&dev->condlock);
	}
}

void s5p_mfc_cleanup_timeout_and_try_run(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	s5p_mfc_cleanup_timeout(ctx);

	s5p_mfc_try_run(dev);
}

