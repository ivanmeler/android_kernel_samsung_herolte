/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot.h
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

#ifndef __EXYNOS_G2D1SHOT_H_
#define __EXYNOS_G2D1SHOT_H_

#include <linux/videodev2.h>
#include <media/m2m1shot2.h>

#include "g2d1shot_regs.h"

#define MODULE_NAME	"exynos-g2d"
#define NODE_NAME	"fimg2d"

#define	G2D_MAX_PLANES	1
#define	G2D_MAX_SOURCES	3

#define G2D_MAX_NORMAL_SIZE	16383
#define G2D_MAX_COMP_SIZE	8192

#define G2D_SHARABILITY_CONTROL

#if defined(G2D_SHARABILITY_CONTROL)
#define SYSREG_BLK_MSCL		0x151C0000
#define G2D_SHARABILITY_CTRL	0x700
#define	WLU_EN_SHIFT		4
#define	WLU_EN_MASK		0x1
#endif


/* flags for G2D supported format */
#define G2D_FMT_FLAG_SUPPORT_COMP	(1 << 0)
#define G2D_DATAFORMAT_RGB8888		(0 << 16)
#define G2D_DATAFORMAT_RGB565		(1 << 16)

struct g2d1shot_fmt {
	char	*name;
	u32	pixelformat;
	u8	bpp[G2D_MAX_PLANES];
	u8	num_planes;
	u8	flag;
	u32	src_value;
	u32	dst_value;
};

#define G2D_TIMEOUT_INTERVAL		4000	/* 4 sec */
#define G2D_STATE_RUNNING		(1 << 0)
#define G2D_STATE_SUSPENDING		(1 << 1)
#define G2D_STATE_TIMEOUT		(1 << 2)

struct g2d1shot_dev {
	struct m2m1shot2_device *oneshot2_dev;
	struct device *dev;
	struct clk *clock;
	void __iomem *reg;

	u32 version;
	unsigned long state;
	spinlock_t state_lock;
	struct timer_list timer;

	wait_queue_head_t suspend_wait;
	struct m2m1shot2_context *suspend_ctx;
	bool suspend_ctx_success;

	bool wlu_disable;
	void __iomem *syscon_reg;
};

struct g2d1shot_ctx {
	struct g2d1shot_dev *g2d_dev;
	u32 src_fmt_value[G2D_MAX_SOURCES];
	u32 dst_fmt_value;
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

#define MAX_G2D_BLIT_OP		(M2M1SHOT2_BLEND_LIGHTEN + 1)

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
enum g2d_coeff {
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

struct g2d_blend_coeff {
	bool s_coeff_inv;
	u8 s_coeff;
	bool d_coeff_inv;
	u8 d_coeff;
};

static inline void g2d_hw_init(struct g2d1shot_dev *g2d_dev)
{
	/* sfr clear */
	__raw_writel(G2D_SFR_CLEAR, g2d_dev->reg + G2D_SOFT_RESET_REG);
}

static inline void g2d_hw_stop(struct g2d1shot_dev *g2d_dev)
{
	/* clear irq */
	__raw_writel(G2D_BLIT_INT_FLAG, g2d_dev->reg + G2D_INTC_PEND_REG);
	/* disable irq */
	__raw_writel(0, g2d_dev->reg + G2D_INTEN_REG);
}

static inline u32 g2d_hw_read_version(struct g2d1shot_dev *g2d_dev)
{
	return __raw_readl(g2d_dev->reg + G2D_VERSION_INFO_REG);
}

static inline void g2d_hw_set_source_color(struct g2d1shot_dev *g2d_dev,
							int n, u32 color)
{
	/* set constant color */
	__raw_writel(color, g2d_dev->reg + G2D_LAYERn_COLOR_REG(n));
}

static inline void g2d_hw_set_source_type(struct g2d1shot_dev *g2d_dev,
							int n, u32 select)
{
	__raw_writel(select, g2d_dev->reg + G2D_LAYERn_SELECT_REG(n));
}

static inline void g2d_hw_set_dest_addr(struct g2d1shot_dev *g2d_dev,
						int plane, dma_addr_t addr)
{
	/* set dest address */
	__raw_writel(addr, g2d_dev->reg + G2D_DST_BASE_ADDR_REG);
}

static inline void g2d_hw_set_dither(struct g2d1shot_dev *g2d_dev)
{
	u32 cfg;

	/* set dithering */
	cfg = __raw_readl(g2d_dev->reg + G2D_BITBLT_COMMAND_REG);
	cfg |= G2D_ENABLE_DITHER;

	__raw_writel(cfg, g2d_dev->reg + G2D_BITBLT_COMMAND_REG);
}

#if defined(G2D_SHARABILITY_CONTROL)
static inline void g2d_wlu_disable(struct g2d1shot_dev *g2d_dev)
{
	u32 cfg;

	cfg = readl(g2d_dev->syscon_reg + G2D_SHARABILITY_CTRL);
	cfg &= ~(0x1 << WLU_EN_SHIFT);
	writel(cfg, g2d_dev->syscon_reg + G2D_SHARABILITY_CTRL);
}
#else
static inline void g2d_wlu_disable(struct g2d1shot_dev *g2d_dev)
{
	/* Dummy for compile error */
}
#endif

void g2d_hw_start(struct g2d1shot_dev *g2d_dev);
void g2d_hw_set_source_blending(struct g2d1shot_dev *g2d_dev, int n,
						struct m2m1shot2_extra *ext);
void g2d_hw_set_source_premult(struct g2d1shot_dev *g2d_dev, int n, u32 flags);
void g2d_hw_set_source_format(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_context_format *ctx_fmt, bool compressed);
void g2d_hw_set_source_address(struct g2d1shot_dev *g2d_dev, int n,
		int plane, bool compressed, dma_addr_t addr);
void g2d_hw_set_source_repeat(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext);
void g2d_hw_set_source_scale(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext, u32 flags,
		struct m2m1shot2_context_format *ctx_fmt);
void g2d_hw_set_source_rotate(struct g2d1shot_dev *g2d_dev, int n,
		struct m2m1shot2_extra *ext);
void g2d_hw_set_source_valid(struct g2d1shot_dev *g2d_dev, int n);
void g2d_hw_set_dest_format(struct g2d1shot_dev *g2d_dev,
		struct m2m1shot2_context_format *ctx_fmt);
void g2d_hw_set_dest_premult(struct g2d1shot_dev *g2d_dev, u32 flags);
#endif /* __EXYNOS_G2D1SHOT_H_ */
