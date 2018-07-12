/*
 * include/linux/exynos_ion.h
 *
 * Copyright (C) 2011 Google, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef _LINUX_EXYNOS_ION_H
#define _LINUX_EXYNOS_ION_H

#include <linux/ion.h>
#include <linux/device.h>
#include <linux/dma-buf.h>
#include <linux/dma-direction.h>

#define EXYNOS_ION_HEAP_SYSTEM_ID 0
#define EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID 4
#define EXYNOS_ION_HEAP_CARVEOUT_ID EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID

#define EXYNOS_ION_HEAP_SYSTEM_MASK	(1 << EXYNOS_ION_HEAP_SYSTEM_ID)
#define EXYNOS_ION_HEAP_EXYNOS_CONTIG_MASK	\
				(1 << EXYNOS_ION_HEAP_EXYNOS_CONTIG_ID)
#define EXYNOS_ION_HEAP_CARVEOUT_MASK	\
				(1 << EXYNOS_ION_HEAP_CARVEOUT_ID)

enum {
	ION_EXYNOS_ID_COMMON		= 0,
	ION_EXYNOS_ID_VIDEO_EXT		= 1,
	ION_EXYNOS_ID_MFC_SH		= 2,
	ION_EXYNOS_ID_MSGBOX_SH		= 3,
	ION_EXYNOS_ID_FIMD_VIDEO	= 4,
	ION_EXYNOS_ID_GSC		= 5,
	ION_EXYNOS_ID_MFC_OUTPUT	= 6,
	ION_EXYNOS_ID_MFC_INPUT		= 7,
	ION_EXYNOS_ID_MFC_FW		= 8,
	ION_EXYNOS_ID_SECTBL		= 9,
	ION_EXYNOS_ID_G2D_WFD		= 10,
	ION_EXYNOS_ID_VIDEO             = 11,
	ION_EXYNOS_ID_MFC_NFW           = 12,
	ION_EXYNOS_ID_SECDMA		= 13,
	ION_EXYNOS_ID_AUDIO_FW		= 14,
	EXYNOS_ION_CONTIG_ID_NUM,
};

enum {
	ION_EXYNOS_HEAP_ID_CRYPTO		= 1,
	ION_EXYNOS_HEAP_ID_VIDEO_FW		= 2,
	ION_EXYNOS_HEAP_ID_VIDEO_STREAM		= 3,
	ION_EXYNOS_HEAP_ID_RESERVED		= 4,
	ION_EXYNOS_HEAP_ID_VIDEO_FRAME		= 5,
	ION_EXYNOS_HEAP_ID_VIDEO_SCALER		= 6,
	ION_EXYNOS_HEAP_ID_VIDEO_NFW		= 7,
	ION_EXYNOS_HEAP_ID_GPU_CRC		= 8,
	ION_EXYNOS_HEAP_ID_GPU_BUFFER		= 9,
	ION_EXYNOS_HEAP_ID_CAMERA		= 10,
	ION_EXYNOS_HEAP_ID_SECURE_CAMERA	= 11,
};

#define EXYNOS_ION_HEAP_CRYPTO_MASK		(1 << ION_EXYNOS_HEAP_ID_CRYPTO)
#define EXYNOS_ION_HEAP_VIDEO_FW_MASK		(1 << ION_EXYNOS_HEAP_ID_VIDEO_FW)
#define EXYNOS_ION_HEAP_VIDEO_STREAM_MASK	(1 << ION_EXYNOS_HEAP_ID_VIDEO_STREAM)
#define EXYNOS_ION_HEAP_VIDEO_FRAME_MASK	(1 << ION_EXYNOS_HEAP_ID_VIDEO_FRAME)
#define EXYNOS_ION_HEAP_VIDEO_SCALER_MASK	(1 << ION_EXYNOS_HEAP_ID_VIDEO_SCALER)
#define EXYNOS_ION_HEAP_VIDEO_NFW_MASK		(1 << ION_EXYNOS_HEAP_ID_VIDEO_NFW)
#define EXYNOS_ION_HEAP_GPU_CRC			(1 << ION_EXYNOS_HEAP_ID_GPU_CRC)
#define EXYNOS_ION_HEAP_GPU_BUFFER		(1 << ION_EXYNOS_HEAP_ID_GPU_BUFFER)
#define EXYNOS_ION_HEAP_CAMERA			(1 << ION_EXYNOS_HEAP_ID_CAMERA)
#define EXYNOS_ION_HEAP_SECURE_CAMERA		(1 << ION_EXYNOS_HEAP_ID_SECURE_CAMERA)

#ifndef BITS_PER_BYTE
#define BITS_PER_BYTE 8
#endif

#ifndef BITS_PER_INT
#define BITS_PER_INT (4 * BITS_PER_BYTE)
#endif

#define EXYNOS_ION_CONTIG_MAX_ID      16
#define EXYNOS_ION_CONTIG_ID_MASK	\
	~((1 << (BITS_PER_INT - EXYNOS_ION_CONTIG_ID_NUM)) - 1)

#define ION_EXYNOS_DEPRECATED_VIDEO_MASK				\
	(ION_EXYNOS_FIMD_VIDEO_MASK | ION_EXYNOS_MFC_OUTPUT_MASK |	\
	 ION_EXYNOS_MFC_INPUT_MASK)

#define FIXUP_CONTIG_ID(flag)						\
	!(flag & ION_EXYNOS_DEPRECATED_VIDEO_MASK) ? (flag) :		\
	((flag & ~ION_EXYNOS_DEPRECATED_VIDEO_MASK) | ION_EXYNOS_VIDEO_MASK)

#define MAKE_CONTIG_ID(flag)	\
	((flag & EXYNOS_ION_CONTIG_ID_MASK) ? \
	((BITS_PER_INT - fls64(flag & EXYNOS_ION_CONTIG_ID_MASK)) + 1) : 0)
#define MAKE_CONTIG_FLAG(id)	(1 << (BITS_PER_INT - id))

#ifdef CONFIG_OF_RESERVED_MEM
#define MAKE_CARVEOUT_ID(id)	\
	(BITS_PER_INT - EXYNOS_ION_CONTIG_MAX_ID + id)

#define __NEW_HEAP_MASK(id, flag) \
	((id & EXYNOS_ION_HEAP_CARVEOUT_MASK) ? \
	((id & ~EXYNOS_ION_HEAP_CARVEOUT_MASK) | \
	(1 << (MAKE_CARVEOUT_ID(MAKE_CONTIG_ID(flag))))) : id)
#define MAKE_NEW_HEAP_MASK(id, flag)	__NEW_HEAP_MASK(id, FIXUP_CONTIG_ID(flag))
#else
#define MAKE_NEW_HEAP_MASK(id, flag)	(id)
#endif

/*
 * The highest 16 bits in the flag argument to ion_alloc() is allocated for
 * mask values of region IDs of exynos_contig heap.
 * All flag of exynos_contig regions have their own bit position.
 * The bit positions of below masks are the same with the value
 * of each ID in the reserse order.
 * For example, mask value of ION_EXYNOS_ID_GSC is 0x08000000.
 * Note that no bit position is assigned to ION_EXYNOS_ID_COMMON.
 */
#define ION_EXYNOS_MFC_SH_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_SH)
#define ION_EXYNOS_MSGBOX_SH_MASK    MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MSGBOX_SH)
#define ION_EXYNOS_FIMD_VIDEO_MASK   MAKE_CONTIG_FLAG(ION_EXYNOS_ID_FIMD_VIDEO)
#define ION_EXYNOS_GSC_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_GSC)
#define ION_EXYNOS_MFC_OUTPUT_MASK   MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_OUTPUT)
#define ION_EXYNOS_MFC_INPUT_MASK    MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_INPUT)
#define ION_EXYNOS_MFC_FW_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_FW)
#define ION_EXYNOS_SECTBL_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_SECTBL)
#define ION_EXYNOS_G2D_WFD_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_G2D_WFD)
#define ION_EXYNOS_VIDEO_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_VIDEO)
#define ION_EXYNOS_MFC_NFW_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_MFC_NFW)
#define ION_EXYNOS_SECDMA_MASK	     MAKE_CONTIG_FLAG(ION_EXYNOS_ID_SECDMA)
#define ION_EXYNOS_VIDEO_EXT_MASK    MAKE_CONTIG_FLAG(ION_EXYNOS_ID_VIDEO_EXT)

/* return value of ion_exynos_contig_region_mask() that indicates error */
#define EXYNOS_CONTIG_REGION_NOMASK ~0

#ifdef __KERNEL__

#ifdef CONFIG_ION_EXYNOS
void exynos_ion_sync_dmabuf_for_device(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir);
void exynos_ion_sync_vaddr_for_device(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir);
void exynos_ion_sync_sg_for_device(struct device *dev, size_t size,
					struct sg_table *sgt,
					enum dma_data_direction dir);
void exynos_ion_sync_dmabuf_for_cpu(struct device *dev,
					struct dma_buf *dmabuf,
					size_t size,
					enum dma_data_direction dir);
void exynos_ion_sync_vaddr_for_cpu(struct device *dev,
					void *vaddr,
					size_t size,
					off_t offset,
					enum dma_data_direction dir);
void exynos_ion_sync_sg_for_cpu(struct device *dev, size_t size,
					struct sg_table *sgt,
					enum dma_data_direction dir);
unsigned int ion_exynos_contig_region_mask(char *region_name);
int ion_exynos_contig_heap_info(int region_id, phys_addr_t *phys, size_t *size);
int ion_exynos_contig_heap_isolate(int region_id);
void ion_exynos_contig_heap_deisolate(int region_id);
#else
#define exynos_ion_sync_dmabuf_for_device(dev, dmabuf, size, dir) \
							do { } while (0)
#define exynos_ion_sync_vaddr_for_device(dev, vaddr, size, offset, dir) \
							do { } while (0)
#define exynos_ion_sync_sg_for_device(dev, size, sgt, dir) do { } while (0)
#define exynos_ion_sync_dmabuf_for_cpu(dev, dmabuf, size, dir) do { } while (0)
#define exynos_ion_sync_vaddr_for_cpu(dev, vaddr, size, offset, dir) \
							do { } while (0)
#define exynos_ion_sync_sg_for_cpu(dev, size, sgt, dir) do { } while (0)
#define ion_secure_protect(buffer) do { } while (0)
#define ion_secure_unprotect(buffer) do { } while (0)
#define ion_is_heap_available(heap, flags, data) (1)

static inline int ion_exynos_contig_region_mask(char *region_name)
{
	return 0;
}

static inline int ion_exynos_contig_heap_info(int region_id, phys_addr_t *phys,
					size_t *size)
{
	return -ENODEV;
}

static inline int ion_exynos_contig_heap_isolate(int region_id)
{
	return -ENOSYS;
}

#define ion_exynos_contig_heap_deisolate(region_id) do { } while (0);

#endif

#if defined(CONFIG_ION_EXYNOS)
int init_exynos_ion_contig_heap(void);
#else
static inline int fdt_init_exynos_ion(void)
{
	return 0;
}
#endif

#endif /* __KERNEL */

#endif /* _LINUX_ION_H */
