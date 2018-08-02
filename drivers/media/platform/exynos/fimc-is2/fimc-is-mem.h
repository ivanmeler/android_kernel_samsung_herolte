/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_MEM_H
#define FIMC_IS_MEM_H

#include <linux/platform_device.h>
#include <media/videobuf2-core.h>
#if defined(CONFIG_VIDEOBUF2_CMA_PHYS)
#include <media/videobuf2-cma-phys.h>
#elif defined(CONFIG_VIDEOBUF2_ION)
#include <media/videobuf2-ion.h>
#endif

struct fimc_is_minfo {
	dma_addr_t	base;		/* buffer base */
	size_t		size;		/* total length */
	dma_addr_t	vaddr_base;	/* buffer base */
	dma_addr_t	vaddr_curr;	/* current addr */
	void		*bitproc_buf;
	void		*fw_cookie;

	dma_addr_t	dvaddr;
	ulong		kvaddr;
	u32		dvaddr_debug;
	ulong		kvaddr_debug;
	u32		dvaddr_fshared;
	ulong		kvaddr_fshared;
	u32		dvaddr_region;
	ulong		kvaddr_region;
	u32		dvaddr_shared; /*shared region of is region*/
	ulong		kvaddr_shared;
	u32		dvaddr_odc;
	ulong		kvaddr_odc;
	u32		dvaddr_dis;
	ulong		kvaddr_dis;
	u32		dvaddr_3dnr;
	ulong		kvaddr_3dnr;
	u32		dvaddr_fd[3];	/* FD map buffer region */
	ulong		kvaddr_fd[3];	/* NUM_FD_INTERNAL_BUF = 3 */
	u32		dvaddr_vra;
	ulong		kvaddr_vra;
};

struct fimc_is_vb2 {
	const struct vb2_mem_ops *ops;
	void *(*init)(struct platform_device *pdev);
	void (*cleanup)(void *alloc_ctx);

	unsigned long (*plane_dvaddr)(ulong cookie, u32 plane_no);
	unsigned long (*plane_kvaddr)(struct vb2_buffer *vb, u32 plane_no);
	unsigned long (*plane_cookie)(struct vb2_buffer *vb, u32 plane_no);

	int (*resume)(void *alloc_ctx);
	void (*suspend)(void *alloc_ctx);

	void (*set_cacheable)(void *alloc_ctx, bool cacheable);
};

struct fimc_is_mem {
	struct platform_device		*pdev;
	struct vb2_alloc_ctx		*alloc_ctx;

	const struct fimc_is_vb2	*vb2;
};

int fimc_is_mem_probe(struct fimc_is_mem *this,
	struct platform_device *pdev);

#endif
