/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot_hw5x.c
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

#include <linux/io.h>
#include <linux/sched.h>

#include "g2d1shot.h"
#include "g2d1shot_regs.h"

void g2d_hw_start(struct g2d1shot_dev *g2d_dev)
{
	/* start h/w */
	__raw_writel(G2D_BLIT_INT_ENABLE, g2d_dev->reg + G2D_INTEN_REG);
	__raw_writel(G2D_BLIT_INT_FLAG, g2d_dev->reg + G2D_INTC_PEND_REG);

	writel(G2D_START_BITBLT, g2d_dev->reg + G2D_BITBLT_START_REG);
}

/**
 * Four channels of the image are computed with:
 *	R = [ coeff(S)*Sc  + coeff(D)*Dc ]
 *	where
 *	Rc is result color or alpha
 *	Sc is source color or alpha
 *	Dc is destination color or alpha
 *
 * Caution: supposed that Sc and Dc are perpixel-alpha-premultiplied value
 *
 * MODE:             Formula
 * ----------------------------------------------------------------------------
 * FILL:
 * CLEAR:	     R = 0
 * SRC:		     R = Sc
 * DST:		     R = Dc
 * SRC_OVER:         R = Sc + (1-Sa)*Dc
 * DST_OVER:         R = (1-Da)*Sc + Dc
 * SRC_IN:	     R = Da*Sc
 * DST_IN:           R = Sa*Dc
 * SRC_OUT:          R = (1-Da)*Sc
 * DST_OUT:          R = (1-Sa)*Dc
 * SRC_ATOP:         R = Da*Sc + (1-Sa)*Dc
 * DST_ATOP:         R = (1-Da)*Sc + Sa*Dc
 * XOR:              R = (1-Da)*Sc + (1-Sa)*Dc
 * ADD:              R = Sc + Dc
 * MULTIPLY:         R = Sc*Dc
 * SCREEN:           R = Sc + (1-Sc)*Dc
 * DARKEN:           R = (Da*Sc<Sa*Dc)? Sc+(1-Sa)*Dc : (1-Da)*Sc+Dc
 * LIGHTEN:          R = (Da*Sc>Sa*Dc)? Sc+(1-Sa)*Dc : (1-Da)*Sc+Dc
 */
static struct g2d_blend_coeff const coeff_table[MAX_G2D_BLIT_OP] = {
	{ 0, COEFF_SA,		1, COEFF_SA },		/* None */
	{ 0, COEFF_ZERO,	0, COEFF_ZERO },	/* CLEAR */
	{ 0, COEFF_ONE,		0, COEFF_ZERO },	/* SRC */
	{ 0, COEFF_ZERO,	0, COEFF_ONE },		/* DST */
	{ 0, COEFF_ONE,		1, COEFF_SA },		/* SRC_OVER */
	{ 1, COEFF_DA,		0, COEFF_ONE },		/* DST_OVER */
	{ 0, COEFF_DA,		0, COEFF_ZERO },	/* SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_SA },		/* DST_IN */
	{ 1, COEFF_DA,		0, COEFF_ZERO },	/* SRC_OUT */
	{ 0, COEFF_ZERO,	1, COEFF_SA },		/* DST_OUT */
	{ 0, COEFF_DA,		1, COEFF_SA },		/* SRC_ATOP */
	{ 1, COEFF_DA,		0, COEFF_SA },		/* DST_ATOP */
	{ 1, COEFF_DA,		1, COEFF_SA },		/* XOR */
	{ 0, COEFF_ONE,		0, COEFF_ONE },		/* ADD */
	{ 0, COEFF_DC,		0, COEFF_ZERO },	/* MULTIPLY */
	{ 0, COEFF_ONE,		1, COEFF_SC },		/* SCREEN */
	{ 0, 0, 0, 0 },					/* DARKEN */
	{ 0, 0, 0, 0 },					/* LIGHTEN */
};

/*
 * coefficient table with global (constant) alpha
 * replace COEFF_ONE with COEFF_GA
 *
 * MODE:             Formula with Global Alpha
 *                   (Ga is multiplied to both Sc and Sa)
 * ----------------------------------------------------------------------------
 * FILL:
 * CLEAR:	     R = 0
 * SRC:		     R = Ga*Sc
 * DST:		     R = Dc
 * SRC_OVER:         R = Ga*Sc + (1-Sa*Ga)*Dc
 * DST_OVER:         R = (1-Da)*Ga*Sc + Dc --> (W/A) 1st:Ga*Sc, 2nd:DST_OVER
 * SRC_IN:	     R = Da*Ga*Sc
 * DST_IN:           R = Sa*Ga*Dc
 * SRC_OUT:          R = (1-Da)*Ga*Sc --> (W/A) 1st: Ga*Sc, 2nd:SRC_OUT
 * DST_OUT:          R = (1-Sa*Ga)*Dc
 * SRC_ATOP:         R = Da*Ga*Sc + (1-Sa*Ga)*Dc
 * DST_ATOP:         R = (1-Da)*Ga*Sc + Sa*Ga*Dc -->
 *					(W/A) 1st: Ga*Sc, 2nd:DST_ATOP
 * XOR:              R = (1-Da)*Ga*Sc + (1-Sa*Ga)*Dc -->
 *					(W/A) 1st: Ga*Sc, 2nd:XOR
 * ADD:              R = Ga*Sc + Dc
 * MULTIPLY:         R = Ga*Sc*Dc --> (W/A) 1st: Ga*Sc, 2nd: MULTIPLY
 * SCREEN:           R = Ga*Sc + (1-Ga*Sc)*Dc --> (W/A) 1st: Ga*Sc, 2nd: SCREEN
 * DARKEN:           R = (W/A) 1st: Ga*Sc, 2nd: OP
 * LIGHTEN:          R = (W/A) 1st: Ga*Sc, 2nd: OP
 */
static struct g2d_blend_coeff const ga_coeff_table[MAX_G2D_BLIT_OP] = {
	{ 0, COEFF_SA,		1, COEFF_SA },		/* None */
	{ 0, COEFF_ZERO,	0, COEFF_ZERO },	/* CLEAR */
	{ 0, COEFF_GA,		0, COEFF_ZERO },	/* SRC */
	{ 0, COEFF_ZERO,	0, COEFF_ONE },		/* DST */
	{ 0, COEFF_GA,		1, COEFF_SA },		/* SRC_OVER */
	{ 1, COEFF_DA,		0, COEFF_ONE },		/* DST_OVER (use W/A) */
	{ 0, COEFF_DA,		0, COEFF_ZERO },	/* SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_SA },		/* DST_IN */
	{ 1, COEFF_DA,		0, COEFF_ZERO },	/* SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_SA },		/* DST_OUT */
	{ 0, COEFF_DA,		1, COEFF_SA },		/* SRC_ATOP */
	{ 1, COEFF_DA,		0, COEFF_SA },		/* DST_ATOP (use W/A) */
	{ 1, COEFF_DA,		1, COEFF_SA },		/* XOR (use W/A) */
	{ 0, COEFF_GA,		0, COEFF_ONE },		/* ADD */
	{ 0, COEFF_DC,		0, COEFF_ZERO },	/* MULTIPLY (use W/A) */
	{ 0, COEFF_ONE,		1, COEFF_SC },		/* SCREEN (use W/A) */
	{ 0, 0,	0, 0 },					/* DARKEN (use W/A) */
	{ 0, 0,	0, 0 },					/* LIGHTEN (use W/A) */
};

void g2d_hw_set_alpha_composite(struct g2d1shot_dev *g2d_dev,
		int n, u16 composit_mode, unsigned char galpha)
{
	int alphamode;
	u32 cfg = 0;
	struct g2d_blend_coeff const *tbl;

	switch (composit_mode) {
	case M2M1SHOT2_BLEND_DARKEN:
		cfg |= G2D_DARKEN;
		break;
	case M2M1SHOT2_BLEND_LIGHTEN:
		cfg |= G2D_LIGHTEN;
		break;
	default:
		if (galpha < 0xff) {	/* with global alpha */
			tbl = &ga_coeff_table[composit_mode];
			alphamode = ALPHA_PERPIXEL_MUL_GLOBAL;
		} else {
			tbl = &coeff_table[composit_mode];
			alphamode = ALPHA_PERPIXEL;
		}

		/* src coefficient */
		cfg |= tbl->s_coeff << G2D_SRC_COEFF_SHIFT;

		cfg |= alphamode << G2D_SRC_COEFF_SA_SHIFT;
		cfg |= alphamode << G2D_SRC_COEFF_DA_SHIFT;

		if (tbl->s_coeff_inv)
			cfg |= G2D_INV_SRC_COEFF;

		/* dst coefficient */
		cfg |= tbl->d_coeff << G2D_DST_COEFF_SHIFT;

		cfg |= alphamode << G2D_DST_COEFF_DA_SHIFT;
		cfg |= alphamode << G2D_DST_COEFF_SA_SHIFT;

		if (tbl->d_coeff_inv)
			cfg |= G2D_INV_DST_COEFF;

		break;
	}

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_BLEND_FUNCTION_REG(n));

	cfg = __raw_readl(g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));
	cfg |= G2D_ALPHA_BLEND_MODE;
	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));
}

void g2d_hw_set_source_blending(struct g2d1shot_dev *g2d_dev,
				int n, struct m2m1shot2_extra *ext)
{
	u32 cfg;

	cfg = ext->galpha;
	cfg |= ext->galpha << 8;
	cfg |= ext->galpha << 16;
	cfg |= ext->galpha << 24;

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_ALPHA_COLOR_REG(n));

	/* No more operations are needed for layer 0 */
	if (n == 0)
		return;

	g2d_hw_set_alpha_composite(g2d_dev, n, ext->composit_mode, ext->galpha);
}

void g2d_hw_set_source_premult(struct g2d1shot_dev *g2d_dev, int n, u32 flags)
{
	u32 cfg;

	/* set source premult */
	cfg = __raw_readl(g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));

	if (flags & M2M1SHOT2_IMGFLAG_PREMUL_ALPHA)
		cfg &= ~G2D_PREMULT_PER_PIXEL_MUL_GALPHA;
	else
		cfg |= G2D_PREMULT_PER_PIXEL_MUL_GALPHA;

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));
}

void g2d_hw_set_source_format(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_context_format *ctx_fmt, bool compressed)
{
	struct v4l2_rect *s = &ctx_fmt->fmt.crop;
	struct v4l2_rect *d = &ctx_fmt->fmt.window;
	struct g2d1shot_fmt *fmt = ctx_fmt->priv;
	u32 cfg;

	/* set source rect */
	__raw_writel(s->left, g2d_dev->reg + G2D_LAYERn_LEFT_REG(n));
	__raw_writel(s->top, g2d_dev->reg + G2D_LAYERn_TOP_REG(n));
	__raw_writel(s->left + s->width,
			g2d_dev->reg + G2D_LAYERn_RIGHT_REG(n));
	__raw_writel(s->top + s->height,
			g2d_dev->reg + G2D_LAYERn_BOTTOM_REG(n));

	/* set dest clip */
	__raw_writel(d->left, g2d_dev->reg + G2D_LAYERn_DST_LEFT_REG(n));
	__raw_writel(d->top, g2d_dev->reg + G2D_LAYERn_DST_TOP_REG(n));
	__raw_writel(d->left + d->width,
			g2d_dev->reg + G2D_LAYERn_DST_RIGHT_REG(n));
	__raw_writel(d->top + d->height,
			g2d_dev->reg + G2D_LAYERn_DST_BOTTOM_REG(n));

	/* set pixel format */
	cfg = fmt->src_value;

	if (compressed)
		cfg |= G2D_LAYER_COMP_FORMAT;
	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_COLOR_MODE_REG(n));

	if (compressed) {
		cfg = ctx_fmt->fmt.width;
		if (!IS_ALIGNED(cfg, G2D_COMP_ALIGN_WIDTH))
			cfg = ALIGN(cfg, G2D_COMP_ALIGN_WIDTH);
		cfg = G2D_COMP_SET_WH(cfg);
		__raw_writel(cfg,
			g2d_dev->reg + G2D_LAYERn_COMP_IMAGE_WIDTH_REG(n));

		cfg = ctx_fmt->fmt.height;
		if (!IS_ALIGNED(cfg, G2D_COMP_ALIGN_HEIGHT))
			cfg = ALIGN(cfg, G2D_COMP_ALIGN_HEIGHT);
		cfg = G2D_COMP_SET_WH(cfg);
		__raw_writel(cfg,
			g2d_dev->reg + G2D_LAYERn_COMP_IMAGE_HEIGHT_REG(n));
	} else {
		cfg = (fmt->bpp[0] * ctx_fmt->fmt.width / 8);
		__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_STRIDE_REG(n));
	}
}

void g2d_hw_set_source_address(struct g2d1shot_dev *g2d_dev, int n,
		int plane, bool compressed, dma_addr_t addr)
{
	if (compressed) {
		__raw_writel(addr,
			g2d_dev->reg + G2D_LAYERn_PAYLOAD_BASE_ADDR_REG(n));
		__raw_writel(addr,
			g2d_dev->reg + G2D_LAYERn_HEADER_BASE_ADDR_REG(n));
	} else {
		__raw_writel(addr, g2d_dev->reg + G2D_LAYERn_BASE_ADDR_REG(n));
	}

}

void g2d_hw_set_source_repeat(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext)
{
	u32 cfg = 0;

	/* set repeat, or default NONE */
	if (ext->xrepeat)
		cfg |= G2D_LAYER_REPEAT_X_SET(ext->xrepeat - 1);
	else if (ext->xrepeat == M2M1SHOT2_REPEAT_NONE)
		cfg |= G2D_LAYER_REPEAT_X_NONE;

	if (ext->yrepeat)
		cfg |= G2D_LAYER_REPEAT_Y_SET(ext->yrepeat - 1);
	else if (ext->yrepeat == M2M1SHOT2_REPEAT_NONE)
		cfg |= G2D_LAYER_REPEAT_Y_NONE;

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_REPEAT_MODE_REG(n));

	/* repeat pad color */
	if (ext->xrepeat == M2M1SHOT2_REPEAT_PAD ||
			ext->yrepeat == M2M1SHOT2_REPEAT_PAD)
		__raw_writel(ext->fillcolor,
				g2d_dev->reg + G2D_LAYERn_PAD_VALUE_REG(n));
}

#define MAX_PRECISION		16
#define DEFAULT_SCALE_RATIO	0x10000

/**
 * scale_factor_to_fixed16 - convert scale factor to fixed point 16
 * @n: numerator
 * @d: denominator
 */
static inline u32 scale_factor_to_fixed16(u32 n, u32 d)
{
	u64 value;

	value = (u64)n << MAX_PRECISION;
	value /= d;

	return (u32)(value & 0xffffffff);
}

void g2d_hw_set_source_scale(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext, u32 flags,
		struct m2m1shot2_context_format *ctx_fmt)
{
	struct v4l2_rect *s = &ctx_fmt->fmt.crop;
	struct v4l2_rect *d = &ctx_fmt->fmt.window;
	u32 wcfg, hcfg;
	u32 mode;

	/* set scaling ratio, default NONE */
	/*
	 * scaling ratio in pixels
	 * e.g scale-up: src(1,1)-->dst(2,2), src factor: 0.5 (0x000080000)
	 *     scale-down: src(2,2)-->dst(1,1), src factor: 2.0 (0x000200000)
	 */

	/* inversed scaling factor: src is numerator */

	if (flags & M2M1SHOT2_IMGFLAG_XSCALE_FACTOR)
		wcfg = ext->horizontal_factor;
	else
		wcfg = scale_factor_to_fixed16(s->width, d->width);
	if (flags & M2M1SHOT2_IMGFLAG_YSCALE_FACTOR)
		hcfg = ext->vertical_factor;
	else
		hcfg = scale_factor_to_fixed16(s->height, d->height);

	if (wcfg == DEFAULT_SCALE_RATIO && hcfg == DEFAULT_SCALE_RATIO)
		return;

	__raw_writel(wcfg, g2d_dev->reg + G2D_LAYERn_XSCALE_REG(n));
	__raw_writel(hcfg, g2d_dev->reg + G2D_LAYERn_YSCALE_REG(n));

	/* scaling algorithm */
	if (ext->scaler_filter == M2M1SHOT2_SCFILTER_BILINEAR)
		mode = G2D_LAYER_SCALE_MODE_BILINEAR;
	else
		mode = 0x0;

	__raw_writel(mode, g2d_dev->reg + G2D_LAYERn_SCALE_CTRL_REG(n));
}

void g2d_hw_set_source_rotate(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext)
{
	u32 cfg = 0;

	if (ext->transform & M2M1SHOT2_IMGTRFORM_XFLIP)
		cfg |= G2D_LAYER_X_DIR_NEGATIVE;
	if (ext->transform & M2M1SHOT2_IMGTRFORM_YFLIP)
		cfg |= G2D_LAYER_Y_DIR_NEGATIVE;

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_DIRECT_REG(n));
}

void g2d_hw_set_source_valid(struct g2d1shot_dev *g2d_dev, int n)
{
	u32 cfg;

	/* set valid flag */
	cfg = __raw_readl(g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));
	cfg |= G2D_LAYER_VALID;

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYERn_COMMAND_REG(n));

	/* set update layer flag */
	cfg = __raw_readl(g2d_dev->reg + G2D_LAYER_UPDATE_REG);
	cfg |= (1 << n);

	__raw_writel(cfg, g2d_dev->reg + G2D_LAYER_UPDATE_REG);
}

void g2d_hw_set_dest_format(struct g2d1shot_dev *g2d_dev,
				struct m2m1shot2_context_format *ctx_fmt)
{
	struct g2d1shot_fmt *fmt = ctx_fmt->priv;
	struct v4l2_rect *d = &ctx_fmt->fmt.crop;
	u32 cfg;

	/* set dest rect */
	__raw_writel(d->left, g2d_dev->reg + G2D_DST_LEFT_REG);
	__raw_writel(d->top, g2d_dev->reg + G2D_DST_TOP_REG);
	__raw_writel(d->left + d->width, g2d_dev->reg + G2D_DST_RIGHT_REG);
	__raw_writel(d->top + d->height, g2d_dev->reg + G2D_DST_BOTTOM_REG);

	/* set dest stride */
	cfg = (fmt->bpp[0] * ctx_fmt->fmt.width / 8);
	__raw_writel(cfg, g2d_dev->reg + G2D_DST_STRIDE_REG);

	/* set dest pixelformat */
	cfg = fmt->dst_value;
	__raw_writel(cfg, g2d_dev->reg + G2D_DST_COLOR_MODE_REG);
}

void g2d_hw_set_dest_premult(struct g2d1shot_dev *g2d_dev, u32 flags)
{
	u32 cfg;

	/* set dest premult if flag is set */
	cfg = __raw_readl(g2d_dev->reg + G2D_BITBLT_COMMAND_REG);

	if (!(flags & M2M1SHOT2_IMGFLAG_PREMUL_ALPHA))
		cfg |= G2D_DST_DE_PREMULT;

	__raw_writel(cfg, g2d_dev->reg + G2D_BITBLT_COMMAND_REG);
}
