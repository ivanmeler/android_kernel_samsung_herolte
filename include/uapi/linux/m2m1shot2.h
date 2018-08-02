/*
 *  M2M One-shot2 header file
 *    - Handling H/W accellerators for non-streaming media processing
 *
 *  Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 *
 *  Alternatively you can redistribute this file under the terms of the
 *  BSD license as stated below:
 *
 *  Redistribution and use in source and binary forms, with or without
 *  modification, are permitted provided that the following conditions
 *  are met:
 *  1. Redistributions of source code must retain the above copyright
 *     notice, this list of conditions and the following disclaimer.
 *  2. Redistributions in binary form must reproduce the above copyright
 *     notice, this list of conditions and the following disclaimer in
 *     the documentation and/or other materials provided with the
 *     distribution.
 *  3. The names of its contributors may not be used to endorse or promote
 *     products derived from this software without specific prior written
 *     permission.
 *
 *  THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 *  "AS IS" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 *  LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 *  A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 *  OWNER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 *  SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 *  TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 *  PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 *  LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 *  NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 *  SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */

#ifndef _UAPI__M2M1SHOT2_H_
#define _UAPI__M2M1SHOT2_H_

#include <linux/ioctl.h>
#include <linux/types.h>
#include <linux/videodev2.h>

/*
 * m2m1shot2_image.ext.transform
 *
 * NOTE that clock-wise rotation is assuemd.
 *
 * NOTE also that the rotation values are not flags. If ROT270 is set, ROT90 and
 * ROT180 is also set. Therefore, to check the rotation in the value,
 * use M2M1SHOT2_IMGTRFORM_ROTVAL() macro to get the rotation value.
 * Use M2M1SHOT2_IMGTRFORM_SET_ROTVAL() macro to configure a rotation value.
 * Do not use AND(&) or OR(|) operators to configure the rotation values
 */
#define M2M1SHOT2_IMGTRFORM_ROT90		(1 << 0)
#define M2M1SHOT2_IMGTRFORM_ROT270CCW		M2M1SHOT2_IMGTRFORM_ROT90
#define M2M1SHOT2_IMGTRFORM_ROT180		(1 << 1)
#define M2M1SHOT2_IMGTRFORM_ROT270		\
			(M2M1SHOT2_IMGTRFORM_ROT90 + M2M1SHOT2_IMGTRFORM_ROT180)
#define M2M1SHOT2_IMGTRFORM_ROT90CCW		M2M1SHOT2_IMGTRFORM_ROT270
#define M2M1SHOT2_IMGTRFORM_XFLIP		(1 << 4)
#define M2M1SHOT2_IMGTRFORM_YFLIP		(1 << 5)

#define M2M1SHOT2_IMGTRFORM_ROTVAL(val)		((val) & 0xF)
#define M2M1SHOT2_IMGTRFORM_SET_ROTVAL(var, val) \
					((var) = ((val) & 0xF) | ((val) & ~0xF))

/* m2m1shot2_image.ext.scaler_filter */
#define M2M1SHOT2_SCFILTER_DEFAULT	0
#define M2M1SHOT2_SCFILTER_NONE		1
#define M2M1SHOT2_SCFILTER_NEAREST	2
#define M2M1SHOT2_SCFILTER_BILINEAR	3
#define M2M1SHOT2_SCFILTER_BICUBIC	4

/* m2m1shot2_image.ext.x/yrepeat */
#define M2M1SHOT2_REPEAT_NONE		0
#define M2M1SHOT2_REPEAT_REPEAT		1
#define M2M1SHOT2_REPEAT_PAD		2
#define M2M1SHOT2_REPEAT_REFLECT	3
#define M2M1SHOT2_REPEAT_CLAMP		4

/* m2m1shot2_image.ext.composit_mode */
#define M2M1SHOT2_BLEND_NONE		0
#define M2M1SHOT2_BLEND_CLEAR		1
#define M2M1SHOT2_BLEND_SRC		2
#define M2M1SHOT2_BLEND_DST		3
#define M2M1SHOT2_BLEND_SRCOVER		4
#define M2M1SHOT2_BLEND_DSTOVER		5
#define M2M1SHOT2_BLEND_SRCIN		6
#define M2M1SHOT2_BLEND_DSTIN		7
#define M2M1SHOT2_BLEND_SRCOUT		8
#define M2M1SHOT2_BLEND_DSTOUT		9
#define M2M1SHOT2_BLEND_SRCATOP		10
#define M2M1SHOT2_BLEND_DSTATOP		11
#define M2M1SHOT2_BLEND_XOR		12
#define M2M1SHOT2_BLEND_PLUS		13
#define M2M1SHOT2_BLEND_MULTIPLY	14
#define M2M1SHOT2_BLEND_SCREEN		15
#define M2M1SHOT2_BLEND_DARKEN		16
#define M2M1SHOT2_BLEND_LIGHTEN		17
#define M2M1SHOT2_BLEND_SRCOVER_DISJ	18
#define M2M1SHOT2_BLEND_DSTOVER_DISJ	19
#define M2M1SHOT2_BLEND_SRCIN_DISJ	20
#define M2M1SHOT2_BLEND_DSTIN_DISJ	21
#define M2M1SHOT2_BLEND_SRCOUT_DISJ	22
#define M2M1SHOT2_BLEND_DSTOUT_DISJ	23
#define M2M1SHOT2_BLEND_SRCATOP_DISJ	24
#define M2M1SHOT2_BLEND_DSTATOP_DISJ	25
#define M2M1SHOT2_BLEND_XOR_DISJ	26
#define M2M1SHOT2_BLEND_SRCOVER_CONJ	27
#define M2M1SHOT2_BLEND_DSTOVER_CONJ	28
#define M2M1SHOT2_BLEND_SRCIN_CONJ	29
#define M2M1SHOT2_BLEND_DSTIN_CONJ	30
#define M2M1SHOT2_BLEND_SRCOUT_CONJ	31
#define M2M1SHOT2_BLEND_DSTOUT_CONJ	32
#define M2M1SHOT2_BLEND_SRCATOP_CONJ	33
#define M2M1SHOT2_BLEND_DSTATOP_CONJ	34
#define M2M1SHOT2_BLEND_XOR_CONJ	35

/* struct m2m1shot2_extra - extra image processing attributes
 *
 * @horizontal_factor: horizontal scaling factor in fixed point form. The
 *		  most significant 12 bits are integral part and the
 *		  rest 20 bits are fractional part of the factor.
 * @vertical_factor : vertical scaling factor in fixed point form. The format of
 *		  the number is the same as @horizontal_factor.
 * @fillcolor	: the 32bit color value to fill to the destination area.
 * @transform	: a combination of the values defined by M2M1SHOT2_IMGTRFORM_XXX
 * @composit_mode : alhpa compositing mode. It should be one of the values
 *		  defined by M2M1SHOT2_BLEND_XXX. This value is only valid if
 *		  M2M1SHOT2_IMGFLAG_ALPHABLEND is set in m2m1shot2_image.flags.
 * @galpha	: the transparent amount that overrides or modifies the value of
 *		  the alpha channel
 * @galpha_red	: the transparent amount of the red channal that is applied to
 *		  the entire image area. This value is only valid if
 *		  M2M1SHOT2_IMGFLAG_GLOBAL_ALPHA is set in m2m1shot2_image.flags
 * @galpha_green: the transparent abount of the green channel that is applied to
 *		  the entire image area. This value is only valid if
 *		  M2M1SHOT2_IMGFLAG_GLOBAL_ALPHA is set in m2m1shot2_image.flags
 * @galpha_blue	: the transparent abount of the blue channel that is applied to
 *		  the entire image area. This value is only valid if
 *		  M2M1SHOT2_IMGFLAG_GLOBAL_ALPHA is set in m2m1shot2_image.flags
 * @xrepeat	: the horizontal source image repeat pattern defined by
 *		  M2M1SHOT2_REPEAT_XXX
 * @yrepeat	: the vertical source image repeat pattern defined by
 *		  M2M1SHOT2_REPEAT_XXX
 * @scaler_filter : interpolation filter used when image is rescaled. It should
 *		  be one of the values defined as M2M1SHOT2_SCFILTER_XXX
 */
struct m2m1shot2_extra {
	__u32			horizontal_factor;
	__u32			vertical_factor;
	__u32			fillcolor;
	__u16			transform;
	__u16			composit_mode;
	__u8			galpha;
	__u8			galpha_red;
	__u8			galpha_green;
	__u8			galpha_blue;
	__u8			xrepeat;
	__u8			yrepeat;
	__u8			scaler_filter;
};

#define M2M1SHOT2_MAX_PLANES 4
#define M2M1SHOT2_MAX_IMAGES 8

/* The buffer type definition to select whether m2m1shot2_buffer.userptr or
 * m2m1shot2_buffer.fd is valid. If neither of M2M1SHOT2_BUFTYPE_USERPTR nor
 * M2M1SHOT2_BUFTYPE_DMABUF is set but M2M1SHOT2_BUFTYPE_VALID is set, the
 * brightness of each pixel of the image is defined by
 * m2m1shot2_extra.fillcolor.
 */
#define M2M1SHOT2_BUFTYPE_NONE		0
#define M2M1SHOT2_BUFTYPE_EMPTY		1 /* NO EFFECTIVE BUFFER */
#define M2M1SHOT2_BUFTYPE_USERPTR	2 /* USERPTR */
#define M2M1SHOT2_BUFTYPE_DMABUF	3 /* DMABUF */

#define M2M1SHOT2_BUFTYPE_VALID(type)	(((type) > 0) && ((type) < 4))

/* struct m2m1shot2_buffer - buffer description
 * @userptr	: the buffer address in the address space of the user. It is
 *		  only valid if M2M1SHOT2_BUFTYPE_USERPTR is set in
 *		  m2m1shot2_image.memory.
 * @fd		: the open file descriptor of a buffer valid in the address
 *		  space of the user. It is only valid if
 *		  M2M1SHOT2_BUFTYPE_DMABUF is set in m2m1shot2_image.memory.
 * @offset	: the offset where the effective data starts from in the given
 *		  buffer. It is only valid if m2m1shot2_image.memory is
 *		  M2M1SHOT2_BUFTYPE_DMABUF.
 * @payload	: the length of the effective data stored in the buffer of the
 *		  image. If the image is the source image, it should be
 *		  initialized by the user. If the image is the target image,
 *		  then it should be set by the client driver.
 * @reserved	: reserved for later use or for the purpose of the client
 *		  driver's private use.
 */
struct m2m1shot2_buffer {
	union {
		unsigned long userptr;
		__s32 fd;
	};
	__u32 offset;
	__u32 length;
	__u32 payload;
	unsigned long reserved;
};

/* struct m2m1shot2_format - image format description
 * @width	: horizontal pixel count that defines the dimension of the image
 * @height	: vertical pixel count that defines the dimension of the image
 * @pixelformat	: defines how the pixels are arranged and the brightness of each
 *		  pixels means. The values available for @pixelformat can be
 *		  found in <linux/videodev2.h>
 * @crop	: The rectangle in the image dimension that participates in the
 *		  image processing
 * @window	: The valid rectangle in the target image area. Whatever the
 *		  result image dimension defined, the result image should be
 *		  drawn on the area of @window
 */
struct m2m1shot2_format {
	__u32			width;
	__u32			height;
	__u32			pixelformat; /* V4L2_FMT_XXX */
	struct v4l2_rect	crop;
	struct v4l2_rect	window;
};

/* informs the driver that should wait for the given fence to be signaled */
#define M2M1SHOT2_IMGFLAG_ACQUIRE_FENCE		(1 << 1)
/* informs the driver to generate a fence to wait in userspace */
#define M2M1SHOT2_IMGFLAG_RELEASE_FENCE		(1 << 2)
/* pixel values are multiplied with the alpha value */
#define M2M1SHOT2_IMGFLAG_PREMUL_ALPHA		(1 << 4)
/* global alpha should be used instead of alpha channel */
#define M2M1SHOT2_IMGFLAG_GLOBAL_ALPHA		(1 << 5)
/* image is stored in compressed form */
#define M2M1SHOT2_IMGFLAG_COMPRESSED		(1 << 7)
/* do not rescale image although sizes of the src/dst image are different */
#define M2M1SHOT2_IMGFLAG_NO_RESCALING		(1 << 8)
/* ext.horizontal_factor is valid. */
#define M2M1SHOT2_IMGFLAG_XSCALE_FACTOR		(1 << 9)
/* ext.vertical_factor is valid. */
#define M2M1SHOT2_IMGFLAG_YSCALE_FACTOR		(1 << 10)
/* ext.fillcolor is valid */
#define M2M1SHOT2_IMGFLAG_COLORFILL		(1 << 11)
/* cleaning of the CPU caches is unneccessary before processing */
#define M2M1SHOT2_IMGFLAG_NO_CACHECLEAN		(1 << 16)
/* invalidation of the CPU caches is unneccessary after processing */
#define M2M1SHOT2_IMGFLAG_NO_CACHEINV		(1 << 17)

/* struct m2m1shot2_image - description of an image
 *
 * @flags	: flags to determine if an attribute of the image is valid or
 *		  not. The valule in @flags is a combination of
 *		  M2M1SHOT2_IMGFLAG_XXX.
 * @fence	: a open file descriptor of the acquire fence if
 *		  M2M1SHOT2_IMGFLAG_ACQUIRE_FENCE is set in
 *		  m2m1shot2_image.flags. On return of M2M1SHOT2_IOC_PROCESS
 *		  ioctl, it is a valid open file descriptor of the release fence
 *		  of the image if M2M1SHOT2_IMGFLAG_RELEASE_FENCE is set by the
 *		  user.
 * @memory	: determines whether m2m1shot2_buffer.fd or
 *		  m2m1shot2_buffer.userptr is valid. It should be one of
 *		  M2M1SHOT2_BUFTYPE_USERPTR, M2M1SHOT2_BUFTYPE_DMABUF and
 *		  M2M1SHOT2_BUFTYPE_EMPTY.
 * @num_planes	: the number of valid elements in @plane array.
 * @plane	: the description of the buffers that contains the color and
 *		  brightness information of the image.
 * @fmt		: the description how to read the pixels in the buffers
 *		: described in @plane
 * @ext		: the detailed image processing attributes
 * @reserved	: reserved for later use or driver's private use.
 */
struct m2m1shot2_image {
	__u32			flags;
	__s32			fence;
	__u8			memory;
	__u8			num_planes;
	struct m2m1shot2_buffer	plane[M2M1SHOT2_MAX_PLANES];
	struct m2m1shot2_format	fmt;
	struct m2m1shot2_extra	ext;
	__u32			reserved[4];
};

/* m2m1shot2.flags */
/* dithering required at the end of the image processing */
#define M2M1SHOT2_FLAG_DITHER		(1 << 1)
/*
 * M2M1SHOT2_IOC_PROCESS ioctl returns immediately after validation of the
 * values in the parameters finishes. The user should call ioctl with
 * M2M1SHOT2_IOC_WAIT_PROCESS command to get the result
 */
#define M2M1SHOT2_FLAG_NONBLOCK		(1 << 2)
/*
 * indicate that an error occurred during image processing. Configured by the
 * framework. The users should check if an error occurred on return of ioctl.
 */
#define M2M1SHOT2_FLAG_ERROR		(1 << 4)

/* struct m2m1shot2 - description of a image processing task
 *
 * @sources	: the descriptions of source images.
 * @target	: the description of the target image that is the result of
 *		  the image processing
 * @num_sources	: the number of valid source images.
 * @flags	: the flags to indicate the status or the attributes of the
 *		  image processing. It should be the combination of the values
 *		  defined by M2M1SHOT2_FLAG_XXX.
 * @reserved1-4	: reserved for the later user or the driver's private use
 */
struct m2m1shot2 {
	struct m2m1shot2_image	*sources;
	struct m2m1shot2_image	target;
	__u8			num_sources;
	__u32			flags;

	__u32			reserved1;
	__u32			reserved2;
	unsigned long		reserved3;
	unsigned long		reserved4;
};

#define M2M1SHOT2_IOC_PROCESS		_IOWR('M', 4, struct m2m1shot2)
/* m2m1shot2.sources is not required for M2M1SHOT2_IOC_WAIT_PROCESS */
#define M2M1SHOT2_IOC_WAIT_PROCESS	_IOR('M', 5, struct m2m1shot2)

#endif /* _UAPI__M2M1SHOT2_H_ */
