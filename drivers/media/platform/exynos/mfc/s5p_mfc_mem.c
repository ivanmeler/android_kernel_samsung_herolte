/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_mem.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/dma-mapping.h>
#include <media/videobuf2-core.h>
#include <media/videobuf2-memops.h>
#include <asm/cacheflush.h>
#include <linux/firmware.h>

#include "s5p_mfc_mem.h"

struct vb2_mem_ops *s5p_mfc_mem_ops(void)
{
	return (struct vb2_mem_ops *)&vb2_ion_memops;
}

void s5p_mfc_mem_set_cacheable(void *alloc_ctx, bool cacheable)
{
	vb2_ion_set_cached(alloc_ctx, cacheable);
}

void s5p_mfc_mem_clean_priv(void *vb_priv, void *start, off_t offset,
							size_t size)
{
	vb2_ion_sync_for_device(vb_priv, offset, size, DMA_TO_DEVICE);
}

void s5p_mfc_mem_inv_priv(void *vb_priv, void *start, off_t offset,
							size_t size)
{
	vb2_ion_sync_for_device(vb_priv, offset, size, DMA_FROM_DEVICE);
}

int s5p_mfc_mem_clean_vb(struct vb2_buffer *vb, u32 num_planes)
{
	struct vb2_ion_cookie *cookie;
	int i;
	size_t size;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(vb, i);
		if (!cookie)
			continue;

		size = vb->v4l2_planes[i].length;
		vb2_ion_sync_for_device(cookie, 0, size, DMA_TO_DEVICE);
	}

	return 0;
}

int s5p_mfc_mem_inv_vb(struct vb2_buffer *vb, u32 num_planes)
{
	struct vb2_ion_cookie *cookie;
	int i;
	size_t size;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(vb, i);
		if (!cookie)
			continue;

		size = vb->v4l2_planes[i].length;
		vb2_ion_sync_for_device(cookie, 0, size, DMA_FROM_DEVICE);
	}

	return 0;
}

int s5p_mfc_mem_flush_vb(struct vb2_buffer *vb, u32 num_planes)
{
	struct vb2_ion_cookie *cookie;
	int i;
	enum dma_data_direction dir;
	size_t size;

	dir = V4L2_TYPE_IS_OUTPUT(vb->v4l2_buf.type) ?
					DMA_TO_DEVICE : DMA_FROM_DEVICE;

	for (i = 0; i < num_planes; i++) {
		cookie = vb2_plane_cookie(vb, i);
		if (!cookie)
			continue;

		size = vb->v4l2_planes[i].length;
		vb2_ion_sync_for_device(cookie, 0, size, dir);
	}

	return 0;
}

/* Allocate firmware */
int s5p_mfc_alloc_firmware(struct s5p_mfc_dev *dev)
{
	unsigned int base_align;
	size_t firmware_size;
	void *alloc_ctx;
	struct s5p_mfc_buf_size_v6 *buf_size;

	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	buf_size = dev->variant->buf_size->buf;
	base_align = dev->variant->buf_align->mfc_base_align;
	firmware_size = dev->variant->buf_size->firmware_code;
	dev->fw_region_size = firmware_size + buf_size->dev_ctx;
	alloc_ctx = dev->alloc_ctx;

	if (dev->fw_info.alloc)
		return 0;

	mfc_debug(2, "Allocating memory for firmware.\n");

	alloc_ctx = dev->alloc_ctx_fw;
	dev->fw_info.alloc = s5p_mfc_mem_alloc_priv(alloc_ctx, dev->fw_region_size);
	if (IS_ERR(dev->fw_info.alloc)) {
		dev->fw_info.alloc = 0;
		printk(KERN_ERR "Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	dev->fw_info.ofs = s5p_mfc_mem_daddr_priv(dev->fw_info.alloc);
	if (dev->fw_info.ofs & ((1 << base_align) - 1)) {
		mfc_err_dev("The base memory is not aligned to %dBytes.\n",
				(1 << base_align));
		s5p_mfc_mem_free_priv(dev->fw_info.alloc);
		dev->fw_info.ofs = 0;
		dev->fw_info.alloc = 0;
		return -EIO;
	}

	dev->fw_info.virt =
		s5p_mfc_mem_vaddr_priv(dev->fw_info.alloc);
	mfc_debug(2, "Virtual address for FW: %08lx\n",
			(long unsigned int)dev->fw_info.virt);
	if (!dev->fw_info.virt) {
		mfc_err_dev("Bitprocessor memory remap failed\n");
		s5p_mfc_mem_free_priv(dev->fw_info.alloc);
		dev->fw_info.ofs = 0;
		dev->fw_info.alloc = 0;
		return -EIO;
	}

	mfc_debug(2, "FW: %08lx size: %08zu\n",
			dev->fw_info.ofs,
			firmware_size);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	alloc_ctx = dev->alloc_ctx_drm_fw;

	dev->drm_fw_info.alloc = s5p_mfc_mem_alloc_priv(alloc_ctx, dev->fw_region_size);
	if (IS_ERR(dev->drm_fw_info.alloc)) {
		/* Release normal F/W buffer */
		s5p_mfc_mem_free_priv(dev->fw_info.alloc);
		dev->fw_info.ofs = 0;
		dev->fw_info.alloc = 0;
		printk(KERN_ERR "Allocating bitprocessor buffer failed\n");
		return -ENOMEM;
	}

	dev->drm_fw_info.ofs = s5p_mfc_mem_daddr_priv(dev->drm_fw_info.alloc);
	if (dev->drm_fw_info.ofs & ((1 << base_align) - 1)) {
		mfc_err_dev("The base memory is not aligned to %dBytes.\n",
				(1 << base_align));
		s5p_mfc_mem_free_priv(dev->drm_fw_info.alloc);
		/* Release normal F/W buffer */
		s5p_mfc_mem_free_priv(dev->fw_info.alloc);
		dev->fw_info.ofs = 0;
		dev->fw_info.alloc = 0;
		return -EIO;
	}

	dev->drm_fw_info.virt =
		s5p_mfc_mem_vaddr_priv(dev->drm_fw_info.alloc);
	mfc_info_dev("Virtual address for DRM FW: %08lx\n",
			(long unsigned int)dev->drm_fw_info.virt);
	if (!dev->drm_fw_info.virt) {
		mfc_err_dev("Bitprocessor memory remap failed\n");
		s5p_mfc_mem_free_priv(dev->drm_fw_info.alloc);
		/* Release normal F/W buffer */
		s5p_mfc_mem_free_priv(dev->fw_info.alloc);
		dev->fw_info.ofs = 0;
		dev->fw_info.alloc = 0;
		return -EIO;
	}

	dev->drm_fw_info.phys =
		s5p_mfc_mem_phys_addr(dev->drm_fw_info.alloc);

	mfc_info_dev("Port for DRM F/W : 0x%lx\n", dev->drm_fw_info.ofs);
#endif

	mfc_debug_leave();

	return 0;
}

/* Load firmware to MFC */
int s5p_mfc_load_firmware(struct s5p_mfc_dev *dev)
{
	struct firmware *fw_blob;
	size_t firmware_size;
	int err;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	firmware_size = dev->variant->buf_size->firmware_code;

	/* Firmare has to be present as a separate file or compiled
	 * into kernel. */
	mfc_debug_enter();
	mfc_debug(2, "Requesting fw\n");
	err = request_firmware((const struct firmware **)&fw_blob,
					MFC_FW_NAME, dev->v4l2_dev.dev);

	if (err != 0) {
		mfc_err_dev("Firmware is not present in the /lib/firmware directory nor compiled in kernel.\n");
		return -EINVAL;
	}

	mfc_debug(2, "Ret of request_firmware: %d Size: %zu\n", err, fw_blob->size);

	if (fw_blob->size > firmware_size) {
		mfc_err_dev("MFC firmware is too big to be loaded.\n");
		release_firmware(fw_blob);
		return -ENOMEM;
	}

	if (dev->fw_info.alloc == 0 || dev->fw_info.ofs == 0) {
		mfc_err_dev("MFC firmware is not allocated or was not mapped correctly.\n");
		release_firmware(fw_blob);
		return -EINVAL;
	}
	dev->fw_size = fw_blob->size;
	memcpy(dev->fw_info.virt, fw_blob->data, fw_blob->size);
	s5p_mfc_mem_clean_priv(dev->fw_info.alloc, dev->fw_info.virt, 0,
			fw_blob->size);
	s5p_mfc_mem_inv_priv(dev->fw_info.alloc, dev->fw_info.virt, 0,
			fw_blob->size);
	if (dev->drm_fw_info.virt) {
		memcpy(dev->drm_fw_info.virt, fw_blob->data, fw_blob->size);
		mfc_debug(2, "copy firmware to secure region\n");
		s5p_mfc_mem_clean_priv(dev->drm_fw_info.alloc,
				dev->drm_fw_info.virt, 0, fw_blob->size);
		s5p_mfc_mem_inv_priv(dev->drm_fw_info.alloc,
				dev->drm_fw_info.virt, 0, fw_blob->size);
	}
	release_firmware(fw_blob);
	mfc_debug_leave();
	return 0;
}

/* Release firmware memory */
int s5p_mfc_release_firmware(struct s5p_mfc_dev *dev)
{
	/* Before calling this function one has to make sure
	 * that MFC is no longer processing */
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (!dev->fw_info.alloc)
		return -EINVAL;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->drm_fw_info.alloc) {
		s5p_mfc_mem_free_priv(dev->drm_fw_info.alloc);
		dev->drm_fw_info.virt = 0;
		dev->drm_fw_info.alloc = 0;
		dev->drm_fw_info.ofs = 0;
	}
#endif
	s5p_mfc_mem_free_priv(dev->fw_info.alloc);

	dev->fw_info.virt =  0;
	dev->fw_info.ofs = 0;
	dev->fw_info.alloc = 0;

	return 0;
}
