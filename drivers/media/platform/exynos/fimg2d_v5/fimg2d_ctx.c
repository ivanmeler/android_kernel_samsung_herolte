/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_ctx.c
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
#include <linux/sched.h>
#include <linux/uaccess.h>
#include <linux/exynos_ion.h>
#include "fimg2d.h"
#include "fimg2d_clk.h"
#include "fimg2d_ctx.h"
#include "fimg2d_cache.h"
#include "fimg2d_helper.h"

int bit_per_pixel(struct fimg2d_image *img, int plane)
{
	switch (img->fmt) {
	case CF_ARGB_8888:
	case CF_COMP_RGB8888:
		return 32;
	case CF_RGB_565:
	case CF_COMP_RGB565:
		return 16;
	default:
		return 0;
	}
}

static int fimg2d_check_address_range(unsigned long addr, size_t size)
{
	struct vm_area_struct *vma;
	int ret = 0;

	if (addr + size <= addr) {
		fimg2d_err("address overflow. addr:%#lx, size:%zd\n",
				addr, size);
		return -EINVAL;
	}

	down_read(&current->mm->mmap_sem);
	vma = find_vma(current->mm, addr);

	/* traverse contiguous vmas list until getting the last one */
	while (vma && (vma->vm_end < addr + size) &&
		vma->vm_next && (vma->vm_next->vm_start == vma->vm_end)) {
		vma = vma->vm_next;
	}

	if (!vma) {
		fimg2d_err("vma is invalid for %zd bytes@%#lx\n", size, addr);
		ret = -EFAULT;
	}

	if (vma->vm_end < addr + size) {
		fimg2d_err("vma %#lx--%#lx overflow for %zd bytes@%#lx\n",
				vma->vm_start, vma->vm_end, size, addr);
		ret = -EFAULT;
	}

	up_read(&current->mm->mmap_sem);

	return ret;
}

static int fimg2d_check_address(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_dma *c;
	struct fimg2d_image *img;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		c = &cmd->dma_src[i].base;

		if (fimg2d_check_address_range(c->addr, c->size))
			return -EINVAL;
	}

	img = &cmd->image_dst;
	if (!img->addr.type)
		return -EINVAL;

	c = &cmd->dma_dst.base;

	if (fimg2d_check_address_range(c->addr, c->size))
		return -EINVAL;

	return 0;
}

int fimg2d_check_image(struct fimg2d_image *img)
{
	struct fimg2d_rect *r;
	int w, h;

	if (!img->addr.type)
		return 0;

	w = img->width;
	h = img->height;
	r = &img->rect;

	/* 16383: max width & height */
	/* 8192: COMP max width & height */
	switch (img->fmt) {
	case CF_COMP_RGB8888:
	case CF_COMP_RGB565:
		if (w > 8192 || h > 8192)
			return -1;
		break;
	default:
		if (w > 16383 || h > 16383)
			return -1;
		break;
	}

	/* Is it correct to compare (x1 >= w) and (y1 >= h) ? */
	if (r->x1 < 0 || r->y1 < 0 ||
			r->x1 >= w || r->y1 >= h ||
			r->x1 >= r->x2 || r->y1 >= r->y2) {
		fimg2d_err("r(%d, %d, %d, %d) w,h(%d, %d)\n",
				r->x1, r->y1, r->x2, r->y2, w, h);
		return -2;
	}

	/* DO support UVA & DVA(fd) */
	if (img->addr.type != ADDR_USER &&
			img->addr.type != ADDR_DEVICE)
		return -3;

	return 0;
}

static int fimg2d_check_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_image *img;
	struct fimg2d_image *dst;
	struct fimg2d_scale *scl;
	struct fimg2d_clip *clp;
	struct fimg2d_rect *r;
	enum addr_space addr_type;
	int w, h, i;
	int ret;

	/* dst is mandatory */
	if (WARN_ON(!blt->dst || !blt->dst->addr.type))
		return -1;

	addr_type = blt->dst->addr.type;

	for (i = 0; i < MAX_SRC; i++) {
		img = blt->src[i];
		if (img && (cmd->src_flag & (1 << i)) &&
				(img->addr.type != addr_type)) {
			if (img->op != BLIT_OP_SOLID_FILL)
				return -2;
		}
	}

	/* Check for destination */
	dst = &cmd->image_dst;
	ret = fimg2d_check_image(dst);
	if (ret) {
		fimg2d_err("check dst image failed, ret = %d\n", ret);
		return -3;
	}

	/* Check for source */
	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (fimg2d_check_image(img)) {
			fimg2d_err("check src[%d] image failed, ret = %d\n",
								i, ret);
			return -4;
		}

		clp = &img->param.clipping;
		if (clp->enable) {
			w = dst->width;
			h = dst->height;
			r = &dst->rect;

			if (clp->x1 < 0 || clp->y1 < 0 ||
				clp->x1 >= w || clp->y1 >= h ||
				clp->x1 >= clp->x2 || clp->y1 >= clp->y2) {
				fimg2d_err("Src[%d] clp(%d,%d,%d,%d)\n",
					i, clp->x1, clp->y1, clp->x2, clp->y2);
				fimg2d_err("Dst w,h(%d,%d) r(%d,%d,%d,%d)\n",
					w, h, r->x1, r->y1, r->x2, r->y2);
				return -5;
			}
		}

		scl = &img->param.scaling;
		if (scl->mode) {
			if (!scl->src_w || !scl->src_h ||
					!scl->dst_w || !scl->dst_h)
				return -6;
		}

	}

	if (fimg2d_register_memops(cmd, addr_type)) {
		fimg2d_err("Failed to register memops, addr_type = %d\n",
								addr_type);
		return -7;
	}

	return 0;
}

static void fimg2d_fixup_params(struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_scale *scl;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		switch (img->fmt) {
		case CF_COMP_RGB8888:
		case CF_COMP_RGB565:
			if (!IS_ALIGNED(img->width, FIMG2D_COMP_ALIGN_WIDTH)) {
				fimg2d_info("SRC[%d], COMP format width(%d)",
								i, img->width);
				img->width = ALIGN(img->width,
						FIMG2D_COMP_ALIGN_WIDTH);
				fimg2d_info("becomes %d, (aligned by %d)\n",
					img->width, FIMG2D_COMP_ALIGN_WIDTH);
			}
			if (!IS_ALIGNED(img->height,
						FIMG2D_COMP_ALIGN_HEIGHT)) {
				fimg2d_info("SRC[%d], COMP format height(%d)",
								i, img->height);
				img->height = ALIGN(img->height,
						FIMG2D_COMP_ALIGN_HEIGHT);
				fimg2d_info("becomes %d, (aligned by %d)\n",
					img->height, FIMG2D_COMP_ALIGN_HEIGHT);
			}
			break;
		default:
			/* NOP */
			break;
		}

		scl = &img->param.scaling;

		/* avoid divided-by-zero */
		if (scl->mode &&
			(scl->src_w == scl->dst_w && scl->src_h == scl->dst_h))
			scl->mode = NO_SCALING;
	}
}

int fimg2d_calc_image_dma_size(struct fimg2d_bltcmd *cmd,
		struct fimg2d_image *img, struct fimg2d_dma *c, int offset)
{
	struct fimg2d_rect *r;
	int y1, y2, stride, clp_h, bpp;
	exynos_iova_t iova;

	iova = FIMG2D_IOVA_START +
		(FIMG2D_IOVA_PLANE_SIZE * FIMG2D_IOVA_MAX_PLANES) * offset;
	r = &img->rect;

	y1 = r->y1;
	y2 = r->y2;

	/* 1st plane */
	bpp = bit_per_pixel(img, 0);
	stride = width2bytes(img->width, bpp);

	clp_h = y2 - y1;

	c->addr = img->addr.start + (stride * y1);
	c->size = stride * clp_h;
	c->offset = stride * y1;
	c->iova = iova + (img->addr.start & ~PAGE_MASK);

	if (img->need_cacheopr) {
		c->cached = c->size;
		cmd->dma_all += c->cached;
	}

	return 0;
}

static int fimg2d_check_dma_sync(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_blit *blt = &cmd->blt;
	struct fimg2d_memops *memops = cmd->memops;
	enum addr_space addr_type;
	int ret = 0;

	addr_type = blt->dst->addr.type;

	if (!memops) {
		fimg2d_err("No memops registered, type = %d\n", addr_type);
		return -EFAULT;
	}

	ret = memops->prepare(ctrl, cmd);
	if (ret) {
		fimg2d_err("Failed to memory prepare, %d\n", ret);
		return ret;
	}

	return 0;
}

struct fimg2d_bltcmd *fimg2d_add_command(struct fimg2d_control *ctrl,
	struct fimg2d_context *ctx, struct fimg2d_blit __user *buf, int *info)
{
	unsigned long flags;
	struct fimg2d_blit *blt;
	struct fimg2d_bltcmd *cmd;
	struct fimg2d_image *img;
	int len = sizeof(struct fimg2d_image);
	int i, ret = 0;

	cmd = kzalloc(sizeof(*cmd), GFP_KERNEL);
	if (!cmd) {
		*info = -ENOMEM;
		return NULL;
	}

	if (copy_from_user(&cmd->blt, buf, sizeof(cmd->blt))) {
		ret = -EFAULT;
		goto err;
	}

	INIT_LIST_HEAD(&cmd->job);
	cmd->ctx = ctx;

	blt = &cmd->blt;

	for (i = 0; i < MAX_SRC; i++) {
		if (blt->src[i]) {
			img = &cmd->image_src[i];
			if (copy_from_user(img, blt->src[i], len)) {
				ret = -EFAULT;
				goto err;
			}
			blt->src[i] = img;
			cmd->src_flag |= (1 << i);
		}
	}

	if (blt->dst) {
		if (copy_from_user(&cmd->image_dst, blt->dst, len)) {
			ret = -EFAULT;
			goto err;
		}
		blt->dst = &cmd->image_dst;
	}

	fimg2d_dump_command(cmd);

	perf_start(cmd, PERF_TOTAL);

	ret = fimg2d_check_params(cmd);
	if (ret) {
		fimg2d_err("check param fails, ret = %d\n", ret);
		ret = -EINVAL;
		goto err;
	}

	fimg2d_fixup_params(cmd);

	if (fimg2d_check_dma_sync(ctrl, cmd)) {
		ret = -EFAULT;
		goto err;
	}

	/* TODO: PM QoS */

	/* add command node and increase ncmd */
	g2d_spin_lock(&ctrl->bltlock, flags);
	if (atomic_read(&ctrl->suspended)) {
		fimg2d_debug("driver is unavailable, do sw fallback\n");
		g2d_spin_unlock(&ctrl->bltlock, flags);
		ret = -EPERM;
		goto err;
	}
	g2d_spin_unlock(&ctrl->bltlock, flags);

	return cmd;

err:
	kfree(cmd);
	*info = ret;
	return NULL;
}

void fimg2d_del_command(struct fimg2d_control *ctrl, struct fimg2d_bltcmd *cmd)
{
	unsigned long flags;
	struct fimg2d_context *ctx = cmd->ctx;

	perf_end(cmd, PERF_TOTAL);
	perf_print(cmd);
	g2d_spin_lock(&ctrl->bltlock, flags);
	fimg2d_dequeue(cmd, ctrl);
	kfree(cmd);
	atomic_dec(&ctx->ncmd);

	/* wake up context */
	if (!atomic_read(&ctx->ncmd))
		wake_up(&ctx->wait_q);

	g2d_spin_unlock(&ctrl->bltlock, flags);
}

struct fimg2d_bltcmd *fimg2d_get_command(struct fimg2d_control *ctrl,
						int is_wait_q)
{
	unsigned long flags;
	struct fimg2d_bltcmd *cmd;

	g2d_spin_lock(&ctrl->bltlock, flags);
	cmd = fimg2d_get_first_command(ctrl, is_wait_q);
	g2d_spin_unlock(&ctrl->bltlock, flags);
	return cmd;
}

void fimg2d_add_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_set(&ctx->ncmd, 0);
	init_waitqueue_head(&ctx->wait_q);

	atomic_inc(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}

void fimg2d_del_context(struct fimg2d_control *ctrl, struct fimg2d_context *ctx)
{
	atomic_dec(&ctrl->nctx);
	fimg2d_debug("ctx %p nctx(%d)\n", ctx, atomic_read(&ctrl->nctx));
}

/* Begin: callback functions for user buffers */
static int fimg2d_prepare_user_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma *c;
	int i;

	/* Calculate source DMA size */
	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		c = &cmd->dma_src[i].base;

		fimg2d_calc_image_dma_size(cmd, img, c, i);

		fimg2d_debug("SRC[%d] addr : %p, size : %zd\n",
				i, (void *)c->addr, c->size);
	}

	/* Calculate destination DMA size */
	img = &cmd->image_dst;
	c = &cmd->dma_dst.base;

	fimg2d_calc_image_dma_size(cmd, img, c, DST_OFFSET);

	fimg2d_debug("DST addr : %p, size : %zd\n",
			(void *)c->addr, c->size);

	if (fimg2d_check_address(cmd)) {
		fimg2d_err("Failed to check address\n");
		return -EINVAL;
	}

	return 0;
}

static void fimg2d_unmap_each_user_buffer(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd,
					struct fimg2d_dma_group *dgroup)
{
	if (dgroup->base.size > 0) {
		exynos_sysmmu_unmap_user_pages(ctrl->dev, cmd->ctx->mm,
				dgroup->base.addr,
				dgroup->base.iova +
				dgroup->base.offset,
				dgroup->base.size);
		dgroup->is_mapped = 0;
	}
}

static int fimg2d_unmap_user_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma_group *dgroup;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		dgroup = &cmd->dma_src[i];
		if (!dgroup->is_mapped)
			continue;

		fimg2d_unmap_each_user_buffer(ctrl, cmd, dgroup);
	}

	img = &cmd->image_dst;

	dgroup = &cmd->dma_dst;
	if (!dgroup->is_mapped)
		return 0;

	fimg2d_unmap_each_user_buffer(ctrl, cmd, dgroup);

	return 0;
}

static int fimg2d_map_each_user_buffer(struct fimg2d_control *ctrl,
		struct fimg2d_bltcmd *cmd, struct fimg2d_dma_group *dgroup,
		bool write)
{
	int ret;

	ret = exynos_sysmmu_map_user_pages(
			ctrl->dev, cmd->ctx->mm,
			dgroup->base.addr,
			dgroup->base.iova +
			dgroup->base.offset,
			dgroup->base.size, write,
			IS_ENABLED(CONFIG_FIMG2D_CCI_SNOOP));
	if (ret) {
		dgroup->is_mapped = 0;
		fimg2d_info("s/w fallback (%d)\n", ret);
		return ret;
	}

	dgroup->is_mapped = 1;

	return 0;
}

static int fimg2d_map_user_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma_group *dgroup;
	int ret = 0;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		dgroup = &cmd->dma_src[i];
		ret = fimg2d_map_each_user_buffer(ctrl, cmd, dgroup, 0);
		if (ret) {
			fimg2d_err("Failed to map source[%d] user buffer\n", i);
			goto map_user_fail;
		}
	}

	img = &cmd->image_dst;
	dgroup = &cmd->dma_dst;

	ret = fimg2d_map_each_user_buffer(ctrl, cmd, dgroup, 1);
	if (ret)
		goto map_user_fail;

	return 0;

map_user_fail:
	fimg2d_unmap_user_buffers(ctrl, cmd);

	return ret;
}

static int fimg2d_finish_user_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	/* NOP */
	return 0;
}
/* End: callback functions for user buffers */

/* Begin: callback functions for dmabuf buffers */
static int fimg2d_unmap_dmabuf_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma_dva *dva;
	struct fimg2d_dma *base;
	int i;

	dva = &cmd->dma_dst.dva;
	base = &cmd->dma_dst.base;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		dva = &cmd->dma_src[i].dva;
		base = &cmd->dma_src[i].base;

		if (base->iova)
			ion_iovmm_unmap(dva->attachment, base->iova);
	}

	dva = &cmd->dma_dst.dva;
	base = &cmd->dma_dst.base;

	if (base->iova)
		ion_iovmm_unmap(dva->attachment, base->iova);

	return 0;
}

int fimg2d_finish_each_dmabuf_buffer(struct fimg2d_control *ctrl,
			struct fimg2d_dma *base,
			struct fimg2d_dma_dva *dva, int offset)
{
	int direction;

	/* Nothing allocated */
	if (dva->fd == -1)
		return 0;

	if (offset < DST_OFFSET)
		direction = DMA_TO_DEVICE;
	else
		direction = DMA_FROM_DEVICE;

	dma_buf_unmap_attachment(dva->attachment, dva->sg_table, direction);
	dma_buf_detach(dva->dma_buf, dva->attachment);
	dma_buf_put(dva->dma_buf);
	dva->fd = -1;

	return 0;
}

static int fimg2d_finish_dmabuf_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma_dva *dva;
	struct fimg2d_dma *base;
	bool use_fence = cmd->blt.use_fence;
	int i;

	dva = &cmd->dma_dst.dva;
	base = &cmd->dma_dst.base;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		img->release_fence_fd = -1;
		if (use_fence)
			sw_sync_timeline_inc(ctrl->timeline, 1);

		dva = &cmd->dma_src[i].dva;
		base = &cmd->dma_src[i].base;

		fimg2d_finish_each_dmabuf_buffer(ctrl, base, dva, i);
	}

	dva = &cmd->dma_dst.dva;
	base = &cmd->dma_dst.base;

	if (use_fence)
		sw_sync_timeline_inc(ctrl->timeline, 1);

	fimg2d_finish_each_dmabuf_buffer(ctrl, base, dva, DST_OFFSET);

	return 0;
}

int fimg2d_prepare_each_dmabuf_buffer(struct fimg2d_control *ctrl,
		struct fimg2d_image *img, struct fimg2d_dma_dva *dva,
		int offset, bool use_fence)
{
	struct sync_fence *fence;
	struct sync_pt *pt;
	int fd;
	int direction;
	int ret = 0;

	dva->fd = (int)img->addr.start;
	dva->dma_buf = dma_buf_get(dva->fd);
	if (IS_ERR(dva->dma_buf)) {
		fimg2d_err("Failed to dma_buf_get\n");
		ret = PTR_ERR(dva->dma_buf);
		goto buf_get_err;
	}

	dva->attachment = dma_buf_attach(dva->dma_buf, ctrl->dev);
	if (IS_ERR(dva->attachment)) {
		fimg2d_err("Failed to dma_buf_attach\n");
		ret = PTR_ERR(dva->attachment);
		goto buf_attach_err;
	}

	if (offset < DST_OFFSET)
		direction = DMA_TO_DEVICE;
	else
		direction = DMA_FROM_DEVICE;
	dva->sg_table = dma_buf_map_attachment(dva->attachment, direction);
	if (IS_ERR(dva->sg_table)) {
		fimg2d_err("Failed to dma_buf_map_attachment\n");
		ret = PTR_ERR(dva->sg_table);
		goto map_attachment_err;
	}

	if (use_fence) {
		fd = get_unused_fd();
		if (fd < 0) {
			fimg2d_err("Failed to get unused fd, fd = %d\n", fd);
			ret = -EINVAL;
			goto get_unused_fd_err;
		}

		ctrl->timeline_max++;
		pt = sw_sync_pt_create(ctrl->timeline, ctrl->timeline_max);
		if (!pt) {
			fimg2d_err("Failed to create sync pt\n");
			goto use_fence_err;
		}
		fence = sync_fence_create("g2d_fence", pt);
		if (!fence) {
			fimg2d_err("Failed to create sync fence\n");
			sync_pt_free(pt);
			goto use_fence_err;
		}
		sync_fence_install(fence, fd);
		img->release_fence_fd = fd;
		if (img->acquire_fence_fd >= 0) {
			img->fence = sync_fence_fdget(img->acquire_fence_fd);
			if (!img->fence) {
				fimg2d_err("Failed to import fence fd: %d\n",
						img->acquire_fence_fd);
				sync_pt_free(pt);
				sync_fence_put(fence);
				goto use_fence_err;
			}
		} else {
			fimg2d_debug("No acquire fence for offset[%d]\n", offset);
			img->fence = NULL;
		}
	}

	return 0;

use_fence_err:
	if (use_fence)
		put_unused_fd(fd);
get_unused_fd_err:
	if (use_fence)
		img->release_fence_fd = -1;
	dma_buf_unmap_attachment(dva->attachment, dva->sg_table, direction);
map_attachment_err:
	dma_buf_detach(dva->dma_buf, dva->attachment);
buf_attach_err:
	dma_buf_put(dva->dma_buf);
buf_get_err:
	dva->fd = -1;

	return ret;
}

static int fimg2d_prepare_dmabuf_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma_dva *dva;
	int ret = 0;
	int i;
	bool use_fence = cmd->blt.use_fence;

	/* Calculate source DMA size */
	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		dva = &cmd->dma_src[i].dva;

		ret = fimg2d_prepare_each_dmabuf_buffer(ctrl, img,
							dva, i, use_fence);
		if (ret)
			goto prepare_fail;
	}

	/* Calculate destination DMA size */
	img = &cmd->image_dst;
	dva = &cmd->dma_dst.dva;

	ret = fimg2d_prepare_each_dmabuf_buffer(ctrl, img,
						dva, DST_OFFSET, use_fence);
	if (ret)
		goto prepare_fail;

	return 0;

prepare_fail:
	fimg2d_finish_dmabuf_buffers(ctrl, cmd);

	return ret;
}

int fimg2d_map_each_dmabuf_buffer(struct fimg2d_control *ctrl,
		struct fimg2d_image *img,
		struct fimg2d_dma *base,
		struct fimg2d_dma_dva *dva, int offset)
{
	struct fimg2d_rect *r;
	int y1, y2, stride, clp_h, bpp;
	int direction;
	int ret = 0;

	if (offset < DST_OFFSET)
		direction = DMA_TO_DEVICE;
	else
		direction = DMA_FROM_DEVICE;

	base->iova = ion_iovmm_map(dva->attachment, 0, dva->dma_buf->size,
			direction, 0);
	if (IS_ERR_VALUE(base->iova)) {
		fimg2d_err("Failed to iovmm_map: %d\n", base->iova);
		base->iova = 0;
		return -EINVAL;
	}

	r = &img->rect;

	y1 = r->y1;
	y2 = r->y2;

	bpp = bit_per_pixel(img, 0);
	stride = width2bytes(img->width, bpp);
	clp_h = y2 - y1;

	base->size = stride * clp_h;

	exynos_ion_sync_dmabuf_for_device(ctrl->dev, dva->dma_buf,
				dva->dma_buf->size, direction);

	return ret;
}

static int fimg2d_map_dmabuf_buffers(struct fimg2d_control *ctrl,
					struct fimg2d_bltcmd *cmd)
{
	struct fimg2d_image *img;
	struct fimg2d_dma *base;
	struct fimg2d_dma_dva *dva;
	int ret = 0;
	int i;

	for (i = 0; i < MAX_SRC; i++) {
		img = &cmd->image_src[i];
		if (!img->addr.type)
			continue;

		base = &cmd->dma_src[i].base;
		dva = &cmd->dma_src[i].dva;

		ret = fimg2d_map_each_dmabuf_buffer(ctrl, img, base, dva, i);
		if (ret)
			goto map_fail;
	}

	/* Calculate destination DMA size */
	base = &cmd->dma_dst.base;
	dva = &cmd->dma_dst.dva;

	ret = fimg2d_map_each_dmabuf_buffer(ctrl, img, base, dva, DST_OFFSET);
	if (ret)
		goto map_fail;

	return 0;

map_fail:
	fimg2d_unmap_dmabuf_buffers(ctrl, cmd);

	return ret;
}

/* End: callback functions for dmabuf buffers */
static struct fimg2d_memops fimg2d_user_memops = {
	.prepare	= fimg2d_prepare_user_buffers,
	.map		= fimg2d_map_user_buffers,
	.unmap		= fimg2d_unmap_user_buffers,
	.finish		= fimg2d_finish_user_buffers,
};

static struct fimg2d_memops fimg2d_dmabuf_memops = {
	.prepare	= fimg2d_prepare_dmabuf_buffers,
	.map		= fimg2d_map_dmabuf_buffers,
	.unmap		= fimg2d_unmap_dmabuf_buffers,
	.finish		= fimg2d_finish_dmabuf_buffers,
};

int fimg2d_register_memops(struct fimg2d_bltcmd *cmd, enum addr_space addr_type)
{
	if (addr_type == ADDR_USER)
		cmd->memops = &fimg2d_user_memops;
	else if (addr_type == ADDR_DEVICE)
		cmd->memops = &fimg2d_dmabuf_memops;
	else
		return -EINVAL;

	return 0;
}
