/* include/media/videobuf2-ion.h
 *
 * Copyright 2011-2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Definition of Android ION memory allocator for videobuf2
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef _MEDIA_VIDEOBUF2_ION_H
#define _MEDIA_VIDEOBUF2_ION_H

#include <linux/scatterlist.h>
#include <linux/dma-mapping.h>
#include <linux/ion.h>
#include <linux/exynos_ion.h>
#include <linux/err.h>

/* flags to vb2_ion_create_context
 * These flags are dependet upon heap flags in ION.
 *
 * bit 0 ~ ION_NUM_HEAPS: ion heap flags
 * bit ION_NUM_HEAPS+1 ~ 20: non-ion flags (cached, iommu)
 * bit 21 ~ BITS_PER_INT - 1: ion specific flags
 */

/* Allocate physically contiguous memory */
#define VB2ION_CTX_PHCONTIG	EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK
/* Allocate virtually contiguous memory */
#define VB2ION_CTX_VMCONTIG	ION_HEAP_SYSTEM_MASK
/* Provide device a virtual address space */
#define VB2ION_CTX_IOMMU	(1 << (ION_NUM_HEAPS + 1))
/* Non-cached mapping to user when mmap */
#define VB2ION_CTX_UNCACHED	(1 << (ION_NUM_HEAPS + 2))

/* flags for contents protection */
#define VB2ION_CTX_DRM_MFCSH	(EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK | \
				ION_EXYNOS_MFC_SH_MASK)
#define VB2ION_CTX_DRM_VIDEO	(EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK | \
				ION_EXYNOS_MFC_OUTPUT_MASK)
#define VB2ION_CTX_DRM_MFCFW	(EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK | \
				ION_EXYNOS_MFC_FW_MASK)
#define VB2ION_CTX_DRM_MFCNFW	(EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK | \
				ION_EXYNOS_MFC_NFW_MASK)

#define VB2ION_CONTIG_ID_NUM	16
#define VB2ION_NUM_HEAPS	8
/* below 6 is the above vb2-ion flags (ION_NUM_HEAPS + 1 ~ 6) */
#if (BITS_PER_INT <= (VB2ION_CONTIG_ID_NUM + VB2ION_NUM_HEAPS + 6))
#error "Bits are too small to express all flags"
#endif

#define VB2ION_CTX_MASK_ION_HEAP	((1 << VB2ION_NUM_HEAPS) - 1)
#define VB2ION_CTX_MASK_ION_FLAG	~((1 << (BITS_PER_INT - \
					VB2ION_CONTIG_ID_NUM)) - 1)
/* mask out all non-ion flags */
#define ion_heapflag(flag)	(flag & VB2ION_CTX_MASK_ION_HEAP)
#define ion_flag(flag)		(flag & VB2ION_CTX_MASK_ION_FLAG)

struct device;
struct vb2_buffer;

/* vb2_ion_create_context - create a new vb2 context for buffer manipulation
 * dev - device that needs to use vb2
 * alignment - minimum alignment requirement for the start address of the
 *             allocated buffer from vb2.
 * flags - detailed control to the vb2 context. See above flags that stats
 *         with VB2ION_*
 *
 * This function creates a new videobuf2 context which is internal data of
 * videobuf2 for allocating and manipulating buffers. Drivers that obtain vb2
 * contexts must regard the contexts as keys for videobuf2 to access the
 * requirments of the drivers for the buffers allocated from videobuf2.
 *
 * Once a driver obtains vb2 contexts from vb2_ion_create_context(), it must
 * assign those contexts to alloc_ctxs[] argument of vb2_ops.queue_setup().
 *
 * Some specifications of a vb2 context can be changed after it has been created
 * and assigned to vb2 core with vb2_ops.queue_setup():
 * - vb2_ion_set_cached(): Changes cached attribute of the buffer which will be
 *   allocated after calling this function. Cached attribute of a buffer is
 *   effected when it is mapped to user with mmap() function call.
 * - vb2_ion_set_alignment(): Changes alignment requirement of the buffer which
 *   will be allocated after calling this function.
 *
 * For the devices that needs internal buffers for firmwares or devices'
 * context buffers, their drivers can generate a vb2 context which is not
 * handled by vb2 core but only by vb2-ion.
 * That kinds of vb2 contexts can be passed to the first parameter of
 * vb2_ion_private_alloc(void *alloc_ctx, size_t size).
 *
 * Drivers can generate vb2 contexts as many as they require with different
 * requirements specified in flags argument. The only _restriction_ on
 * generating a vb2 context is that the drivers must call
 * vb2_ion_create_context() in a kernel thread due to the behavior of
 * ion_client_create().
 */
void *vb2_ion_create_context(struct device *dev, size_t alignment, long flags);

/* vb2_ion_destroy_context - destroys a vb2 context
 * This function removes the given vb2 context which is created by
 * vb2_ion_create_context()
 */
void vb2_ion_destroy_context(void *ctx);

void vb2_ion_set_cached(void *ctx, bool cached);
int vb2_ion_set_alignment(void *ctx, size_t alignment);

/* Data type of the cookie returned by vb2_plane_cookie() function call.
 * The drivers do not need the definition of this structure. The only reason
 * why it is defined outside of videobuf2-ion.c is to make some functions
 * inline.
 */
struct vb2_ion_cookie {
	dma_addr_t ioaddr;
	dma_addr_t paddr;
	struct sg_table	*sgt;
	off_t offset;
};

/* vb2_ion_buffer_offset - return the mapped offset of the buffer
 * - cookie: pointer returned by vb2_plane_cookie()
 *
 * Returns offset value that the mapping starts from.
 */

static inline off_t vb2_ion_buffer_offset(void *cookie)
{
	return IS_ERR_OR_NULL(cookie) ?
		-EINVAL : ((struct vb2_ion_cookie *)cookie)->offset;
}

/* vb2_ion_phys_address - returns the physical address of the given buffer
 * - cookie: pointer returned by vb2_plane_cookie()
 * - phys_addr: pointer to the store of the physical address of the buffer
 *   specified by cookie.
 *
 * Returns -EINVAL if the buffer does not have nor physically contiguous memory.
 */
static inline int vb2_ion_phys_address(void *cookie, phys_addr_t *phys_addr)
{
	struct vb2_ion_cookie *vb2cookie = cookie;

	if (WARN_ON(!phys_addr || IS_ERR_OR_NULL(cookie)))
		return -EINVAL;

	if (vb2cookie->paddr) {
		*phys_addr = vb2cookie->paddr;
	} else {
		if (vb2cookie->sgt && vb2cookie->sgt->nents == 1) {
			*phys_addr = sg_phys(vb2cookie->sgt->sgl) +
						vb2cookie->offset;
		} else {
			*phys_addr = 0;
			return -EINVAL;
		}
	}

	return 0;
}

/* vb2_ion_dma_address - returns the DMA address that device can see
 * - cookie: pointer returned by vb2_plane_cookie()
 * - dma_addr: pointer to the store of the address of the buffer specified
 *   by cookie. It can be either IO virtual address or physical address
 *   depending on the specification of allocation context which allocated
 *   the buffer.
 *
 * Returns -EINVAL if the buffer has neither IO virtual address nor physically
 * contiguous memory
 */
static inline int vb2_ion_dma_address(void *cookie, dma_addr_t *dma_addr)
{
	struct vb2_ion_cookie *vb2cookie = cookie;

	if (WARN_ON(!dma_addr || IS_ERR_OR_NULL(cookie)))
		return -EINVAL;

	if (vb2cookie->ioaddr == 0)
		return vb2_ion_phys_address(cookie, (phys_addr_t *)dma_addr);

	*dma_addr = vb2cookie->ioaddr;

	return 0;
}

/* vb2_ion_get_sg - returns scatterlist of the given cookie.
 * - cookie: pointer returned by vb2_plane_cookie()
 * - nents: pointer to the store of number of elements in the returned
 *   scatterlist
 *
 * Returns the scatterlist of the buffer specified by cookie.
 * If the arguments are not correct, returns NULL.
 */
static inline struct scatterlist *vb2_ion_get_sg(void *cookie, int *nents)
{
	struct vb2_ion_cookie *vb2cookie = cookie;

	if (WARN_ON(!nents || IS_ERR_OR_NULL(cookie)))
		return NULL;

	*nents = vb2cookie->sgt->nents;
	return vb2cookie->sgt->sgl;
}

/***** Device's internal/context buffer support *****/

/* vb2_ion_private_vaddr - the kernelspace address for the given cookie
 * cookie - pointer returned by vb2_ion_private_alloc()
 *
 * This function returns minus error value on error.
 */
void *vb2_ion_private_vaddr(void *cookie);

/* vb2_ion_private_alloc - allocate a buffer for device drivers's private use
 * alloc_ctx - pointer returned by vb2_ion_create_context
 * size - size of the buffer allocated
 *
 * This function returns the pointer to a cookie that represents the allocated
 * buffer or minus error value. With the cookie you can:
 * - retrieve scatterlist of the buffer.
 * - retrieve dma address. (IO virutal address if IOMMU is enabled when
 *   creating alloc_ctx or physical address)
 * - retrieve virtual address in the kernel space.
 * - free the allocated buffer
 */
void *vb2_ion_private_alloc(void *alloc_ctx, size_t size);

/* vb2_ion_private_free - free the buffer allocated by vb2_ion_private_alloc */
void vb2_ion_private_free(void *cookie);

/***** Cache mainatenance operations *****/
void vb2_ion_sync_for_device(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir);
void vb2_ion_sync_for_cpu(void *cookie, off_t offset, size_t size,
						enum dma_data_direction dir);
int vb2_ion_buf_prepare(struct vb2_buffer *vb);
void vb2_ion_buf_finish(struct vb2_buffer *vb);
int vb2_ion_buf_prepare_exact(struct vb2_buffer *vb);
int vb2_ion_buf_finish_exact(struct vb2_buffer *vb);

/* IOMMU support */

/* vb2_ion_attach_iommu - enables IOMMU of the device specified in alloc_ctx */
int vb2_ion_attach_iommu(void *alloc_ctx);

/* vb2_ion_attach_iommu - disables IOMMU of the device specified in alloc_ctx */
void vb2_ion_detach_iommu(void *alloc_ctx);

extern const struct vb2_mem_ops vb2_ion_memops;
extern struct ion_device *ion_exynos; /* drivers/gpu/ion/exynos/exynos-ion.c */

#endif /* _MEDIA_VIDEOBUF2_ION_H */
