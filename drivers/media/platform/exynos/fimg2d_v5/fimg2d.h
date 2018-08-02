/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d.h
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

#ifndef __FIMG2D_H
#define __FIMG2D_H __FILE__

#ifdef __KERNEL__

#include <linux/clk.h>
#include <linux/list.h>
#include <linux/device.h>
#include <linux/workqueue.h>
#include <linux/platform_device.h>
#include <linux/atomic.h>
#include <linux/mutex.h>
#include <linux/pm_qos.h>
#include <linux/mm_types.h>
#include <linux/exynos_iovmm.h>
#include <linux/compat.h>
#include <linux/ion.h>
#include "../../../../staging/android/sw_sync.h"

#define FIMG2D_MINOR		(240)

#define FIMG2D_IOVMM_PAGETABLE
#define FIMG2D_IOVA_START		(0x80000000)
#define FIMG2D_IOVA_PLANE_SIZE		(0x10000000)
#define FIMG2D_IOVA_MAX_PLANES		(1)

enum fimg2d_ip_version {
	IP_VER_G2D_8J = 10,
};

struct fimg2d_platdata {
	int ip_ver;
	const char *parent_clkname;
	const char *clkname;
	const char *gate_clkname;
	const char *gate_clkname2;
	unsigned long clkrate;
	int  cpu_min;
	int  kfc_min;
	int  mif_min;
	int  int_min;
};

#define to_fimg2d_plat(d)	(to_platform_device(d)->dev.platform_data)
#define ip_is_g2d_8j()		(fimg2d_ip_version_is() == IP_VER_G2D_8J)

#ifdef DEBUG
/*
 * g2d_debug value:
 *	0: no print
 *	1: err
 *	2: info
 *	3: perf
 *	4: oneline (simple)
 *	5: debug
 */
extern int g2d_debug;

enum debug_level {
	DBG_NO,
	DBG_ERR,
	DBG_INFO,
	DBG_PERF,
	DBG_ONELINE,
	DBG_DEBUG,
};

#define g2d_print(level, fmt, args...)				\
	do {							\
		if (g2d_debug >= level)				\
			pr_info("[%s] " fmt, __func__, ##args);	\
	} while (0)

#define fimg2d_err(fmt, args...)	g2d_print(DBG_ERR, fmt, ##args)
#define fimg2d_info(fmt, args...)	g2d_print(DBG_INFO, fmt, ##args)
#define fimg2d_debug(fmt, args...)	g2d_print(DBG_DEBUG, fmt, ##args)
#else
#define g2d_print(level, fmt, args...)
#define fimg2d_err(fmt, args...)
#define fimg2d_info(fmt, args...)
#define fimg2d_debug(fmt, arg...)
#endif

#endif /* __KERNEL__ */

/* ioctl commands */
#define FIMG2D_IOCTL_MAGIC	'F'
#define FIMG2D_BITBLT_BLIT	_IOWR(FIMG2D_IOCTL_MAGIC, 0, struct fimg2d_blit)
#define FIMG2D_BITBLT_SYNC	_IOW(FIMG2D_IOCTL_MAGIC, 1, int)

enum fimg2d_qos_level {
	G2D_LV0 = 0,
	G2D_LV1,
	G2D_LV2,
	G2D_LV3,
	G2D_LV4,
	G2D_LV_END
};

enum fimg2d_ctx_state {
	CTX_READY = 0,
	CTX_ERROR,
};

/**
 * @ADDR_NONE: No buffers (it might be solid color)
 * @ADDR_USER: user virtual address (physically Non-contiguous)
 * @ADDR_DEVICE: specific device virtual address (fd is coming from user-side)
 */
enum addr_space {
	ADDR_NONE,
	ADDR_USER,
	ADDR_DEVICE,
};

enum fmt_order {
	FMT_ARGB_8888 = 0,
	FMT_BGRA_8888,
	FMT_RGBA_8888,
	FMT_RGB_565,
	FMT_CONSTANT_ARGB,
	FMT_NONE,
};

/**
 * Pixel order complies with little-endian style
 *
 * DO NOT CHANGE THIS ORDER
 */
enum pixel_order {
	PO_ARGB = 0,
	PO_BGRA,
	PO_RGBA,
	PO_RGB,
	ARGB_ORDER_END,

	PO_COMP_RGB,
	COMP_ORDER_END,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum color_format {
	CF_ARGB_8888 = 0,
	CF_RGB_565,
	SRC_DST_RGB_FORMAT_END,

	CF_COMP_RGB8888,
	CF_COMP_RGB565,		/* Not supported yet. */
	SRC_DST_COMP_FORMAT_END,
};

enum rotation {
	ORIGIN,
	ROT_90,	/* clockwise */
	ROT_180,
	ROT_270,
	XFLIP,	/* x-axis flip */
	YFLIP,	/* y-axis flip */
};

/**
 * @NO_REPEAT: no effect
 * @REPEAT_NORMAL: repeat horizontally and vertically
 * @REPEAT_PAD: pad with pad color
 * @REPEAT_REFLECT: reflect horizontally and vertically
 * @REPEAT_CLAMP: pad with edge color of original image
 *
 * DO NOT CHANGE THIS ORDER
 */
enum repeat {
	NO_REPEAT = 0,
	REPEAT_NORMAL,	/* default setting */
	REPEAT_PAD,
	REPEAT_REFLECT, REPEAT_MIRROR = REPEAT_REFLECT,
	REPEAT_CLAMP,
};

enum scaling {
	NO_SCALING,
	SCALING_BILINEAR,
};

/**
 * premultiplied alpha
 */
enum premultiplied {
	PREMULTIPLIED,
	NON_PREMULTIPLIED,
};

/**
 * DO NOT CHANGE THIS ORDER
 */
enum blit_op {
	BLIT_OP_NONE = 0,

	BLIT_OP_SOLID_FILL,
	BLIT_OP_CLR,
	BLIT_OP_SRC, BLIT_OP_SRC_COPY = BLIT_OP_SRC,
	BLIT_OP_DST,
	BLIT_OP_SRC_OVER,
	BLIT_OP_DST_OVER, BLIT_OP_OVER_REV = BLIT_OP_DST_OVER,
	BLIT_OP_SRC_IN,
	BLIT_OP_DST_IN, BLIT_OP_IN_REV = BLIT_OP_DST_IN,
	BLIT_OP_SRC_OUT,
	BLIT_OP_DST_OUT, BLIT_OP_OUT_REV = BLIT_OP_DST_OUT,
	BLIT_OP_SRC_ATOP,
	BLIT_OP_DST_ATOP, BLIT_OP_ATOP_REV = BLIT_OP_DST_ATOP,
	BLIT_OP_XOR,

	BLIT_OP_ADD,
	BLIT_OP_MULTIPLY,
	BLIT_OP_SCREEN,
	BLIT_OP_DARKEN,
	BLIT_OP_LIGHTEN,

	BLIT_OP_DISJ_SRC_OVER,
	BLIT_OP_DISJ_DST_OVER, BLIT_OP_SATURATE = BLIT_OP_DISJ_DST_OVER,
	BLIT_OP_DISJ_SRC_IN,
	BLIT_OP_DISJ_DST_IN, BLIT_OP_DISJ_IN_REV = BLIT_OP_DISJ_DST_IN,
	BLIT_OP_DISJ_SRC_OUT,
	BLIT_OP_DISJ_DST_OUT, BLIT_OP_DISJ_OUT_REV = BLIT_OP_DISJ_DST_OUT,
	BLIT_OP_DISJ_SRC_ATOP,
	BLIT_OP_DISJ_DST_ATOP, BLIT_OP_DISJ_ATOP_REV = BLIT_OP_DISJ_DST_ATOP,
	BLIT_OP_DISJ_XOR,

	BLIT_OP_CONJ_SRC_OVER,
	BLIT_OP_CONJ_DST_OVER, BLIT_OP_CONJ_OVER_REV = BLIT_OP_CONJ_DST_OVER,
	BLIT_OP_CONJ_SRC_IN,
	BLIT_OP_CONJ_DST_IN, BLIT_OP_CONJ_IN_REV = BLIT_OP_CONJ_DST_IN,
	BLIT_OP_CONJ_SRC_OUT,
	BLIT_OP_CONJ_DST_OUT, BLIT_OP_CONJ_OUT_REV = BLIT_OP_CONJ_DST_OUT,
	BLIT_OP_CONJ_SRC_ATOP,
	BLIT_OP_CONJ_DST_ATOP, BLIT_OP_CONJ_ATOP_REV = BLIT_OP_CONJ_DST_ATOP,
	BLIT_OP_CONJ_XOR,

	/* user select coefficient manually */
	BLIT_OP_USER_COEFF,

	BLIT_OP_USER_SRC_GA,

	/* Add new operation type here */

	/* end of blit operation */
	BLIT_OP_END,
};
#define MAX_FIMG2D_BLIT_OP	((int)BLIT_OP_END)

#define FIMG2D_COMP_ALIGN_WIDTH		16
#define FIMG2D_COMP_ALIGN_HEIGHT	4

#define MAX_SRC			3
#define DST_OFFSET		(MAX_SRC + 1)

struct fimg2d_dma {
	unsigned long addr;
	exynos_iova_t iova;
	off_t offset;
	size_t size;
	size_t cached;
};

struct fimg2d_dma_dva {
	int fd;
	struct dma_buf *dma_buf;
	struct dma_buf_attachment *attachment;
	struct sg_table *sg_table;
};

struct fimg2d_dma_group {
	int is_mapped;
	struct fimg2d_dma base;
	struct fimg2d_dma_dva dva;
};

struct fimg2d_qos {
	unsigned int freq_int;
	unsigned int freq_mif;
	unsigned int freq_cpu;
	unsigned int freq_kfc;
};

/**
 * @start: start address or unique id of image
 * @header: used for COMPRESSED format header
 */
struct fimg2d_addr {
	enum addr_space type;
	unsigned long start;
	unsigned long header;
};

struct fimg2d_rect {
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2;	/* y1 + height */
};

/**
 * pixels can be different from src, dst or clip rect
 */
struct fimg2d_scale {
	enum scaling mode;

	/* ratio in pixels */
	int src_w, src_h;
	int dst_w, dst_h;
};

struct fimg2d_clip {
	bool enable;
	int x1;
	int y1;
	int x2;	/* x1 + width */
	int y2;	/* y1 + height */
};

struct fimg2d_repeat {
	enum repeat mode;
	unsigned long pad_color;
};

/**
 * @solid_color:
 *         src color instead of src image
 *         color format and order must be ARGB8888(A is MSB).
 * @g_alpha: global(constant) alpha. 0xff is opaque, 0 is transparnet
 * @rotate: rotation degree in clockwise
 * @premult: alpha premultiplied mode for read & write
 * @scaling: common scaling info for src and mask image.
 * @repeat: repeat type (tile mode)
 * @clipping: clipping rect within dst rect
 */
struct fimg2d_param {
	unsigned long solid_color;
	unsigned char g_alpha;
	enum rotation rotate;
	enum premultiplied premult;
	struct fimg2d_scale scaling;
	struct fimg2d_repeat repeat;
	struct fimg2d_clip clipping;
};

/**
 * @op: blit operation mode
 * @rect: rect for src and dst respectively
 * @need_cacheopr: true if cache coherency is required
 * @fence: acquire fence that received from producer
 * @fence_fd: release fence that will be passed to consumer
 */
struct fimg2d_image {
	int layer_num;
	int width;
	int height;
	int stride;
	enum blit_op op;
	enum pixel_order order;
	enum color_format fmt;
	struct fimg2d_param param;
	struct fimg2d_addr addr;
	struct fimg2d_rect rect;
	bool need_cacheopr;
	int acquire_fence_fd;
	int release_fence_fd;
	struct sync_fence *fence;
};

/**
 * @use_fence: set when buffers have sync-fence
 * @dither: dithering
 * @src: set when using src image
 * @dst: dst must not be null
 * @seq_no: user debugging info.
 *          for example, user can set sequence number or pid.
 */
struct fimg2d_blit {
	bool use_fence;
	bool dither;
	struct fimg2d_image *src[MAX_SRC];
	struct fimg2d_image *dst;
	unsigned int seq_no;
	enum fimg2d_qos_level qos_lv;
};

/* compat layer support */
#ifdef CONFIG_COMPAT

#define COMPAT_FIMG2D_BITBLT_BLIT	\
	_IOWR(FIMG2D_IOCTL_MAGIC, 0, struct compat_fimg2d_blit)
#define COMPAT_FIMG2D_BITBLT_SYNC	\
	_IOW(FIMG2D_IOCTL_MAGIC, 1, int)

struct compat_fimg2d_addr {
	enum addr_space type;
	compat_ulong_t start;
	compat_ulong_t header;
};

struct compat_fimg2d_rect {
	compat_int_t x1;
	compat_int_t y1;
	compat_int_t x2;
	compat_int_t y2;
};

struct compat_fimg2d_scale {
	enum scaling mode;
	compat_int_t src_w, src_h;
	compat_int_t dst_w, dst_h;
};

struct compat_fimg2d_clip {
	bool enable;
	compat_int_t x1;
	compat_int_t y1;
	compat_int_t x2;
	compat_int_t y2;
};

struct compat_fimg2d_repeat {
	enum repeat mode;
	compat_ulong_t pad_color;
};

struct compat_fimg2d_param {
	compat_ulong_t solid_color;
	unsigned char g_alpha;
	enum rotation rotate;
	enum premultiplied premult;
	struct compat_fimg2d_scale scaling;
	struct compat_fimg2d_repeat repeat;
	struct compat_fimg2d_clip clipping;
};

struct compat_fimg2d_image {
	compat_int_t layer_num;
	compat_int_t width;
	compat_int_t height;
	compat_int_t stride;
	enum blit_op op;
	enum pixel_order order;
	enum color_format fmt;
	struct compat_fimg2d_param param;
	struct compat_fimg2d_addr addr;
	struct compat_fimg2d_rect rect;
	bool need_cacheopr;
	compat_int_t acquire_fence_fd;
	compat_int_t release_fence_fd;
	compat_uptr_t fence;
};

struct compat_fimg2d_blit {
	bool use_fence;
	bool dither;
	compat_uptr_t src[MAX_SRC];
	compat_uptr_t dst;
	compat_uint_t seq_no;
	enum fimg2d_qos_level qos_lv;
};
#endif

#ifdef __KERNEL__

enum perf_desc {
	PERF_CACHE = 0,
	PERF_SFR,
	PERF_BLIT,
	PERF_UNMAP,
	PERF_TOTAL,
	PERF_END
};
#define MAX_PERF_DESCS		PERF_END

struct fimg2d_perf {
	unsigned int seq_no;
	struct timeval start;
	struct timeval end;
};

/**
 * @pgd: base address of arm mmu pagetable
 * @ncmd: request count in blit command queue
 * @wait_q: conext wait queue head
*/
struct fimg2d_context {
	struct mm_struct *mm;
	atomic_t ncmd;
	wait_queue_head_t wait_q;
	struct fimg2d_perf perf[MAX_PERF_DESCS];
	unsigned long state;
};

/**
 * @node: list head of blit command queue
 * @image_src: image for source, array is used for multi-layer composition
 * @image_dst: image for destination
 * @dma_all: total dma size of src, msk, dst
 * @dma: array of dma info for each src and dst
 * @ctx: context is created when user open fimg2d device.
 * @memops: function pointer for memory operation
 */
struct fimg2d_bltcmd {
	struct list_head node;
	struct list_head job;
	struct fimg2d_blit blt;
	unsigned long src_flag;
	struct fimg2d_image image_src[MAX_SRC];
	struct fimg2d_image image_dst;
	size_t dma_all;
	struct fimg2d_dma_group dma_src[MAX_SRC];
	struct fimg2d_dma_group dma_dst;
	struct fimg2d_context *ctx;
	struct fimg2d_memops *memops;
	struct mm_struct *mm;	/* deprecated */
};

/**
 * @suspended: in suspend mode
 * @clkon: power status for runtime pm
 * @mem: resource platform device
 * @regs: base address of hardware
 * @dev: pointer to device struct
 * @err: true if hardware is timed out while blitting
 * @irq: irq number
 * @nctx: context count
 * @busy: 1 if hardware is running
 * @bltlock: spinlock for blit
 * @wait_q: blit wait queue head
 * @cmd_q: blit command queue
 * @workqueue: workqueue_struct for kfimg2dd
*/
struct fimg2d_control {
	struct clk *clock;
	struct device *dev;
	struct resource *mem;
	void __iomem *regs;
	bool boost;

	atomic_t suspended;
	atomic_t clkon;

	atomic_t nctx;
	atomic_t busy;
	spinlock_t bltlock;
	spinlock_t qoslock;
	struct mutex drvlock;
	int irq;
	wait_queue_head_t wait_q;
	struct list_head cmd_q;

	atomic_t n_running_q;
	wait_queue_head_t running_waitq;
	struct list_head running_job_q;
	struct workqueue_struct *running_workqueue;
	struct work_struct running_work;

	atomic_t n_waiting_q;
	wait_queue_head_t waiting_waitq;
	struct list_head waiting_job_q;
	struct workqueue_struct *waiting_workqueue;
	struct work_struct waiting_work;

	struct sw_sync_timeline *timeline;
	int timeline_max;

	struct fimg2d_platdata *pdata;
#ifdef CONFIG_ION_EXYNOS
	struct ion_client *g2d_ion_client;
#endif

	int (*blit)(struct fimg2d_control *ctrl);
	int (*configure)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
	void (*run)(struct fimg2d_control *ctrl);
	void (*stop)(struct fimg2d_control *ctrl);
	void (*dump)(struct fimg2d_control *ctrl);
	void (*finalize)(struct fimg2d_control *ctrl);
};

struct fimg2d_memops {
	int (*prepare)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
	int (*map)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
	int (*unmap)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
	int (*finish)(struct fimg2d_control *ctrl,
			struct fimg2d_bltcmd *cmd);
};

int fimg2d_register_memops(struct fimg2d_bltcmd *cmd,
				enum addr_space addr_type);
int fimg2d_register_ops(struct fimg2d_control *ctrl);
int fimg2d_ip_version_is(void);
int bit_per_pixel(struct fimg2d_image *img, int plane);

static inline int width2bytes(int width, int bpp)
{
	switch (bpp) {
	case 32:
	case 24:
	case 16:
	case 8:
		return (width * bpp) >> 3;
	case 1:
		return (width + 7) >> 3;
	case 4:
		return (width + 1) >> 1;
	default:
		return 0;
	}
}

#define g2d_lock(x)		mutex_lock(x)
#define g2d_unlock(x)		mutex_unlock(x)
#define g2d_spin_lock(x, f)	spin_lock_irqsave(x, f)
#define g2d_spin_unlock(x, f)	spin_unlock_irqrestore(x, f)

#endif /* __KERNEL__ */

#ifdef CONFIG_ION_EXYNOS
extern struct ion_device *ion_exynos;
#endif

#endif /* __FIMG2D_H__ */
