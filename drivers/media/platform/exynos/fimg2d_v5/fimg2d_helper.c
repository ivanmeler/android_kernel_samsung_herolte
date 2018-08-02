/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_helper.c
 *
 * Copyright (c) 2015 Samsung Electronics Co., Ltd.
 *	http://www.samsung.com/
 *
 * Samsung Graphics 2D driver
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include "fimg2d.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

static char *opname(enum blit_op op)
{
	switch (op) {
	case BLIT_OP_NONE:
		return "None";
	case BLIT_OP_SOLID_FILL:
		return "FILL";
	case BLIT_OP_CLR:
		return "CLR";
	case BLIT_OP_SRC:
		return "SRC";
	case BLIT_OP_DST:
		return "DST";
	case BLIT_OP_SRC_OVER:
		return "SRC_OVER";
	case BLIT_OP_DST_OVER:
		return "DST_OVER";
	case BLIT_OP_SRC_IN:
		return "SRC_IN";
	case BLIT_OP_DST_IN:
		return "DST_IN";
	case BLIT_OP_SRC_OUT:
		return "SRC_OUT";
	case BLIT_OP_DST_OUT:
		return "DST_OUT";
	case BLIT_OP_SRC_ATOP:
		return "SRC_ATOP";
	case BLIT_OP_DST_ATOP:
		return "DST_ATOP";
	case BLIT_OP_XOR:
		return "XOR";
	case BLIT_OP_ADD:
		return "ADD";
	case BLIT_OP_MULTIPLY:
		return "MULTIPLY";
	case BLIT_OP_SCREEN:
		return "SCREEN";
	case BLIT_OP_DARKEN:
		return "DARKEN";
	case BLIT_OP_LIGHTEN:
		return "LIGHTEN";
	default:
		return "CHECK";
	}
}

static char *cfname(enum color_format fmt)
{
	switch (fmt) {
	case CF_ARGB_8888:
		return "ARGB_8888";
	case CF_RGB_565:
		return "RGB_565";
	case CF_COMP_RGB8888:
		return "COMP_RGB8888";
	case CF_COMP_RGB565:
		return "COMP_RGB565";
	default:
		return "CHECK";
	}
}

static char *perfname(enum perf_desc id)
{
	switch (id) {
	case PERF_CACHE:
		return "CACHE";
	case PERF_SFR:
		return "SFR";
	case PERF_BLIT:
		return "BLT";
	case PERF_UNMAP:
		return "UNMAP";
	case PERF_TOTAL:
		return "TOTAL";
	default:
		return "";
	}
}

void fimg2d_debug_command(struct fimg2d_bltcmd *cmd)
{
	int i;
	struct fimg2d_blit *blt;
	struct fimg2d_image *img;
	struct fimg2d_param *p;
	struct fimg2d_rect *r;
	struct fimg2d_dma *c;

	if (WARN_ON(!cmd->ctx))
		return;

	blt = &cmd->blt;

	/* Common information */
	pr_info("\n[%s] ctx: %p seq_no(%u)\n", __func__,
				cmd->ctx, cmd->blt.seq_no);
	pr_info(" use fence: %d\n", blt->use_fence);
	pr_info(" Update layer : 0x%lx\n", cmd->src_flag);
	if (blt->dither)
		pr_info(" dither: %d\n", blt->dither);
	/* End of common information */

	/* Source information */
	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		p = &img->param;
		r = &img->rect;
		c = &cmd->dma_src[i].base;

		pr_info(" SRC[%d] op: %s(%d)\n",
				i, opname(img->op), img->op);
		pr_info(" SRC[%d] type: %d addr: 0x%lx\n",
				i, img->addr.type, img->addr.start);
		pr_info(" SRC[%d] width: %d height: %d ",
				i, img->width, img->height);
		pr_info(" SRC[%d] stride: %d order: %d format: %s(%d)\n",
				i, img->stride, img->order,
				cfname(img->fmt), img->fmt);
		pr_info(" SRC[%d] rect LT(%d,%d) RB(%d,%d) WH(%d,%d)\n",
				i, r->x1, r->y1, r->x2, r->y2,
				rect_w(r), rect_h(r));

		pr_info(" solid color: 0x%lx\n", p->solid_color);
		pr_info(" g_alpha: 0x%x\n", p->g_alpha);
		pr_info(" premultiplied: %d\n", p->premult);

		if (p->rotate)
			pr_info(" rotate: %d\n", p->rotate);
		if (p->repeat.mode) {
			pr_info(" repeat: %d, pad color: 0x%lx\n",
					p->repeat.mode, p->repeat.pad_color);
		}
		if (p->scaling.mode) {
			pr_info(" scaling %d, s:%d,%d d:%d,%d\n",
					p->scaling.mode,
					p->scaling.src_w, p->scaling.src_h,
					p->scaling.dst_w, p->scaling.dst_h);
		}
		if (p->clipping.enable) {
			pr_info(" clipping LT(%d,%d) RB(%d,%d) WH(%d,%d)\n",
					p->clipping.x1, p->clipping.y1,
					p->clipping.x2, p->clipping.y2,
					rect_w(&p->clipping),
					rect_h(&p->clipping));
		}

		if (c->size) {
			pr_info(" SRC[%d] dma base addr: %#lx size: %zd cached: %zd\n",
					i, c->addr, c->size, c->cached);
			pr_info(" SRC[%d] dma iova: %#lx, offset: %zd\n",
					i, (unsigned long)c->iova, c->offset);
		}

	}
	/* End of source information */

	/* Destination information */
	img = &cmd->image_dst;

	p = &img->param;
	r = &img->rect;
	c = &cmd->dma_dst.base;

	pr_info(" DST type: %d addr: 0x%lx\n",
			img->addr.type, img->addr.start);
	pr_info(" DST width: %d height: %d stride: %d order: %d format: %s(%d)\n",
			img->width, img->height, img->stride, img->order,
			cfname(img->fmt), img->fmt);
	pr_info(" DST rect LT(%d,%d) RB(%d,%d) WH(%d,%d)\n",
			r->x1, r->y1, r->x2, r->y2,
			rect_w(r), rect_h(r));

	pr_info(" solid color: 0x%lx\n", p->solid_color);
	pr_info(" g_alpha: 0x%x\n", p->g_alpha);
	pr_info(" premultiplied: %d\n", p->premult);

	if (c->size) {
		pr_info(" DST dma base addr: %#lx size: %zd cached: %zd\n",
				c->addr, c->size, c->cached);
		pr_info(" DST dma iova: %#lx offset: %zd\n",
				(unsigned long)c->iova, c->offset);
	}
	/* End of destination */

	if (cmd->dma_all)
		pr_info(" dma size all: %zd bytes\n", cmd->dma_all);
}

void fimg2d_debug_command_simple(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_clip *clp;
	struct fimg2d_scale *scl;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		scl = &img->param.scaling;
		clp = &img->param.clipping;

		pr_info("\n src[%d] whs(%d,%d,%d) rect(%d,%d,%d,%d) clip(%d,%d,%d,%d)\n",
				i, img->width, img->height, img->stride,
				img->rect.x1, img->rect.y1,
				img->rect.x2, img->rect.y2,
				clp->x1, clp->y1, clp->x2, clp->y2);
		if (scl)
			pr_info("scale wh(%d, %d) -> (%d, %d)\n",
				scl->src_w, scl->src_h,
				scl->dst_w, scl->dst_h);
	}

	img = &cmd->image_dst;

	pr_info("\n dst fs(%d,%d) rect(%d,%d,%d,%d)\n",
			img->width, img->height,
			img->rect.x1, img->rect.y1,
			img->rect.x2, img->rect.y2);
}

static long elapsed_usec(struct fimg2d_context *ctx, enum perf_desc desc)
{
	struct fimg2d_perf *perf = &ctx->perf[desc];
	struct timeval *start = &perf->start;
	struct timeval *end = &perf->end;
	long sec, usec;

	sec = end->tv_sec - start->tv_sec;
	if (end->tv_usec >= start->tv_usec) {
		usec = end->tv_usec - start->tv_usec;
	} else {
		usec = end->tv_usec + 1000000 - start->tv_usec;
		sec--;
	}
	return sec * 1000000 + usec;
}

void fimg2d_perf_start(struct fimg2d_bltcmd *cmd, enum perf_desc desc)
{
	struct fimg2d_perf *perf;
	struct timeval time;

	if (WARN_ON(!cmd->ctx))
		return;

	perf = &cmd->ctx->perf[desc];

	do_gettimeofday(&time);
	perf->start = time;
	perf->seq_no = cmd->blt.seq_no;
}

void fimg2d_perf_end(struct fimg2d_bltcmd *cmd, enum perf_desc desc)
{
	struct fimg2d_perf *perf;
	struct timeval time;

	if (WARN_ON(!cmd->ctx))
		return;

	perf = &cmd->ctx->perf[desc];

	do_gettimeofday(&time);
	perf->end = time;
	perf->seq_no = cmd->blt.seq_no;
}

void fimg2d_perf_print(struct fimg2d_bltcmd *cmd)
{
	int i;
	long time;
	struct fimg2d_perf *perf;

	if (WARN_ON(!cmd->ctx))
		return;

	for (i = 0; i < MAX_PERF_DESCS; i++) {
		perf = &cmd->ctx->perf[i];
		time = elapsed_usec(cmd->ctx, i);
		pr_info("[FIMG2D PERF (%8s)] ctx(%#lx) seq(%d) %8ld   usec\n",
				perfname(i), (unsigned long)cmd->ctx,
				perf->seq_no, time);
	}
	pr_info("[FIMG2D PERF ** seq(%d)]\n", cmd->blt.seq_no);
}
