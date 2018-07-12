/* linux/drivers/media/platform/exynos/fimg2d_v5/g2d1shot_drv.c
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
#include <linux/interrupt.h>
#include <linux/clk.h>
#include <linux/pm_runtime.h>
#include <linux/slab.h>
#include <linux/mutex.h>
#include <linux/exynos_iovmm.h>
#include <linux/smc.h>

#include "g2d1shot.h"
#include "g2d1shot_helper.h"

int g2d_debug = DBG_INFO;
module_param(g2d_debug, int, S_IRUGO | S_IWUSR);

int g2d_dump;
module_param(g2d_dump, int, S_IRUGO | S_IWUSR);

static int m2m1shot2_g2d_init_context(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(ctx->m21dev->dev);
	struct g2d1shot_ctx *g2d_ctx;
	int ret;

	g2d_dbg_begin();

	g2d_ctx = kzalloc(sizeof(*g2d_ctx), GFP_KERNEL);
	if (!g2d_ctx)
		return -ENOMEM;

	ctx->priv = g2d_ctx;

	g2d_ctx->g2d_dev = g2d_dev;

	ret = clk_prepare(g2d_dev->clock);
	if (ret) {
		pr_err("Failed to prepare clock (%d)\n", ret);
		goto err_clk;
	}

	g2d_dbg_end();

	return 0;
err_clk:
	kfree(g2d_ctx);

	g2d_dbg_end_err();

	return ret;
}

static int m2m1shot2_g2d_free_context(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(ctx->m21dev->dev);
	struct g2d1shot_ctx *g2d_ctx = ctx->priv;

	g2d_dbg_begin();

	clk_unprepare(g2d_dev->clock);
	kfree(g2d_ctx);

	g2d_dbg_end();

	return 0;
}

static const struct g2d1shot_fmt g2d_formats[] = {
	{
		.name		= "ABGR8888",
		.pixelformat	= V4L2_PIX_FMT_ABGR32,	/* [31:0] ABGR */
		.bpp		= { 32 },
		.num_planes	= 1,
		.src_value	= G2D_DATAFORMAT_RGB8888 | 0x3012,
		.dst_value	= G2D_DATAFORMAT_RGB8888 | 0x3012,
	}, {
		.name		= "XBGR8888",
		.pixelformat	= V4L2_PIX_FMT_XBGR32,	/* [31:0] XBGR */
		.bpp		= { 32 },
		.num_planes	= 1,
		.src_value	= G2D_DATAFORMAT_RGB8888 | 0x5012,
		.dst_value	= G2D_DATAFORMAT_RGB8888 | 0x5012,
	}, {
		.name		= "ARGB8888",
		.pixelformat	= V4L2_PIX_FMT_ARGB32,	/* [31:0] ARGB */
		.bpp		= { 32 },
		.num_planes	= 1,
		.src_value	= G2D_DATAFORMAT_RGB8888 | 0x3210,
		.dst_value	= G2D_DATAFORMAT_RGB8888 | 0x3210,
	}, {
		.name		= "RGB565",
		.pixelformat	= V4L2_PIX_FMT_RGB565,	/* [15:0] RGB */
		.bpp		= { 16 },
		.num_planes	= 1,
		.src_value	= G2D_DATAFORMAT_RGB565 | 0x5210,
		.dst_value	= G2D_DATAFORMAT_RGB565 | 0x0210,
	},
};

static const struct g2d1shot_fmt *find_format(bool is_source, u32 fmt)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(g2d_formats); i++) {
		if (fmt == g2d_formats[i].pixelformat) {
			/* TODO: check more.. H/W version, source/dest.. */
			return &g2d_formats[i];
		}
	}

	return NULL;
}

static int m2m1shot2_g2d_prepare_format(
			struct m2m1shot2_context_format *ctx_fmt,
			unsigned int index, enum dma_data_direction dir,
			size_t payload[], unsigned int *num_planes)
{
	const struct g2d1shot_fmt *g2d_fmt;
	struct m2m1shot2_format *fmt = &ctx_fmt->fmt;
	int i;

	g2d_dbg_begin();

	g2d_fmt = find_format((dir == DMA_TO_DEVICE), fmt->pixelformat);
	if (!g2d_fmt) {
		pr_err("Not supported format (%u)\n", fmt->pixelformat);
		return -EINVAL;
	}

	if (fmt->width == 0 || fmt->height == 0 ||
			fmt->width > G2D_MAX_NORMAL_SIZE ||
			fmt->height > G2D_MAX_NORMAL_SIZE) {
		pr_err("Invalid width or height (%u, %u)\n",
				fmt->width, fmt->height);
		return -EINVAL;
	}

	if (fmt->crop.left < 0 || fmt->crop.top < 0 ||
			(fmt->crop.left + fmt->crop.width > fmt->width) ||
			(fmt->crop.top + fmt->crop.height > fmt->height)) {
		pr_err("Invalid range of crop ltwh(%d, %d, %d, %d)\n",
				fmt->crop.left, fmt->crop.top,
				fmt->crop.width, fmt->crop.height);
		pr_err("width/height : %u/%u\n", fmt->width, fmt->height);
		return -EINVAL;
	}

	*num_planes = g2d_fmt->num_planes;
	for (i = 0; i < g2d_fmt->num_planes; i++) {
		payload[i] = fmt->width * fmt->height * g2d_fmt->bpp[i];
		payload[i] /= 8;
	}
	ctx_fmt->priv = (void *)g2d_fmt;

	g2d_dbg_end();

	return 0;
}

static int m2m1shot2_g2d_prepare_source(struct m2m1shot2_context *ctx,
			unsigned int index, struct m2m1shot2_source_image *img)
{
	/* TODO: compare between dest rect and clipping rect of each source */

	return 0;
}

static int enable_g2d(struct g2d1shot_dev *g2d_dev)
{
	int ret;

	g2d_dbg_begin();

	ret = in_irq() ? pm_runtime_get(g2d_dev->dev) :
			pm_runtime_get_sync(g2d_dev->dev);
	if (ret < 0) {
		pr_err("Failed to enable power (%d)\n", ret);
		return ret;
	}

	ret = clk_enable(g2d_dev->clock);
	if (ret) {
		pr_err("Failed to enable clock (%d)\n", ret);
		pm_runtime_put(g2d_dev->dev);
		return ret;
	}

	g2d_dbg_end();

	return 0;
}

static int disable_g2d(struct g2d1shot_dev *g2d_dev)
{
	g2d_dbg_begin();

	clk_disable(g2d_dev->clock);
	pm_runtime_put(g2d_dev->dev);

	g2d_dbg_end();

	return 0;
}

static void g2d_set_source(struct g2d1shot_dev *g2d_dev,
		struct m2m1shot2_context *ctx,
		struct m2m1shot2_source_image *source, int layer_num)
{
	struct m2m1shot2_context_format *ctx_fmt =
				m2m1shot2_src_format(ctx, layer_num);
	u32 src_type = G2D_LAYER_SELECT_NORMAL;
	u32 img_flags = source->img.flags;
	int i;
	bool compressed = img_flags & M2M1SHOT2_IMGFLAG_COMPRESSED;

	g2d_dbg_begin();

	if (img_flags & M2M1SHOT2_IMGFLAG_COLORFILL) {
		g2d_hw_set_source_color(g2d_dev, layer_num,
						source->ext.fillcolor);
		src_type = G2D_LAYER_SELECT_CONSTANT_COLOR;
	} else {
		/* use composit_mode */
		g2d_hw_set_source_blending(g2d_dev, layer_num, &source->ext);
		g2d_hw_set_source_premult(g2d_dev, layer_num, img_flags);
	}

	/* set source type */
	g2d_hw_set_source_type(g2d_dev, layer_num, src_type);
	/* set source rect, dest clip and pixel format */
	g2d_hw_set_source_format(g2d_dev, layer_num, ctx_fmt, compressed);
	/* set source address */
	/* TODO: check COMPRESSED and set if it is true */
	for (i = 0; i < source->img.num_planes; i++) {
		/* more than 1 planes are not supported yet, but for later. */
		g2d_hw_set_source_address(g2d_dev, layer_num, i, compressed,
				m2m1shot2_src_dma_addr(ctx, layer_num, i));
	}
	/* set repeat mode */
	g2d_hw_set_source_repeat(g2d_dev, layer_num, &source->ext);
	/* set scaling mode */
	if (!(img_flags & M2M1SHOT2_IMGFLAG_NO_RESCALING))
		g2d_hw_set_source_scale(g2d_dev, layer_num, &source->ext,
					img_flags, ctx_fmt);
	/* set rotation mode */
	g2d_hw_set_source_rotate(g2d_dev, layer_num, &source->ext);
	/* set layer valid */
	g2d_hw_set_source_valid(g2d_dev, layer_num);

	g2d_dbg_end();
}

static void g2d_set_target(struct g2d1shot_dev *g2d_dev,
		struct m2m1shot2_context *ctx,
		struct m2m1shot2_context_image *target)
{
	struct m2m1shot2_context_format *ctx_fmt = m2m1shot2_dst_format(ctx);
	int i;

	g2d_dbg_begin();

	/* more than 1 planes are not supported yet, but for later. */
	for (i = 0; i < target->num_planes; i++)
		g2d_hw_set_dest_addr(g2d_dev, i,
			m2m1shot2_dst_dma_addr(ctx, i));

	/* set dest rect and pixel format */
	g2d_hw_set_dest_format(g2d_dev, ctx_fmt);
	/* set dest premult */
	g2d_hw_set_dest_premult(g2d_dev, target->flags);
	/* set dither */
	if (ctx->flags & M2M1SHOT2_FLAG_DITHER)
		g2d_hw_set_dither(g2d_dev);

	g2d_dbg_end();
}

static int m2m1shot2_g2d_device_run(struct m2m1shot2_context *ctx)
{
	struct g2d1shot_ctx *g2d_ctx = ctx->priv;
	struct g2d1shot_dev *g2d_dev = g2d_ctx->g2d_dev;
	unsigned long flags;
	int ret;
	int i;

	g2d_dbg_begin();

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	if (g2d_dev->state & G2D_STATE_SUSPENDING) {
		dev_info(g2d_dev->dev, "G2D is in suspend state.\n");
		spin_unlock_irqrestore(&g2d_dev->state_lock, flags);
		return -EAGAIN;
	}
	g2d_dev->state |= G2D_STATE_RUNNING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	/* enable power, clock */
	ret = enable_g2d(g2d_dev);
	if (ret) {
		spin_lock_irqsave(&g2d_dev->state_lock, flags);
		g2d_dev->state &= ~G2D_STATE_RUNNING;
		spin_unlock_irqrestore(&g2d_dev->state_lock, flags);
		return ret;
	}

	/* H/W initialization */
	g2d_hw_init(g2d_dev);

	/* setting for source */
	for (i = 0; i < ctx->num_sources; i++)
		g2d_set_source(g2d_dev, ctx, &ctx->source[i], i);

	/* setting for destination */
	g2d_set_target(g2d_dev, ctx, &ctx->target);

	/* setting for common */

	if (g2d_dump) {
		g2d_disp_info(ctx);
		g2d_dump_regs(g2d_dev);
	}

	mod_timer(&g2d_dev->timer,
			jiffies + msecs_to_jiffies(G2D_TIMEOUT_INTERVAL));
	/* run H/W */
	g2d_hw_start(g2d_dev);

	g2d_dbg_end();

	return 0;
}

static const struct m2m1shot2_devops m2m1shot2_g2d_ops = {
	.init_context = m2m1shot2_g2d_init_context,
	.free_context = m2m1shot2_g2d_free_context,
	.prepare_format = m2m1shot2_g2d_prepare_format,
	.prepare_source = m2m1shot2_g2d_prepare_source,
	.device_run = m2m1shot2_g2d_device_run,
};

static irqreturn_t exynos_g2d_irq_handler(int irq, void *priv)
{
	struct g2d1shot_dev *g2d_dev = priv;
	struct m2m1shot2_context *m21ctx;
	unsigned long flags;
	bool suspending;

	g2d_dbg_begin();

	m21ctx = m2m1shot2_current_context(g2d_dev->oneshot2_dev);
	if (!m21ctx) {
		dev_err(g2d_dev->dev, "received null in irq handler\n");
		return IRQ_HANDLED;
	}

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	if (!(g2d_dev->state & G2D_STATE_RUNNING)) {
		dev_err(g2d_dev->dev, "interrupt, but no running\n");
		spin_unlock_irqrestore(&g2d_dev->state_lock, flags);
		return IRQ_HANDLED;
	}
	if (g2d_dev->state & G2D_STATE_TIMEOUT) {
		dev_err(g2d_dev->dev, "interrupt, but already timedout\n");
		g2d_dev->state &= ~G2D_STATE_RUNNING;
		spin_unlock_irqrestore(&g2d_dev->state_lock, flags);
		return IRQ_HANDLED;
	}
	suspending = g2d_dev->state & G2D_STATE_SUSPENDING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	del_timer(&g2d_dev->timer);

	/* IRQ handling */
	g2d_hw_stop(g2d_dev);
	disable_g2d(g2d_dev);

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	g2d_dev->state &= ~G2D_STATE_RUNNING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	if (!suspending)
		m2m1shot2_finish_context(m21ctx, true);
	else {
		g2d_dev->suspend_ctx = m21ctx;
		g2d_dev->suspend_ctx_success = true;
		wake_up(&g2d_dev->suspend_wait);
	}

	g2d_dbg_end();

	return IRQ_HANDLED;
}

static int g2d_iommu_fault_handler(
		struct iommu_domain *domain, struct device *dev,
		unsigned long fault_addr, int fault_flags, void *token)
{
	struct g2d1shot_dev *g2d_dev = token;

	g2d_dbg_begin();

	g2d_dump_regs(g2d_dev);

	g2d_dbg_end();

	return 0;
}

static void g2d_timeout_handler(unsigned long arg)
{
	struct g2d1shot_dev *g2d_dev = (struct g2d1shot_dev *)arg;
	struct m2m1shot2_context *m21ctx;
	unsigned long flags;
	bool suspending;

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	if (!(g2d_dev->state & G2D_STATE_RUNNING)) {
		dev_err(g2d_dev->dev, "timeout, but not processing state\n");
		spin_unlock_irqrestore(&g2d_dev->state_lock, flags);
		return;
	}

	g2d_dev->state |= G2D_STATE_TIMEOUT;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	dev_err(g2d_dev->dev, "G2D timeout is happened.\n");

	m21ctx = m2m1shot2_current_context(g2d_dev->oneshot2_dev);
	if (!m21ctx) {
		dev_err(g2d_dev->dev, "received null in timeout handler\n");
		return;
	}

	/* display information */
	g2d_disp_info(m21ctx);
	g2d_dump_regs(g2d_dev);

	/* uninitialize H/W */
	g2d_hw_stop(g2d_dev);
	disable_g2d(g2d_dev);

	/* notify m2m1shot2 with false */
	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	g2d_dev->state &= ~G2D_STATE_TIMEOUT;
	suspending = g2d_dev->state & G2D_STATE_SUSPENDING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	if (!suspending)
		m2m1shot2_finish_context(m21ctx, false);
	else {
		g2d_dev->suspend_ctx = m21ctx;
		g2d_dev->suspend_ctx_success = false;
		wake_up(&g2d_dev->suspend_wait);
	}
};

static int g2d_init_clock(struct device *dev, struct g2d1shot_dev *g2d_dev)
{
	g2d_dev->clock = devm_clk_get(dev, "gate");
	if (IS_ERR(g2d_dev->clock)) {
		dev_err(dev, "Failed to get clock (%ld)\n",
					PTR_ERR(g2d_dev->clock));
		return PTR_ERR(g2d_dev->clock);
	}

	return 0;
}

static int g2d_get_hw_version(struct device *dev, struct g2d1shot_dev *g2d_dev)
{
	int ret;

	ret = pm_runtime_get_sync(dev);
	if (ret < 0) {
		dev_err(dev, "Failed to enable power (%d)\n", ret);
		return ret;
	}

	ret = clk_prepare_enable(g2d_dev->clock);
	if (!ret) {
		g2d_dev->version = g2d_hw_read_version(g2d_dev);
		dev_info(dev, "G2D version : %#010x\n", g2d_dev->version);

		clk_disable_unprepare(g2d_dev->clock);
	}

	pm_runtime_put(dev);

	return 0;
}

static int exynos_g2d_probe(struct platform_device *pdev)
{
	struct g2d1shot_dev *g2d_dev;
	struct resource *res;
	int ret;

	g2d_dev = devm_kzalloc(&pdev->dev, sizeof(*g2d_dev), GFP_KERNEL);
	if (!g2d_dev)
		return -ENOMEM;

	g2d_dev->dev = &pdev->dev;

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	g2d_dev->reg = devm_ioremap_resource(&pdev->dev, res);
	if (IS_ERR(g2d_dev->reg))
		return PTR_ERR(g2d_dev->reg);

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "Failed to get IRQ resource");
		return -ENOENT;
	}

	ret = devm_request_irq(&pdev->dev, res->start, exynos_g2d_irq_handler,
				0, pdev->name, g2d_dev);
	if (ret) {
		dev_err(&pdev->dev, "Failed to install IRQ handler");
		return ret;
	}

	ret = g2d_init_clock(&pdev->dev, g2d_dev);
	if (ret)
		return ret;

	g2d_dev->oneshot2_dev = m2m1shot2_create_device(&pdev->dev,
		&m2m1shot2_g2d_ops, NODE_NAME, -1, M2M1SHOT2_DEVATTR_COHERENT);
	if (IS_ERR(g2d_dev->oneshot2_dev))
		return PTR_ERR(g2d_dev->oneshot2_dev);

#if defined(G2D_SHARABILITY_CONTROL)
	g2d_dev->syscon_reg = ioremap(SYSREG_BLK_MSCL, SZ_4K);
	if (!g2d_dev->syscon_reg) {
		dev_err(&pdev->dev, "Failed to map BLK_MSCL for sharability\n");
		goto err_hwver;
	}
	g2d_dev->wlu_disable = true;
#else
	/* It should be NULL if SHARABILITY is not set */
	g2d_dev->syscon_reg = NULL;
	g2d_dev->wlu_disable = false;
#endif
	platform_set_drvdata(pdev, g2d_dev);

	pm_runtime_enable(&pdev->dev);

	ret = g2d_get_hw_version(&pdev->dev, g2d_dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to get H/W version\n");
		goto err_hwver;
	}

	iovmm_set_fault_handler(&pdev->dev, g2d_iommu_fault_handler, g2d_dev);

	ret = iovmm_activate(&pdev->dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to activate iommu\n");
		goto err_hwver;
	}

	setup_timer(&g2d_dev->timer, g2d_timeout_handler,
						(unsigned long)g2d_dev);
	spin_lock_init(&g2d_dev->state_lock);
	init_waitqueue_head(&g2d_dev->suspend_wait);

	dev_info(&pdev->dev, "G2D with m2m1shot2 is probed successfully.\n");

	return 0;
err_hwver:
	m2m1shot2_destroy_device(g2d_dev->oneshot2_dev);

	dev_err(&pdev->dev, "G2D m2m1shot2 probe is failed.\n");

	return ret;
}

static int exynos_g2d_remove(struct platform_device *pdev)
{
	struct g2d1shot_dev *g2d_dev = platform_get_drvdata(pdev);

	m2m1shot2_destroy_device(g2d_dev->oneshot2_dev);

	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int g2d_suspend(struct device *dev)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	g2d_dev->state |= G2D_STATE_SUSPENDING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	wait_event(g2d_dev->suspend_wait,
			!(g2d_dev->state & G2D_STATE_RUNNING));

	return 0;
}

static int g2d_resume(struct device *dev)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(dev);
	unsigned long flags;

	spin_lock_irqsave(&g2d_dev->state_lock, flags);
	g2d_dev->state &= ~G2D_STATE_SUSPENDING;
	spin_unlock_irqrestore(&g2d_dev->state_lock, flags);

	if (g2d_dev->suspend_ctx) {
		m2m1shot2_finish_context(
			g2d_dev->suspend_ctx, g2d_dev->suspend_ctx_success);
		g2d_dev->suspend_ctx = NULL;
	}

	return 0;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int g2d_runtime_suspend(struct device *dev)
{
	return 0;
}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
static int g2d_runtime_resume(struct device *dev)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(dev);
	int ret;

	ret = exynos_smc(MC_FC_SET_CFW_PROT, MC_FC_DRM_SET_CFW_PROT,
			PROT_G2D, 0);
	if (ret != SMC_TZPC_OK)
		g2d_err("fail to set cfw protection (%d)\n", ret);
	else
		g2d_debug("success to set cfw protection\n");

	if (g2d_dev->wlu_disable)
		g2d_wlu_disable(g2d_dev);

	return 0;
}
#else
static int g2d_runtime_resume(struct device *dev)
{
	struct g2d1shot_dev *g2d_dev = dev_get_drvdata(dev);

	if (g2d_dev->wlu_disable)
		g2d_wlu_disable(g2d_dev);

	return 0;
}
#endif
#endif

static const struct dev_pm_ops exynos_g2d_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(g2d_suspend, g2d_resume)
	SET_RUNTIME_PM_OPS(g2d_runtime_suspend, g2d_runtime_resume, NULL)
};

static const struct of_device_id exynos_g2d_match[] = {
	{
		.compatible = "samsung,s5p-fimg2d",
	},
	{},
};

static struct platform_driver exynos_g2d_driver = {
	.probe		= exynos_g2d_probe,
	.remove		= exynos_g2d_remove,
	.driver = {
		.name	= MODULE_NAME,
		.owner	= THIS_MODULE,
		.pm	= &exynos_g2d_pm_ops,
		.of_match_table = of_match_ptr(exynos_g2d_match),
	}
};

module_platform_driver(exynos_g2d_driver);

MODULE_AUTHOR("Janghyuck Kim <janghyuck.kim@samsung.com>");
MODULE_DESCRIPTION("Exynos Graphics 2D driver");
MODULE_LICENSE("GPL");
