/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d5x_blt.c
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

#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/atomic.h>
#include <linux/dma-mapping.h>
#include <linux/rmap.h>
#include <linux/fs.h>
#include <linux/clk-private.h>
#include <asm/cacheflush.h>
#ifdef CONFIG_PM_RUNTIME
#include <linux/pm_runtime.h>
#endif
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d5x.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

#define BLIT_TIMEOUT	msecs_to_jiffies(8000)

#define MAX_PREFBUFS	6
static int nbufs;
static struct sysmmu_prefbuf prefbuf[MAX_PREFBUFS];

static int fimg2d5x_blit_wait(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	int ret;

	ret = wait_event_timeout(ctrl->wait_q, !atomic_read(&ctrl->busy),
			BLIT_TIMEOUT);
	if (!ret) {
		fimg2d_err("blit wait timeout\n");

		fimg2d5x_disable_irq(ctrl);
		if (!fimg2d5x_blit_done_status(ctrl))
			fimg2d_err("blit not finished\n");

		fimg2d_dump_command(cmd);
		fimg2d5x_reset(ctrl);

		return -1;
	}
	return 0;
}

int fimg2d5x_bitblt(struct fimg2d_control *ctrl)
{
	int ret = 0;
	enum addr_space addr_type;
	struct fimg2d_context *ctx;
	struct fimg2d_bltcmd *cmd;
	struct fimg2d_memops *memops;
	unsigned long *pgd;

	fimg2d_debug("%s : enter blitter\n", __func__);

	cmd = fimg2d_get_command(ctrl, 0);
	if (!cmd)
		return 0;

	ctx = cmd->ctx;
	ctx->state = CTX_READY;

	list_del(&cmd->job);
	if (fimg2d5x_get_clk_cnt(ctrl->clock) == false)
		fimg2d_err("2D clock is not set\n");

	addr_type = cmd->image_dst.addr.type;

	atomic_set(&ctrl->busy, 1);

	perf_start(cmd, PERF_SFR);
	ret = ctrl->configure(ctrl, cmd);
	perf_end(cmd, PERF_SFR);
	if (IS_ERR_VALUE(ret)) {
		fimg2d_err("failed to configure\n");
		ctx->state = CTX_ERROR;
		goto fail_n_del;
	}

	/* memops is not NULL because configure is successed. */
	memops = cmd->memops;

	if (addr_type == ADDR_USER) {
		if (!ctx->mm || !ctx->mm->pgd) {
			atomic_set(&ctrl->busy, 0);
			fimg2d_err("ctx->mm:0x%p or ctx->mm->pgd:0x%p\n",
					ctx->mm,
					(ctx->mm) ? ctx->mm->pgd : NULL);
			ret = -EPERM;
			goto fail_n_unmap;
		}
		pgd = (unsigned long *)ctx->mm->pgd;
		fimg2d_debug("%s : sysmmu enable: pgd %p ctx %p seq_no(%u)\n",
				__func__, pgd, ctx, cmd->blt.seq_no);
	}

	if (iovmm_activate(ctrl->dev)) {
		fimg2d_err("failed to iovmm activate\n");
		ret = -EPERM;
		goto fail_n_unmap;
	}

	perf_start(cmd, PERF_BLIT);
	/* start blit */
	fimg2d_debug("%s : start blit\n", __func__);
	ctrl->run(ctrl);
	ret = fimg2d5x_blit_wait(ctrl, cmd);
	perf_end(cmd, PERF_BLIT);

	iovmm_deactivate(ctrl->dev);

fail_n_unmap:
	perf_start(cmd, PERF_UNMAP);
	memops->unmap(ctrl, cmd);
	memops->finish(ctrl, cmd);
	perf_end(cmd, PERF_UNMAP);
fail_n_del:
	fimg2d_del_command(ctrl, cmd);

	fimg2d_debug("%s : exit blitter\n", __func__);

	return ret;
}

static int fast_op(struct fimg2d_image *s)
{
	int fop;

	fop = s->op;

	/* TODO: translate op */

	return fop;
}

static int fimg2d5x_configure(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd)
{
	int op;
	enum image_sel srcsel;
	struct fimg2d_param *p;
	struct fimg2d_image *src, *dst;
	struct sysmmu_prefbuf *pbuf;
	struct fimg2d_memops *memops = cmd->memops;
	enum addr_space addr_type;
	int i;
	int ret;

	addr_type = cmd->image_dst.addr.type;

	fimg2d_debug("ctx %p seq_no(%u)\n", cmd->ctx, cmd->blt.seq_no);

	p = &cmd->image_src[0].param;
	dst = &cmd->image_dst;

	fimg2d5x_init(ctrl);

	nbufs = 0;
	pbuf = &prefbuf[nbufs];

	if (!memops) {
		fimg2d_err("No memops registered, type = %d\n", addr_type);
		return -EFAULT;
	}

	ret = memops->map(ctrl, cmd);
	if (ret) {
		fimg2d_err("Failed to memory mapping\n");
		fimg2d_info("s/w fallback (ret = %d)\n", ret);
		return ret;
	}

	/* src */
	for (i = 0; i < MAX_SRC; i++) {
		src = &cmd->image_src[i];
		if (!src->addr.type) {
			if (src->op != BLIT_OP_SOLID_FILL)
				continue;
		}

		p = &cmd->image_src[i].param;

		srcsel = IMG_MEMORY;
		op = fast_op(src);

		switch (op) {
		case BLIT_OP_SOLID_FILL:
			srcsel = IMG_CONSTANT_COLOR;
			fimg2d5x_set_fgcolor(ctrl, src, p->solid_color);
			break;
		case BLIT_OP_CLR:
			srcsel = IMG_CONSTANT_COLOR;
			fimg2d5x_set_color_fill(ctrl, 0);
			break;
		case BLIT_OP_DST:
			srcsel = IMG_CONSTANT_COLOR;
			break;
		default:
			fimg2d5x_enable_alpha(ctrl, src, p->g_alpha);
			fimg2d5x_set_alpha_composite(ctrl, src, op, p->g_alpha);
			fimg2d5x_set_premultiplied(ctrl, src, p->premult);
			break;
		}

		fimg2d5x_set_src_type(ctrl, src, srcsel);
		fimg2d5x_set_src_rect(ctrl, src, &src->rect);
		fimg2d5x_set_src_repeat(ctrl, src, &p->repeat);

		if (srcsel != IMG_CONSTANT_COLOR)
			fimg2d5x_set_src_image(ctrl, src, &cmd->dma_src[i]);

		if (p->scaling.mode)
			fimg2d5x_set_src_scaling(ctrl, src,
					&p->scaling, &p->repeat);

		/* TODO: clipping enable should be always true. */
		if (p->clipping.enable)
			fimg2d5x_enable_clipping(ctrl, src, &p->clipping);

		/* rotation */
		if (p->rotate)
			fimg2d5x_set_rotation(ctrl, src, p->rotate);

		fimg2d5x_set_layer_valid(ctrl, src);

		/* prefbuf */
		pbuf->base = cmd->dma_src[i].base.iova;
		pbuf->size = cmd->dma_src[i].base.size;
		pbuf->config = SYSMMU_PBUFCFG_DEFAULT_INPUT;
		nbufs++;
		pbuf++;
	}

	/* dst */
	if (dst->addr.type) {
		fimg2d5x_set_dst_image(ctrl, dst, &cmd->dma_dst);
		fimg2d5x_set_dst_rect(ctrl, &dst->rect);

		/* prefbuf */
		pbuf->base = cmd->dma_dst.base.iova;
		pbuf->size = cmd->dma_dst.base.size;
		pbuf->config = SYSMMU_PBUFCFG_DEFAULT_OUTPUT;
		nbufs++;
		pbuf++;
	}

	sysmmu_set_prefetch_buffer_by_region(ctrl->dev, prefbuf, nbufs);

	/* dithering */
	if (cmd->blt.dither)
		fimg2d5x_enable_dithering(ctrl);

	/* Update flag */
	if (cmd->src_flag) {
		fimg2d5x_layer_update(ctrl, cmd->src_flag);
		if (dst->param.premult == NON_PREMULTIPLIED)
			fimg2d5x_set_dst_depremult(ctrl);
	}

	return 0;
}

static void fimg2d5x_run(struct fimg2d_control *ctrl)
{
	fimg2d_debug("start blit\n");
	fimg2d5x_enable_irq(ctrl);
	fimg2d5x_clear_irq(ctrl);
	fimg2d5x_start_blit(ctrl);
}

static void fimg2d5x_stop(struct fimg2d_control *ctrl)
{
	if (fimg2d5x_is_blit_done(ctrl)) {
		fimg2d_debug("blit done\n");
		fimg2d5x_clear_irq(ctrl);
		fimg2d5x_disable_irq(ctrl);
		atomic_set(&ctrl->busy, 0);
		wake_up(&ctrl->wait_q);
	}
}

static void fimg2d5x_dump(struct fimg2d_control *ctrl)
{
	fimg2d5x_dump_regs(ctrl);
}

int fimg2d_register_ops(struct fimg2d_control *ctrl)
{
	/* TODO */
	ctrl->blit = fimg2d5x_bitblt;
	ctrl->configure = fimg2d5x_configure;
	ctrl->run = fimg2d5x_run;
	ctrl->dump = fimg2d5x_dump;
	ctrl->stop = fimg2d5x_stop;

	return 0;
};
