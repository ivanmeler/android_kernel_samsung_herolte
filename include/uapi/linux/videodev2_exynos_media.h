/*
 * Video for Linux Two header file for Exynos
 *
 * Copyright (c) 2012 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com
 *
 * This header file contains several v4l2 APIs to be proposed to v4l2
 * community and until being accepted, will be used restrictly for Exynos.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __LINUX_VIDEODEV2_EXYNOS_MEDIA_H
#define __LINUX_VIDEODEV2_EXYNOS_MEDIA_H

#include <linux/videodev2.h>

/* added for lihwjpeg.so */

/* yuv444 of JFIF JPEG */
#define V4L2_PIX_FMT_JPEG_444 v4l2_fourcc('J', 'P', 'G', '4')
/* yuv422 of JFIF JPEG */
#define V4L2_PIX_FMT_JPEG_422 v4l2_fourcc('J', 'P', 'G', '2')
/* yuv420 of JFIF JPEG */
#define V4L2_PIX_FMT_JPEG_420 v4l2_fourcc('J', 'P', 'G', '0')
/* grey of JFIF JPEG */
#define V4L2_PIX_FMT_JPEG_GRAY v4l2_fourcc('J', 'P', 'G', 'G')

/*
 *	C O N T R O L S
 */
/* CID base for Exynos controls (USER_CLASS) */
#define V4L2_CID_EXYNOS_BASE		(V4L2_CTRL_CLASS_USER | 0x2000)

/* cacheable configuration */
#define V4L2_CID_CACHEABLE		(V4L2_CID_EXYNOS_BASE + 10)

/* for color space conversion equation selection */
#define V4L2_CID_CSC_EQ_MODE		(V4L2_CID_EXYNOS_BASE + 100)
#define V4L2_CID_CSC_EQ			(V4L2_CID_EXYNOS_BASE + 101)
#define V4L2_CID_CSC_RANGE		(V4L2_CID_EXYNOS_BASE + 102)

/* for DRM playback scenario */
#define V4L2_CID_CONTENT_PROTECTION	(V4L2_CID_EXYNOS_BASE + 201)

/*
 *	V I D E O   I M A G E   F O R M A T
 */
/* 1 plane -- one Y, one Cb + Cr interleaved, non contiguous  */
#define V4L2_PIX_FMT_NV12N		v4l2_fourcc('N', 'N', '1', '2')
#define V4L2_PIX_FMT_NV12NT		v4l2_fourcc('T', 'N', '1', '2')

/* 1 plane -- one Y, one Cb, one Cr, non contiguous */
#define V4L2_PIX_FMT_YUV420N		v4l2_fourcc('Y', 'N', '1', '2')

/* 1 plane -- 8bit Y, 2bit Y, 8bit Cb + Cr interleaved, 2bit Cb + Cr interleaved, non contiguous */
#define V4L2_PIX_FMT_NV12N_10B		v4l2_fourcc('B', 'N', '1', '2')

/* helper macros */
#ifndef __ALIGN_UP
#define __ALIGN_UP(x, a)		(((x) + ((a) - 1)) & ~((a) - 1))
#endif

#define NV12N_Y_SIZE(w, h)		(__ALIGN_UP((w), 16) * __ALIGN_UP((h), 16) + 256)
#define NV12N_CBCR_SIZE(w, h)		(__ALIGN_UP((__ALIGN_UP((w), 16) * (__ALIGN_UP((h), 16) / 2) + 256), 16))
#define NV12N_CBCR_BASE(base, w, h)	((base) + NV12N_Y_SIZE((w), (h)))
#define NV12N_10B_Y_2B_SIZE(w,h)	((__ALIGN_UP((w) / 4, 16) * __ALIGN_UP((h), 16) + 64))
#define NV12N_10B_CBCR_2B_SIZE(w,h)	((__ALIGN_UP((w) / 4, 16) * (__ALIGN_UP((h), 16) / 2) + 64))
#define NV12N_10B_CBCR_BASE(base,w,h)	((base) + NV12N_Y_SIZE((w), (h)) + NV12N_10B_Y_2B_SIZE((w), (h)))

#define YUV420N_Y_SIZE(w, h)		(__ALIGN_UP((w), 16) * __ALIGN_UP((h), 16) + 256)
#define YUV420N_CB_SIZE(w, h)		(__ALIGN_UP((__ALIGN_UP((w) / 2, 16) * (__ALIGN_UP((h), 16) / 2) + 256), 16))
#define YUV420N_CR_SIZE(w, h)		(__ALIGN_UP((__ALIGN_UP((w) / 2, 16) * (__ALIGN_UP((h), 16) / 2) + 256), 16))
#define YUV420N_CB_BASE(base, w, h)	((base) + YUV420N_Y_SIZE((w), (h)))
#define YUV420N_CR_BASE(base, w, h)	(YUV420N_CB_BASE((base), (w), (h)) + YUV420N_CB_SIZE((w), (h)))

#endif /* __LINUX_VIDEODEV2_EXYNOS_MEDIA_H */
