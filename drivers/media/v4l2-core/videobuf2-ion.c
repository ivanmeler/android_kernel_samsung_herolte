/* linux/drivers/media/video/videobuf2-ion.c
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Implementation of Android ION memory allocator for videobuf2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
*/

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/file.h>
#include <linux/sched.h>
#include <linux/slab.h>
#include <linux/highmem.h>
#include <linux/dma-buf.h>
#include <linux/fs.h>
#include <linux/mutex.h>
#include <linux/vmalloc.h>

#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <media/videobuf2-ion.h>

#include <linux/exynos_iovmm.h>

#include <asm/cacheflush.h>

#define vb2ion_err(dev, fmt, ...) \
	dev_err(dev, "VB2ION: " pr_fmt(fmt), ##__VA_ARGS__)

struct vb2_ion_context {
	struct device		*dev;
	struct ion_client	*client;
	unsigned long		alignment;
	long			flags;

	/* protects iommu_active_cnt and protected */
	struct mutex		lock;
	int			iommu_active_cnt;
	bool			protected;
};

struct vb2_ion_buf {
	struct vb2_ion_context		*ctx;
	struct vb2_vmarea_handler	handler;
	struct vm_area_struct		*vma;
	struct ion_handle		*handle;
	struct dma_buf			*dma_buf;
	struct dma_buf_attachment	*attachment;
	enum dma_data_direction		direction;
	void				*kva;
	unsigned long			size;
	atomic_t			ref;
	bool				cached;
	bool				ion;
	struct vb2_ion_cookie		cookie;
};

#define CACHE_FLUSH_ALL_SIZE	SZ_8M
#define DMA_SYNC_SIZE		SZ_512K
#define OUTER_FLUSH_ALL_SIZE	SZ_1M

#define ctx_cached(ctx) (!(ctx->flags & VB2ION_CTX_UNCACHED))
#define ctx_iommu(ctx) (!!(ctx->flags & VB2ION_CTX_IOMMU))

void vb2_ion_set_cached(void *ctx, bool cached)
{
	struct vb2_ion_context *vb2ctx = ctx;

	if (cached)
		vb2ctx->flags &= ~VB2ION_CTX_UNCACHED;
	else
		vb2ctx->flags |= VB2ION_CTX_UNCACHED;
}
EXPORT_SYMBOL(vb2_ion_set_cached);

int vb2_ion_set_alignment(void *ctx, size_t alignment)
{
	struct vb2_ion_context *vb2ctx = ctx;

	if ((alignment != 0) && (alignment < PAGE_SIZE))
		return -EINVAL;

	if (alignment & ~alignment)
		return -EINVAL;

	if (alignment == 0)
		vb2ctx->alignment = PAGE_SIZE;
	else
		vb2ctx->alignment = alignment;

	return 0;
}
EXPORT_SYMBOL(vb2_ion_set_alignment);

void *vb2_ion_create_context(struct device *dev, size_t alignment, long flags)
{
	struct vb2_ion_context *ctx;

	 /* non-contigous memory without H/W virtualization is not supported */
	if ((flags & VB2ION_CTX_VMCONTIG) && !(flags & VB2ION_CTX_IOMMU))
		return ERR_PTR(-EINVAL);

	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx)
		return ERR_PTR(-ENOMEM);

	ctx->dev = dev;
	ctx->client = ion_client_create(ion_exynos, dev_name(dev));
	if (IS_ERR(ctx->client)) {
		void *retp = ctx->client;
		kfree(ctx);
		return retp;
	}

	vb2_ion_set_alignment(ctx, alignment);
	ctx->flags = flags;
	mutex_init(&ctx->lock);

	return ctx;
}
EXPORT_SYMBOL(vb2_ion_create_context);

void vb2_ion_destroy_context(void *ctx)
{
	struct vb2_ion_context *vb2ctx = ctx;

	mutex_destroy(&vb2ctx->lock);
	ion_client_destroy(vb2ctx->client);
	kfree(vb2ctx);
}
EXPORT_SYMBOL(vb2_ion_destroy_context);

void *vb2_ion_private_alloc(void *alloc_ctx, size_t size)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	struct vb2_ion_buf *buf;
	int heapflags = ion_heapflag(ctx->flags);
	int flags = ion_flag(ctx->flags);
	int ret = 0;

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf)
		return ERR_PTR(-ENOMEM);

	size = PAGE_ALIGN(size);

	flags |= ctx_cached(ctx) ?
		ION_FLAG_CACHED | ION_FLAG_CACHED_NEEDS_SYNC : 0;

	if ((ion_flag(ctx->flags) & VB2ION_CTX_DRM_VIDEO) ||
			(ion_flag(ctx->flags) & VB2ION_CTX_DRM_MFCFW))
		flags |= ION_FLAG_PROTECTED;

	buf->handle = ion_alloc(ctx->client, size, ctx->alignment,
				heapflags, flags);
	if (IS_ERR(buf->handle)) {
		ret = -ENOMEM;
		goto err_alloc;
	}

	buf->dma_buf = ion_share_dma_buf(ctx->client, buf->handle);
	if (IS_ERR(buf->dma_buf)) {
		ret = PTR_ERR(buf->dma_buf);
		goto err_share;
	}

	buf->attachment = dma_buf_attach(buf->dma_buf, ctx->dev);
	if (IS_ERR(buf->attachment)) {
		ret = PTR_ERR(buf->attachment);
		goto err_attach;
	}

	buf->cookie.sgt = dma_buf_map_attachment(
				buf->attachment, DMA_BIDIRECTIONAL);
	if (IS_ERR(buf->cookie.sgt)) {
		ret = PTR_ERR(buf->cookie.sgt);
		goto err_map_dmabuf;
	}

	buf->ctx = ctx;
	buf->size = size;
	buf->cached = ctx_cached(ctx);
	buf->direction = DMA_BIDIRECTIONAL;
	buf->ion = true;

	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected) {
		buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
						   buf->size, 0, 0);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			ret = (int)buf->cookie.ioaddr;
			mutex_unlock(&ctx->lock);
			goto err_ion_map_io;
		}
	}
	mutex_unlock(&ctx->lock);

	return &buf->cookie;

err_ion_map_io:
	if (buf->kva)
		ion_unmap_kernel(ctx->client, buf->handle);
	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
				DMA_BIDIRECTIONAL);
err_map_dmabuf:
	dma_buf_detach(buf->dma_buf, buf->attachment);
err_attach:
	dma_buf_put(buf->dma_buf);
err_share:
	ion_free(ctx->client, buf->handle);
err_alloc:
	kfree(buf);

	pr_err("%s: Error occured while allocating\n", __func__);
	return ERR_PTR(ret);
}

void vb2_ion_private_free(void *cookie)
{
	struct vb2_ion_buf *buf =
			container_of(cookie, struct vb2_ion_buf, cookie);
	struct vb2_ion_context *ctx;

	if (WARN_ON(IS_ERR_OR_NULL(cookie)))
		return;

	ctx = buf->ctx;
	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected)
		ion_iovmm_unmap(buf->attachment, buf->cookie.ioaddr);
	mutex_unlock(&ctx->lock);

	dma_buf_unmap_attachment(buf->attachment, buf->cookie.sgt,
				DMA_BIDIRECTIONAL);
	dma_buf_detach(buf->dma_buf, buf->attachment);
	dma_buf_put(buf->dma_buf);

	if (buf->kva)
		ion_unmap_kernel(ctx->client, buf->handle);
	ion_free(ctx->client, buf->handle);

	kfree(buf);
}

static void vb2_ion_put(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (atomic_dec_and_test(&buf->ref))
		vb2_ion_private_free(&buf->cookie);
}

static void *vb2_ion_alloc(void *alloc_ctx, unsigned long size, gfp_t gfp_flags)
{
	struct vb2_ion_buf *buf;
	void *cookie;

	cookie = vb2_ion_private_alloc(alloc_ctx, size);
	if (IS_ERR(cookie))
		return cookie;

	buf = container_of(cookie, struct vb2_ion_buf, cookie);

	buf->handler.refcount = &buf->ref;
	buf->handler.put = vb2_ion_put;
	buf->handler.arg = buf;
	atomic_set(&buf->ref, 1);

	return buf;
}


void *vb2_ion_private_vaddr(void *cookie)
{
	struct vb2_ion_buf *buf =
			container_of(cookie, struct vb2_ion_buf, cookie);
	if (WARN_ON(IS_ERR_OR_NULL(cookie)))
		return NULL;

	if (!buf->kva) {
		buf->kva = ion_map_kernel(buf->ctx->client, buf->handle);
		if (IS_ERR_OR_NULL(buf->kva))
			buf->kva = NULL;

		buf->kva += buf->cookie.offset;
	}

	return buf->kva;
}

static void *vb2_ion_cookie(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	return (void *)&buf->cookie;
}

static void *vb2_ion_vaddr(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return NULL;

	if (buf->kva != NULL)
		return buf->kva;

	if (buf->handle)
		return vb2_ion_private_vaddr(&buf->cookie);

	if (dma_buf_begin_cpu_access(buf->dma_buf,
		0, buf->size, buf->direction))
		return NULL;

	buf->kva = dma_buf_kmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE);

	if (buf->kva == NULL)
		dma_buf_end_cpu_access(buf->dma_buf, 0,
			buf->size, buf->direction);
	else
		buf->kva += buf->cookie.offset & ~PAGE_MASK;

	return buf->kva;
}

static unsigned int vb2_ion_num_users(void *buf_priv)
{
	struct vb2_ion_buf *buf = buf_priv;

	if (WARN_ON(!buf))
		return 0;

	return atomic_read(&buf->ref);
}

static int vb2_ion_mmap(void *buf_priv, struct vm_area_struct *vma)
{
	struct vb2_ion_buf *buf = buf_priv;
	unsigned long vm_start = vma->vm_start;
	unsigned long vm_end = vma->vm_end;
	struct scatterlist *sg = buf->cookie.sgt->sgl;
	unsigned long size;
	int ret = -EINVAL;

	if (buf->size  < (vm_end - vm_start))
		return ret;

	if (!buf->cached)
		vma->vm_page_prot = pgprot_writecombine(vma->vm_page_prot);

	size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));

	ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
				size, vma->vm_page_prot);

	for (sg = sg_next(sg), vm_start += size;
			!ret && sg && (vm_start < vm_end);
			vm_start += size, sg = sg_next(sg)) {
		size = min_t(size_t, vm_end - vm_start, sg_dma_len(sg));
		ret = remap_pfn_range(vma, vm_start, page_to_pfn(sg_page(sg)),
						size, vma->vm_page_prot);
	}

	if (ret)
		return ret;

	if (vm_start < vm_end)
		return -EINVAL;

	vma->vm_flags		|= VM_DONTEXPAND;
	vma->vm_private_data	= &buf->handler;
	vma->vm_ops		= &vb2_common_vm_ops;

	vma->vm_ops->open(vma);

	return ret;
}

static int vb2_ion_map_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;
	struct vb2_ion_context *ctx = buf->ctx;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to pin a non attached buffer\n");
		return -EINVAL;
	}

	if (WARN_ON(buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already pinned\n");
		return 0;
	}

	/* get the associated scatterlist for this buffer */
	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
						buf->direction);
	if (IS_ERR_OR_NULL(buf->cookie.sgt)) {
		pr_err("Error getting dmabuf scatterlist\n");
		return -EINVAL;
	}

	buf->cookie.offset = 0;
	buf->cookie.paddr = sg_phys(buf->cookie.sgt->sgl) + buf->cookie.offset;

	mutex_lock(&ctx->lock);
	if (ctx_iommu(ctx) && !ctx->protected && buf->cookie.ioaddr == 0) {
		buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
					buf->size, buf->direction, 0);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			pr_err("buf->cookie.ioaddr is error: %pa\n",
					&buf->cookie.ioaddr);
			mutex_unlock(&ctx->lock);
			dma_buf_unmap_attachment(buf->attachment,
					buf->cookie.sgt, buf->direction);
			return (int)buf->cookie.ioaddr;
		}
	}
	mutex_unlock(&ctx->lock);

	return 0;
}

static void vb2_ion_unmap_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	if (WARN_ON(!buf->attachment)) {
		pr_err("trying to unpin a not attached buffer\n");
		return;
	}

	if (WARN_ON(!buf->cookie.sgt)) {
		pr_err("dmabuf buffer is already unpinned\n");
		return;
	}

	dma_buf_unmap_attachment(buf->attachment,
			buf->cookie.sgt, buf->direction);

	buf->cookie.sgt = NULL;
}

static void vb2_ion_detach_dmabuf(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;
	struct vb2_ion_context *ctx = buf->ctx;

	mutex_lock(&ctx->lock);
	if (buf->cookie.ioaddr && ctx_iommu(ctx) && !ctx->protected ) {
		ion_iovmm_unmap(buf->attachment, buf->cookie.ioaddr);
		buf->cookie.ioaddr = 0;
	}
	mutex_unlock(&ctx->lock);

	if (buf->kva != NULL) {
		dma_buf_kunmap(buf->dma_buf, 0, buf->kva);
		dma_buf_end_cpu_access(buf->dma_buf, 0, buf->size, 0);
	}

	/* detach this attachment */
	dma_buf_detach(buf->dma_buf, buf->attachment);
	kfree(buf);
}

static void *vb2_ion_attach_dmabuf(void *alloc_ctx, struct dma_buf *dbuf,
				  unsigned long size, int write)
{
	struct vb2_ion_buf *buf;
	struct dma_buf_attachment *attachment;

	if (dbuf->size < size) {
		WARN(1, "dbuf->size(%zd) is smaller than size(%ld)\n",
				dbuf->size, size);
		return ERR_PTR(-EFAULT);
	}

	buf = kzalloc(sizeof *buf, GFP_KERNEL);
	if (!buf) {
		pr_err("out of memory\n");
		return ERR_PTR(-ENOMEM);
	}

	buf->ctx = alloc_ctx;
	/* create attachment for the dmabuf with the user device */
	attachment = dma_buf_attach(dbuf, buf->ctx->dev);
	if (IS_ERR(attachment)) {
		pr_err("failed to attach dmabuf\n");
		kfree(buf);
		return attachment;
	}

	buf->direction = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;
	buf->dma_buf = dbuf;
	buf->ion = true;
	buf->attachment = attachment;

	return buf;
}

static void vb2_ion_put_vma(struct vm_area_struct *vma)
{
	while (vma) {
		struct vm_area_struct *tmp;

		tmp = vma;
		vma = vma->vm_prev;
		vb2_put_vma(tmp);
	}
}

/*
 * Verify the user's input and
 * obtain the reference to the vma against the user's area
 * Returns the last node of doubly linked list of replicated vma.
 */
static struct vm_area_struct *vb2_ion_get_vma(struct device *dev,
				unsigned long vaddr, unsigned long size)
{
	struct vm_area_struct *new_vma = NULL;
	struct vm_area_struct *vma;
	unsigned long addr = vaddr;
	unsigned long len = size;

	if ((vaddr + size) <= vaddr) {
		vb2ion_err(dev, "size overflow in user area, [%#lx, %#lx)\n",
				vaddr, size);
		return NULL;
	}

	for (vma = find_vma(current->mm, addr);
		vma && len && (addr >= vma->vm_start); vma = vma->vm_next) {
		struct vm_area_struct *cur_vma;

		cur_vma = vb2_get_vma(vma);

		if (new_vma) {
			if ((cur_vma->vm_file != new_vma->vm_file) ||
				(cur_vma->vm_ops != new_vma->vm_ops)) {
				vb2ion_err(dev,
					"[%#lx, %#lx) crosses disparate vmas\n",
					vaddr, size);
				vb2_put_vma(cur_vma);
				break;
			}
			new_vma->vm_next = cur_vma;
		}
		cur_vma->vm_prev = new_vma;
		new_vma = cur_vma;

		if ((addr + len) <= vma->vm_end) {
			addr = addr + len;
			len = 0;
		} else {
			len -= vma->vm_end - addr;
			addr = vma->vm_end;
		}
	}

	if (len) { /* error detected */
		vb2ion_err(dev, "Invalid user area [%#lx, %#lx)\n",
				vaddr, size);
		vb2_ion_put_vma(new_vma);
		return NULL;
	}

	while (new_vma && new_vma->vm_prev)
		new_vma = new_vma->vm_prev;

	return new_vma;
}

static void vb2_ion_put_userptr_dmabuf(struct vb2_ion_context *ctx,
					struct vb2_ion_buf *buf)
{
	if (ctx_iommu(ctx))
		ion_iovmm_unmap(buf->attachment,
				buf->cookie.ioaddr - buf->cookie.offset);

	dma_buf_unmap_attachment(buf->attachment,
				 buf->cookie.sgt, buf->direction);
	dma_buf_detach(buf->dma_buf, buf->attachment);
}

static void *vb2_ion_get_userptr_dmabuf(struct vb2_ion_context *ctx,
				struct vb2_ion_buf *buf, unsigned long vaddr)
{
	void *ret;

	buf->attachment = dma_buf_attach(buf->dma_buf, ctx->dev);
	if (IS_ERR(buf->attachment)) {
		dev_err(ctx->dev,
			"%s: Failed to pin user buffer @ %#lx/%#lx\n",
			__func__, vaddr, buf->size);
		return ERR_CAST(buf->attachment);
	}

	buf->cookie.sgt = dma_buf_map_attachment(buf->attachment,
						buf->direction);
	if (IS_ERR(buf->cookie.sgt)) {
		dev_err(ctx->dev,
			"%s: Failed to get sgt of user buffer @ %#lx/%#lx\n",
			__func__, vaddr, buf->size);
		ret = ERR_CAST(buf->cookie.sgt);
		goto err_map;
	}

	if (ctx_iommu(ctx)) {
		buf->cookie.ioaddr = ion_iovmm_map(buf->attachment, 0,
					buf->size, buf->direction, 0);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			ret = ERR_PTR(buf->cookie.ioaddr);
			goto err_iovmm;
		}

		buf->cookie.ioaddr += buf->cookie.offset;
	}

	return NULL;
err_iovmm:
	dma_buf_unmap_attachment(buf->attachment,
				 buf->cookie.sgt, buf->direction);
err_map:
	dma_buf_detach(buf->dma_buf, buf->attachment);
	return ret;
}

static void *vb2_ion_get_userptr(void *alloc_ctx, unsigned long vaddr,
				unsigned long size, int write)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	struct vb2_ion_buf *buf = NULL;
	struct vm_area_struct *vma;
	void *p_ret = ERR_PTR(-ENOMEM);;

	if (ctx->protected) {
		dev_err(ctx->dev,
			"%s: protected mode is not supported with userptr\n",
			__func__);
		return ERR_PTR(-EINVAL);
	}

	vma = vb2_ion_get_vma(ctx->dev, vaddr, size);
	if (!vma) {
		dev_err(ctx->dev,
			"%s: Failed to holding user buffer @ %#lx/%#lx\n",
			__func__, vaddr, size);
		return ERR_PTR(-EINVAL);
	}

	buf = kzalloc(sizeof(*buf), GFP_KERNEL);
	if (!buf) {
		dev_err(ctx->dev, "%s: Not enough memory\n", __func__);
		p_ret = ERR_PTR(-ENOMEM);
		goto err_alloc;
	}

	if (vma->vm_file) {
		get_file(vma->vm_file);
		buf->dma_buf = get_dma_buf_file(vma->vm_file);
	}

	buf->ctx = ctx;
	buf->direction = write ? DMA_FROM_DEVICE : DMA_TO_DEVICE;
	buf->size = size;
	buf->vma = vma;

	if (buf->dma_buf) {
		buf->ion = true;
		buf->cookie.offset = vaddr - vma->vm_start;

		p_ret = vb2_ion_get_userptr_dmabuf(ctx, buf, vaddr);
		if (IS_ERR(p_ret))
			goto err_map;
	} else if (ctx_iommu(ctx)) {
		int prot = IOMMU_READ;

		if (write)
			prot |= IOMMU_WRITE;
		buf->cookie.ioaddr = exynos_iovmm_map_userptr(
						ctx->dev, vaddr, size, prot);
		if (IS_ERR_VALUE(buf->cookie.ioaddr)) {
			p_ret = ERR_PTR(buf->cookie.ioaddr);
			goto err_map;
		}
	}

	if ((pgprot_noncached(buf->vma->vm_page_prot)
				== buf->vma->vm_page_prot)
			|| (pgprot_writecombine(buf->vma->vm_page_prot)
				== buf->vma->vm_page_prot))
		buf->cached = false;
	else
		buf->cached = true;

	return buf;
err_map:
	if (buf->dma_buf)
		dma_buf_put(buf->dma_buf);
	if (vma->vm_file)
		fput(vma->vm_file);
	kfree(buf);
err_alloc:
	vb2_ion_put_vma(vma);

	return p_ret;
}

static void vb2_ion_put_userptr(void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;

	/* TODO: kva with non dmabuf */
	if (buf->kva) {
		dma_buf_kunmap(buf->dma_buf, buf->cookie.offset / PAGE_SIZE,
				buf->kva - (buf->cookie.offset & ~PAGE_SIZE));
		dma_buf_end_cpu_access(buf->dma_buf, buf->cookie.offset,
					buf->size, DMA_FROM_DEVICE);
	}

	if (buf->dma_buf)
		vb2_ion_put_userptr_dmabuf(buf->ctx, buf);
	else if (ctx_iommu(buf->ctx))
		exynos_iovmm_unmap_userptr(buf->ctx->dev, buf->cookie.ioaddr);

	if (buf->dma_buf)
		dma_buf_put(buf->dma_buf);
	if (buf->vma->vm_file)
		fput(buf->vma->vm_file);

	vb2_ion_put_vma(buf->vma);

	kfree(buf);
}

static int vb2_ion_verify_userptr(unsigned long vaddr, void *mem_priv)
{
	struct vb2_ion_buf *buf = mem_priv;
	struct vm_area_struct *vma = find_vma(current->mm, vaddr);
	struct dma_buf *dbuf;

	if (!vma || !vma->vm_file)
		return 1;

	dbuf = get_dma_buf_file(vma->vm_file);
	if (!dbuf)
		return 1;

	/*
	 * We just compare the pointer values
	 * but the contents of the data pointed by them
	 */
	dma_buf_put(dbuf);

	return (dbuf == buf->dma_buf) ? 0 : 1;
}

const struct vb2_mem_ops vb2_ion_memops = {
	.alloc		= vb2_ion_alloc,
	.put		= vb2_ion_put,
	.cookie		= vb2_ion_cookie,
	.vaddr		= vb2_ion_vaddr,
	.mmap		= vb2_ion_mmap,
	.map_dmabuf	= vb2_ion_map_dmabuf,
	.unmap_dmabuf	= vb2_ion_unmap_dmabuf,
	.attach_dmabuf	= vb2_ion_attach_dmabuf,
	.detach_dmabuf	= vb2_ion_detach_dmabuf,
	.get_userptr	= vb2_ion_get_userptr,
	.put_userptr	= vb2_ion_put_userptr,
	.num_users	= vb2_ion_num_users,
	.verify_userptr	= vb2_ion_verify_userptr,
};
EXPORT_SYMBOL_GPL(vb2_ion_memops);

void vb2_ion_sync_for_device(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	dev_dbg(buf->ctx->dev, "syncing for device, dmabuf: %p, kva: %p, "
		"size: %zd, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->kva) {
		BUG_ON((offset < 0) || (offset > buf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > buf->size) || ((offset + size) > buf->size));

		exynos_ion_sync_vaddr_for_device(buf->ctx->dev,
						buf->kva, size, offset, dir);
	} else if (buf->dma_buf) {
		exynos_ion_sync_dmabuf_for_device(buf->ctx->dev,
						buf->dma_buf, size, dir);
	} else if (buf->vma && buf->cached) {
		if (size < CACHE_FLUSH_ALL_SIZE)
			exynos_iommu_sync_for_device(buf->ctx->dev,
						buf->cookie.ioaddr, size, dir);
		else
			flush_all_cpu_caches();
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_device);

void vb2_ion_sync_for_cpu(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir)
{
	struct vb2_ion_buf *buf = container_of(cookie,
					struct vb2_ion_buf, cookie);

	dev_dbg(buf->ctx->dev, "syncing for cpu, dmabuf: %p, kva: %p, "
		"size: %zd, dir: %d\n", buf->dma_buf, buf->kva, size, dir);

	if (buf->kva) {
		BUG_ON((offset < 0) || (offset > buf->size));
		BUG_ON((offset + size) < size);
		BUG_ON((size > buf->size) || ((offset + size) > buf->size));

		exynos_ion_sync_vaddr_for_cpu(buf->ctx->dev,
						buf->kva, size, offset, dir);
	} else if (buf->dma_buf) {
		exynos_ion_sync_dmabuf_for_cpu(buf->ctx->dev,
						buf->dma_buf, size, dir);
	} else if (buf->vma && buf->cached) {
		if (size < CACHE_FLUSH_ALL_SIZE)
			exynos_iommu_sync_for_cpu(buf->ctx->dev,
						buf->cookie.ioaddr, size, dir);
		else
			flush_all_cpu_caches();
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_sync_for_cpu);

int vb2_ion_buf_prepare(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_device((void *) &buf->cookie, 0,
							buf->size, dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_prepare);

void vb2_ion_buf_finish(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_cpu((void *) &buf->cookie, 0, buf->size, dir);
	}
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_finish);

int vb2_ion_buf_prepare_exact(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_device((void *) &buf->cookie, 0,
					vb2_get_plane_payload(vb, i), dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_prepare_exact);

int vb2_ion_buf_finish_exact(struct vb2_buffer *vb)
{
	int i;
	enum dma_data_direction dir;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < vb->num_planes; i++) {
		struct vb2_ion_buf *buf = vb->planes[i].mem_priv;

		vb2_ion_sync_for_cpu((void *) &buf->cookie, 0,
					vb2_get_plane_payload(vb, i), dir);
	}

	return 0;
}
EXPORT_SYMBOL_GPL(vb2_ion_buf_finish_exact);

void vb2_ion_detach_iommu(void *alloc_ctx)
{
	struct vb2_ion_context *ctx = alloc_ctx;

	if (!ctx_iommu(ctx))
		return;

	mutex_lock(&ctx->lock);
	BUG_ON(ctx->iommu_active_cnt == 0);

	if (--ctx->iommu_active_cnt == 0 && !ctx->protected)
		iovmm_deactivate(ctx->dev);
	mutex_unlock(&ctx->lock);
}
EXPORT_SYMBOL_GPL(vb2_ion_detach_iommu);

int vb2_ion_attach_iommu(void *alloc_ctx)
{
	struct vb2_ion_context *ctx = alloc_ctx;
	int ret = 0;

	if (!ctx_iommu(ctx))
		return -ENOENT;

	mutex_lock(&ctx->lock);
	if (ctx->iommu_active_cnt == 0 && !ctx->protected)
		ret = iovmm_activate(ctx->dev);
	if (!ret)
		ctx->iommu_active_cnt++;
	mutex_unlock(&ctx->lock);

	return ret;
}
EXPORT_SYMBOL_GPL(vb2_ion_attach_iommu);

MODULE_AUTHOR("Cho KyongHo <pullip.cho@samsung.com>");
MODULE_AUTHOR("Jinsung Yang <jsgood.yang@samsung.com>");
MODULE_DESCRIPTION("Android ION allocator handling routines for videobuf2");
MODULE_LICENSE("GPL");
