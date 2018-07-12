/*
 * include/media/m2m1shot2.h
 *
 * Copyright (C) 2015 Samsung Electronics Co., Ltd.
 *
 * Contact: Cho KyongHo <pullip.cho@samsung.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
 * General Public License for more details.
 *
 */

#ifndef _M2M1SHOT2_H_
#define _M2M1SHOT2_H_

#include <linux/kernel.h>
#include <linux/videodev2.h>
#include <linux/list.h>
#include <linux/miscdevice.h>
#include <linux/dma-direction.h>
#include <linux/kref.h>
#include <linux/completion.h>
#include <linux/dma-buf.h>

#include <uapi/linux/m2m1shot2.h>

#include "../../drivers/staging/android/sw_sync.h"

struct m2m1shot2_device;
struct m2m1shot2_context;

/**
 * struct m2m1shot2_dma_buffer - buffer description of a plane of an image
 *
 * @payload	: length of the effective data
 * @offset	: the offset where the effective dta is stored from the start
 *		  of the buffer if the buffer type is dmabuf.
 * @dmabuf	: the pointer to dmabuf structure if the buffer type is dmabuf
 * @attachment	: the pointer to attachment structure if the buffer type is
 * @length	: length of the buffer if the buffer type is userptr
 * @addr	: userptr address if the memory type is userptr
 * @vma		: copy of the head vma of the list of vm_area_struct that
 *		   describes the user area [@addr, @addr + @length)
 * @sgt		: scatterlist table if the buffer type is dmabuf
 * @dma_addr	: DMA address for the client device
 * @priv	: for the private use of the client device driver
 */
struct m2m1shot2_dma_buffer {
	u32				payload;
	union {
		struct {
			u32				offset;
			struct dma_buf			*dmabuf;
			struct dma_buf_attachment	*attachment;
		} dmabuf;
		struct {
			u32				length;
			unsigned long			addr;
			struct vm_area_struct		*vma;
		} userptr;
	};
	struct sg_table			*sgt;
	dma_addr_t			dma_addr;
	void				*priv;
};

/**
 * struct m2m1shot2_context_format - image format description of an image
 *
 * @fmt		: the image format information given by the userspace
 * @priv	: the client drivers' private data that can be initialized in
 *		  m2m1shot2_devops.prepare_format(). It must be static data so
 *		  hat the resources related to the data don't need to be
 *		  released. The framework does not provide a chance to release
 *		  any resouce related to @priv.
 */
struct m2m1shot2_context_format {
	struct m2m1shot2_format fmt;
	void *priv;
};

/**
 * struct m2m1shot2_context_image
 *			- description for both of source and target images
 *
 * @index	: index of the current image in m2m1shot2_context.source array
 * @flags	: flags about the image configured in the userspace.
 *		  See include/uapi/linux/m2m1shot2.h
 * @fmt		: image format information given by the userspace
 * @memory	: memory type of all buffers in @plane
 * @plane	: buffer information of all planes of the image
 * @fence_waiter: waiter object of @fence of the image to support for
 *		  asynchronous fence waiter
 * @fence	: acquire fence of the image. The image processing deos not
 *		  start until @fence is signaled.
 */
struct m2m1shot2_context_image {
	unsigned int			index;
	u32				flags;
	struct m2m1shot2_context_format	fmt;

	u32				memory;
	unsigned int			num_planes;
	struct m2m1shot2_dma_buffer	plane[M2M1SHOT2_MAX_PLANES];

	struct sync_fence_waiter	waiter;
	struct sync_fence		*fence;
};

/**
 * struct m2m1shot2_source_image - source image description
 *
 * @img		: image description
 * @ext		: extra image processing description.
 *		  See include/uapi/linux/m2m1shot2.h
 */
struct m2m1shot2_source_image {
	struct m2m1shot2_context_image	img;
	struct m2m1shot2_extra		ext;
};

/**
 * struct m2m1shot2_devops
 *
 * @init_context	: [MANDATORY]
 *			  called on creation of a context. The given parameter
 *			  is initialized by the caller. The callee can register
 *                        its private data to m2m1shot2_context.priv. A context
 *                        is created when a userspace opens the device node.
 * @free_context	: [MANDATORY]
 *			  called on destruction of a context. The given context
 *			  has complete information until its return. The callee
 *			  should cleaning up all the relevant data in this
 *			  function.
 * @prepare_format	: [MANDATORY]
 *			  called when userspace pushes a work to the device
 *			  through m2m1shot2. m2m1shot2 calls this function for
 *			  every image provided from the userspace and the callee
 *			  should initializes @payload and @num_planes on return
 *			  according to the format. @num_planes should not be
 *			  greater than M2M1SHOT2_MAX_PLANES and @num_planes
 *			  number of elements in @payload should be initialized
 *			  by the callee.
 * @prepare_source	: [OPTIONAL]
 *			  called when userspace pushes a task to the device
 *			  through m2m1shot2 and the driver verified that the
 *			  image format and the buffer do not have a problem.
 *			  The callee then should identify and confirm the rest
 *			  of attributes specified in m2m1shot2_source_image.
 * @device_run		: [MANDATORY]
 *			  called when work is ready to be processed by H/W.
 */

struct m2m1shot2_devops {
	int (*init_context)(struct m2m1shot2_context *ctx);
	int (*free_context)(struct m2m1shot2_context *ctx);
	int (*prepare_format)(struct m2m1shot2_context_format *fmt,
				unsigned int index,
				enum dma_data_direction dir,
				size_t payload[],
				unsigned int *num_planes);
	int (*prepare_source)(struct m2m1shot2_context *ctx,
				unsigned int index,
				struct m2m1shot2_source_image *img);
	int (*device_run)(struct m2m1shot2_context *ctx);
};

/* m2m1shot2_context.state flags */

/* H/W is working on the context or it is waiting for H/W time */
#define M2M1S2_CTXSTATE_PROCESSING		0
/* The context is waiting for H/W time */
#define M2M1S2_CTXSTATE_PENDING			1
/* A process is waiting for a context to finish*/
#define M2M1S2_CTXSTATE_WAITING			2
/* The context is finished but userspace does not yet subscribe */
#define M2M1S2_CTXSTATE_PROCESSED		8
/* A error is occurred during processing the context */
#define M2M1S2_CTXSTATE_ERROR			9

/* force clean all the caches before DMA for better performance */
#define M2M1S2_CTXSTATE_CACHECLEANALL		16
/* force flush all the caches after DMA for better performance */
#define M2M1S2_CTXSTATE_CACHEINVALALL		17

#define M2M1S2_CTXSTATE_IDLE(ctx)		(((ctx)->state & 0xFFFF) == 0)
/* PROCESSIG | PENDING */
#define M2M1S2_CTXSTATE_BUSY(ctx)		(((ctx)->state & 0x3) == 0)

/**
 * struct m2m1shot2_context - context of tasks
 *
 * @node	: node entry to m2m1shot2_device.contexts
 * @state	: state of the context
 * @starter	: counter of acquire fences to wait.
 * @m21dev	: the singleton device instance that the context is born
 * @priv	: private data that is allowed to store client drivers' private
 * @mutex	: serialization of user's requests
 * @complete	: sync point between the waiter and the driver
 * @work	: work to schedule the context that is scheduled by a fence
 *		  because asynchronous waiter of an Android fence is invoked
 *		  with IRQ disabled.
 * @dwork	: work to schedule the destruction of the context because it
 *		  is required to wait until a pending context to be finished.
 * @flags	: flags specified in m2m1shot2.flags.
 * @num_sources	: the number of effective elements in @source
 * @source	: source image information
 * @target	: destination image information
 * @timeline	: monotonic timeline of Android sync that signals the release
 *		  fences
 * @timeline_max: the timestamp of the most recent release fence
 * @ctx_private	: the framework' private use
 */
struct m2m1shot2_context {
	struct list_head		node;
	unsigned long			state;
	struct kref			starter;
	struct m2m1shot2_device		*m21dev;
	void				*priv;
	struct mutex			mutex;
	struct completion		complete;
	struct work_struct		work;
	struct work_struct		dwork;

	unsigned int			flags;
	unsigned int			num_sources;
	struct m2m1shot2_source_image	source[M2M1SHOT2_MAX_IMAGES];
	struct m2m1shot2_context_image	target;

	struct sw_sync_timeline		*timeline;
	u32				timeline_max;
	u32				ctx_private;
};

#define M2M1SHOT2_DEVATTR_COHERENT		(1 << 0)
/**
 * struct m2m1shot2_device
 *
 * @misc	: misc device desciptor for user-kernel interface
 * @attr	: attribute flags that describes the characteristics of the
 *		  client device.
 * @dev		: the client device desciptor
 * @contexts	: list of instances of m2m1shot2_context that are not currently
 *		  under processing request.
 *		  - a node added in m2m1shot2_open() and
 *		    __m2m1shot2_finish_context()
 *		  - a node removed in m2m1shot2_start_context() and
 *		    m2m1shot2_cancel_context().
 * @active_contexts: the list of the contexts that is waiting to be serviced
 *		  - a node added in m2m1shot2_start_context().
 *		  - a node removed in m2m1shot2_schedule() and
 *		    m2m1shot2_cancel_context().
 * @current_task: indicate the context that is currently being processed
 *		  - a value set in m2m1shot2_schedule()
 *		  - NULL is set in __m2m1shot2_finish_context().
 * @lock_ctx	: lock to protect the consistency of @contexts, @active_contexts
 *		  and @current_task.
 * @ops		: callback functions that the client device driver must
 *                implement according to the events.
 * @schedule_workqueue: queue of work to schedule a image processing task.
 *		  An workqueue is needed to run m2m1shot2_schedule() in a
 *		  process context when the task need to wait for a fence because
 *		  the callback of the fence waiter is in IRQ disabled context.
 *
 * LIFECYCLE of m2m1shot2_context:
 * - added to @contexts on creation
 * - moved from @contexts to @active_contexts on process request
 * - moved from @active_contexts to @current_task on processing
 * - moved from @current_task to @contexts on completion
 * - removed from @contexts on destruction
 */
struct m2m1shot2_device {
	struct miscdevice		misc;
	unsigned long			attr;
	struct device			*dev;
	struct list_head		contexts;
	struct list_head		active_contexts;
	struct m2m1shot2_context	*current_ctx;
	spinlock_t			lock_ctx;
	const struct m2m1shot2_devops	*ops;
	struct workqueue_struct		*schedule_workqueue;
	struct workqueue_struct		*destroy_workqueue;
};

#ifdef CONFIG_MEDIA_M2M1SHOT2
struct m2m1shot2_context *m2m1shot2_current_context(
				const struct m2m1shot2_device *m21dev);
struct m2m1shot2_device *m2m1shot2_create_device(struct device *dev,
				const struct m2m1shot2_devops *ops,
			const char *nodename, int id, unsigned long attr);
void m2m1shot2_destroy_device(struct m2m1shot2_device *m21dev);
void m2m1shot2_finish_context(struct m2m1shot2_context *ctx, bool success);

static inline u32 m2m1shot2_get_payload(struct m2m1shot2_context *ctx,
					unsigned int index, unsigned int plane)
{
	BUG_ON(index >= M2M1SHOT2_MAX_IMAGES);
	BUG_ON(plane >= ctx->source[index].img.num_planes);

	return ctx->source[index].img.plane[plane].payload;
}

static inline void m2m1shot2_set_payload(struct m2m1shot2_context *ctx,
					unsigned int plane, u32 payload)
{
	BUG_ON(plane >= ctx->target.num_planes);
	BUG_ON((ctx->target.memory == M2M1SHOT2_BUFTYPE_DMABUF)
		&& (payload > ctx->target.plane[plane].dmabuf.dmabuf->size));
	BUG_ON((ctx->target.memory == M2M1SHOT2_BUFTYPE_USERPTR)
		&& (payload > ctx->target.plane[plane].userptr.length));

	ctx->target.plane[plane].payload = payload;
}

static inline dma_addr_t m2m1shot2_src_dma_addr(struct m2m1shot2_context *ctx,
					unsigned int index, unsigned int plane)
{
	BUG_ON(index >= M2M1SHOT2_MAX_IMAGES);
	BUG_ON(plane >= ctx->source[index].img.num_planes);

	return ctx->source[index].img.plane[plane].dma_addr;
}

static inline dma_addr_t m2m1shot2_dst_dma_addr(struct m2m1shot2_context *ctx,
						unsigned int plane)
{
	BUG_ON(plane >= ctx->target.num_planes);

	return ctx->target.plane[plane].dma_addr;
}

static inline struct m2m1shot2_context_format *m2m1shot2_src_format(
			struct m2m1shot2_context *ctx, unsigned int index)
{
	BUG_ON(index >= M2M1SHOT2_MAX_IMAGES);

	return &ctx->source[index].img.fmt;
}

static inline struct m2m1shot2_context_format *m2m1shot2_dst_format(
					struct m2m1shot2_context *ctx)
{
	return &ctx->target.fmt;
}

#else /* !CONFIG_MEDIA_M2M1SHOT2 */
static inline struct m2m1shot2_context *m2m1shot2_current_context(
				const struct m2m1shot2_device *m21dev)
{
	return NULL;
}
static inline struct m2m1shot2_device *m2m1shot2_create_device(
		struct device *dev, const struct m2m1shot2_devops *ops,
		const char *nodename, int id, unsigned long attr)
{
	return NULL;
}
#define m2m1shot2_destroy_device(m21dev)		do { } while (0)
#define m2m1shot2_finish_context(ctx, success)		do { } while (0)
static inline u32 m2m1shot2_get_payload(struct m2m1shot2_context *ctx,
					unsigned int index, unsigned int plane)
{
	return 0;
}

#define m2m1shot2_set_payload(ctx, plane, payload)	do { } while (0)
static inline dma_addr_t m2m1shot2_src_dma_addr(struct m2m1shot2_context *ctx,
					unsigned int index, unsigned int plane)
{
	return 0;
}

static inline dma_addr_t m2m1shot2_dst_dma_addr(struct m2m1shot2_context *ctx,
						unsigned int plane)
{
	return 0;
}
#endif /* CONFIG_MEDIA_M2M1SHOT2 */

#endif /* _M2M1SHOT2_H_ */
