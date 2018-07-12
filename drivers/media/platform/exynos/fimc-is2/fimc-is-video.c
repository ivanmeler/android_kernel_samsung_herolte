/*
* Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is video functions
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/module.h>
#include <linux/delay.h>
#include <linux/kernel.h>
#include <linux/errno.h>
#include <linux/interrupt.h>
#include <linux/device.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <video/videonode.h>
#include <media/exynos_mc.h>
#include <asm/cacheflush.h>
#include <asm/pgtable.h>
#include <linux/firmware.h>
#include <linux/dma-mapping.h>
#include <linux/scatterlist.h>
#include <linux/videodev2.h>
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>
#include <linux/syscalls.h>

#include <media/videobuf2-core.h>
#include <media/v4l2-ctrls.h>
#include <media/v4l2-device.h>
#include <media/v4l2-mem2mem.h>
#include <media/v4l2-mediabus.h>
#include <media/exynos_mc.h>

#include "fimc-is-time.h"
#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-debug.h"
#include "fimc-is-video.h"

#define SPARE_PLANE 1
#define SPARE_SIZE (32 * 1024)

struct fimc_is_fmt fimc_is_formats[] = {
	{
		.name		= "YUV 4:4:4 packed, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV444,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= 0, /* Not Defined */
		.bitwidth	= 24,
		.bitsperpixel	= { 24 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV444,
		.hw_order	= DMA_OUTPUT_ORDER_YCbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, YCbYCr",
		.pixelformat	= V4L2_PIX_FMT_YUYV,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_YUYV8_2X8,
		.bitwidth	= 16,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_YCbYCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 1,
	}, {
		.name		= "YUV 4:2:2 packed, CbYCrY",
		.pixelformat	= V4L2_PIX_FMT_UYVY,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_UYVY8_2X8,
		.bitwidth	= 16,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbYCrY,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 1,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV16,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 16,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YUV 4:2:2 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV61,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 16,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YUV 4:2:2 planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV422P,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 16,
		.bitsperpixel	= { 8, 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV422,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 3,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YUV420,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 3,
	}, {
		.name		= "YUV 4:2:0 planar, YCbCr",
		.pixelformat	= V4L2_PIX_FMT_YVU420,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 3,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YUV 4:2:0 planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 2-planar, Y/CbCr",
		.pixelformat	= V4L2_PIX_FMT_NV12M,
		.num_planes	= 2 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CbCr,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YVU 4:2:0 non-contiguous 2-planar, Y/CrCb",
		.pixelformat	= V4L2_PIX_FMT_NV21M,
		.num_planes	= 2 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_CrCb,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 2,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cb/Cr",
		.pixelformat	= V4L2_PIX_FMT_YUV420M,
		.num_planes	= 3 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 3,
	}, {
		.name		= "YUV 4:2:0 non-contiguous 3-planar, Y/Cr/Cb",
		.pixelformat	= V4L2_PIX_FMT_YVU420M,
		.num_planes	= 3 + SPARE_PLANE,
		.bitwidth	= 12,
		.bitsperpixel	= { 8, 4, 4 },
		.hw_format	= DMA_OUTPUT_FORMAT_YUV420,
		.hw_order	= DMA_OUTPUT_ORDER_NO,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 3,
	}, {
		.name		= "BAYER 8 bit(GRBG)",
		.pixelformat	= V4L2_PIX_FMT_SGRBG8,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 8,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 1,
	}, {
		.name		= "BAYER 8 bit(BA81)",
		.pixelformat	= V4L2_PIX_FMT_SBGGR8,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 8,
		.bitsperpixel	= { 8 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_8BIT,
		.hw_plane	= 1,
	}, {
		.name		= "BAYER 10 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR10,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 10,
		.bitsperpixel	= { 10 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_10BIT, /* memory width per pixel */
		.hw_plane	= 1,
	}, {
		.name		= "BAYER 12 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR12,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 10, /* FIXME */
		.bitsperpixel	= { 12 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER_PACKED,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_12BIT, /* memory width per pixel */
		.hw_plane	= 1,
	}, {
		.name		= "BAYER 16 bit",
		.pixelformat	= V4L2_PIX_FMT_SBGGR16,
		.num_planes	= 1 + SPARE_PLANE,
		.bitwidth	= 16,
		.bitsperpixel	= { 16 },
		.hw_format	= DMA_OUTPUT_FORMAT_BAYER,
		.hw_order	= DMA_OUTPUT_ORDER_GB_BG,
		.hw_bitwidth	= DMA_OUTPUT_BIT_WIDTH_16BIT, /* memory width per pixel */
		.hw_plane	= 1,
	}, {
		.name		= "JPEG",
		.pixelformat	= V4L2_PIX_FMT_JPEG,
		.num_planes	= 1 + SPARE_PLANE,
		.mbus_code	= V4L2_MBUS_FMT_JPEG_1X8,
		.bitwidth	= 8,
		.bitsperpixel	= { 8 },
		.hw_format	= 0,
		.hw_order	= 0,
		.hw_bitwidth	= 0,
		.hw_plane	= 0,
	}
};

struct fimc_is_fmt *fimc_is_find_format(u32 pixelformat,
	u32 mbus_code)
{
	ulong i;
	struct fimc_is_fmt *result, *fmt;

	result = NULL;

	for (i = 0; i < ARRAY_SIZE(fimc_is_formats); ++i) {
		fmt = &fimc_is_formats[i];
		if (pixelformat && (fmt->pixelformat == pixelformat)) {
			result = fmt;
			break;
		}

		if (mbus_code && (fmt->mbus_code == mbus_code)) {
			result = fmt;
			break;
		}
	}

	return result;
}

void fimc_is_set_plane_size(struct fimc_is_frame_cfg *frame, unsigned int sizes[])
{
	u32 plane;
	u32 width[FIMC_IS_MAX_PLANES];

	BUG_ON(!frame);
	BUG_ON(!frame->format);

	for (plane = 0; plane < FIMC_IS_MAX_PLANES; ++plane)
		width[plane] = max(frame->width * frame->format->bitsperpixel[plane] / BITS_PER_BYTE,
					frame->bytesperline[plane]);

	switch (frame->format->pixelformat) {
	case V4L2_PIX_FMT_YUV444:
		dbg("V4L2_PIX_FMT_YUV444(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUYV:
	case V4L2_PIX_FMT_UYVY:
		dbg("V4L2_PIX_FMT_YUYV(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV16:
	case V4L2_PIX_FMT_NV61:
		dbg("V4L2_PIX_FMT_NV16(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height
			+ width[1] * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUV422P:
		dbg("V4L2_PIX_FMT_YUV422P(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height
			+ width[1] * frame->height / 2
			+ width[2] * frame->height / 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV12:
	case V4L2_PIX_FMT_NV21:
		dbg("V4L2_PIX_FMT_NV12(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height
			+ width[1] * frame->height / 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_NV12M:
	case V4L2_PIX_FMT_NV21M:
		dbg("V4L2_PIX_FMT_NV12M(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = width[1] * frame->height / 2;
		sizes[2] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUV420:
	case V4L2_PIX_FMT_YVU420:
		dbg("V4L2_PIX_FMT_YVU420(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height
			+ width[1] * frame->height / 2
			+ width[2] * frame->height / 2;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_YUV420M:
	case V4L2_PIX_FMT_YVU420M:
		dbg("V4L2_PIX_FMT_YVU420M(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = width[0] * frame->height;
		sizes[1] = width[1] * frame->height / 2;
		sizes[2] = width[2] * frame->height / 2;
		sizes[3] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SGRBG8:
		dbg("V4L2_PIX_FMT_SGRBG8(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = frame->width * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR8:
		dbg("V4L2_PIX_FMT_SBGGR8(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = frame->width * frame->height;
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR10:
		dbg("V4L2_PIX_FMT_SBGGR10(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = ALIGN((frame->width * 5) >> 2, 16) * frame->height;
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= ALIGN((frame->width * 5) >> 2, 16)) {
			sizes[0] = frame->bytesperline[0]
			    * frame->height;
			} else {
				err("Bytesperline too small\
					(fmt(V4L2_PIX_FMT_SBGGR10), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR12:
		dbg("V4L2_PIX_FMT_SBGGR12(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = ALIGN((frame->width * 3) >> 1, 16) * frame->height;
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= ALIGN((frame->width * 3) >> 1, 16)) {
				sizes[0] = frame->bytesperline[0] * frame->height;
			} else {
				err("Bytesperline too small\
					(fmt(V4L2_PIX_FMT_SBGGR12), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	case V4L2_PIX_FMT_SBGGR16:
		dbg("V4L2_PIX_FMT_SBGGR16(w:%d)(h:%d)\n", frame->width, frame->height);
		sizes[0] = frame->width * frame->height * 2;
		if (frame->bytesperline[0]) {
			if (frame->bytesperline[0] >= frame->width * 2) {
				sizes[0] = frame->bytesperline[0] * frame->height;
			} else {
				err("Bytesperline too small\
					(fmt(V4L2_PIX_FMT_SBGGR16), W(%d), Bytes(%d))",
				frame->width,
				frame->bytesperline[0]);
			}
		}
		sizes[1] = SPARE_SIZE;
		break;
	default:
		err("unknown pixelformat(%c%c%c%c)\n", (char)((frame->format->pixelformat >> 0) & 0xFF),
			(char)((frame->format->pixelformat >> 8) & 0xFF),
			(char)((frame->format->pixelformat >> 16) & 0xFF),
			(char)((frame->format->pixelformat >> 24) & 0xFF));
		break;
	}
}

static inline void vref_init(struct fimc_is_video *video)
{
	atomic_set(&video->refcount, 0);
}

static inline int vref_get(struct fimc_is_video *video)
{
	return atomic_inc_return(&video->refcount) - 1;
}

static inline int vref_put(struct fimc_is_video *video,
	void (*release)(struct fimc_is_video *video))
{
	int ret = 0;

	ret = atomic_sub_and_test(1, &video->refcount);
	if (ret)
		pr_debug("closed all instacne");

	return atomic_read(&video->refcount);
}

static int queue_init(void *priv, struct vb2_queue *vbq,
	struct vb2_queue *vbq_dst)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = priv;
	struct fimc_is_video *video;
	u32 type;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!vbq);

	video = GET_VIDEO(vctx);

	if (video->type == FIMC_IS_VIDEO_TYPE_CAPTURE)
		type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
	else
		type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;

	vbq->type		= type;
	vbq->io_modes		= VB2_MMAP | VB2_USERPTR | VB2_DMABUF;
	vbq->drv_priv		= vctx;
	vbq->ops		= vctx->vb2_ops;
	vbq->mem_ops		= vctx->mem_ops;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,18,0))
	vbq->timestamp_flags	= V4L2_BUF_FLAG_TIMESTAMP_COPY;
#elif (LINUX_VERSION_CODE < KERNEL_VERSION(3,18,0)) && (LINUX_VERSION_CODE >= KERNEL_VERSION(3,9,0))
	vbq->timestamp_type	= V4L2_BUF_FLAG_TIMESTAMP_COPY;
#endif

	ret = vb2_queue_init(vbq);
	if (ret) {
		mverr("vb2_queue_init fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->queue.vbq = vbq;

p_err:
	return ret;
}

int open_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx **vctx,
	u32 instance,
	u32 id)
{
	int ret = 0;

	BUG_ON(!file);
	BUG_ON(!video);

	if (atomic_read(&video->refcount) > FIMC_IS_MAX_NODES) {
		err("can't open vctx, refcount is invalid");
		ret = -EINVAL;
		goto p_err;
	}

	*vctx = vzalloc(sizeof(struct fimc_is_video_ctx));
	if (*vctx == NULL) {
		err("kzalloc is fail");
		ret = -ENOMEM;
		goto p_err;
	}

	(*vctx)->refcount = vref_get(video);
	(*vctx)->instance = instance;
	(*vctx)->queue.id = id;
	(*vctx)->state = BIT(FIMC_IS_VIDEO_CLOSE);

	file->private_data = *vctx;

p_err:
	return ret;
}

int close_vctx(struct file *file,
	struct fimc_is_video *video,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;

	vfree(vctx);
	file->private_data = NULL;
	ret = vref_put(video, NULL);

	return ret;
}

/*
 * =============================================================================
 * Queue Opertation
 * =============================================================================
 */

static int fimc_is_queue_open(struct fimc_is_queue *queue,
	u32 rdycount)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	queue->buf_rdycount = rdycount;
	queue->buf_req = 0;
	queue->buf_pre = 0;
	queue->buf_que = 0;
	queue->buf_com = 0;
	queue->buf_dqe = 0;
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	memset(&queue->framecfg, 0, sizeof(struct fimc_is_frame_cfg));
	frame_manager_probe(&queue->framemgr, queue->id);

	return ret;
}

static int fimc_is_queue_close(struct fimc_is_queue *queue)
{
	int ret = 0;

	queue->buf_maxcount = 0;
	queue->buf_refcount = 0;
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	frame_manager_close(&queue->framemgr);

	return ret;
}

static int fimc_is_queue_set_format_mplane(struct fimc_is_queue *queue,
	void *device,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 plane;
	struct v4l2_pix_format_mplane *pix;
	struct fimc_is_fmt *fmt;

	BUG_ON(!queue);

	pix = &format->fmt.pix_mp;
	fmt = fimc_is_find_format(pix->pixelformat, 0);
	if (!fmt) {
		err("pixel format is not found\n");
		ret = -EINVAL;
		goto p_err;
	}

	queue->framecfg.format			= fmt;
	queue->framecfg.colorspace		= pix->colorspace;
	queue->framecfg.width			= pix->width;
	queue->framecfg.height			= pix->height;

	for (plane = 0; plane < fmt->hw_plane; ++plane) {
		if (pix->plane_fmt[plane].bytesperline) {
			queue->framecfg.bytesperline[plane] =
				pix->plane_fmt[plane].bytesperline;
		} else {
			queue->framecfg.bytesperline[plane] = 0;
		}
	}

	ret = CALL_QOPS(queue, s_format, device, queue);
	if (ret) {
		err("s_format is fail(%d)", ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_queue_setup(struct fimc_is_queue *queue,
	void *alloc_ctx,
	unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	u32 ret = 0;
	u32 plane;

	BUG_ON(!queue);
	BUG_ON(!alloc_ctx);
	BUG_ON(!num_planes);
	BUG_ON(!sizes);
	BUG_ON(!allocators);
	BUG_ON(!queue->framecfg.format);

	*num_planes = (unsigned int)(queue->framecfg.format->num_planes);

	fimc_is_set_plane_size(&queue->framecfg, sizes);

	for (plane = 0; plane < *num_planes; plane++) {
		allocators[plane] = alloc_ctx;
		queue->framecfg.size[plane] = sizes[plane];
		mdbgv_vid("queue[%d] size : %d\n", plane, sizes[plane]);
	}

	return ret;
}

int fimc_is_queue_buffer_queue(struct fimc_is_queue *queue,
	struct vb2_buffer *vb)
{
	u32 ret = 0, i;
	u32 index;
	u32 ext_size;
	u32 spare;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	const struct fimc_is_vb2 *vb2;

	index = vb->v4l2_buf.index;
	framemgr = &queue->framemgr;
	BUG_ON(framemgr->id == FRAMEMGR_ID_INVALID);
	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	video = GET_VIDEO(vctx);
	BUG_ON(!video);
	vb2 = video->vb2;

	/* plane address is updated for checking everytime */
	for (i = 0; i < vb->num_planes; i++) {
		queue->buf_box[index][i] = vb2->plane_cookie(vb, i);
		queue->buf_dva[index][i] = vb2->plane_dvaddr(queue->buf_box[index][i], i);
#ifdef DBG_IMAGE_KMAPPING
		queue->buf_kva[index][i] = vb2->plane_kvaddr(vb, i);
#endif
	}

	frame = &framemgr->frames[index];

	/* uninitialized frame need to get info */
	if (!test_bit(FRAME_MEM_INIT, &frame->mem_state))
		goto set_info;

	/* plane count check */
	if (frame->planes != vb->num_planes) {
		mverr("plane count is changed(%08X != %08X)", vctx, video,
			frame->planes, vb->num_planes);
		ret = -EINVAL;
		goto exit;
	}

	/* plane address check */
	for (i = 0; i < frame->planes; i++) {
		if (frame->dvaddr_buffer[i] != queue->buf_dva[index][i]) {
			if (video->resourcemgr->hal_version == IS_HAL_VER_3_2) {
				frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
			} else {
				mverr("buffer[%d][%d] is changed(%08X != %08lX)", vctx, video, index, i,
					frame->dvaddr_buffer[i], queue->buf_dva[index][i]);
				ret = -EINVAL;
				goto exit;
			}
		}
	}

	goto exit;

set_info:
	if (test_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state)) {
		mverr("already prepared but new index(%d) is came", vctx, video, index);
		ret = -EINVAL;
		goto exit;
	}

	frame->planes = vb->num_planes;
	spare = frame->planes - 1;

	for (i = 0; i < frame->planes; i++) {
		frame->dvaddr_buffer[i] = queue->buf_dva[index][i];
#ifdef PRINT_BUFADDR
		mvinfo("%04X %d.%d %08X\n", vctx, video, framemgr->id, index, i, frame->dvaddr_buffer[i]);
#endif
	}

	if (framemgr->id & FRAMEMGR_ID_SHOT) {
		ext_size = sizeof(struct camera2_shot_ext) - sizeof(struct camera2_shot);

		/* Create Kvaddr for Metadata */
		queue->buf_kva[index][spare] = vb2->plane_kvaddr(vb, spare);
		if (!queue->buf_kva[index][spare]) {
			mverr("plane_kvaddr is fail(%08X)", vctx, video, framemgr->id);
			ret = -EINVAL;
			goto exit;
		}

		frame->dvaddr_shot = queue->buf_dva[index][spare] + ext_size;
		frame->kvaddr_shot = queue->buf_kva[index][spare] + ext_size;
		frame->cookie_shot = queue->buf_box[index][spare];
		frame->shot = (struct camera2_shot *)frame->kvaddr_shot;
		frame->shot_ext = (struct camera2_shot_ext *)queue->buf_kva[index][spare];
		frame->shot_size = queue->framecfg.size[spare] - ext_size;
#ifdef MEASURE_TIME
		frame->tzone = (struct timeval *)frame->shot_ext->timeZone;
#endif
	} else {
		/* Create Kvaddr for frame sync */
		queue->buf_kva[index][spare] = vb2->plane_kvaddr(vb, spare);
		if (!queue->buf_kva[index][spare]) {
			mverr("plane_kvaddr is fail(%08X)", vctx, video, framemgr->id);
			ret = -EINVAL;
			goto exit;
		}

		frame->stream = (struct camera2_stream *)queue->buf_kva[index][spare];
		frame->stream->address = queue->buf_kva[index][spare];
	}

	set_bit(FRAME_MEM_INIT, &frame->mem_state);

	queue->buf_refcount++;

	if (queue->buf_rdycount == queue->buf_refcount)
		set_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);

	if (queue->buf_maxcount == queue->buf_refcount)
		set_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);

exit:
	queue->buf_que++;
	return ret;
}

int fimc_is_queue_prepare(struct vb2_buffer *vb)
{
	struct fimc_is_video_ctx *vctx;

	BUG_ON(!vb);
	BUG_ON(!vb->vb2_queue);

	vctx = vb->vb2_queue->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return -EINVAL;
	}

	vctx->queue.buf_pre++;

	return 0;
}

void fimc_is_queue_wait_prepare(struct vb2_queue *vbq)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;

	BUG_ON(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_unlock(&video->lock);
}

void fimc_is_queue_wait_finish(struct vb2_queue *vbq)
{
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_video *video;

	BUG_ON(!vbq);

	vctx = vbq->drv_priv;
	if (!vctx) {
		err("vctx is NULL");
		return;
	}

	video = vctx->video;
	mutex_lock(&video->lock);
}

int fimc_is_queue_start_streaming(struct fimc_is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	BUG_ON(!queue);

	if (test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		err("already stream on(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	if (queue->buf_rdycount && !test_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state)) {
		err("buffer state is not ready(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, start_streaming, qdevice, queue);
	if (ret) {
		err("start_streaming is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	set_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);

p_err:
	return ret;
}

int fimc_is_queue_stop_streaming(struct fimc_is_queue *queue,
	void *qdevice)
{
	int ret = 0;

	BUG_ON(!queue);

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		err("already stream off(%ld)", queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	ret = CALL_QOPS(queue, stop_streaming, qdevice, queue);
	if (ret) {
		err("stop_streaming is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	clear_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
	clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);

p_err:
	return ret;
}

int fimc_is_video_probe(struct fimc_is_video *video,
	char *video_name,
	u32 video_number,
	u32 vfl_dir,
	struct fimc_is_mem *mem,
	struct v4l2_device *v4l2_dev,
	const struct v4l2_file_operations *fops,
	const struct v4l2_ioctl_ops *ioctl_ops)
{
	int ret = 0;
	u32 video_id;

	vref_init(video);
	mutex_init(&video->lock);
	sema_init(&video->smp_multi_input, 1);
	video->try_smp		= false;
	snprintf(video->vd.name, sizeof(video->vd.name), "%s", video_name);
	video->id		= video_number;
	video->vb2		= mem->vb2;
	video->alloc_ctx	= mem->alloc_ctx;
	video->type		= (vfl_dir == VFL_DIR_RX) ? FIMC_IS_VIDEO_TYPE_CAPTURE : FIMC_IS_VIDEO_TYPE_LEADER;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,7,0))
	video->vd.vfl_dir	= vfl_dir;
#endif
	video->vd.v4l2_dev	= v4l2_dev;
	video->vd.fops		= fops;
	video->vd.ioctl_ops	= ioctl_ops;
	video->vd.minor		= -1;
	video->vd.release	= video_device_release;
	video->vd.lock		= &video->lock;
	video_set_drvdata(&video->vd, video);

	video_id = EXYNOS_VIDEONODE_FIMC_IS + video_number;
	ret = video_register_device(&video->vd, VFL_TYPE_GRABBER, video_id);
	if (ret) {
		err("Failed to register video device");
		goto p_err;
	}

p_err:
	info("[VID] %s(%d) is created\n", video_name, video_id);
	return ret;
}

int fimc_is_video_open(struct fimc_is_video_ctx *vctx,
	void *device,
	u32 buf_rdycount,
	struct fimc_is_video *video,
	const struct vb2_ops *vb2_ops,
	const struct fimc_is_queue_ops *qops)
{
	int ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!video);
	BUG_ON(!video->vb2);
	BUG_ON(!vb2_ops);

	if (!(vctx->state & BIT(FIMC_IS_VIDEO_CLOSE))) {
		mverr("already open(%lX)", vctx, video, vctx->state);
		return -EEXIST;
	}

	queue = GET_QUEUE(vctx);
	queue->vbq = NULL;
	queue->qops = qops;

	vctx->device		= device;
	vctx->subdev		= NULL;
	vctx->video		= video;
	vctx->vb2_ops		= vb2_ops;
	vctx->mem_ops		= video->vb2->ops;
	vctx->vops.qbuf		= fimc_is_video_qbuf;
	vctx->vops.dqbuf	= fimc_is_video_dqbuf;
	vctx->vops.done 	= fimc_is_video_buffer_done;
	mutex_init(&vctx->lock);

	ret = fimc_is_queue_open(queue, buf_rdycount);
	if (ret) {
		mverr("fimc_is_queue_open is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	queue->vbq = kzalloc(sizeof(struct vb2_queue), GFP_KERNEL);
	if (!queue->vbq) {
		mverr("kzalloc is fail", vctx, video);
		ret = -ENOMEM;
		goto p_err;
	}

	ret = queue_init(vctx, queue->vbq, NULL);
	if (ret) {
		mverr("queue_init fail", vctx, video);
		kfree(queue->vbq);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_OPEN);

p_err:
	return ret;
}

int fimc_is_video_close(struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (vctx->state < BIT(FIMC_IS_VIDEO_OPEN)) {
		mverr("already close(%lX)", vctx, video, vctx->state);
		return -ENOENT;
	}

	fimc_is_queue_close(queue);
	vb2_queue_release(queue->vbq);
	kfree(queue->vbq);

	/*
	 * vb2 release can call stop callback
	 * not if video node is not stream off
	 */
	vctx->device = NULL;
	vctx->state = BIT(FIMC_IS_VIDEO_CLOSE);

	return ret;
}

int fimc_is_video_s_input(struct file *file,
	struct fimc_is_video_ctx *vctx)
{
	u32 ret = 0;

	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_OPEN) | BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS)))) {
		err("[V%02d] invalid s_input is requested(%lX)", vctx->video->id, vctx->state);
		return -EINVAL;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_INPUT);

	return ret;
}

int fimc_is_video_poll(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);

	queue = GET_QUEUE(vctx);
	ret = vb2_poll(queue->vbq, file, wait);

	return ret;
}

int fimc_is_video_mmap(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct vm_area_struct *vma)
{
	u32 ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_mmap(queue->vbq, vma);

	return ret;
}

int fimc_is_video_reqbufs(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_requestbuffers *request)
{
	int ret = 0;
	struct fimc_is_queue *queue;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_video *video;

	BUG_ON(!vctx);
	BUG_ON(!request);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_S_FORMAT) | BIT(FIMC_IS_VIDEO_STOP) | BIT(FIMC_IS_VIDEO_S_BUFS)))) {
		mverr("invalid reqbufs is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	if (test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("video is stream on, not applied", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	/* before call queue ops if request count is zero */
	if (!request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			err("request_bufs is fail(%d)", ret);
			goto p_err;
		}
	}

	ret = vb2_reqbufs(queue->vbq, request);
	if (ret) {
		mverr("vb2_reqbufs is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	framemgr = &queue->framemgr;
	queue->buf_maxcount = request->count;
	if (queue->buf_maxcount == 0) {
		queue->buf_req = 0;
		queue->buf_pre = 0;
		queue->buf_que = 0;
		queue->buf_com = 0;
		queue->buf_dqe = 0;
		queue->buf_refcount = 0;
		clear_bit(FIMC_IS_QUEUE_BUFFER_READY, &queue->state);
		clear_bit(FIMC_IS_QUEUE_BUFFER_PREPARED, &queue->state);
		frame_manager_close(framemgr);
	} else {
		if (queue->buf_maxcount < queue->buf_rdycount) {
			mverr("buffer count is not invalid(%d < %d)", vctx, video,
				queue->buf_maxcount, queue->buf_rdycount);
			ret = -EINVAL;
			goto p_err;
		}

		ret = frame_manager_open(framemgr, queue->buf_maxcount);
		if (ret) {
			mverr("fimc_is_frame_open is fail(%d)", vctx, video, ret);
			goto p_err;
		}
	}

	/* after call queue ops if request count is not zero */
	if (request->count) {
		ret = CALL_QOPS(queue, request_bufs, GET_DEVICE(vctx), queue, request->count);
		if (ret) {
			err("request_bufs is fail(%d)", ret);
			goto p_err;
		}
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_BUFS);

p_err:
	return ret;
}

int fimc_is_video_querybuf(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);

	queue = GET_QUEUE(vctx);

	ret = vb2_querybuf(queue->vbq, buf);

	return ret;
}

int fimc_is_video_set_format_mplane(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_format *format)
{
	int ret = 0;
	u32 condition;
	void *device;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!format);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	/* capture video node can skip s_input */
	if (video->type  == FIMC_IS_VIDEO_TYPE_LEADER)
		condition = BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS);
	else
		condition = BIT(FIMC_IS_VIDEO_S_INPUT) | BIT(FIMC_IS_VIDEO_S_BUFS) | BIT(FIMC_IS_VIDEO_OPEN)
											   | BIT(FIMC_IS_VIDEO_STOP);

	if (!(vctx->state & condition)) {
		mverr("invalid s_format is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	ret = fimc_is_queue_set_format_mplane(queue, device, format);
	if (ret) {
		mverr("fimc_is_queue_set_format_mplane is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_S_FORMAT);

p_err:
	mdbgv_vid("set_format(%d x %d)\n", queue->framecfg.width, queue->framecfg.height);
	return ret;
}

int fimc_is_video_qbuf(struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct fimc_is_video *video;

	BUG_ON(!vctx);
	BUG_ON(!buf);

	buf->flags &= ~V4L2_BUF_FLAG_USE_SYNC;
	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->fileio) {
		mverr("file io in progress", vctx, video);
		ret = -EBUSY;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video,
			buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->index >= vbq->num_buffers) {
		mverr("buffer index%d out of range", vctx, video, buf->index);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->memory != vbq->memory) {
		mverr("invalid memory type%d", vctx, video, buf->memory);
		ret = -EINVAL;
		goto p_err;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_req++;

	ret = vb2_qbuf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_qbuf is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_video_dqbuf(struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf,
	bool blocking)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!buf);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video, buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		mverr("queue is not streamon(%ld)", vctx,  video, queue->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_dqe++;

	ret = vb2_dqbuf(queue->vbq, buf, blocking);
	if (ret) {
		mverr("vb2_dqbuf is fail(%d)", vctx,  video, ret);
		goto p_err;
	}

#ifdef DBG_IMAGE_DUMP
	if ((vctx->video->id == DBG_IMAGE_DUMP_VIDEO) && (buf->index == DBG_IMAGE_DUMP_INDEX))
		imgdump_request(queue->buf_box[buf->index][0], queue->buf_kva[buf->index][0], queue->framecfg.size[0]);
#endif

p_err:
	return ret;
}

int fimc_is_video_prepare(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	int index = 0;
	struct fimc_is_device_ischain *device;
	struct fimc_is_queue *queue;
	struct vb2_queue *vbq;
	struct vb2_buffer *vb;
	struct fimc_is_video *video;
	struct fimc_is_pipe *pipe;

	BUG_ON(!file);
	BUG_ON(!vctx);
	BUG_ON(!buf);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);
	vbq = queue->vbq;
	video = GET_VIDEO(vctx);
	index = buf->index;
	pipe = &device->pipe;

	if ((pipe->dst) && (pipe->dst->leader.vid == video->id)) {
		/* Destination */
		memcpy(&pipe->buf[PIPE_SLOT_DST][index], buf, sizeof(struct v4l2_buffer));
		memcpy(pipe->planes[PIPE_SLOT_DST][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
		pipe->buf[PIPE_SLOT_DST][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_DST][index];
	} else if ((pipe->vctx[PIPE_SLOT_JUNCTION]) && (pipe->vctx[PIPE_SLOT_JUNCTION]->video->id == video->id)) {
		/* Junction */
		if ((pipe->dst) && test_bit(FIMC_IS_GROUP_PIPE_INPUT, &pipe->dst->state)) {
			memcpy(&pipe->buf[PIPE_SLOT_JUNCTION][index], buf, sizeof(struct v4l2_buffer));
			memcpy(pipe->planes[PIPE_SLOT_JUNCTION][index], buf->m.planes, sizeof(struct v4l2_plane) * buf->length);
			pipe->buf[PIPE_SLOT_JUNCTION][index].m.planes = (struct v4l2_plane *)pipe->planes[PIPE_SLOT_JUNCTION][index];
		}
	}

	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->fileio) {
		mverr("file io in progress", vctx, video);
		ret = -EBUSY;
		goto p_err;
	}

	if (buf->type != queue->vbq->type) {
		mverr("buf type is invalid(%d != %d)", vctx, video,
			buf->type, queue->vbq->type);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->index >= vbq->num_buffers) {
		mverr("buffer index%d out of range", vctx, video, buf->index);
		ret = -EINVAL;
		goto p_err;
	}

	if (buf->memory != vbq->memory) {
		mverr("invalid memory type%d", vctx, video, buf->memory);
		ret = -EINVAL;
		goto p_err;
	}

	vb = vbq->bufs[buf->index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_prepare_buf(queue->vbq, buf);
	if (ret) {
		mverr("vb2_prepare_buf is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

	ret = fimc_is_queue_buffer_queue(queue, vb);
	if (ret) {
		mverr("fimc_is_queue_buffer_queue is fail(index : %d, %d)", vctx, video, buf->index, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_video_streamon(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct vb2_queue *vbq;
	struct fimc_is_video *video;

	BUG_ON(!file);
	BUG_ON(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & (BIT(FIMC_IS_VIDEO_S_BUFS) | BIT(FIMC_IS_VIDEO_STOP)))) {
		mverr("invalid streamon is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	vbq = GET_QUEUE(vctx)->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (vbq->streaming) {
		mverr("streamon: already streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamon(vbq, type);
	if (ret) {
		mverr("vb2_streamon is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_START);

p_err:
	return ret;
}

int fimc_is_video_streamoff(struct file *file,
	struct fimc_is_video_ctx *vctx,
	enum v4l2_buf_type type)
{
	int ret = 0;
	u32 qcount;
	struct fimc_is_queue *queue;
	struct vb2_queue *vbq;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_video *video;

	BUG_ON(!file);
	BUG_ON(!vctx);

	video = GET_VIDEO(vctx);
	if (!(vctx->state & BIT(FIMC_IS_VIDEO_START))) {
		mverr("invalid streamoff is requested(%lX)", vctx, video, vctx->state);
		return -EINVAL;
	}

	queue = GET_QUEUE(vctx);
	framemgr = &queue->framemgr;
	vbq = queue->vbq;
	if (!vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr_e_barrier_irq(framemgr, FMGR_IDX_0);
	qcount = framemgr->queued_count[FS_REQUEST] +
		framemgr->queued_count[FS_PROCESS] +
		framemgr->queued_count[FS_COMPLETE];
	framemgr_x_barrier_irq(framemgr, FMGR_IDX_0);

	if (qcount > 0)
		mwarn("video%d stream off : queued buffer is not empty(%d)", vctx,
			vctx->video->id, qcount);

	if (vbq->type != type) {
		mverr("invalid stream type(%d != %d)", vctx, video, vbq->type, type);
		ret = -EINVAL;
		goto p_err;
	}

	if (!vbq->streaming) {
		mverr("streamoff: not streaming", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	ret = vb2_streamoff(vbq, type);
	if (ret) {
		mverr("vb2_streamoff is fail(%d)", vctx, video, ret);
		goto p_err;
	}

	ret = frame_manager_flush(framemgr);
	if (ret) {
		err("fimc_is_frame_flush is fail(%d)", ret);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->state = BIT(FIMC_IS_VIDEO_STOP);

p_err:
	return ret;
}

int fimc_is_video_s_ctrl(struct file *file,
	struct fimc_is_video_ctx *vctx,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_device_ischain *device;
	struct fimc_is_resourcemgr *resourcemgr;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!ctrl);

	device = GET_DEVICE(vctx);
	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);
	resourcemgr = device->resourcemgr;

	ret = fimc_is_vender_video_s_ctrl(ctrl, device);
	if (ret) {
		merr("fimc_is_vender_video_s_ctrl is fail(%d)", device, ret);
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_END_OF_STREAM:
		ret = fimc_is_ischain_open_wrap(device, true);
		if (ret) {
			mverr("fimc_is_ischain_open_wrap is fail(%d)", vctx, video, ret);
			goto p_err;
		}
		break;
	case V4L2_CID_IS_SET_SETFILE:
		if (test_bit(FIMC_IS_ISCHAIN_START, &device->state)) {
			mverr("device is already started, setfile applying is fail", vctx, video);
			ret = -EINVAL;
			goto p_err;
		}

		device->setfile = ctrl->value;
		break;
	case V4L2_CID_IS_HAL_VERSION:
		if (ctrl->value < 0 || ctrl->value >= IS_HAL_VER_MAX) {
			mverr("hal version(%d) is invalid", vctx, video, ctrl->value);
			ret = -EINVAL;
			goto p_err;
		}
		resourcemgr->hal_version = ctrl->value;
		break;
	case V4L2_CID_IS_DEBUG_DUMP:
		info("Print fimc-is info dump by HAL");
		fimc_is_resource_dump();

		if (ctrl->value)
			panic("intentional panic from camera HAL");
		break;
	case V4L2_CID_IS_DVFS_CLUSTER0:
	case V4L2_CID_IS_DVFS_CLUSTER1:
		fimc_is_resource_ioctl(resourcemgr, ctrl);
		break;
	case V4L2_CID_IS_DEBUG_SYNC_LOG:
		fimc_is_logsync(device->interface, ctrl->value, IS_MSG_TEST_SYNC_LOG);
		break;
	case V4L2_CID_HFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_X_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_VFLIP:
		if (ctrl->value)
			set_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		else
			clear_bit(SCALER_FLIP_COMMAND_Y_MIRROR, &queue->framecfg.flip);
		break;
	case V4L2_CID_IS_INTENT:
		device->group_3aa.intent_ctl.captureIntent = ctrl->value;
		minfo("[3AA:V] s_ctrl intent(%d)\n", vctx, ctrl->value);
		break;
	case V4L2_CID_IS_CAMERA_TYPE:
		switch (ctrl->value) {
		case IS_COLD_BOOT:
			/* change value to X when !TWIZ | front */
			fimc_is_itf_fwboot_init(device->interface);
			break;
		case IS_WARM_BOOT:
			/* change value to X when TWIZ & back | frist time back camera */
			if (!test_bit(IS_IF_LAUNCH_FIRST, &device->interface->launch_state))
				device->interface->fw_boot_mode = FIRST_LAUNCHING;
			else
				device->interface->fw_boot_mode = WARM_BOOT;
			break;
		default:
			err("unsupported ioctl(0x%X)", ctrl->id);
			ret = -EINVAL;
			break;
		}
		break;
	case VENDER_S_CTRL:
		/* This s_ctrl is needed to skip, when the s_ctrl id was found. */
		break;
	default:
		mverr("unsupported ioctl(0x%X)", vctx, video, ctrl->id);
		ret = -EINVAL;
		break;
	}

p_err:
	return ret;
}

int fimc_is_video_buffer_done(struct fimc_is_video_ctx *vctx,
	u32 index, u32 state)
{
	int ret = 0;
	struct vb2_buffer *vb;
	struct fimc_is_queue *queue;
	struct fimc_is_video *video;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);
	BUG_ON(index >= FIMC_IS_MAX_BUFS);

	queue = GET_QUEUE(vctx);
	video = GET_VIDEO(vctx);

	if (!queue->vbq) {
		mverr("vbq is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	vb = queue->vbq->bufs[index];
	if (!vb) {
		mverr("vb is NULL", vctx, video);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_QUEUE_STREAM_ON, &queue->state)) {
		warn("%d video queue is not stream on", vctx->video->id);
		ret = -EINVAL;
		goto p_err;
	}

	if (vb->state != VB2_BUF_STATE_ACTIVE) {
		mverr("vb buffer[%d] state is not active(%d)", vctx, video, index, vb->state);
		ret = -EINVAL;
		goto p_err;
	}

	queue->buf_com++;

	vb2_buffer_done(vb, state);

p_err:
	return ret;
}
