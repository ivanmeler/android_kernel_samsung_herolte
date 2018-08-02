/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot_helper.c
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

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/platform_device.h>
#include <linux/of.h>

#include "g2d1shot.h"
#include "g2d1shot_helper.h"

struct regs_info {
	int start;
	int size;
	const char *name;
};

void g2d_dump_regs(struct g2d1shot_dev *g2d_dev)
{
	int i, count;
	struct regs_info info[] = {
		/* Start, Size, Name */
		{ 0x0,   0x20, "General 1" },
		{ 0xF0,  0x10, "General 2" },
		{ 0x100, 0x10, "Command" },
		{ 0x120, 0x30, "Destination" },
		{ 0x200, 0x80, "Layer0" },
		{ 0x300, 0x80, "Layer1" },
		{ 0x400, 0x80, "Layer2" },
	};

	count = ARRAY_SIZE(info);

	pr_err(">>> DUMP all regs (base = %p)\n", g2d_dev->reg);

	for (i = 0; i < count; i++) {
		pr_err("[%s: %04X .. %04X]\n",
			info[i].name, info[i].start,
			info[i].start + info[i].size);
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			g2d_dev->reg + info[i].start, info[i].size, false);
	}

	pr_err("DUMP finished <<<\n");
}

const char *op_string[] = {
	"NONE",
	"CLEAR",
	"SRC",
	"DST",
	"SRCOVER",
	"DSTOVER",
	"SRCIN	",
	"DSTIN	",
	"SRCOUT	",
	"DSTOUT	",
	"SRCATOP",
	"DSTATOP",
	"XOR",
	"PLUS",
	"MULTIPLY",
	"SCREEN",
	"DARKEN",
	"LIGHTEN",
};

const char *buf_type_string[] = {
	"NONE",
	"EMPTY",
	"DMABUF",
	"USERPTR",
};

static void _g2d_disp_format(struct m2m1shot2_context_format *ctx_fmt)
{
	const struct g2d1shot_fmt *g2d_fmt = ctx_fmt->priv;
	struct m2m1shot2_format *fmt = &ctx_fmt->fmt;

	pr_info("-- Format\n");
	pr_info("w x h : %d x %d, format : 0x%x (%s)\n",
		fmt->width, fmt->height, fmt->pixelformat, g2d_fmt->name);
	pr_info("crop : lt(%d, %d) rb(%d, %d)\n",
		fmt->crop.left, fmt->crop.top,
		fmt->crop.left + fmt->crop.width,
		fmt->crop.top + fmt->crop.height);
	pr_info("window : lt(%d, %d) rb(%d, %d)\n",
		fmt->window.left, fmt->window.top,
		fmt->window.left + fmt->window.width,
		fmt->window.top + fmt->window.height);
}

static void _g2d_disp_extra(struct m2m1shot2_extra *ext)
{
	pr_info("-- Extra setting\n");
	pr_info("composit mode : %d (%s), fillcolor(0x%x), transform(0x%x)\n",
			ext->composit_mode, op_string[ext->composit_mode],
			ext->fillcolor, ext->transform);
	pr_info("galpha r/g/b : %d/%d/%d, repeat x/y : %d/%d, scale : %d\n",
			ext->galpha_red, ext->galpha_green, ext->galpha_blue,
			ext->xrepeat, ext->yrepeat, ext->scaler_filter);
}

static void _g2d_disp_image(struct m2m1shot2_context_image *img)
{
	pr_info("-- Image\n");
	pr_info("buffer type : %d(%s), flags:0x%x\n",
			img->memory, buf_type_string[img->memory],
			img->flags);
}

static void _g2d_disp_source(struct m2m1shot2_context *ctx, int index)
{
	int i;

	_g2d_disp_image(&ctx->source[index].img);
	_g2d_disp_format(&ctx->source[index].img.fmt);
	_g2d_disp_extra(&ctx->source[index].ext);
	for (i = 0; i < ctx->source[index].img.num_planes; i++)
		pr_info("Addr[%d] : %#lx\n", i,
			(unsigned long)m2m1shot2_src_dma_addr(ctx, index, i));
}

static void _g2d_disp_target(struct m2m1shot2_context *ctx)
{
	int i;

	_g2d_disp_image(&ctx->target);
	_g2d_disp_format(&ctx->target.fmt);
	for (i = 0; i < ctx->target.num_planes; i++)
		pr_info("Addr[%d] : %#lx\n", i,
			(unsigned long)m2m1shot2_dst_dma_addr(ctx, i));
}

int g2d_disp_info(struct m2m1shot2_context *ctx)
{
	int i;

	pr_info(">>> Disp info (num of source : %d)\n", ctx->num_sources);

	for (i = 0; i < ctx->num_sources; i++) {
		pr_info("[ Source %d ]\n", i);
		_g2d_disp_source(ctx, i);
	}
	pr_info("[ Destination ]\n");
	_g2d_disp_target(ctx);

	pr_info("Flags:0x%x (Dither : %s, %s block mode) <<<\n\n",
		ctx->flags,
		(ctx->flags & M2M1SHOT2_FLAG_DITHER ? "enabled" : "disabled"),
		(ctx->flags & M2M1SHOT2_FLAG_NONBLOCK ? "Non" : ""));

	return 0;
}
