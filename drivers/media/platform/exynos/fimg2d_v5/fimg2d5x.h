/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d5x.h
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

#ifndef __FIMG2D5X_H
#define __FIMG2D5X_H __FILE__

#include "fimg2d5x_regs.h"

/**
 * @IMG_MEMORY: read from external memory
 * @IMG_CONSTANT_COLOR: read from constant color
 */
enum image_sel {
	IMG_MEMORY,
	IMG_CONSTANT_COLOR,
};

/**
 * @FORWARD_ADDRESSING: read data in forward direction
 * @REVERSE_ADDRESSING: read data in reverse direction
 */
enum addressing {
	FORWARD_ADDRESSING,
	REVERSE_ADDRESSING,
};

/**
 * @ALPHA_PERPIXEL: perpixel alpha
 * @ALPHA_PERPIXEL_SUM_GLOBAL: perpixel + global
 * @ALPHA_PERPIXEL_MUL_GLOBAL: perpixel x global
 *
 * DO NOT CHANGE THIS ORDER
 */
enum alpha_opr {
	ALPHA_PERPIXEL = 0,	/* initial value */
	ALPHA_PERPIXEL_SUM_GLOBAL,
	ALPHA_PERPIXEL_MUL_GLOBAL,
};

#define DEFAULT_ALPHA_OPR	ALPHA_PERPIXEL

/**
 * @COEFF_ONE: 1
 * @COEFF_ZERO: 0
 * @COEFF_SA: src alpha
 * @COEFF_SC: src color
 * @COEFF_DA: dst alpha
 * @COEFF_DC: dst color
 * @COEFF_GA: global(constant) alpha
 * @COEFF_GC: global(constant) color
 * @COEFF_DISJ_S:
 * @COEFF_DISJ_D:
 * @COEFF_CONJ_S:
 * @COEFF_CONJ_D:
 *
 * DO NOT CHANGE THIS ORDER
 */
enum fimg2d_coeff {
	COEFF_ONE = 0,
	COEFF_ZERO,
	COEFF_SA,
	COEFF_SC,
	COEFF_DA,
	COEFF_DC,
	COEFF_GA,
	COEFF_GC,
	COEFF_DISJ_S,
	COEFF_DISJ_D,
	COEFF_CONJ_S,
	COEFF_CONJ_D,
};

/**
 * @PREMULT_ROUND_0: (A*B) >> 8
 * @PREMULT_ROUND_1: (A+1)*B) >> 8
 * @PREMULT_ROUND_2: (A+(A>>7))* B) >> 8
 * @PREMULT_ROUND_3: TMP= A*8 + 0x80, (TMP + (TMP >> 8)) >> 8
 *
 * DO NOT CHANGE THIS ORDER
 */
enum premult_round {
	PREMULT_ROUND_0 = 0,
	PREMULT_ROUND_1,
	PREMULT_ROUND_2,
	PREMULT_ROUND_3,	/* initial value */
};

#define DEFAULT_PREMULT_ROUND_MODE	PREMULT_ROUND_3

/**
 * @BLEND_ROUND_0: (A+1)*B) >> 8
 * @BLEND_ROUND_1: (A+(A>>7))* B) >> 8
 * @BLEND_ROUND_2: TMP= A*8 + 0x80, (TMP + (TMP >> 8)) >> 8
 * @BLEND_ROUND_3: TMP= (A*B + C*D + 0x80), (TMP + (TMP >> 8)) >> 8
 *
 * DO NOT CHANGE THIS ORDER
 */
enum blend_round {
	BLEND_ROUND_0 = 0,
	BLEND_ROUND_1,
	BLEND_ROUND_2,
	BLEND_ROUND_3,	/* initial value */
};

#define DEFAULT_BLEND_ROUND_MODE	BLEND_ROUND_3

struct fimg2d_blend_coeff {
	bool s_coeff_inv;
	enum fimg2d_coeff s_coeff;
	bool d_coeff_inv;
	enum fimg2d_coeff d_coeff;
};

struct fimg2d_channel_src {
	unsigned char alpha;
	unsigned char red;
	unsigned char green;
	unsigned char blue;
};

struct fimg2d_channel_dst {
	unsigned char ch3;
	unsigned char ch2;
	unsigned char ch1;
	unsigned char ch0;
};

void fimg2d5x_init(struct fimg2d_control *ctrl);
void fimg2d5x_reset(struct fimg2d_control *ctrl);
void fimg2d5x_enable_irq(struct fimg2d_control *ctrl);
void fimg2d5x_disable_irq(struct fimg2d_control *ctrl);
void fimg2d5x_clear_irq(struct fimg2d_control *ctrl);
int fimg2d5x_is_blit_done(struct fimg2d_control *ctrl);
int fimg2d5x_blit_done_status(struct fimg2d_control *ctrl);
void fimg2d5x_start_blit(struct fimg2d_control *ctrl);
void fimg2d5x_set_layer_valid(struct fimg2d_control *ctrl,
				struct fimg2d_image *s);
void fimg2d5x_layer_update(struct fimg2d_control *ctrl,
				u32 flag);
void fimg2d5x_set_dst_depremult(struct fimg2d_control *ctrl);
void fimg2d5x_set_src_type(struct fimg2d_control *ctrl, struct fimg2d_image *s,
				enum image_sel type);
void fimg2d5x_set_src_image(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_dma_group *dma);
void fimg2d5x_set_src_rect(struct fimg2d_control *ctrl, struct fimg2d_image *s,
				struct fimg2d_rect *r);
void fimg2d5x_set_dst_type(struct fimg2d_control *ctrl, enum image_sel type);
void fimg2d5x_set_dst_image(struct fimg2d_control *ctrl,
				struct fimg2d_image *d,
				struct fimg2d_dma_group *dma);
void fimg2d5x_set_dst_rect(struct fimg2d_control *ctrl, struct fimg2d_rect *r);
void fimg2d5x_set_color_fill(struct fimg2d_control *ctrl, u32 color);
void fimg2d5x_set_premultiplied(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, enum premultiplied p);
void fimg2d5x_enable_transparent(struct fimg2d_control *ctrl);
void fimg2d5x_enable_clipping(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_clip *clp);
void fimg2d5x_enable_dithering(struct fimg2d_control *ctrl);
void fimg2d5x_set_src_scaling(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_scale *scl,
				struct fimg2d_repeat *rep);
void fimg2d5x_set_src_repeat(struct fimg2d_control *ctrl,
				struct fimg2d_image *s,
				struct fimg2d_repeat *rep);
void fimg2d5x_set_rotation(struct fimg2d_control *ctrl, struct fimg2d_image *s,
				enum rotation rot);
void fimg2d5x_set_fgcolor(struct fimg2d_control *ctrl, struct fimg2d_image *s,
				u32 fg);
void fimg2d5x_enable_alpha(struct fimg2d_control *ctrl, struct fimg2d_image *s,
				unsigned char g_alpha);
void fimg2d5x_set_alpha_composite(struct fimg2d_control *ctrl,
				struct fimg2d_image *s, enum blit_op op,
				unsigned char g_alpha);
void fimg2d5x_dump_regs(struct fimg2d_control *ctrl);
#endif /* __FIMG2D5X_H__ */
