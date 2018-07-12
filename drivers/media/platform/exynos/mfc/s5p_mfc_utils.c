/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_utils.c
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include "s5p_mfc_utils.h"
#include "s5p_mfc_mem.h"

#define COL_FRAME_RATE		0
#define COL_FRAME_INTERVAL	1

/*
 * A framerate table determines framerate by the interval(us) of each frame.
 * Framerate is not accurate, just rough value to seperate overload section.
 * Base line of each section are selected from 40fps(25000us), 80fps(12500us),
 * 145fps(6940us) and 205fps(4860us).
 *
 * interval(us) | 0         4860          6940          12500         25000          |
 * framerate    |    240fps   |    180fps   |    120fps   |    60fps    |    30fps   |
 */
static unsigned long framerate_table[][2] = {
	{ 30000, 25000 },
	{ 60000, 12500 },
	{ 120000, 6940 },
	{ 180000, 4860 },
	{ 240000, 0 },
};

int get_framerate_by_interval(int interval)
{
	unsigned long i;

	/* if the interval is too big (2sec), framerate set to 0 */
	if (interval > MFC_MAX_INTERVAL)
		return 0;

	for (i = 0; i < ARRAY_SIZE(framerate_table); i++) {
		if (interval > framerate_table[i][COL_FRAME_INTERVAL])
			return framerate_table[i][COL_FRAME_RATE];
	}

	return 0;
}

int get_framerate(struct timeval *to, struct timeval *from)
{
	unsigned long interval;

	if (timeval_compare(to, from) <= 0)
		return 0;

	interval = timeval_diff(to, from);

	return get_framerate_by_interval(interval);
}

void s5p_mfc_cleanup_queue(struct list_head *lh)
{
	struct s5p_mfc_buf *b;
	int i;

	while (!list_empty(lh)) {
		b = list_entry(lh->next, struct s5p_mfc_buf, list);
		for (i = 0; i < b->vb.num_planes; i++)
			vb2_set_plane_payload(&b->vb, i, 0);
		vb2_buffer_done(&b->vb, VB2_BUF_STATE_ERROR);
		list_del(&b->list);
	}
}

int check_vb_with_fmt(struct s5p_mfc_fmt *fmt, struct vb2_buffer *vb)
{
	struct vb2_queue *vq = vb->vb2_queue;
	struct s5p_mfc_ctx *ctx = vq->drv_priv;
	int i;

	if (!fmt)
		return -EINVAL;

	if (fmt->mem_planes != vb->num_planes) {
		mfc_err("plane number is different (%d != %d)\n",
				fmt->mem_planes, vb->num_planes);
		return -EINVAL;
	}

	for (i = 0; i < vb->num_planes; i++) {
		if (!s5p_mfc_mem_plane_addr(ctx, vb, i)) {
			mfc_err("failed to get plane cookie\n");
			return -ENOMEM;
		}

		mfc_debug(2, "index: %d, plane[%d] cookie: 0x%08llx\n",
				vb->v4l2_buf.index, i,
				s5p_mfc_mem_plane_addr(ctx, vb, i));
	}

	return 0;
}

int s5p_mfc_stream_buf_prot(struct s5p_mfc_ctx *ctx,
				struct s5p_mfc_buf *buf, bool en)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec = NULL;
	struct s5p_mfc_enc *enc = NULL;
	int ret = 0;
	void *cookie = NULL;
	phys_addr_t addr = 0;
	u32 cmd_id;

	dev = ctx->dev;
	if (!dev) {
		mfc_err_ctx("no mfc device to run\n");
		return ret;
	}

	if (en)
		cmd_id = SMC_DRM_SECBUF_CFW_PROT;
	else
		cmd_id = SMC_DRM_SECBUF_CFW_UNPROT;

	cookie = vb2_plane_cookie(&buf->vb, 0);
	addr = s5p_mfc_mem_phys_addr(cookie);

	if (ctx->type == MFCINST_DECODER) {
		dec = ctx->dec_priv;
		ret = exynos_smc(cmd_id, addr, dec->src_buf_size, dev->id);
		if (ret != DRMDRV_OK) {
			mfc_err_ctx("failed to CFW %sPROT (%#x)\n",
					en ? "" : "UN", ret);
			mfc_err_ctx("DEC src buf, 0x%08llx(0x%08llx) size:%ld\n",
					buf->planes.stream, addr, dec->src_buf_size);
		} else {
			mfc_debug(3, "succeeded to CFW %sPROT\n",
					en ? "" : "UN");
			mfc_debug(3, "DEC src buf, 0x%08llx(0x%08llx) size:%ld\n",
					buf->planes.stream, addr, dec->src_buf_size);
		}
	} else if (ctx->type == MFCINST_ENCODER) {
		enc = ctx->enc_priv;
		ret = exynos_smc(cmd_id, addr, enc->dst_buf_size, dev->id);
		if (ret != DRMDRV_OK) {
			mfc_err_ctx("failed to CFW %sPROT (%#x)\n",
					en ? "" : "UN", ret);
			mfc_err_ctx("ENC dst buf, 0x%08llx(0x%08llx) size:%ld\n",
					buf->planes.stream, addr, enc->dst_buf_size);
		} else {
			mfc_debug(3, "succeeded to CFW %sPROT\n",
					en ? "" : "UN");
			mfc_debug(3, "ENC dst buf, 0x%08llx(0x%08llx) size:%ld\n",
					buf->planes.stream, addr, enc->dst_buf_size);
		}
	}

	return ret;
}

int s5p_mfc_raw_buf_prot(struct s5p_mfc_ctx *ctx,
				struct s5p_mfc_buf *buf, bool en)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_raw_info *raw = &ctx->raw_buf;
	int num_planes = 0, i, ret = 0;
	void *cookie = NULL;
	phys_addr_t addr = 0;
	u32 cmd_id;

	dev = ctx->dev;
	if (!dev) {
		mfc_err_ctx("no mfc device to run\n");
		return -EINVAL;
	}

	if (ctx->type == MFCINST_DECODER)
		num_planes = ctx->dst_fmt->mem_planes;
	else if (ctx->type == MFCINST_ENCODER)
		num_planes = ctx->src_fmt->mem_planes;

	if (!num_planes) {
		mfc_err_ctx("there is no plane(%d)\n", num_planes);
		return -EINVAL;
	}

	if (en)
		cmd_id = SMC_DRM_SECBUF_CFW_PROT;
	else
		cmd_id = SMC_DRM_SECBUF_CFW_UNPROT;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(&buf->vb, i);
		addr = s5p_mfc_mem_phys_addr(cookie);
		if (num_planes == 1) {
			ret = exynos_smc(cmd_id, addr, raw->total_plane_size, dev->id);
			if (ret != DRMDRV_OK) {
				mfc_err_ctx("failed to CFW %sPROT (%#x)\n",
						en ? "" : "UN", ret);
				mfc_err_ctx("%s buf, addr:0x%08llx(0x%08llx) size:%d\n",
					ctx->type == MFCINST_DECODER ? "DEC dst" : "ENC src",
					buf->planes.raw[0], addr, raw->total_plane_size);
			} else {
				mfc_debug(3, "succeeded to CFW %sPROT\n",
						en ? "" : "UN");
				mfc_debug(3, "%s buf, addr:0x%08llx(0x%08llx) size:%d\n",
					ctx->type == MFCINST_DECODER ? "DEC dst" : "ENC src",
					buf->planes.raw[0], addr, raw->total_plane_size);
			}
		} else {
			ret = exynos_smc(cmd_id, addr, raw->plane_size[i], dev->id);
			if (ret != DRMDRV_OK) {
				mfc_err_ctx("failed to buf[%d] CFW %sPROT (%#x)\n",
						i, en ? "" : "UN", ret);
				mfc_err_ctx("%s buf, addr:0x%08llx(0x%08llx) size:%d\n",
					ctx->type == MFCINST_DECODER ? "DEC dst" : "ENC src",
					buf->planes.raw[i], addr, raw->plane_size[i]);
			} else {
				mfc_debug(3, "succeeded to buf[%d] CFW %sPROT\n",
						i, en ? "" : "UN");
				mfc_debug(3, "%s buf, addr:0x%08llx(0x%08llx) size:%d\n",
					ctx->type == MFCINST_DECODER ? "DEC dst" : "ENC src",
					buf->planes.raw[i], addr, raw->plane_size[i]);
			}
		}
	}

	return ret;
}
