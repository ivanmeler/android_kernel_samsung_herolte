/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d4x_hw.c
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

#include "fimg2d.h"
#include "fimg2d5x.h"
#include "fimg2d_clk.h"

#define wr(d, a)	writel((d), ctrl->regs + (a))
#define rd(a)		readl(ctrl->regs + (a))

void fimg2d5x_init(struct fimg2d_control *ctrl)
{
	/* sfr clear */
	wr(FIMG2D_SFR_CLEAR, FIMG2D_SOFT_RESET_REG);
}

void fimg2d5x_reset(struct fimg2d_control *ctrl)
{
	wr(FIMG2D_SOFT_RESET, FIMG2D_SOFT_RESET_REG);
}

void fimg2d5x_enable_irq(struct fimg2d_control *ctrl)
{
	wr(FIMG2D_BLIT_INT_ENABLE, FIMG2D_INTEN_REG);
}

void fimg2d5x_disable_irq(struct fimg2d_control *ctrl)
{
	wr(0, FIMG2D_INTEN_REG);
}

void fimg2d5x_clear_irq(struct fimg2d_control *ctrl)
{
	wr(FIMG2D_BLIT_INT_FLAG, FIMG2D_INTC_PEND_REG);
}

int fimg2d5x_is_blit_done(struct fimg2d_control *ctrl)
{
	return rd(FIMG2D_INTC_PEND_REG) & FIMG2D_BLIT_INT_FLAG;
}

int fimg2d5x_blit_done_status(struct fimg2d_control *ctrl)
{
	volatile u32 sts;

	/* read twice */
	sts = rd(FIMG2D_FIFO_STAT_REG);
	sts = rd(FIMG2D_FIFO_STAT_REG);

	return (int)(sts & FIMG2D_BLIT_FINISHED);
}

void fimg2d5x_start_blit(struct fimg2d_control *ctrl)
{
	wr(FIMG2D_START_BITBLT, FIMG2D_BITBLT_START_REG);
}

void fimg2d5x_set_layer_valid(struct fimg2d_control *ctrl,
				struct fimg2d_image *s)
{
	u32 cfg;
	int n;

	n = s->layer_num;

	cfg = rd(FIMG2D_LAYERn_COMMAND_REG(n));

	cfg |= FIMG2D_LAYER_VALID;

	wr(cfg, FIMG2D_LAYERn_COMMAND_REG(n));
}

void fimg2d5x_layer_update(struct fimg2d_control *ctrl, u32 flag)
{
	wr(flag, FIMG2D_LAYER_UPDATE_REG);
}

void fimg2d5x_set_dst_depremult(struct fimg2d_control *ctrl)
{
	u32 cfg;

	cfg = rd(FIMG2D_BITBLT_COMMAND_REG);

	cfg |= FIMG2D_DST_DE_PREMULT;

	wr(cfg, FIMG2D_BITBLT_COMMAND_REG);
}

void fimg2d5x_set_src_type(struct fimg2d_control *ctrl,
			struct fimg2d_image *s, enum image_sel type)
{
	u32 cfg;
	int n;

	n = s->layer_num;

	if (type == IMG_MEMORY)
		cfg = FIMG2D_LAYER_SELECT_NORMAL;
	else if (type == IMG_CONSTANT_COLOR)
		cfg = FIMG2D_LAYER_SELECT_CONSTANT_COLOR;

	wr(cfg, FIMG2D_LAYERn_SELECT_REG(n));
}

/**
 * TODO: How to expand it with compatibility
 * src_order_table index should be matched with enum fmt_order.
 *
 * Each entry of structure means { Alpha, Red, Green, Blue }.
 * Each value means,
 * 0 : Channel 0 of input data
 * 1 : Channel 1 of input data
 * 2 : Channel 2 of input data
 * 3 : Channel 3 of input data
 * 4 : Zero
 * 5 : One
 *
 */
static struct fimg2d_channel_src src_order_table[] = {
	{ 3, 0, 1, 2 },		/* ARGB_8888 */
	{ 3, 2, 1, 0 },		/* BGRA_8888 */
	{ 0, 3, 2, 1 },		/* RGBA_8888 */
	{ 5, 2, 1, 0 },		/* RGB_565 */
	{ 9, 8, 7, 6 },		/* Constant ARGB */
	{ 0, 0, 0, 0 },		/* None */
};

#define set_src_channel(x)	\
	((((x)->alpha & 0xf) << FIMG2D_LAYER_CHANNEL_SEL_A_SHIFT) |	\
	 (((x)->red & 0xf) << FIMG2D_LAYER_CHANNEL_SEL_R_SHIFT)   |	\
	 (((x)->green & 0xf) << FIMG2D_LAYER_CHANNEL_SEL_G_SHIFT) |	\
	 (((x)->blue & 0xf) << FIMG2D_LAYER_CHANNEL_SEL_B_SHIFT))

static inline u32 fimg2d_src_color_mode(struct fimg2d_image *s)
{
	struct fimg2d_channel_src *channel;
	u32 cfg = 0;

	switch (s->fmt) {
	case CF_ARGB_8888:
		channel = &src_order_table[s->order];
		cfg |= FIMG2D_LAYER_DATA_FORMAT_8888;
		break;
	case CF_RGB_565:
		channel = &src_order_table[FMT_RGB_565];
		cfg |= FIMG2D_LAYER_DATA_FORMAT_565;
		break;
	case CF_COMP_RGB8888:
		channel = &src_order_table[s->order];
		cfg |= FIMG2D_LAYER_DATA_FORMAT_8888;
		cfg |= FIMG2D_LAYER_COMP_FORMAT;
		break;
	case CF_COMP_RGB565:
		channel = &src_order_table[s->order];
		cfg |= FIMG2D_LAYER_DATA_FORMAT_565;
		cfg |= FIMG2D_LAYER_COMP_FORMAT;
		break;
	default:
		channel = &src_order_table[FMT_NONE];
		break;
	}

	cfg |= set_src_channel(channel);

	return cfg;
}

void fimg2d5x_set_src_image(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_dma_group *dma)
{
	u32 cfg;
	int n;

	n = s->layer_num;

	switch (s->fmt) {
	case CF_COMP_RGB8888:
	case CF_COMP_RGB565:
		/* Header and payload are stored in the successive address */
		cfg = FIMG2D_ADDR(dma->base.iova);
		if (!IS_ALIGNED(cfg, FIMG2D_COMP_ALIGN_ADDR)) {
			fimg2d_info("COMP addr(%#x) is not aligned by %d\n",
					cfg, FIMG2D_COMP_ALIGN_ADDR);
			cfg = ALIGN(cfg, FIMG2D_COMP_ALIGN_ADDR);
			fimg2d_info("forcefully aligned, %#x\n", cfg);
		}
		wr(cfg, FIMG2D_LAYERn_PAYLOAD_BASE_ADDR_REG(n));
		wr(cfg, FIMG2D_LAYERn_HEADER_BASE_ADDR_REG(n));

		cfg = FIMG2D_COMP_WIDTH(s->width);
		wr(cfg, FIMG2D_LAYERn_COMP_IMAGE_WIDTH_REG(n));

		cfg = FIMG2D_COMP_HEIGHT(s->height);
		wr(cfg, FIMG2D_LAYERn_COMP_IMAGE_HEIGHT_REG(n));
		break;
	default:
		wr(FIMG2D_ADDR(dma->base.iova), FIMG2D_LAYERn_BASE_ADDR_REG(n));
		wr(FIMG2D_STRIDE(s->stride), FIMG2D_LAYERn_STRIDE_REG(n));
		break;
	}

	cfg = fimg2d_src_color_mode(s);

	wr(cfg, FIMG2D_LAYERn_COLOR_MODE_REG(n));
}

void fimg2d5x_set_src_rect(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, struct fimg2d_rect *r)
{
	int n = s->layer_num;

	wr(FIMG2D_OFFSET(r->x1), FIMG2D_LAYERn_LEFT_REG(n));
	wr(FIMG2D_OFFSET(r->y1), FIMG2D_LAYERn_TOP_REG(n));
	wr(FIMG2D_OFFSET(r->x2), FIMG2D_LAYERn_RIGHT_REG(n));
	wr(FIMG2D_OFFSET(r->y2), FIMG2D_LAYERn_BOTTOM_REG(n));
}

/**
 * TODO: How to expand it with compatibility
 * dst_order_table index should be matched with enum fmt_order.
 *
 * Each entry of structure means { 3rd Ch, 2nd Ch, 1st Ch, 0th Ch }.
 * Each value means,
 * 0 : Blue 8 bits of internal data
 * 1 : Green 8 bits of internal data
 * 2 : Red 8 bits of internal data
 * 3 : Alpha 8 bits of internal data
 * 4 : Zero
 * 5 : One
 *
 */
static struct fimg2d_channel_dst dst_order_table[] = {
	{ 3, 0, 1, 2 },		/* ARGB_8888 */
	{ 3, 2, 1, 0 },		/* BGRA_8888 */
	{ 2, 1, 0, 3 },		/* RGBA_8888 */
	{ 0, 2, 1, 0 },		/* RGB_565 : 3rd Ch has no meaning */
	{ 9, 8, 7, 6 },		/* Constant ARGB */
	{ 0, 0, 0, 0 },		/* None */
};

#define set_dst_channel(x)	\
		((((x)->ch3 & 0xf) << FIMG2D_DST_CHANNEL_SEL3_SHIFT) |	\
		 (((x)->ch2 & 0xf) << FIMG2D_DST_CHANNEL_SEL2_SHIFT) |	\
		 (((x)->ch1 & 0xf) << FIMG2D_DST_CHANNEL_SEL1_SHIFT) |	\
		 (((x)->ch0 & 0xf) << FIMG2D_DST_CHANNEL_SEL0_SHIFT))

static inline u32 fimg2d_dst_color_mode(struct fimg2d_image *d)
{
	struct fimg2d_channel_dst *channel;
	u32 cfg = 0;

	switch (d->fmt) {
	case CF_ARGB_8888:
		channel = &dst_order_table[d->order];
		cfg |= FIMG2D_DST_DATA_FORMAT_8888;
		break;
	case CF_RGB_565:
		channel = &dst_order_table[FMT_RGB_565];
		cfg |= FIMG2D_DST_DATA_FORMAT_565;
		break;
	default:
		channel = &dst_order_table[FMT_NONE];
		break;
	}

	cfg |= set_dst_channel(channel);

	return cfg;
}
/**
 * @d: set base address, stride, color format, order
 */
void fimg2d5x_set_dst_image(struct fimg2d_control *ctrl,
				struct fimg2d_image *d,
				struct fimg2d_dma_group *dma)
{
	u32 cfg;

	wr(FIMG2D_ADDR(dma->base.iova), FIMG2D_DST_BASE_ADDR_REG);
	wr(FIMG2D_STRIDE(d->stride), FIMG2D_DST_STRIDE_REG);

	cfg = fimg2d_dst_color_mode(d);

	wr(cfg, FIMG2D_DST_COLOR_MODE_REG);
}

void fimg2d5x_set_dst_rect(struct fimg2d_control *ctrl, struct fimg2d_rect *r)
{
	wr(FIMG2D_OFFSET(r->x1), FIMG2D_DST_LEFT_REG);
	wr(FIMG2D_OFFSET(r->y1), FIMG2D_DST_TOP_REG);
	wr(FIMG2D_OFFSET(r->x2), FIMG2D_DST_RIGHT_REG);
	wr(FIMG2D_OFFSET(r->y2), FIMG2D_DST_BOTTOM_REG);
}

/**
 * If solid color fill is enabled, other blit command is ignored.
 * Color format of solid color is considered to be
 *	the same as destination color format
 * Channel order of solid color is A-R-G-B or Y-Cb-Cr
 */
void fimg2d5x_set_color_fill(struct fimg2d_control *ctrl, u32 color)
{
	/* sf color */
	wr(color, FIMG2D_DST_COLOR_REG);
}

/**
 * set alpha-multiply mode for src, dst, pat read (pre-bitblt)
 * set alpha-demultiply for dst write (post-bitblt)
 */
void fimg2d5x_set_premultiplied(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, enum premultiplied p)
{
	u32 cfg;
	int n;

	n = s->layer_num;

	cfg = rd(FIMG2D_LAYERn_COMMAND_REG(n));

	if (p == PREMULTIPLIED)
		cfg &= ~FIMG2D_PREMULT_PER_PIXEL_MUL_GALPHA;
	else
		cfg |= FIMG2D_PREMULT_PER_PIXEL_MUL_GALPHA;

	wr(cfg, FIMG2D_LAYERn_COMMAND_REG(n));
}

/**
 * @c: destination clipping region
 */
void fimg2d5x_enable_clipping(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_clip *clp)
{
	int n;

	n = s->layer_num;

	wr(FIMG2D_OFFSET(clp->x1), FIMG2D_LAYERn_DST_LEFT_REG(n));
	wr(FIMG2D_OFFSET(clp->y1), FIMG2D_LAYERn_DST_TOP_REG(n));
	wr(FIMG2D_OFFSET(clp->x2), FIMG2D_LAYERn_DST_RIGHT_REG(n));
	wr(FIMG2D_OFFSET(clp->y2), FIMG2D_LAYERn_DST_BOTTOM_REG(n));
}

void fimg2d5x_enable_dithering(struct fimg2d_control *ctrl)
{
	u32 cfg;

	cfg = rd(FIMG2D_BITBLT_COMMAND_REG);
	cfg |= FIMG2D_ENABLE_DITHER;

	wr(cfg, FIMG2D_BITBLT_COMMAND_REG);
}

#define MAX_PRECISION		16
#define DEFAULT_SCALE_RATIO	0x10000

/**
 * scale_factor_to_fixed16 - convert scale factor to fixed pint 16
 * @n: numerator
 * @d: denominator
 */
static inline u32 scale_factor_to_fixed16(int n, int d)
{
	int i;
	u32 fixed16;

	if (!d)
		return DEFAULT_SCALE_RATIO;

	fixed16 = (n/d) << 16;
	n %= d;

	for (i = 0; i < MAX_PRECISION; i++) {
		if (!n)
			break;
		n <<= 1;
		if (n/d)
			fixed16 |= 1 << (15-i);
		n %= d;
	}

	return fixed16;
}

void fimg2d5x_set_src_scaling(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_scale *scl,
				struct fimg2d_repeat *rep)
{
	u32 wcfg, hcfg;
	u32 mode;
	int n = s->layer_num;

	/*
	 * scaling ratio in pixels
	 * e.g scale-up: src(1,1)-->dst(2,2), src factor: 0.5 (0x000080000)
	 *     scale-down: src(2,2)-->dst(1,1), src factor: 2.0 (0x000200000)
	 */

	/* inversed scaling factor: src is numerator */
	wcfg = scale_factor_to_fixed16(scl->src_w, scl->dst_w);
	hcfg = scale_factor_to_fixed16(scl->src_h, scl->dst_h);

	if (wcfg == DEFAULT_SCALE_RATIO && hcfg == DEFAULT_SCALE_RATIO)
		return;

	wr(wcfg, FIMG2D_LAYERn_XSCALE_REG(n));
	wr(hcfg, FIMG2D_LAYERn_YSCALE_REG(n));

	/* scaling algorithm */
	if (scl->mode == SCALING_BILINEAR)
		mode = FIMG2D_LAYER_SCALE_MODE_BILINEAR;
	else
		mode = 0x0;

	wr(mode, FIMG2D_LAYERn_SCALE_CTRL_REG(n));
}

void fimg2d5x_set_src_repeat(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_repeat *rep)
{
	u32 cfg;
	int n = s->layer_num;

	if (rep->mode == NO_REPEAT) {
		cfg = (FIMG2D_LAYER_REPEAT_X_NONE | FIMG2D_LAYER_REPEAT_Y_NONE);
		wr(cfg, FIMG2D_LAYERn_REPEAT_MODE_REG(n));
		return;
	}

	/* Repeat X and Y is same mode */
	cfg = (rep->mode - REPEAT_NORMAL);
	cfg |= (rep->mode - REPEAT_NORMAL) << FIMG2D_LAYER_REPEAT_Y_MASK;

	wr(cfg, FIMG2D_LAYERn_REPEAT_MODE_REG(n));

	/* src pad color */
	if (rep->mode == REPEAT_PAD)
		wr(rep->pad_color, FIMG2D_LAYERn_PAD_VALUE_REG(n));
}

void fimg2d5x_set_rotation(struct fimg2d_control *ctrl,
			struct fimg2d_image *s, enum rotation rot)
{
	u32 cfg;
	enum addressing dirx, diry;
	int n;

	n = s->layer_num;
	dirx = diry = FORWARD_ADDRESSING;

	switch (rot) {
	case ROT_180:
		dirx = REVERSE_ADDRESSING;
		diry = REVERSE_ADDRESSING;
		break;
	case XFLIP:
		diry = REVERSE_ADDRESSING;
		break;
	case YFLIP:
		dirx = REVERSE_ADDRESSING;
		break;
	case ORIGIN:
	default:
		break;
	}

	/* destination direction */
	if (dirx == REVERSE_ADDRESSING || diry == REVERSE_ADDRESSING) {
		cfg = rd(FIMG2D_LAYERn_DIRECT_REG(n));

		if (dirx == REVERSE_ADDRESSING)
			cfg |= FIMG2D_LAYER_X_DIR_NEGATIVE;

		if (diry == REVERSE_ADDRESSING)
			cfg |= FIMG2D_LAYER_Y_DIR_NEGATIVE;

		wr(cfg, FIMG2D_LAYERn_DIRECT_REG(n));
	} else {
		wr(0, FIMG2D_LAYERn_DIRECT_REG(n));
	}

	/* TODO: Consider destination is one of source.
	 * This case, direction should be same. */
}

void fimg2d5x_set_fgcolor(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, u32 fg)
{
	int n;
	struct fimg2d_channel_src *channel;
	u32 cfg = 0;

	n = s->layer_num;

	wr(fg, FIMG2D_LAYERn_COLOR_REG(n));

	channel = &src_order_table[FMT_CONSTANT_ARGB];
	cfg = set_src_channel(channel);

	wr(cfg, FIMG2D_LAYERn_COLOR_MODE_REG(n));
}

void fimg2d5x_enable_alpha(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, unsigned char g_alpha)
{
	unsigned int cfg;
	unsigned int alpha = g_alpha;
	int n;

	n = s->layer_num;

	/* enable alpha */
	/* Layer0 is skipped. */
	if (n != 0) {
		cfg = rd(FIMG2D_LAYERn_COMMAND_REG(n));
		cfg |= FIMG2D_ALPHA_BLEND_MODE;

		wr(cfg, FIMG2D_LAYERn_COMMAND_REG(n));
	}

	/*
	 * global(constant) alpha
	 * ex. if global alpha is 0x80, must set 0x80808080
	 */
	cfg = alpha;
	cfg |= alpha << 8;
	cfg |= alpha << 16;
	cfg |= alpha << 24;
	wr(cfg, FIMG2D_LAYERn_ALPHA_COLOR_REG(n));
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
 * DISJ_SRC_OVER:    R = Sc + (min(1,(1-Sa)/Da))*Dc
 * DISJ_DST_OVER:    R = (min(1,(1-Da)/Sa))*Sc + Dc
 * DISJ_SRC_IN:      R = (max(1-(1-Da)/Sa,0))*Sc
 * DISJ_DST_IN:      R = (max(1-(1-Sa)/Da,0))*Dc
 * DISJ_SRC_OUT:     R = (min(1,(1-Da)/Sa))*Sc
 * DISJ_DST_OUT:     R = (min(1,(1-Sa)/Da))*Dc
 * DISJ_SRC_ATOP:    R = (max(1-(1-Da)/Sa,0))*Sc + (min(1,(1-Sa)/Da))*Dc
 * DISJ_DST_ATOP:    R = (min(1,(1-Da)/Sa))*Sc + (max(1-(1-Sa)/Da,0))*Dc
 * DISJ_XOR:         R = (min(1,(1-Da)/Sa))*Sc + (min(1,(1-Sa)/Da))*Dc
 * CONJ_SRC_OVER:    R = Sc + (max(1-Sa/Da,0))*Dc
 * CONJ_DST_OVER:    R = (max(1-Da/Sa,0))*Sc + Dc
 * CONJ_SRC_IN:      R = (min(1,Da/Sa))*Sc
 * CONJ_DST_IN:      R = (min(1,Sa/Da))*Dc
 * CONJ_SRC_OUT:     R = (max(1-Da/Sa,0)*Sc
 * CONJ_DST_OUT:     R = (max(1-Sa/Da,0))*Dc
 * CONJ_SRC_ATOP:    R = (min(1,Da/Sa))*Sc + (max(1-Sa/Da,0))*Dc
 * CONJ_DST_ATOP:    R = (max(1-Da/Sa,0))*Sc + (min(1,Sa/Da))*Dc
 * CONJ_XOR:         R = (max(1-Da/Sa,0))*Sc + (max(1-Sa/Da,0))*Dc
 */
static struct fimg2d_blend_coeff const coeff_table[MAX_FIMG2D_BLIT_OP] = {
	{ 0, 0, 0, 0 },					/* No Operation */
	{ 0, 0, 0, 0 },					/* FILL */
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
	{ 0, 0, 0, 0 },		/* DARKEN */
	{ 0, 0, 0, 0 },		/* LIGHTEN */
	{ 0, COEFF_ONE,		0, COEFF_DISJ_S },	/* DISJ_SRC_OVER */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_DST_OVER */
	{ 1, COEFF_DISJ_D,	0, COEFF_ZERO },	/* DISJ_SRC_IN */
	{ 0, COEFF_ZERO,	1, COEFF_DISJ_S },	/* DISJ_DST_IN */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },		/* DISJ_SRC_OUT */
	{ 0, COEFF_ZERO,	0, COEFF_DISJ_S },	/* DISJ_DST_OUT */
	{ 1, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_SRC_ATOP */
	{ 0, COEFF_DISJ_D,	1, COEFF_DISJ_S },	/* DISJ_DST_ATOP */
	{ 0, COEFF_DISJ_D,	0, COEFF_DISJ_S },	/* DISJ_XOR */
	{ 0, COEFF_ONE,		1, COEFF_DISJ_S },	/* CONJ_SRC_OVER */
	{ 1, COEFF_DISJ_D,	0, COEFF_ONE },		/* CONJ_DST_OVER */
	{ 0, COEFF_CONJ_D,	0, COEFF_ONE },		/* CONJ_SRC_IN */
	{ 0, COEFF_ZERO,	0, COEFF_CONJ_S },	/* CONJ_DST_IN */
	{ 1, COEFF_CONJ_D,	0, COEFF_ZERO },	/* CONJ_SRC_OUT */
	{ 0, COEFF_ZERO,	1, COEFF_CONJ_S },	/* CONJ_DST_OUT */
	{ 0, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_SRC_ATOP */
	{ 1, COEFF_CONJ_D,	0, COEFF_CONJ_D },	/* CONJ_DST_ATOP */
	{ 1, COEFF_CONJ_D,	1, COEFF_CONJ_S },	/* CONJ_XOR */
	{ 0, 0, 0, 0 },		/* USER */
	{ 1, COEFF_GA,		1, COEFF_ZERO },	/* USER_SRC_GA */
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
 * DISJ_SRC_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_SRC_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_DST_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * DISJ_XOR:         R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_OVER:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_IN:      R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_OUT:     R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_SRC_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_DST_ATOP:    R = (W/A) 1st: Ga*Sc, 2nd: OP
 * CONJ_XOR:         R = (W/A) 1st: Ga*Sc, 2nd: OP
 */
static struct fimg2d_blend_coeff const ga_coeff_table[MAX_FIMG2D_BLIT_OP] = {
	{ 0, 0, 0, 0 },					/* No Operation */
	{ 0, 0, 0, 0 },					/* FILL */
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
	{ 0, 0, 0, 0 },		/* DARKEN (use W/A) */
	{ 0, 0, 0, 0 },		/* LIGHTEN (use W/A) */
	{ 0, COEFF_ONE,		0, COEFF_DISJ_S }, /* DISJ_SRC_OVER (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },	   /* DISJ_DST_OVER (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_ZERO },   /* DISJ_SRC_IN (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_DISJ_S }, /* DISJ_DST_IN (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_ONE },	   /* DISJ_SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	0, COEFF_DISJ_S }, /* DISJ_DST_OUT (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_DISJ_S }, /* DISJ_SRC_ATOP (use W/A) */
	{ 0, COEFF_DISJ_D,	1, COEFF_DISJ_S }, /* DISJ_DST_ATOP (use W/A) */
	{ 0, COEFF_DISJ_D,	0, COEFF_DISJ_S }, /* DISJ_XOR (use W/A) */
	{ 0, COEFF_ONE,		1, COEFF_DISJ_S }, /* CONJ_SRC_OVER (use W/A) */
	{ 1, COEFF_DISJ_D,	0, COEFF_ONE },	   /* CONJ_DST_OVER (use W/A) */
	{ 0, COEFF_CONJ_D,	0, COEFF_ONE },	   /* CONJ_SRC_IN (use W/A) */
	{ 0, COEFF_ZERO,	0, COEFF_CONJ_S }, /* CONJ_DST_IN (use W/A) */
	{ 1, COEFF_CONJ_D,	0, COEFF_ZERO },   /* CONJ_SRC_OUT (use W/A) */
	{ 0, COEFF_ZERO,	1, COEFF_CONJ_S }, /* CONJ_DST_OUT (use W/A) */
	{ 0, COEFF_CONJ_D,	1, COEFF_CONJ_S }, /* CONJ_SRC_ATOP (use W/A) */
	{ 1, COEFF_CONJ_D,	0, COEFF_CONJ_D }, /* CONJ_DST_ATOP (use W/A) */
	{ 1, COEFF_CONJ_D,	1, COEFF_CONJ_S }, /* CONJ_XOR (use W/A) */
	{ 0, 0, 0, 0 },		/* USER */
	{ 1, COEFF_GA,		1, COEFF_ZERO },	/* USER_SRC_GA */
};

void fimg2d5x_set_alpha_composite(struct fimg2d_control *ctrl,
		struct fimg2d_image *s, enum blit_op op, unsigned char g_alpha)
{
	int alphamode;
	u32 cfg = 0;
	struct fimg2d_blend_coeff const *tbl;
	int n;

	n = s->layer_num;

	switch (op) {
	case BLIT_OP_SOLID_FILL:
	case BLIT_OP_CLR:
		/* nop */
		return;
	case BLIT_OP_DARKEN:
		cfg |= FIMG2D_DARKEN;
		break;
	case BLIT_OP_LIGHTEN:
		cfg |= FIMG2D_LIGHTEN;
		break;
	case BLIT_OP_USER_COEFF:
		/* TODO */
		return;
	default:
		if (g_alpha < 0xff) {	/* with global alpha */
			tbl = &ga_coeff_table[op];
			alphamode = ALPHA_PERPIXEL_MUL_GLOBAL;
		} else {
			tbl = &coeff_table[op];
			alphamode = ALPHA_PERPIXEL;
		}

		/* src coefficient */
		cfg |= tbl->s_coeff << FIMG2D_SRC_COEFF_SHIFT;

		cfg |= alphamode << FIMG2D_SRC_COEFF_SA_SHIFT;
		cfg |= alphamode << FIMG2D_SRC_COEFF_DA_SHIFT;

		if (tbl->s_coeff_inv)
			cfg |= FIMG2D_INV_SRC_COEFF;

		/* dst coefficient */
		cfg |= tbl->d_coeff << FIMG2D_DST_COEFF_SHIFT;

		cfg |= alphamode << FIMG2D_DST_COEFF_DA_SHIFT;
		cfg |= alphamode << FIMG2D_DST_COEFF_SA_SHIFT;

		if (tbl->d_coeff_inv)
			cfg |= FIMG2D_INV_DST_COEFF;

		break;
	}

	/* Layer0 is skipped. */
	if (n != 0)
		wr(cfg, FIMG2D_LAYERn_BLEND_FUNCTION_REG(n));
}

struct regs_info {
	int start;
	int size;
	const char *name;
};

void fimg2d5x_dump_regs(struct fimg2d_control *ctrl)
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

	fimg2d_err(">>> DUMP all regs (base = %p)\n", ctrl->regs);

	for (i = 0; i < count; i++) {
		fimg2d_err("[%s: %04X .. %04X]\n",
			info[i].name, info[i].start,
			info[i].start + info[i].size);
		print_hex_dump(KERN_INFO, "", DUMP_PREFIX_ADDRESS, 32, 4,
			ctrl->regs + info[i].start, info[i].size, false);
	}

	fimg2d_err("DUMP finished <<<\n");
}
