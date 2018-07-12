/*
 * drivers/media/platform/exynos/mfc/s5p_mfc.c
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 * Kamil Debski, <k.debski@samsung.com>
 *
 * Samsung S5P Multi Format Codec
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#include <linux/io.h>
#include <linux/sched.h>
#include <linux/clk.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/platform_device.h>
#include <linux/version.h>
#include <linux/workqueue.h>
#include <linux/videodev2.h>
#include <linux/proc_fs.h>
#include <video/videonode.h>
#include <media/videobuf2-core.h>
#include <linux/of.h>
#include <linux/exynos_iovmm.h>
#include <linux/exynos_ion.h>
#include <linux/delay.h>
#include <linux/smc.h>
#include <soc/samsung/bts.h>
#include <soc/samsung/devfreq.h>
#include <asm/cacheflush.h>

#include "s5p_mfc_common.h"

#include "s5p_mfc_intr.h"
#include "s5p_mfc_inst.h"
#include "s5p_mfc_mem.h"
#include "s5p_mfc_ctrl.h"
#include "s5p_mfc_dec.h"
#include "s5p_mfc_enc.h"
#include "s5p_mfc_pm.h"
#include "s5p_mfc_qos.h"
#include "s5p_mfc_cmd.h"
#include "s5p_mfc_opr_v10.h"
#include "s5p_mfc_buf.h"
#include "s5p_mfc_utils.h"
#include "s5p_mfc_nal_q.h"

#define S5P_MFC_NAME		"s5p-mfc"
#define S5P_MFC_DEC_NAME	"s5p-mfc-dec"
#define S5P_MFC_ENC_NAME	"s5p-mfc-enc"
#define S5P_MFC_DEC_DRM_NAME	"s5p-mfc-dec-secure"
#define S5P_MFC_ENC_DRM_NAME	"s5p-mfc-enc-secure"

int debug;
module_param(debug, int, S_IRUGO | S_IWUSR);

int debug_ts;
module_param(debug_ts, int, S_IRUGO | S_IWUSR);

int no_order;
module_param(no_order, int, S_IRUGO | S_IWUSR);

struct _mfc_trace g_mfc_trace[MFC_DEV_NUM_MAX][MFC_TRACE_COUNT_MAX];
struct s5p_mfc_dev *g_mfc_dev[MFC_DEV_NUM_MAX];

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
static struct proc_dir_entry *mfc_proc_entry;

#define MFC_PROC_ROOT			"mfc"
#define MFC_PROC_INSTANCE_NUMBER	"instance_number"
#define MFC_PROC_DRM_INSTANCE_NUMBER	"drm_instance_number"
#define MFC_PROC_FW_STATUS		"fw_status"
#endif

#define MFC_SFR_AREA_COUNT	19
void s5p_mfc_dump_regs(struct s5p_mfc_dev *dev)
{
	int i;
	int addr[MFC_SFR_AREA_COUNT][2] = {
		{ 0x0, 0x80 },
		{ 0x1000, 0xCD0 },
		{ 0xF000, 0xFF8 },
		{ 0x2000, 0xA00 },
		{ 0x3000, 0x40 },
		{ 0x3110, 0x10 },
		{ 0x5000, 0x100 },
		{ 0x5200, 0x300 },
		{ 0x5600, 0x100 },
		{ 0x5800, 0x100 },
		{ 0x5A00, 0x100 },
		{ 0x6000, 0xC4 },
		{ 0x7000, 0x21C },
		{ 0x8000, 0x20C },
		{ 0x9000, 0x10C },
		{ 0xA000, 0x20C },
		{ 0xB000, 0x444 },
		{ 0xC000, 0x84 },
		{ 0xD000, 0x74 },
	};

	pr_err("-----------dumping MFC registers (SFR base = %p, dev = %p)\n",
				dev->regs_base, dev);

	/* Enable all FW clock gating */
	MFC_WRITEL(0xFFFFFFFF, S5P_FIMV_MFC_FW_CLOCK);

	for (i = 0; i < MFC_SFR_AREA_COUNT; i++) {
		printk("[%04X .. %04X]\n", addr[i][0], addr[i][0] + addr[i][1]);
		print_hex_dump(KERN_ERR, "", DUMP_PREFIX_ADDRESS, 32, 4, dev->regs_base + addr[i][0],
				addr[i][1], false);
		printk("...\n");
	}
}

static void s5p_mfc_disp_state(struct s5p_mfc_dev *dev)
{
	int i;

	pr_err("-----------dumping MFC device info-----------\n");
	pr_err("power:%d, clock:%d, num_inst:%d, num_drm_inst:%d, fw_status:%d\n",
			s5p_mfc_get_power_ref_cnt(dev), s5p_mfc_get_clk_ref_cnt(dev),
			dev->num_inst, dev->num_drm_inst, dev->fw_status);
	pr_err("hw_lock:%#lx, curr_ctx:%d, preempt_ctx:%d, continue_ctx:%d, ctx_work_bits:%#lx\n",
			dev->hw_lock, dev->curr_ctx,
			dev->preempt_ctx, dev->continue_ctx, dev->ctx_work_bits);

	for (i = 0; i < MFC_NUM_CONTEXTS; i++)
		if (dev->ctx[i])
			pr_err("MFC ctx[%d] %s(%d) state:%d, queue_cnt(src:%d, dst:%d),"
				" interrupt(cond:%d, type:%d, err:%d)\n",
				dev->ctx[i]->num,
				dev->ctx[i]->type == MFCINST_DECODER ? "DEC" : "ENC",
				dev->ctx[i]->codec_mode, dev->ctx[i]->state,
				dev->ctx[i]->src_queue_cnt, dev->ctx[i]->dst_queue_cnt,
				dev->ctx[i]->int_cond, dev->ctx[i]->int_type,
				dev->ctx[i]->int_err);
}

static void s5p_mfc_check_and_stop_hw(struct s5p_mfc_dev *dev)
{
	MFC_TRACE_DEV("** mfc will stop!!!\n");
	s5p_mfc_disp_state(dev);
	s5p_mfc_dump_regs(dev);
	exynos_sysmmu_show_status(dev->device);
	BUG();
}

int exynos_mfc_sysmmu_fault_handler(struct iommu_domain *iodmn, struct device *device,
		unsigned long addr, int id, void *param)
{
	struct s5p_mfc_dev *dev;

	dev = (struct s5p_mfc_dev *)param;
	s5p_mfc_check_and_stop_hw(dev);

	return 0;
}

static void s5p_mfc_watchdog_worker(struct work_struct *work)
{
	struct s5p_mfc_dev *dev;
	int cmd = 0;

	dev = container_of(work, struct s5p_mfc_dev, watchdog_work);

	if (atomic_read(&dev->watchdog_run)) {
		mfc_err_dev("watchdog already running???\n");
		return;
	}

	if (!atomic_read(&dev->watchdog_cnt)) {
		mfc_err_dev("interrupt handler is called\n");
		return;
	}

	/* Check whether HW interrupt has occured or not */
	if (s5p_mfc_get_power_ref_cnt(dev) && s5p_mfc_get_clk_ref_cnt(dev))
		cmd = s5p_mfc_check_int_cmd(dev);
	if (cmd) {
		mfc_err_dev("interrupt(%d) is occured, wait scheduling\n", cmd);
		return;
	} else {
		mfc_err_dev("Driver timeout error handling\n");
	}

	/* Run watchdog worker */
	atomic_set(&dev->watchdog_run, 1);

	/* Reset the timeout watchdog */
	atomic_set(&dev->watchdog_cnt, 0);

	/* Stop after dumping information */
	s5p_mfc_check_and_stop_hw(dev);
}

void s5p_mfc_watchdog(unsigned long arg)
{
	struct s5p_mfc_dev *dev = (struct s5p_mfc_dev *)arg;

	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	spin_lock_irq(&dev->condlock);
	if (dev->hw_lock)
		atomic_inc(&dev->watchdog_cnt);
	spin_unlock_irq(&dev->condlock);

	if (atomic_read(&dev->watchdog_cnt) >= MFC_WATCHDOG_CNT) {
		/* This means that hw is busy and no interrupts were
		 * generated by hw for the Nth time of running this
		 * watchdog timer. This usually means a serious hw
		 * error. Now it is time to kill all instances and
		 * reset the MFC. */
		mfc_err_dev("[%d] Time out during waiting for HW\n",
				atomic_read(&dev->watchdog_cnt));
		queue_work(dev->watchdog_wq, &dev->watchdog_work);
	}
	dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(MFC_WATCHDOG_INTERVAL);
	add_timer(&dev->watchdog_timer);
}

/* Return the minimum interval between previous and next entry */
int get_interval(struct list_head *head, struct list_head *entry)
{
	int prev_interval = MFC_MAX_INTERVAL, next_interval = MFC_MAX_INTERVAL;
	struct mfc_timestamp *prev_ts, *next_ts, *curr_ts;

	curr_ts = list_entry(entry, struct mfc_timestamp, list);

	if (entry->prev != head) {
		prev_ts = list_entry(entry->prev, struct mfc_timestamp, list);
		prev_interval = timeval_diff(&curr_ts->timestamp, &prev_ts->timestamp);
	}

	if (entry->next != head) {
		next_ts = list_entry(entry->next, struct mfc_timestamp, list);
		next_interval = timeval_diff(&next_ts->timestamp, &curr_ts->timestamp);
	}

	return (prev_interval < next_interval ? prev_interval : next_interval);
}

static inline int dec_add_timestamp(struct s5p_mfc_ctx *ctx,
			struct v4l2_buffer *buf, struct list_head *entry)
{
	int replace_entry = 0;
	struct mfc_timestamp *curr_ts = &ctx->ts_array[ctx->ts_count];

	if (ctx->ts_is_full) {
		/* Replace the entry if list of array[ts_count] is same as entry */
		if (&curr_ts->list == entry)
			replace_entry = 1;
		else
			list_del(&curr_ts->list);
	}

	memcpy(&curr_ts->timestamp, &buf->timestamp, sizeof(struct timeval));
	if (!replace_entry)
		list_add(&curr_ts->list, entry);
	curr_ts->interval =
		get_interval(&ctx->ts_list, &curr_ts->list);
	curr_ts->index = ctx->ts_count;
	ctx->ts_count++;

	if (ctx->ts_count == MFC_TIME_INDEX) {
		ctx->ts_is_full = 1;
		ctx->ts_count %= MFC_TIME_INDEX;
	}

	return 0;
}

int get_framerate_by_timestamp(struct s5p_mfc_ctx *ctx, struct v4l2_buffer *buf)
{
	struct mfc_timestamp *temp_ts;
	int found;
	int index = 0;
	int min_interval = MFC_MAX_INTERVAL;
	int max_framerate;
	int timeval_diff;

	if (debug_ts == 1) {
		/* Debug info */
		mfc_info_ctx("======================================\n");
		mfc_info_ctx("New timestamp = %ld.%06ld, count = %d \n",
			buf->timestamp.tv_sec, buf->timestamp.tv_usec, ctx->ts_count);
	}

	if (list_empty(&ctx->ts_list)) {
		dec_add_timestamp(ctx, buf, &ctx->ts_list);
		return get_framerate_by_interval(0);
	} else {
		found = 0;
		list_for_each_entry_reverse(temp_ts, &ctx->ts_list, list) {
			timeval_diff = timeval_compare(&buf->timestamp, &temp_ts->timestamp);
			if (timeval_diff == 0) {
				/* Do not add if same timestamp already exists */
				found = 1;
				break;
			} else if (timeval_diff > 0) {
				/* Add this after temp_ts */
				dec_add_timestamp(ctx, buf, &temp_ts->list);
				found = 1;
				break;
			}
		}

		if (!found)	/* Add this at first entry */
			dec_add_timestamp(ctx, buf, &ctx->ts_list);
	}

	list_for_each_entry(temp_ts, &ctx->ts_list, list) {
		if (temp_ts->interval < min_interval)
			min_interval = temp_ts->interval;
	}

	max_framerate = get_framerate_by_interval(min_interval);

	if (debug_ts == 1) {
		/* Debug info */
		index = 0;
		list_for_each_entry(temp_ts, &ctx->ts_list, list) {
			mfc_info_ctx("[%d] timestamp [i:%d]: %ld.%06ld\n",
					index, temp_ts->index,
					temp_ts->timestamp.tv_sec,
					temp_ts->timestamp.tv_usec);
			index++;
		}
		mfc_info_ctx("Min interval = %d, It is %d fps\n",
				min_interval, max_framerate);
	} else if (debug_ts == 2) {
		mfc_info_ctx("Min interval = %d, It is %d fps\n",
				min_interval, max_framerate);
	}

	if (!ctx->ts_is_full) {
		if (debug_ts == 1)
			mfc_info_ctx("ts doesn't full, keep %d fps\n", ctx->framerate);
		return ctx->framerate;
	}

	return max_framerate;
}

void mfc_sched_worker(struct work_struct *work)
{
	struct s5p_mfc_dev *dev;

	dev = container_of(work, struct s5p_mfc_dev, sched_work);

	s5p_mfc_try_run(dev);
}

/* Helper functions for interrupt processing */
/* Remove from hw execution round robin */
inline void clear_work_bit(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = NULL;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	if (s5p_mfc_ctx_ready(ctx) == 0) {
		mfc_debug(2, "No need to run again.\n");
		spin_lock_irq(&dev->condlock);
		clear_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);
	}
}

/* Wake up context wait_queue */
static inline void wake_up_ctx(struct s5p_mfc_ctx *ctx, unsigned int reason,
			       unsigned int err)
{
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	ctx->int_cond = 1;
	ctx->int_type = reason;
	ctx->int_err = err;
	wake_up(&ctx->queue);
}

/* Wake up device wait_queue */
static inline void wake_up_dev(struct s5p_mfc_dev *dev, unsigned int reason,
			       unsigned int err)
{
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dev->int_cond = 1;
	dev->int_type = reason;
	dev->int_err = err;
	wake_up(&dev->queue);
}

static inline enum s5p_mfc_node_type s5p_mfc_get_node_type(struct file *file)
{
	struct video_device *vdev = video_devdata(file);
	enum s5p_mfc_node_type node_type;

	if (!vdev) {
		mfc_err("failed to get video_device");
		return MFCNODE_INVALID;
	}

	mfc_debug(2, "video_device index: %d\n", vdev->index);

	switch (vdev->index) {
	case 0:
		node_type = MFCNODE_DECODER;
		break;
	case 1:
		node_type = MFCNODE_ENCODER;
		break;
	case 2:
		node_type = MFCNODE_DECODER_DRM;
		break;
	case 3:
		node_type = MFCNODE_ENCODER_DRM;
		break;
	default:
		node_type = MFCNODE_INVALID;
		break;
	}

	return node_type;

}

static void s5p_mfc_handle_frame_all_extracted(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *dst_buf;
	int index, i, is_first = 1;
	unsigned int interlace_type = 0, is_interlace = 0;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	mfc_debug(2, "Decided to finish\n");
	ctx->sequence++;
	while (!list_empty(&ctx->dst_queue)) {
		dst_buf = list_entry(ctx->dst_queue.next,
				     struct s5p_mfc_buf, list);
		mfc_debug(2, "Cleaning up buffer: %d\n",
					  dst_buf->vb.v4l2_buf.index);
		if (interlaced_cond(ctx))
			is_interlace = s5p_mfc_is_interlace_picture();
		for (i = 0; i < ctx->dst_fmt->mem_planes; i++)
			vb2_set_plane_payload(&dst_buf->vb, i, 0);
		list_del(&dst_buf->list);
		ctx->dst_queue_cnt--;
		dst_buf->vb.v4l2_buf.sequence = (ctx->sequence++);
		if (is_interlace) {
			interlace_type = s5p_mfc_get_interlace_type();
			if (interlace_type)
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED_TB;
			else
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED_BT;
		}
		else
			dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
		clear_bit(dst_buf->vb.v4l2_buf.index, &dec->dpb_status);

		index = dst_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
			mfc_err_ctx("failed in get_buf_ctrls_val\n");

		if (is_first) {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				dec->stored_tag);
			is_first = 0;
		} else {
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
				DEFAULT_TAG);
			call_cop(ctx, get_buf_update_val, ctx,
				&ctx->dst_ctrls[index],
				V4L2_CID_MPEG_VIDEO_H264_SEI_FP_AVAIL,
				0);
		}

		vb2_buffer_done(&dst_buf->vb, VB2_BUF_STATE_DONE);

		/* decoder dst buffer CFW UNPROT */
		if (ctx->is_drm) {
			if (test_bit(index, &ctx->raw_protect_flag)) {
				if (s5p_mfc_raw_buf_prot(ctx, dst_buf, false))
					mfc_err_ctx("failed to CFW_UNPROT\n");
				else
					clear_bit(index, &ctx->raw_protect_flag);
			}
			mfc_debug(2, "[%d] dec dst buf un-prot_flag: %#lx\n",
					index, ctx->raw_protect_flag);
		}

		mfc_debug(2, "Cleaned up buffer: %d\n",
			  dst_buf->vb.v4l2_buf.index);
	}
	if (ctx->state != MFCINST_ABORT && ctx->state != MFCINST_HEAD_PARSED &&
			ctx->state != MFCINST_RES_CHANGE_FLUSH)
		s5p_mfc_change_state(ctx, MFCINST_RUNNING);
	mfc_debug(2, "After cleanup\n");
}

/*
 * Used only when dynamic DPB is enabled.
 * Check released buffers are enqueued again.
 */
static void mfc_check_ref_frame(struct s5p_mfc_ctx *ctx,
			struct list_head *ref_list, int ref_index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *ref_buf, *tmp_buf;
	struct list_head *dst_list;
	int index;
	int found = 0;

	list_for_each_entry_safe(ref_buf, tmp_buf, ref_list, list) {
		index = ref_buf->vb.v4l2_buf.index;
		if (index == ref_index) {
			list_del(&ref_buf->list);
			dec->ref_queue_cnt--;

			list_add_tail(&ref_buf->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;

			dec->assigned_fd[index] =
					ref_buf->vb.v4l2_planes[0].m.fd;
			clear_bit(index, &dec->dpb_status);
			mfc_debug(2, "Move buffer[%d], fd[%d] to dst queue\n",
					index, dec->assigned_fd[index]);
			found = 1;
			break;
		}
	}

	if (is_h264(ctx) && !found) {
		dst_list = &ctx->dst_queue;
		list_for_each_entry_safe(ref_buf, tmp_buf, dst_list, list) {
			index = ref_buf->vb.v4l2_buf.index;
			if (index == ref_index && ref_buf->already) {
				dec->assigned_fd[index] =
					ref_buf->vb.v4l2_planes[0].m.fd;
				clear_bit(index, &dec->dpb_status);
				mfc_debug(2, "re-assigned buffer[%d], fd[%d] for H264\n",
						index, dec->assigned_fd[index]);
				found = 1;
				break;
			}
		}
	}
}

/* Process the released reference information */
static void mfc_handle_released_info(struct s5p_mfc_ctx *ctx,
				struct list_head *dst_queue_addr,
				unsigned int released_flag, int index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct dec_dpb_ref_info *refBuf;
	int t, ncount = 0;

	refBuf = &dec->ref_info[index];

	if (released_flag) {
		for (t = 0; t < MFC_MAX_DPBS; t++) {
			if (released_flag & (1 << t)) {
				if (dec->err_reuse_flag & (1 << t)) {
					mfc_debug(2, "Released, but reuse. FD[%d] = %03d\n",
							t, dec->assigned_fd[t]);
					dec->err_reuse_flag &= ~(1 << t);
				} else {
					mfc_debug(2, "Release FD[%d] = %03d\n",
							t, dec->assigned_fd[t]);
					refBuf->dpb[ncount].fd[0] = dec->assigned_fd[t];
					ncount++;
				}
				dec->assigned_fd[t] = MFC_INFO_INIT_FD;
				mfc_check_ref_frame(ctx, dst_queue_addr, t);
			}
		}
	}

	if (ncount != MFC_MAX_DPBS)
		refBuf->dpb[ncount].fd[0] = MFC_INFO_INIT_FD;
}

static void s5p_mfc_handle_frame_copy_timestamp(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *dst_buf, *src_buf;
	dma_addr_t dec_y_addr;
	struct list_head *dst_queue_addr;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dec = ctx->dec_priv;
	dev = ctx->dev;

	dec_y_addr = (dma_addr_t)s5p_mfc_get_dec_y_addr();

	if (dec->is_dynamic_dpb)
		dst_queue_addr = &dec->ref_queue;
	else
		dst_queue_addr = &ctx->dst_queue;

	/* Copy timestamp from consumed src buffer to decoded dst buffer */
	src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
	list_for_each_entry(dst_buf, dst_queue_addr, list) {
		if (s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0) ==
								dec_y_addr) {
			memcpy(&dst_buf->vb.v4l2_buf.timestamp,
					&src_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			break;
		}
	}
}

static void s5p_mfc_handle_frame_new(struct s5p_mfc_ctx *ctx, unsigned int err)
{
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_buf *dst_buf;
	struct s5p_mfc_raw_info *raw;
	dma_addr_t dspl_y_addr;
	unsigned int index;
	unsigned int frame_type;
	unsigned int interlace_type = 0, is_interlace = 0;
	unsigned int is_video_signal_type = 0, is_colour_description = 0;
	unsigned int is_content_light = 0, is_display_colour = 0;
	int mvc_view_id;
	unsigned int dst_frame_status, last_frame_status;
	struct list_head *dst_queue_addr;
	unsigned int prev_flag, released_flag = 0;
	int i;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	raw = &ctx->raw_buf;
	frame_type = s5p_mfc_get_disp_frame_type();
	mvc_view_id = s5p_mfc_get_mvc_disp_view_id();

	if (FW_HAS_LAST_DISP_INFO(dev))
		last_frame_status = mfc_get_last_disp_info();
	else
		last_frame_status = 0;

	mfc_debug(2, "frame_type : %d\n", frame_type);
	mfc_debug(2, "last_frame_status : %d\n", last_frame_status);

	if (IS_MFCV6(dev) && ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC) {
		if (mvc_view_id == 0)
			ctx->sequence++;
	} else {
		ctx->sequence++;
	}

	dspl_y_addr = s5p_mfc_get_disp_y_addr();

	if (dec->immediate_display == 1) {
		dspl_y_addr = (dma_addr_t)s5p_mfc_get_dec_y_addr();
		frame_type = s5p_mfc_get_dec_frame_type();
	}

	/* If frame is same as previous then skip and do not dequeue */
	if (frame_type == S5P_FIMV_DISPLAY_FRAME_NOT_CODED) {
		if (!(not_coded_cond(ctx) && FW_HAS_NOT_CODED(dev)))
			return;
	}
	if (interlaced_cond(ctx))
		is_interlace = s5p_mfc_is_interlace_picture();
	if (FW_HAS_VIDEO_SIGNAL_TYPE(dev)) {
		is_video_signal_type = s5p_mfc_get_video_signal_type();
		is_colour_description = s5p_mfc_get_colour_description();
	}
	if (FW_HAS_SEI_INFO_FOR_HDR(dev)) {
		is_content_light = s5p_mfc_get_sei_avail_content_light();
		is_display_colour = s5p_mfc_get_sei_avail_mastering_display();
	}
	if (dec->is_dynamic_dpb) {
		prev_flag = dec->dynamic_used;
		dec->dynamic_used = mfc_get_dec_used_flag();
		released_flag = prev_flag & (~dec->dynamic_used);

		mfc_debug(2, "Used flag = %08x, Released Buffer = %08x\n",
				dec->dynamic_used, released_flag);
	}

	/* The MFC returns address of the buffer, now we have to
	 * check which videobuf does it correspond to */
	if (dec->is_dynamic_dpb)
		dst_queue_addr = &dec->ref_queue;
	else
		dst_queue_addr = &ctx->dst_queue;

	/* decoder dst buffer CFW UNPROT */
	if (ctx->is_drm) {
		for (i = 0; i < MFC_MAX_DPBS; i++) {
			if (released_flag & (1 << i)) {
				dst_buf = dec->assigned_dpb[i];
				if (test_bit(i, &ctx->raw_protect_flag)) {
					if (s5p_mfc_raw_buf_prot(ctx, dst_buf, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(i, &ctx->raw_protect_flag);
				}
				mfc_debug(2, "[%d] dec dst buf un-prot_flag: %#lx\n",
						i, ctx->raw_protect_flag);
			}
		}
	}

	list_for_each_entry(dst_buf, dst_queue_addr, list) {
		mfc_debug(2, "Listing: %d\n", dst_buf->vb.v4l2_buf.index);
		/* Check if this is the buffer we're looking for */
		mfc_debug(2, "0x%08llx, 0x%08llx\n",
			(unsigned long long)s5p_mfc_mem_plane_addr(ctx,
			&dst_buf->vb, 0),(unsigned long long)dspl_y_addr);
		if (s5p_mfc_mem_plane_addr(ctx, &dst_buf->vb, 0)
							== dspl_y_addr) {
			index = dst_buf->vb.v4l2_buf.index;
			if ((ctx->codec_mode == S5P_FIMV_CODEC_VC1RCV_DEC &&
					s5p_mfc_err_dspl(err) == S5P_FIMV_ERR_SYNC_POINT_NOT_RECEIVED) ||
					(s5p_mfc_err_dspl(err) == S5P_FIMV_ERR_BROKEN_LINK)) {
				if (released_flag & (1 << index)) {
					list_del(&dst_buf->list);
					dec->ref_queue_cnt--;
					list_add_tail(&dst_buf->list, &ctx->dst_queue);
					ctx->dst_queue_cnt++;
					dec->dpb_status &= ~(1 << index);
					released_flag &= ~(1 << index);
					mfc_debug(2, "Corrupted frame(%d), it will be re-used(released)\n",
							s5p_mfc_err_dspl(err));
				} else {
					dec->err_reuse_flag |= 1 << index;
					mfc_debug(2, "Corrupted frame(%d), it will be re-used(not released)\n",
							s5p_mfc_err_dspl(err));
				}
				dec->dynamic_used |= released_flag;
				break;
			}

			list_del(&dst_buf->list);

			if (dec->is_dynamic_dpb)
				dec->ref_queue_cnt--;
			else
				ctx->dst_queue_cnt--;

			dst_buf->vb.v4l2_buf.sequence = ctx->sequence;

			if (is_interlace) {
				interlace_type = s5p_mfc_get_interlace_type();
				if (interlace_type)
					dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED_TB;
				else
					dst_buf->vb.v4l2_buf.field = V4L2_FIELD_INTERLACED_BT;
			}
			else
				dst_buf->vb.v4l2_buf.field = V4L2_FIELD_NONE;
			mfc_debug(2, "is_interlace : %d interlace_type : %d\n",
				is_interlace, interlace_type);

			/* Set reserved2 bits in order to inform SEI information */
			dst_buf->vb.v4l2_buf.reserved2 = 0;
			if (is_content_light) {
				dst_buf->vb.v4l2_buf.reserved2 |= (1 << 0);
				mfc_debug(2, "content light level parsed\n");
			}
			if (is_display_colour) {
				dst_buf->vb.v4l2_buf.reserved2 |= (1 << 1);
				mfc_debug(2, "mastering display colour parsed\n");
			}
			if (is_video_signal_type) {
				dst_buf->vb.v4l2_buf.reserved2 |= (1 << 4);
				mfc_debug(2, "video signal type parsed\n");
				if (is_colour_description) {
					dst_buf->vb.v4l2_buf.reserved2 |= (1 << 2);
					mfc_debug(2, "matrix coefficients parsed\n");
					dst_buf->vb.v4l2_buf.reserved2 |= (1 << 3);
					mfc_debug(2, "colour description parsed\n");
				}
			}

			if (ctx->src_fmt->mem_planes == 1) {
				vb2_set_plane_payload(&dst_buf->vb, 0,
							raw->total_plane_size);
				mfc_debug(5, "single plane payload: %d\n",
							raw->total_plane_size);
			} else {
				for (i = 0; i < ctx->src_fmt->mem_planes; i++) {
					vb2_set_plane_payload(&dst_buf->vb, i,
							raw->plane_size[i]);
				}
			}

			clear_bit(index, &dec->dpb_status);

			dst_buf->vb.v4l2_buf.flags &=
					~(V4L2_BUF_FLAG_KEYFRAME |
					V4L2_BUF_FLAG_PFRAME |
					V4L2_BUF_FLAG_BFRAME |
					V4L2_BUF_FLAG_ERROR);

			switch (frame_type) {
			case S5P_FIMV_DISPLAY_FRAME_I:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_KEYFRAME;
				break;
			case S5P_FIMV_DISPLAY_FRAME_P:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_PFRAME;
				break;
			case S5P_FIMV_DISPLAY_FRAME_B:
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_BFRAME;
				break;
			default:
				break;
			}

			if (s5p_mfc_err_dspl(err)) {
				mfc_err_ctx("Warning for displayed frame: %d\n",
							s5p_mfc_err_dspl(err));
				dst_buf->vb.v4l2_buf.flags |=
					V4L2_BUF_FLAG_ERROR;
			}

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->dst_ctrls[index]) < 0)
				mfc_err_ctx("failed in get_buf_ctrls_val\n");

			if (dec->is_dynamic_dpb)
				mfc_handle_released_info(ctx, dst_queue_addr,
							released_flag, index);

			if (dec->immediate_display == 1) {
				dst_frame_status = s5p_mfc_get_dec_status()
					& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;

				call_cop(ctx, get_buf_update_val, ctx,
						&ctx->dst_ctrls[index],
						V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
						dst_frame_status);

				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);

				dec->immediate_display = 0;
			}

			/* Update frame tag for packed PB */
			if (dec->is_packedpb &&
					(dec->y_addr_for_pb == dspl_y_addr)) {
				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_FRAME_TAG,
					dec->stored_tag);
				dec->y_addr_for_pb = 0;
			}

			if (last_frame_status &&
					!on_res_change(ctx)) {
				call_cop(ctx, get_buf_update_val, ctx,
					&ctx->dst_ctrls[index],
					V4L2_CID_MPEG_MFC51_VIDEO_DISPLAY_STATUS,
					S5P_FIMV_DEC_STATUS_LAST_DISP);
			}

			if (no_order && !dec->is_dts_mode) {
				mfc_debug(7, "timestamp: %ld %ld\n",
					dst_buf->vb.v4l2_buf.timestamp.tv_sec,
					dst_buf->vb.v4l2_buf.timestamp.tv_usec);
				mfc_debug(7, "qos ratio: %d\n", ctx->qos_ratio);
				ctx->last_framerate =
					(ctx->qos_ratio * get_framerate(
						&dst_buf->vb.v4l2_buf.timestamp,
						&ctx->last_timestamp)) / 100;

				memcpy(&ctx->last_timestamp,
					&dst_buf->vb.v4l2_buf.timestamp,
					sizeof(struct timeval));
			}

			if (!no_order) {
				ctx->last_framerate =
					get_framerate_by_timestamp(
						ctx, &dst_buf->vb.v4l2_buf);
				ctx->last_framerate =
					(ctx->qos_ratio * ctx->last_framerate) / 100;
			}

			vb2_buffer_done(&dst_buf->vb,
				s5p_mfc_err_dspl(err) ?
				VB2_BUF_STATE_ERROR : VB2_BUF_STATE_DONE);

			break;
		}
	}

	if (is_h264(ctx) && dec->is_dynamic_dpb) {
		dst_frame_status = s5p_mfc_get_dspl_status()
			& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;
		if ((dst_frame_status == S5P_FIMV_DEC_STATUS_DISPLAY_ONLY) &&
				!list_empty(&ctx->dst_queue) &&
				(dec->ref_queue_cnt < ctx->dpb_count)) {
			dst_buf = list_entry(ctx->dst_queue.next,
						struct s5p_mfc_buf, list);
			if (dst_buf->already) {
				list_del(&dst_buf->list);
				ctx->dst_queue_cnt--;

				list_add_tail(&dst_buf->list, &dec->ref_queue);
				dec->ref_queue_cnt++;

				dst_buf->already = 0;
				mfc_debug(2, "Move dst buf to ref buf\n");
			}
		}
	}
}

static void s5p_mfc_handle_frame_error(struct s5p_mfc_ctx *ctx,
		unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	struct s5p_mfc_buf *src_buf;
	unsigned long flags;
	unsigned int index;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	if (ctx->type == MFCINST_ENCODER) {
		mfc_err_ctx("Encoder Interrupt Error: %d\n", err);
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	mfc_err_ctx("Interrupt Error: %d\n", err);

	dec->dpb_flush = 0;
	dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);
	if (!list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf, list);
		index = src_buf->vb.v4l2_buf.index;
		if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in recover_buf_ctrls_val\n");

		mfc_debug(2, "MFC needs next buffer.\n");
		dec->consumed = 0;
		list_del(&src_buf->list);
		ctx->src_queue_cnt--;

		if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
			mfc_err_ctx("failed in get_buf_ctrls_val\n");

		/* decoder src buffer CFW UNPROT */
		if (ctx->is_drm) {
			if (test_bit(index, &ctx->stream_protect_flag)) {
				if (s5p_mfc_stream_buf_prot(ctx, src_buf, false))
					mfc_err_ctx("failed to CFW_UNPROT\n");
				else
					clear_bit(index, &ctx->stream_protect_flag);
			}
			mfc_debug(2, "[%d] dec src buf un-prot_flag: %#lx\n",
					index, ctx->stream_protect_flag);
		}

		vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_ERROR);
	}

	mfc_debug(2, "Assesing whether this context should be run again.\n");
	spin_unlock_irqrestore(&dev->irqlock, flags);
}

static void s5p_mfc_handle_ref_frame(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *dec_buf;
	dma_addr_t dec_addr, buf_addr;
	int found = 0;

	dec_buf = list_entry(ctx->dst_queue.next, struct s5p_mfc_buf, list);

	dec_addr = (dma_addr_t)s5p_mfc_get_dec_y_addr();
	buf_addr = s5p_mfc_mem_plane_addr(ctx, &dec_buf->vb, 0);

	if ((buf_addr == dec_addr) && (dec_buf->used == 1)) {
		mfc_debug(2, "Find dec buffer y = 0x%llx\n",
			(unsigned long long)dec_addr);

		list_del(&dec_buf->list);
		ctx->dst_queue_cnt--;

		list_add_tail(&dec_buf->list, &dec->ref_queue);
		dec->ref_queue_cnt++;

		found = 1;
	} else if (is_h264(ctx)) {

		/* Try to search decoded address in whole dst queue */
		list_for_each_entry(dec_buf, &ctx->dst_queue, list) {
			buf_addr = s5p_mfc_mem_plane_addr(ctx, &dec_buf->vb, 0);
			mfc_debug(2, "dec buf: 0x%llx, dst buf: 0x%llx, used: %d\n",
					dec_addr, buf_addr, dec_buf->used);
			if ((buf_addr == dec_addr) && (dec_buf->used == 1)) {
				found = 1;
				break;
			}
		}

		if (found) {
			buf_addr = s5p_mfc_mem_plane_addr(ctx, &dec_buf->vb, 0);
			mfc_debug(2, "Found in dst queue = 0x%llx, buf = 0x%llx\n",
				(unsigned long long)dec_addr,
				(unsigned long long)buf_addr);

			list_del(&dec_buf->list);
			ctx->dst_queue_cnt--;

			list_add_tail(&dec_buf->list, &dec->ref_queue);
			dec->ref_queue_cnt++;
		}
	}

	/* If decoded frame is not referenced, set it as referenced. */
	if (found) {
		if (!(dec->dynamic_set & mfc_get_dec_used_flag())) {
			dec->dynamic_used |= dec->dynamic_set;
		}
	} else {
		mfc_debug(2, "Can't find buffer for addr = 0x%llx\n",
				(unsigned long long)dec_addr);
	}
}

static void s5p_mfc_move_reuse_buffer(struct s5p_mfc_ctx *ctx, int release_index)
{
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	struct s5p_mfc_buf *ref_mb, *tmp_mb;
	int index;

	list_for_each_entry_safe(ref_mb, tmp_mb, &dec->ref_queue, list) {
		index = ref_mb->vb.v4l2_buf.index;
		if (index == release_index) {
			if (ctx->is_drm) {
				if (test_bit(index, &ctx->raw_protect_flag)) {
					if (s5p_mfc_raw_buf_prot(ctx, ref_mb, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(index, &ctx->raw_protect_flag);
				}
				mfc_debug(2, "[%d] dec dst buf un-prot_flag: %#lx\n",
						index, ctx->raw_protect_flag);
			}

			ref_mb->used = 0;

			list_del(&ref_mb->list);
			dec->ref_queue_cnt--;

			list_add_tail(&ref_mb->list, &ctx->dst_queue);
			ctx->dst_queue_cnt++;

			clear_bit(index, &dec->dpb_status);
			mfc_debug(2, "buffer[%d] is moved to dst queue for reuse\n", index);
		}
	}
}

static void s5p_mfc_handle_reuse_buffer(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	struct s5p_mfc_dec *dec = ctx->dec_priv;
	unsigned int prev_flag, released_flag = 0;
	int i;

	if (!dec->is_dynamic_dpb)
		return;

	prev_flag = dec->dynamic_used;
	dec->dynamic_used = mfc_get_dec_used_flag();
	released_flag = prev_flag & (~dec->dynamic_used);

	if (!released_flag)
		return;

	/* reuse not referenced buf anymore */
	for (i = 0; i < MFC_MAX_DPBS; i++)
		if (released_flag & (1 << i))
			s5p_mfc_move_reuse_buffer(ctx, i);
}

/* Handle frame decoding interrupt */
static void s5p_mfc_handle_frame(struct s5p_mfc_ctx *ctx,
					unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	struct s5p_mfc_dec *dec;
	unsigned int dst_frame_status, sei_avail_frame_pack;
	struct s5p_mfc_buf *src_buf;
	unsigned long flags;
	unsigned int res_change;
	unsigned int index, remained;
	unsigned int prev_offset;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	dec = ctx->dec_priv;
	if (!dec) {
		mfc_err("no mfc decoder to run\n");
		return;
	}

	dst_frame_status = s5p_mfc_get_dspl_status()
				& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;
	res_change = (s5p_mfc_get_dspl_status()
				& S5P_FIMV_DEC_STATUS_RESOLUTION_MASK)
				>> S5P_FIMV_DEC_STATUS_RESOLUTION_SHIFT;
	sei_avail_frame_pack = s5p_mfc_get_sei_avail_frame_pack();

	if (dec->immediate_display == 1)
		dst_frame_status = s5p_mfc_get_dec_status()
				& S5P_FIMV_DEC_STATUS_DECODING_STATUS_MASK;

	mfc_debug(2, "Frame Status: %x\n", dst_frame_status);
	mfc_debug(5, "SEI available status: 0x%08x\n", s5p_mfc_get_sei_avail());
	mfc_debug(5, "SEI contents light: 0x%08x\n", s5p_mfc_get_sei_content_light());
	mfc_debug(5, "SEI luminance: 0x%08x, 0x%08x, white point: 0x%08x\n",
				s5p_mfc_get_sei_mastering0(), s5p_mfc_get_sei_mastering1(),
				s5p_mfc_get_sei_mastering2());
	mfc_debug(5, "SEI display primaries: 0x%08x, 0x%08x, 0x%08x\n",
				s5p_mfc_get_sei_mastering3(), s5p_mfc_get_sei_mastering4(),
				s5p_mfc_get_sei_mastering5());
	mfc_debug(2, "Used flag: old = %08x, new = %08x\n",
				dec->dynamic_used, mfc_get_dec_used_flag());

	if (ctx->state == MFCINST_RES_CHANGE_INIT)
		s5p_mfc_change_state(ctx, MFCINST_RES_CHANGE_FLUSH);

	if (res_change) {
		mfc_debug(2, "Resolution change set to %d\n", res_change);
		s5p_mfc_change_state(ctx, MFCINST_RES_CHANGE_INIT);
		ctx->wait_state = WAIT_DECODING;
		mfc_debug(2, "Decoding waiting! : %d\n", ctx->wait_state);

		return;
	}
	if (dec->dpb_flush)
		dec->dpb_flush = 0;
	if (dec->remained)
		dec->remained = 0;

	spin_lock_irqsave(&dev->irqlock, flags);

	if (!ctx->src_queue_cnt && !ctx->dst_queue_cnt) {
		mfc_err("Queue count is zero for src/dst\n");
		goto leave_handle_frame;
	}

	if (ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC &&
		dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_ONLY &&
		FW_HAS_SEI_S3D_REALLOC(dev) && sei_avail_frame_pack) {
		mfc_debug(2, "Frame packing SEI exists for a frame.\n");
		mfc_debug(2, "Reallocate DPBs and issue init_buffer.\n");
		ctx->is_dpb_realloc = 1;
		s5p_mfc_change_state(ctx, MFCINST_HEAD_PARSED);
		ctx->capture_state = QUEUE_FREE;
		ctx->wait_state = WAIT_DECODING;
		s5p_mfc_handle_frame_all_extracted(ctx);
		goto leave_handle_frame;
	}

	/* All frames remaining in the buffer have been extracted  */
	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_EMPTY) {
		if (ctx->state == MFCINST_RES_CHANGE_FLUSH) {
			struct mfc_timestamp *temp_ts = NULL;

			mfc_debug(2, "Last frame received after resolution change.\n");
			s5p_mfc_handle_frame_all_extracted(ctx);
			s5p_mfc_change_state(ctx, MFCINST_RES_CHANGE_END);
			/* If there is no display frame after resolution change,
			 * Some released frames can't be unprotected.
			 * So, check and request unprotection in the end of DRC.
			 */
			if (ctx->is_drm && ctx->raw_protect_flag) {
				struct s5p_mfc_buf *dst_buf;
				int i;

				mfc_debug(2, "raw_protect_flag(%#lx) will be released\n",
						ctx->raw_protect_flag);
				for (i = 0; i < MFC_MAX_DPBS; i++) {
					if (test_bit(i, &ctx->raw_protect_flag)) {
						dst_buf = dec->assigned_dpb[i];
						if (s5p_mfc_raw_buf_prot(ctx, dst_buf, false))
							mfc_err_ctx("failed to CFW_UNPROT\n");
						else
							clear_bit(i, &ctx->raw_protect_flag);
						mfc_debug(2, "[%d] dec dst buf un-prot_flag: %#lx\n",
								i, ctx->raw_protect_flag);
					}
				}
				cleanup_assigned_dpb(ctx);
			}

			/* empty the timestamp queue */
			while (!list_empty(&ctx->ts_list)) {
				temp_ts = list_entry((&ctx->ts_list)->next,
						struct mfc_timestamp, list);
				list_del(&temp_ts->list);
			}
			ctx->ts_count = 0;
			ctx->ts_is_full = 0;
			ctx->last_framerate = 0;
			ctx->framerate = DEC_MAX_FPS;

			goto leave_handle_frame;
		} else {
			s5p_mfc_handle_frame_all_extracted(ctx);
		}
	}

	if (dec->is_dynamic_dpb) {
		switch (dst_frame_status) {
		case S5P_FIMV_DEC_STATUS_DECODING_DISPLAY:
			s5p_mfc_handle_ref_frame(ctx);
			break;
		case S5P_FIMV_DEC_STATUS_DECODING_ONLY:
			s5p_mfc_handle_ref_frame(ctx);
			/*
			 * Some cases can have many decoding only frames like VP9
			 * alt-ref frame. So need handling release buffer
			 * because of DPB full.
			 */
			s5p_mfc_handle_reuse_buffer(ctx);
			break;
		default:
			break;
		}
	}

	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_DISPLAY ||
	    dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_ONLY)
		s5p_mfc_handle_frame_copy_timestamp(ctx);

	/* A frame has been decoded and is in the buffer  */
	if (dst_frame_status == S5P_FIMV_DEC_STATUS_DISPLAY_ONLY ||
	    dst_frame_status == S5P_FIMV_DEC_STATUS_DECODING_DISPLAY) {
		s5p_mfc_handle_frame_new(ctx, err);
	} else {
		mfc_debug(2, "No frame decode.\n");
	}
	/* Mark source buffer as complete */
	if (dst_frame_status != S5P_FIMV_DEC_STATUS_DISPLAY_ONLY
		&& !list_empty(&ctx->src_queue)) {
		src_buf = list_entry(ctx->src_queue.next, struct s5p_mfc_buf,
								list);
		mfc_debug(2, "Packed PB test. Size:%d, prev offset: %ld, this run:"
			" %d\n", src_buf->vb.v4l2_planes[0].bytesused,
			dec->consumed, s5p_mfc_get_consumed_stream());
		prev_offset = dec->consumed;
		dec->consumed += s5p_mfc_get_consumed_stream();
		remained = (unsigned int)(src_buf->vb.v4l2_planes[0].bytesused - dec->consumed);

		if ((dec->consumed > 0) && (remained > STUFF_BYTE) && (err == 0) &&
				(src_buf->vb.v4l2_planes[0].bytesused > dec->consumed)) {
			/* Run MFC again on the same buffer */
			mfc_debug(2, "Running again the same buffer.\n");

			if (dec->is_packedpb)
				dec->y_addr_for_pb = (dma_addr_t)s5p_mfc_get_dec_y_addr();

			dec->remained_size = src_buf->vb.v4l2_planes[0].bytesused -
							dec->consumed;
			/* Do not move src buffer to done_list */
		} else if (s5p_mfc_err_dec(err) == S5P_FIMV_ERR_NON_PAIRED_FIELD) {
			/*
			 * For non-paired field, the same buffer need to be
			 * resubmitted and the consumed stream will be 0
			 */
			mfc_debug(2, "Not paired field. Running again the same buffer.\n");
		} else {
			index = src_buf->vb.v4l2_buf.index;
			if (call_cop(ctx, recover_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				mfc_err_ctx("failed in recover_buf_ctrls_val\n");

			mfc_debug(2, "MFC needs next buffer.\n");
			dec->consumed = 0;
			dec->remained_size = 0;
			list_del(&src_buf->list);
			ctx->src_queue_cnt--;

			if (call_cop(ctx, get_buf_ctrls_val, ctx, &ctx->src_ctrls[index]) < 0)
				mfc_err_ctx("failed in get_buf_ctrls_val\n");

			/* decoder src buffer CFW UNPROT */
			if (ctx->is_drm) {
				if (test_bit(index, &ctx->stream_protect_flag)) {
					if (s5p_mfc_stream_buf_prot(ctx, src_buf, false))
						mfc_err_ctx("failed to CFW_UNPROT\n");
					else
						clear_bit(index, &ctx->stream_protect_flag);
				}
				mfc_debug(2, "[%d] dec src buf un-prot_flag: %#lx\n",
						index, ctx->stream_protect_flag);
			}

			vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
		}
	}

leave_handle_frame:
	mfc_debug(2, "Assesing whether this context should be run again.\n");
	spin_unlock_irqrestore(&dev->irqlock, flags);
}

/* Error handling for interrupt */
static inline void s5p_mfc_handle_error(struct s5p_mfc_ctx *ctx,
	unsigned int reason, unsigned int err)
{
	struct s5p_mfc_dev *dev;
	unsigned long flags;
	struct s5p_mfc_buf *src_buf;
	int index;

	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return;
	}

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return;
	}

	mfc_err_ctx("Interrupt Error: display: %d, decoded: %d\n",
			s5p_mfc_err_dspl(err), s5p_mfc_err_dec(err));
	err = s5p_mfc_err_dec(err);

	/* Error recovery is dependent on the state of context */
	switch (ctx->state) {
	case MFCINST_INIT:
		/* This error had to happen while acquireing instance */
	case MFCINST_GOT_INST:
		/* This error had to happen while parsing vps only */
		if (err == S5P_FIMV_ERR_VPS_ONLY_ERROR) {
			s5p_mfc_change_state(ctx, MFCINST_VPS_PARSED_ONLY);
			if (!list_empty(&ctx->src_queue)) {
				src_buf = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
				index = src_buf->vb.v4l2_buf.index;
				list_del(&src_buf->list);
				ctx->src_queue_cnt--;
				/* decoder src buffer CFW UNPROT */
				if (ctx->is_drm) {
					if (test_bit(index, &ctx->stream_protect_flag)) {
						if (s5p_mfc_stream_buf_prot(ctx, src_buf, false))
							mfc_err_ctx("failed to CFW_UNPROT\n");
						else
							clear_bit(index, &ctx->stream_protect_flag);
					}
					mfc_debug(2, "[%d] dec src buf un-prot_flag: %#lx\n",
							index, ctx->stream_protect_flag);
				}
				vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
			}
		} else if (err == S5P_FIMV_ERR_HEADER_NOT_FOUND) {
			if (!ctx->is_drm) {
				unsigned char *stream_vir = NULL;
				unsigned int strm_size = 0;
				spin_lock_irqsave(&dev->irqlock, flags);
				if (!list_empty(&ctx->src_queue)) {
					src_buf = list_entry(ctx->src_queue.next,
							struct s5p_mfc_buf, list);
					stream_vir = src_buf->vir_addr;
					strm_size = src_buf->vb.v4l2_planes[0].bytesused;
					if (strm_size > 32)
						strm_size = 32;
				}
				spin_unlock_irqrestore(&dev->irqlock, flags);
				if (stream_vir && strm_size)
					print_hex_dump(KERN_ERR, "No header: ",
							DUMP_PREFIX_ADDRESS, strm_size, 4,
							stream_vir, strm_size, false);
			}
			if (!list_empty(&ctx->src_queue)) {
				src_buf = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
				index = src_buf->vb.v4l2_buf.index;
				list_del(&src_buf->list);
				ctx->src_queue_cnt--;
				/* decoder src buffer CFW UNPROT */
				if (ctx->is_drm) {
					if (test_bit(index, &ctx->stream_protect_flag)) {
						if (s5p_mfc_stream_buf_prot(ctx, src_buf, false))
							mfc_err_ctx("failed to CFW_UNPROT\n");
						else
							clear_bit(index, &ctx->stream_protect_flag);
					}
					mfc_debug(2, "[%d] dec src buf un-prot_flag: %#lx\n",
							index, ctx->stream_protect_flag);
				}
				vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
			}
		}
	case MFCINST_RES_CHANGE_END:
		/* This error had to happen while parsing the header */
	case MFCINST_HEAD_PARSED:
		/* This error had to happen while setting dst buffers */
		if (err == S5P_FIMV_ERR_NULL_SCRATCH) {
			s5p_mfc_change_state(ctx, MFCINST_ERROR);
			spin_lock_irqsave(&dev->irqlock, flags);
			/* Mark all dst buffers as having an error */
			s5p_mfc_cleanup_queue(&ctx->dst_queue);
			/* Mark all src buffers as having an error */
			s5p_mfc_cleanup_queue(&ctx->src_queue);
			spin_unlock_irqrestore(&dev->irqlock, flags);
		}
	case MFCINST_RETURN_INST:
		/* This error had to happen while releasing instance */
	case MFCINST_DPB_FLUSHING:
		/* This error had to happen while flushing DPB */
		break;
	case MFCINST_FINISHING:
	case MFCINST_FINISHED:
		/* It is higly probable that an error occured
		 * while decoding a frame */
		s5p_mfc_change_state(ctx, MFCINST_ERROR);
		/* Mark all dst buffers as having an error */
		spin_lock_irqsave(&dev->irqlock, flags);
		s5p_mfc_cleanup_queue(&ctx->dst_queue);
		/* Mark all src buffers as having an error */
		s5p_mfc_cleanup_queue(&ctx->src_queue);
		spin_unlock_irqrestore(&dev->irqlock, flags);
		break;
	default:
		mfc_err_ctx("Encountered an error interrupt which had not been handled.\n");
		mfc_err_ctx("ctx->state = %d, ctx->inst_no = %d\n",
						ctx->state, ctx->inst_no);
		break;
	}

	wake_up_dev(dev, reason, err);

	return;
}

static irqreturn_t s5p_mfc_top_half_irq(int irq, void *priv)
{
	struct s5p_mfc_dev *dev = priv;
	struct s5p_mfc_ctx *ctx;
	unsigned int err;
	unsigned int reason;

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		return IRQ_WAKE_THREAD;
	}

	reason = s5p_mfc_get_int_reason();
	err = s5p_mfc_get_int_err();
	mfc_debug(2, "[c:%d] Int reason: %d (err: %d)\n",
			dev->curr_ctx, reason, err);
	MFC_TRACE_CTX("<< Int reason(top): %d\n", reason);

	return IRQ_WAKE_THREAD;
}

/* Interrupt processing */
static irqreturn_t s5p_mfc_irq(int irq, void *priv)
{
	struct s5p_mfc_dev *dev = priv;
	struct s5p_mfc_buf *src_buf;
	struct s5p_mfc_ctx *ctx;
	struct s5p_mfc_dec *dec = NULL;
	struct s5p_mfc_enc *enc = NULL;
	unsigned int reason;
	unsigned int err;
	unsigned long flags;
	int new_ctx;
#ifdef NAL_Q_ENABLE
	nal_queue_handle *nal_q_handle = dev->nal_q_handle;
	EncoderOutputStr *pOutStr;
#endif
	mfc_debug_enter();

	if (!dev) {
		mfc_err("no mfc device to run\n");
		goto irq_end;
	}

	if (atomic_read(&dev->pm.power) == 0) {
		mfc_err("no mfc power on\n");
		goto irq_end;
	}

	/* Reset the timeout watchdog */
	atomic_set(&dev->watchdog_cnt, 0);
	/* Get the reason of interrupt and the error code */
	reason = s5p_mfc_get_int_reason();
	err = s5p_mfc_get_int_err();
	mfc_debug(2, "Int reason: %d (err: %d)\n", reason, err);
	MFC_TRACE_DEV("<< Int reason: %d\n", reason);

#ifdef NAL_Q_ENABLE
	if (nal_q_handle) {
		switch (reason) {
		case S5P_FIMV_R2H_CMD_QUEUE_DONE_RET:
			pOutStr = s5p_mfc_nal_q_dequeue_out_buf(dev,
				nal_q_handle->nal_q_out_handle, &reason);
			if (pOutStr) {
				if (s5p_mfc_nal_q_get_out_buf(dev, pOutStr))
					mfc_err("NAL Q: Failed to get out buf\n");
			} else {
				mfc_err("NAL Q: pOutStr is NULL\n");
			}
			s5p_mfc_clear_int_flags();
			goto irq_done;
		case S5P_FIMV_R2H_CMD_COMPLETE_QUEUE_RET:
			wake_up_dev(dev, reason, err);
			s5p_mfc_clear_int_flags();
			goto irq_done;
		default:
			if (nal_q_handle->nal_q_state == NAL_Q_STATE_STARTED ||
				nal_q_handle->nal_q_state == NAL_Q_STATE_STOPPED) {
				mfc_err("NAL Q: Should not be here! state: %d, int reason : %d\n",
					nal_q_handle->nal_q_state, reason);
				s5p_mfc_clear_int_flags();
				goto irq_end;
			}
		}
	}
#endif

	switch (reason) {
	case S5P_FIMV_R2H_CMD_CACHE_FLUSH_RET:
		s5p_mfc_clear_int_flags();
		/* Do not clear hw_lock */
		wake_up_dev(dev, reason, err);
		goto irq_end;
	case S5P_FIMV_R2H_CMD_SYS_INIT_RET:
	case S5P_FIMV_R2H_CMD_FW_STATUS_RET:
	case S5P_FIMV_R2H_CMD_SLEEP_RET:
	case S5P_FIMV_R2H_CMD_WAKEUP_RET:
		s5p_mfc_clear_int_flags();
		/* Initialize hw_lock */
		dev->hw_lock = 0;
		wake_up_dev(dev, reason, err);
		goto irq_end;
	}

	ctx = dev->ctx[dev->curr_ctx];
	if (!ctx) {
		mfc_err("no mfc context to run\n");
		s5p_mfc_clear_int_flags();
		s5p_mfc_clock_off(dev);
		goto irq_done;
	}

	if (ctx->type == MFCINST_DECODER)
		dec = ctx->dec_priv;
	else if (ctx->type == MFCINST_ENCODER)
		enc = ctx->enc_priv;

	dev->preempt_ctx = MFC_NO_INSTANCE_SET;
	dev->continue_ctx = MFC_NO_INSTANCE_SET;

	switch (reason) {
	case S5P_FIMV_R2H_CMD_ERR_RET:
		/* An error has occured */
		if (ctx->state == MFCINST_RUNNING || ctx->state == MFCINST_ABORT) {
			if ((s5p_mfc_err_dec(err) >= S5P_FIMV_ERR_WARNINGS_START) &&
				(s5p_mfc_err_dec(err) <= S5P_FIMV_ERR_WARNINGS_END))
				s5p_mfc_handle_frame(ctx, reason, err);
			else
				s5p_mfc_handle_frame_error(ctx, reason, err);
		} else {
			s5p_mfc_handle_error(ctx, reason, err);
		}
		break;
	case S5P_FIMV_R2H_CMD_SLICE_DONE_RET:
	case S5P_FIMV_R2H_CMD_FIELD_DONE_RET:
	case S5P_FIMV_R2H_CMD_FRAME_DONE_RET:
	case S5P_FIMV_R2H_CMD_COMPLETE_SEQ_RET:
	case S5P_FIMV_R2H_CMD_ENC_BUFFER_FULL_RET:
		if (ctx->type == MFCINST_DECODER) {
			if (ctx->state == MFCINST_SPECIAL_PARSING_NAL) {
				s5p_mfc_clear_int_flags();
				s5p_mfc_clock_off(dev);
				spin_lock_irq(&dev->condlock);
				clear_bit(ctx->num, &dev->ctx_work_bits);
				spin_unlock_irq(&dev->condlock);
				s5p_mfc_change_state(ctx, MFCINST_RUNNING);
				if (clear_hw_bit(ctx) == 0)
					s5p_mfc_check_and_stop_hw(dev);
				wake_up_ctx(ctx, reason, err);
				goto irq_end;
			}
			s5p_mfc_handle_frame(ctx, reason, err);
		} else if (ctx->type == MFCINST_ENCODER) {
			if (reason == S5P_FIMV_R2H_CMD_SLICE_DONE_RET) {
				dev->preempt_ctx = ctx->num;
				enc->buf_full = 0;
				enc->in_slice = 1;
			} else if (reason == S5P_FIMV_R2H_CMD_ENC_BUFFER_FULL_RET) {
				mfc_err_ctx("stream buffer size(%d) isn't enough\n",
						s5p_mfc_get_enc_strm_size());
				dev->preempt_ctx = ctx->num;
				enc->buf_full = 1;
				enc->in_slice = 0;
			} else {
				enc->buf_full = 0;
				enc->in_slice = 0;
			}
			if (ctx->c_ops->post_frame_start)
				if (ctx->c_ops->post_frame_start(ctx))
					mfc_err_ctx("post_frame_start() failed\n");
		}
		break;
	case S5P_FIMV_R2H_CMD_SEQ_DONE_RET:
		if (ctx->type == MFCINST_ENCODER) {
			if (ctx->c_ops->post_seq_start(ctx))
				mfc_err_ctx("post_seq_start() failed\n");
		} else if (ctx->type == MFCINST_DECODER) {
			if (ctx->src_fmt->fourcc != V4L2_PIX_FMT_FIMV1) {
				ctx->img_width = s5p_mfc_get_img_width();
				ctx->img_height = s5p_mfc_get_img_height();
			}

			ctx->dpb_count = s5p_mfc_get_dpb_count();
			ctx->scratch_buf_size = s5p_mfc_get_scratch_size();

			dec->internal_dpb = 0;
			s5p_mfc_dec_store_crop_info(ctx);
			dec->mv_count = s5p_mfc_get_mv_count();
			if (ctx->codec_mode == S5P_FIMV_CODEC_HEVC_DEC) {
				dec->profile = s5p_mfc_get_profile();
				if ((dec->profile == S5P_FIMV_D_PROFILE_HEVC_MAIN_10)
					|| (dec->profile == S5P_FIMV_D_PROFILE_HEVC_RANGE_EXT)){
					dec->is_10bit = 1;
					mfc_info_ctx("HEVC 10bit contents, depth: %d\n",
							s5p_mfc_get_bit_depth_minus8() + 8);
				}
			}

			if (ctx->img_width == 0 || ctx->img_height == 0)
				s5p_mfc_change_state(ctx, MFCINST_ERROR);
			else
				s5p_mfc_change_state(ctx, MFCINST_HEAD_PARSED);

			if (ctx->state == MFCINST_HEAD_PARSED)
				dec->is_interlaced =
					s5p_mfc_is_interlace_picture();

			if ((ctx->codec_mode == S5P_FIMV_CODEC_H264_DEC ||
				ctx->codec_mode == S5P_FIMV_CODEC_H264_MVC_DEC ||
				ctx->codec_mode == S5P_FIMV_CODEC_HEVC_DEC) &&
					!list_empty(&ctx->src_queue)) {
				struct s5p_mfc_buf *src_buf;
				src_buf = list_entry(ctx->src_queue.next,
						struct s5p_mfc_buf, list);
				mfc_debug(2, "Check consumed size of header. ");
				mfc_debug(2, "source : %d, consumed : %d\n",
						s5p_mfc_get_consumed_stream(),
						src_buf->vb.v4l2_planes[0].bytesused);
				if (s5p_mfc_get_consumed_stream() <
						src_buf->vb.v4l2_planes[0].bytesused) {
					dec->remained = 1;
					src_buf->consumed += s5p_mfc_get_consumed_stream();
				}
			}
		}
		break;
	case S5P_FIMV_R2H_CMD_OPEN_INSTANCE_RET:
		ctx->inst_no = s5p_mfc_get_inst_no();
		s5p_mfc_change_state(ctx, MFCINST_GOT_INST);
		break;
	case S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET:
		s5p_mfc_change_state(ctx, MFCINST_FREE);
		break;
	case S5P_FIMV_R2H_CMD_NAL_ABORT_RET:
		if (ctx->type == MFCINST_ENCODER) {
			s5p_mfc_change_state(ctx, MFCINST_RUNNING_BUF_FULL);
			enc->buf_full = 0;
			if (ctx->codec_mode == S5P_FIMV_CODEC_VP8_ENC)
				mfc_err_ctx("stream buffer size isn't enough\n");
			if (ctx->c_ops->post_frame_start)
				if (ctx->c_ops->post_frame_start(ctx))
					mfc_err_ctx("post_frame_start() failed\n");
		} else {
			s5p_mfc_change_state(ctx, MFCINST_ABORT);
		}
		break;
	case S5P_FIMV_R2H_CMD_DPB_FLUSH_RET:
		s5p_mfc_change_state(ctx, MFCINST_ABORT);
		break;
	case S5P_FIMV_R2H_CMD_INIT_BUFFERS_RET:
		if (err != 0) {
			mfc_err_ctx("INIT_BUFFERS_RET error: %d\n", err);
			break;
		}

		s5p_mfc_change_state(ctx, MFCINST_RUNNING);
		if (ctx->type == MFCINST_DECODER) {
			if (dec->dst_memtype == V4L2_MEMORY_MMAP) {
				if (!dec->dpb_flush && !dec->remained) {
					mfc_debug(2, "INIT_BUFFERS with dpb_flush - leaving image in src queue.\n");
					spin_lock_irqsave(&dev->irqlock, flags);
					if (!list_empty(&ctx->src_queue)) {
						src_buf = list_entry(ctx->src_queue.next,
								struct s5p_mfc_buf, list);
						list_del(&src_buf->list);
						ctx->src_queue_cnt--;
						vb2_buffer_done(&src_buf->vb, VB2_BUF_STATE_DONE);
					}
					spin_unlock_irqrestore(&dev->irqlock, flags);
				} else {
					if (dec->dpb_flush)
						dec->dpb_flush = 0;
				}
			}
			if (ctx->wait_state == WAIT_DECODING) {
				ctx->wait_state = WAIT_INITBUF_DONE;
				mfc_debug(2, "INIT_BUFFER has done, but can't start decoding\n");
			}
			if (ctx->is_dpb_realloc)
				ctx->is_dpb_realloc = 0;
		}
		break;
	default:
		mfc_err_ctx("Unknown int reason: %d\n", reason);
	}

	/* clean-up interrupt: HW flag, hw_lock, SW work_bit */
	s5p_mfc_clear_int_flags();
	if (ctx->state != MFCINST_RES_CHANGE_INIT)
		clear_work_bit(ctx);

	spin_lock_irq(&dev->condlock);
	new_ctx = s5p_mfc_get_new_ctx(dev);
	if (new_ctx < 0) {
		/* No contexts to run */
		dev->has_job = false;
		spin_unlock_irq(&dev->condlock);
		s5p_mfc_clock_off(dev);
	} else {
		dev->has_job = true;
		spin_unlock_irq(&dev->condlock);
	}
	if (clear_hw_bit(ctx) == 0)
		mfc_err_ctx("hardware bit is already cleared\n");
	wake_up_ctx(ctx, reason, err);

	if (dev->has_job) {
		/* If cache flush command is needed, hander should stop */
		if (dev->curr_ctx_drm != dev->ctx[new_ctx]->is_drm) {
			mfc_debug(2, "DRM attribute is changed %d->%d\n",
					dev->curr_ctx_drm, dev->ctx[new_ctx]->is_drm);
			queue_work(dev->sched_wq, &dev->sched_work);
		} else if (dev->preempt_ctx > MFC_NO_INSTANCE_SET) {
			mfc_debug(5, "next ctx(%d) is preempted\n", new_ctx);
			s5p_mfc_try_run(dev);
		} else {
			mfc_debug(5, "next ctx(%d) is picked\n", new_ctx);
			dev->continue_ctx = new_ctx;
			s5p_mfc_try_run(dev);
		}
	}

irq_done:
	if (!dev->has_job)
		queue_work(dev->sched_wq, &dev->sched_work);

irq_end:
	mfc_debug_leave();
	return IRQ_HANDLED;
}

static inline int is_decoder_node(enum s5p_mfc_node_type node)
{
	if (node == MFCNODE_DECODER || node == MFCNODE_DECODER_DRM)
		return 1;

	return 0;
}

static inline int is_drm_node(enum s5p_mfc_node_type node)
{
	if (node == MFCNODE_DECODER_DRM || node == MFCNODE_ENCODER_DRM)
		return 1;

	return 0;
}

/* Open an MFC node */
static int s5p_mfc_open(struct file *file)
{
	struct s5p_mfc_ctx *ctx = NULL;
	struct s5p_mfc_dev *dev = video_drvdata(file);
	int ret = 0;
	enum s5p_mfc_node_type node;
	struct video_device *vdev = NULL;

	mfc_debug(2, "mfc driver open called\n");

	if (!dev) {
		mfc_err("no mfc device to run\n");
		goto err_no_device;
	}

	if (mutex_lock_interruptible(&dev->mfc_mutex))
		return -ERESTARTSYS;

	node = s5p_mfc_get_node_type(file);
	if (node == MFCNODE_INVALID) {
		mfc_err("cannot specify node type\n");
		ret = -ENOENT;
		goto err_node_type;
	}

	dev->num_inst++;	/* It is guarded by mfc_mutex in vfd */

	/* Allocate memory for context */
	ctx = kzalloc(sizeof(*ctx), GFP_KERNEL);
	if (!ctx) {
		mfc_err("Not enough memory.\n");
		ret = -ENOMEM;
		goto err_ctx_alloc;
	}

	switch (node) {
	case MFCNODE_DECODER:
		vdev = dev->vfd_dec;
		break;
	case MFCNODE_ENCODER:
		vdev = dev->vfd_enc;
		break;
	case MFCNODE_DECODER_DRM:
		vdev = dev->vfd_dec_drm;
		break;
	case MFCNODE_ENCODER_DRM:
		vdev = dev->vfd_enc_drm;
		break;
	default:
		mfc_err("Invalid node(%d)\n", node);
		break;
	}

	if (!vdev)
		goto err_vdev;

	v4l2_fh_init(&ctx->fh, vdev);
	file->private_data = &ctx->fh;
	v4l2_fh_add(&ctx->fh);

	ctx->dev = dev;

	/* Get context number */
	ctx->num = 0;
	while (dev->ctx[ctx->num]) {
		ctx->num++;
		if (ctx->num >= MFC_NUM_CONTEXTS) {
			mfc_err("Too many open contexts.\n");
			ret = -EBUSY;
			goto err_ctx_num;
		}
	}

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);
	dev->ctx[ctx->num] = ctx;

	init_waitqueue_head(&ctx->queue);

	if (is_decoder_node(node))
		ret = s5p_mfc_init_dec_ctx(ctx);
	else
		ret = s5p_mfc_init_enc_ctx(ctx);
	if (ret)
		goto err_ctx_init;

	ret = call_cop(ctx, init_ctx_ctrls, ctx);
	if (ret) {
		mfc_err_ctx("failed in init_ctx_ctrls\n");
		goto err_ctx_ctrls;
	}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (is_drm_node(node)) {
		if (dev->num_drm_inst < MFC_MAX_DRM_CTX) {
			if (ctx->raw_protect_flag || ctx->stream_protect_flag) {
				mfc_err_ctx("protect_flag(%#lx/%#lx) remained\n",
						ctx->raw_protect_flag,
						ctx->stream_protect_flag);
				ret = -EINVAL;
				goto err_drm_start;
			}
			dev->num_drm_inst++;
			ctx->is_drm = 1;

			mfc_info_ctx("DRM instance is opened [%d:%d]\n",
					dev->num_drm_inst, dev->num_inst);
		} else {
			mfc_err_ctx("Too many instance are opened for DRM\n");
			ret = -EINVAL;
			goto err_drm_start;
		}
	} else {
		mfc_info_ctx("NORMAL instance is opened [%d:%d]\n",
				dev->num_drm_inst, dev->num_inst);
	}
#endif

	/* Load firmware if this is the first instance */
	if (dev->num_inst == 1) {
		dev->watchdog_timer.expires = jiffies +
					msecs_to_jiffies(MFC_WATCHDOG_INTERVAL);
		add_timer(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		if (!dev->fw_status) {
			ret = s5p_mfc_alloc_firmware(dev);
			if (ret)
				goto err_fw_alloc;
			dev->fw_status = 1;
		}
		ret = s5p_mfc_load_firmware(dev);
		if (ret)
			goto err_fw_load;

		/* Check for supporting smc */
		ret = exynos_smc(SMC_DCPP_SUPPORT, 0, 0, 0);
		if (ret != DRMDRV_OK) {
			dev->is_support_smc = 0;
			mfc_err_ctx("Does not support DCPP(%#x)\n", ret);
		} else {
			dev->is_support_smc = 1;
			if (!dev->drm_fw_info.ofs) {
				mfc_err_ctx("DRM F/W buffer is not allocated.\n");
				dev->drm_fw_status = 0;
			} else {
				if (IS_MFCv10X(dev)) {
					ret = exynos_smc(SMC_DRM_SECBUF_PROT,
							dev->drm_fw_info.phys,
							dev->fw_region_size,
							ION_EXYNOS_HEAP_ID_VIDEO_FW);
					if (ret != DRMDRV_OK) {
						mfc_err_ctx("failed MFC DRM F/W prot(%#x)\n", ret);
						dev->drm_fw_status = 0;
					} else {
						dev->drm_fw_status = 1;
					}
				} else {
					size_t drm_fw_size, normal_fw_size;

					ion_exynos_contig_heap_info(ION_EXYNOS_ID_MFC_NFW,
							&dev->fw_info.phys, &normal_fw_size);
					ion_exynos_contig_heap_info(ION_EXYNOS_ID_MFC_FW,
							&dev->drm_fw_info.phys, &drm_fw_size);
					ret = exynos_smc(SMC_DRM_FW_LOADING,
							dev->drm_fw_info.phys,
							dev->fw_info.phys, dev->fw_size);
					if (ret != DRMDRV_OK) {
						mfc_err_ctx("MFC DRM F/W(%x) is skipped\n", ret);
						dev->drm_fw_status = 0;
					} else {
						dev->drm_fw_status = 1;
					}
				}
			}
		}
#else
		/* Load the FW */
		if (!dev->fw_status) {
			ret = s5p_mfc_alloc_firmware(dev);
			if (ret)
				goto err_fw_alloc;
			dev->fw_status = 1;
		}

		ret = s5p_mfc_load_firmware(dev);
		if (ret)
			goto err_fw_load;

#endif
		ret = s5p_mfc_alloc_dev_context_buffer(dev);
		if (ret)
			goto err_fw_load;

		mfc_debug(2, "power on\n");
		ret = s5p_mfc_power_on(dev);
		if (ret < 0) {
			mfc_err_ctx("power on failed\n");
			goto err_pwr_enable;
		}

		dev->curr_ctx = ctx->num;
		dev->preempt_ctx = MFC_NO_INSTANCE_SET;
		dev->continue_ctx = MFC_NO_INSTANCE_SET;
		dev->curr_ctx_drm = ctx->is_drm;

		/* Init the FW */
		ret = s5p_mfc_init_hw(dev);
		if (ret) {
			mfc_err_ctx("Failed to init mfc h/w\n");
			goto err_hw_init;
		}

#ifdef NAL_Q_ENABLE
		dev->nal_q_handle = s5p_mfc_nal_q_create(dev);
		if (dev->nal_q_handle == NULL)
			mfc_err("NAL Q: Can't create nal q\n");
#endif
	}

	mfc_info_ctx("MFC open completed [%d:%d] dev = %p, ctx = %p, version = %d\n",
			dev->num_drm_inst, dev->num_inst, dev, ctx, MFC_DRIVER_INFO);
	mutex_unlock(&dev->mfc_mutex);
	return ret;

	/* Deinit when failure occured */
err_hw_init:
	s5p_mfc_power_off(dev);

err_pwr_enable:
	s5p_mfc_release_dev_context_buffer(dev);

err_fw_load:
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (dev->drm_fw_status) {
		int smc_ret = 0;
		dev->is_support_smc = 0;
		dev->drm_fw_status = 0;
		if (IS_MFCv10X(dev)) {
			smc_ret = exynos_smc(SMC_DRM_SECBUF_UNPROT,
					dev->drm_fw_info.phys,
					dev->fw_region_size,
					ION_EXYNOS_HEAP_ID_VIDEO_FW);
			if (smc_ret != DRMDRV_OK)
				mfc_err_ctx("failed MFC DRM F/W unprot(%#x)\n", smc_ret);
		}
	}
#endif
	s5p_mfc_release_firmware(dev);
	dev->fw_status = 0;

err_fw_alloc:
	del_timer_sync(&dev->watchdog_timer);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	if (ctx->is_drm)
		dev->num_drm_inst--;

err_drm_start:
#endif
	call_cop(ctx, cleanup_ctx_ctrls, ctx);

err_ctx_ctrls:
	if (is_decoder_node(node)) {
		kfree(ctx->dec_priv->ref_info);
		kfree(ctx->dec_priv);
	} else {
		kfree(ctx->enc_priv);
	}

err_ctx_init:
	dev->ctx[ctx->num] = 0;

err_ctx_num:
	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);
err_vdev:
	kfree(ctx);

err_ctx_alloc:
	dev->num_inst--;

err_node_type:
	mfc_info_dev("MFC driver open is failed [%d:%d]\n",
			dev->num_drm_inst, dev->num_inst);
	mutex_unlock(&dev->mfc_mutex);

err_no_device:

	return ret;
}

/* Release MFC context */
static int s5p_mfc_release(struct file *file)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	struct s5p_mfc_dev *dev = NULL;
	struct s5p_mfc_enc *enc = NULL;
	int ret = 0;

	dev = ctx->dev;
	if (!dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	mutex_lock(&dev->mfc_mutex);

	mfc_info_ctx("MFC driver release is called [%d:%d], is_drm(%d)\n",
			dev->num_drm_inst, dev->num_inst, ctx->is_drm);

	spin_lock_irq(&dev->condlock);
	set_bit(ctx->num, &dev->ctx_stop_bits);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If a H/W operation is in progress, wait for it complete */
	if (need_to_wait_nal_abort(ctx)) {
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_NAL_ABORT_RET))
			s5p_mfc_cleanup_timeout_and_try_run(ctx);
	} else if (test_bit(ctx->num, &dev->hw_lock)) {
		ret = wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));
		if (ret == 0)
			mfc_err_ctx("wait for event failed\n");
	}

	if (ctx->type == MFCINST_ENCODER) {
		enc = ctx->enc_priv;
		if (!enc) {
			mfc_err_ctx("no mfc encoder to run\n");
			ret = -EINVAL;
			goto err_release;
		}

		if (enc->in_slice || enc->buf_full) {
			s5p_mfc_change_state(ctx, MFCINST_ABORT_INST);
			spin_lock_irq(&dev->condlock);
			set_bit(ctx->num, &dev->ctx_work_bits);
			spin_unlock_irq(&dev->condlock);
			s5p_mfc_try_run(dev);
			if (s5p_mfc_wait_for_done_ctx(ctx,
					S5P_FIMV_R2H_CMD_NAL_ABORT_RET))
				s5p_mfc_cleanup_timeout_and_try_run(ctx);

			enc->in_slice = 0;
			enc->buf_full = 0;
		}
	}

	if (call_cop(ctx, cleanup_ctx_ctrls, ctx) < 0)
		mfc_err_ctx("failed in cleanup_ctx_ctrl\n");

	v4l2_fh_del(&ctx->fh);
	v4l2_fh_exit(&ctx->fh);

	/* Mark context as idle */
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_work_bits);
	spin_unlock_irq(&dev->condlock);

	/* If instance was initialised then
	 * return instance and free reosurces */
	if (!atomic_read(&dev->watchdog_run) &&
		(ctx->inst_no != MFC_NO_INSTANCE_SET)) {
		/* Wait for hw_lock == 0 for this context */
		ret = wait_event_timeout(ctx->queue,
				(test_bit(ctx->num, &dev->hw_lock) == 0),
				msecs_to_jiffies(MFC_INT_TIMEOUT));
		if (ret == 0) {
			mfc_err_ctx("Waiting for hardware to finish timed out\n");
			ret = -EBUSY;
			goto err_release;
		}

		s5p_mfc_clean_ctx_int_flags(ctx);
		s5p_mfc_change_state(ctx, MFCINST_RETURN_INST);
		spin_lock_irq(&dev->condlock);
		set_bit(ctx->num, &dev->ctx_work_bits);
		spin_unlock_irq(&dev->condlock);

		/* To issue the command 'CLOSE_INSTANCE' */
		s5p_mfc_try_run(dev);

		/* Wait until instance is returned or timeout occured */
		if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET)) {
			mfc_err_ctx("Waiting for CLOSE_INSTANCE timed out\n");
			dev->curr_ctx_drm = ctx->is_drm;
			set_bit(ctx->num, &dev->hw_lock);
			s5p_mfc_clock_on(dev);
			s5p_mfc_close_inst(ctx);
			if (s5p_mfc_wait_for_done_ctx(ctx,
				S5P_FIMV_R2H_CMD_CLOSE_INSTANCE_RET)) {
				mfc_err_ctx("Abnormal h/w state.\n");

				/* cleanup for the next open */
				if (dev->curr_ctx == ctx->num)
					clear_bit(ctx->num, &dev->hw_lock);
				if (ctx->is_drm)
					dev->num_drm_inst--;
				dev->num_inst--;

				mfc_info_dev("Failed to release MFC inst[%d:%d]\n",
						dev->num_drm_inst, dev->num_inst);

				if (dev->num_inst == 0) {
					s5p_mfc_deinit_hw(dev);
					del_timer_sync(&dev->watchdog_timer);

					flush_workqueue(dev->sched_wq);

					s5p_mfc_clock_off(dev);
					mfc_debug(2, "power off\n");
					s5p_mfc_power_off(dev);

					s5p_mfc_release_dev_context_buffer(dev);
					dev->drm_fw_status = 0;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
					dev->is_support_smc = 0;
					if (IS_MFCv10X(dev)) {
						ret = exynos_smc(SMC_DRM_SECBUF_UNPROT,
								dev->drm_fw_info.phys,
								dev->fw_region_size,
								ION_EXYNOS_HEAP_ID_VIDEO_FW);
						if (ret != DRMDRV_OK) {
							mfc_err_ctx("failed MFC DRM F/W unprot(%#x)\n",
									ret);
							goto err_release;
						}
					}
#endif
				} else {
					s5p_mfc_clock_off(dev);
				}


				mutex_unlock(&dev->mfc_mutex);

				return -EIO;
			}
		}

		ctx->inst_no = MFC_NO_INSTANCE_SET;
	}
	/* hardware locking scheme */
	if (dev->curr_ctx == ctx->num)
		clear_bit(ctx->num, &dev->hw_lock);

	if (ctx->is_drm)
		dev->num_drm_inst--;
	dev->num_inst--;

	if (dev->num_inst == 0) {
		s5p_mfc_deinit_hw(dev);
		del_timer_sync(&dev->watchdog_timer);

		flush_workqueue(dev->sched_wq);

		mfc_debug(2, "power off\n");
		s5p_mfc_power_off(dev);

		s5p_mfc_release_dev_context_buffer(dev);
		dev->drm_fw_status = 0;

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
		dev->is_support_smc = 0;
		if (IS_MFCv10X(dev)) {
			ret = exynos_smc(SMC_DRM_SECBUF_UNPROT,
					dev->drm_fw_info.phys,
					dev->fw_region_size,
					ION_EXYNOS_HEAP_ID_VIDEO_FW);
			if (ret != DRMDRV_OK) {
				mfc_err_ctx("failed MFC DRM F/W unprot(%#x)\n",
						ret);
				goto err_release;
			}
		}
#endif

#ifdef NAL_Q_ENABLE
		if (dev->nal_q_handle) {
			ret = s5p_mfc_nal_q_destroy(dev, dev->nal_q_handle);
			if (ret) {
				mfc_err_ctx("failed nal_q destroy\n");
				goto err_release;
			}
		}
#endif
	}

	s5p_mfc_qos_off(ctx);

	/* Free resources */
	vb2_queue_release(&ctx->vq_src);
	vb2_queue_release(&ctx->vq_dst);

	s5p_mfc_release_codec_buffers(ctx);
	s5p_mfc_release_instance_buffer(ctx);

	if (ctx->type == MFCINST_DECODER) {
		dec_cleanup_user_shared_handle(ctx);
		kfree(ctx->dec_priv->ref_info);
		kfree(ctx->dec_priv);
	} else if (ctx->type == MFCINST_ENCODER) {
		enc_cleanup_user_shared_handle(ctx, &enc->sh_handle_svc);
		enc_cleanup_user_shared_handle(ctx, &enc->sh_handle_roi);
		s5p_mfc_release_enc_roi_buffer(ctx);
		kfree(ctx->enc_priv);
	}

	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_stop_bits);
	spin_unlock_irq(&dev->condlock);

	dev->ctx[ctx->num] = 0;
	kfree(ctx);

	mfc_info_dev("mfc driver release finished [%d:%d], dev = %p\n",
			dev->num_drm_inst, dev->num_inst, dev);
	mutex_unlock(&dev->mfc_mutex);

	return ret;

err_release:
	spin_lock_irq(&dev->condlock);
	clear_bit(ctx->num, &dev->ctx_stop_bits);
	spin_unlock_irq(&dev->condlock);

	mutex_unlock(&dev->mfc_mutex);

	return ret;
}

/* Poll */
static unsigned int s5p_mfc_poll(struct file *file,
				 struct poll_table_struct *wait)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	unsigned int ret = 0;
	enum s5p_mfc_node_type node_type;

	node_type = s5p_mfc_get_node_type(file);

	if (is_decoder_node(node_type))
		ret = vb2_poll(&ctx->vq_src, file, wait);
	else
		ret = vb2_poll(&ctx->vq_dst, file, wait);

	return ret;
}

/* Mmap */
static int s5p_mfc_mmap(struct file *file, struct vm_area_struct *vma)
{
	struct s5p_mfc_ctx *ctx = fh_to_mfc_ctx(file->private_data);
	unsigned long offset = vma->vm_pgoff << PAGE_SHIFT;
	int ret;

	mfc_debug_enter();

	if (offset < DST_QUEUE_OFF_BASE) {
		mfc_debug(2, "mmaping source.\n");
		ret = vb2_mmap(&ctx->vq_src, vma);
	} else {		/* capture */
		mfc_debug(2, "mmaping destination.\n");
		vma->vm_pgoff -= (DST_QUEUE_OFF_BASE >> PAGE_SHIFT);
		ret = vb2_mmap(&ctx->vq_dst, vma);
	}
	mfc_debug_leave();
	return ret;
}

/* v4l2 ops */
static const struct v4l2_file_operations s5p_mfc_fops = {
	.owner = THIS_MODULE,
	.open = s5p_mfc_open,
	.release = s5p_mfc_release,
	.poll = s5p_mfc_poll,
	.unlocked_ioctl = video_ioctl2,
	.mmap = s5p_mfc_mmap,
};

/* videodec structure */
static struct video_device s5p_mfc_dec_videodev = {
	.name = S5P_MFC_DEC_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static struct video_device s5p_mfc_enc_videodev = {
	.name = S5P_MFC_ENC_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static struct video_device s5p_mfc_dec_drm_videodev = {
	.name = S5P_MFC_DEC_DRM_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static struct video_device s5p_mfc_enc_drm_videodev = {
	.name = S5P_MFC_ENC_DRM_NAME,
	.fops = &s5p_mfc_fops,
	.minor = -1,
	.release = video_device_release,
};

static void *mfc_get_drv_data(struct platform_device *pdev);

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
static int parse_mfc_qos_platdata(struct device_node *np, char *node_name,
	struct s5p_mfc_qos *pdata)
{
	int ret = 0;
	struct device_node *np_qos;

	np_qos = of_find_node_by_name(np, node_name);
	if (!np_qos) {
		pr_err("%s: could not find mfc_qos_platdata node\n",
			node_name);
		return -EINVAL;
	}

	of_property_read_u32(np_qos, "thrd_mb", &pdata->thrd_mb);
	of_property_read_u32(np_qos, "freq_int", &pdata->freq_int);
	of_property_read_u32(np_qos, "freq_mif", &pdata->freq_mif);
	of_property_read_u32(np_qos, "freq_cpu", &pdata->freq_cpu);
	of_property_read_u32(np_qos, "freq_kfc", &pdata->freq_kfc);
	of_property_read_u32(np_qos, "has_enc_table", &pdata->has_enc_table);

	return ret;
}
#endif

static void mfc_parse_dt(struct device_node *np, struct s5p_mfc_dev *mfc)
{
	struct s5p_mfc_platdata	*pdata = mfc->pdata;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	char node_name[50];
	int i;
#endif

	if (!np)
		return;

	of_property_read_u32(np, "ip_ver", &pdata->ip_ver);
	of_property_read_u32(np, "clock_rate", &pdata->clock_rate);
	of_property_read_u32(np, "min_rate", &pdata->min_rate);
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	of_property_read_u32(np, "num_qos_steps", &pdata->num_qos_steps);

	pdata->qos_table = devm_kzalloc(mfc->device,
			sizeof(struct s5p_mfc_qos) * pdata->num_qos_steps, GFP_KERNEL);

	for (i = 0; i < pdata->num_qos_steps; i++) {
		snprintf(node_name, sizeof(node_name), "mfc_qos_variant_%d", i);
		parse_mfc_qos_platdata(np, node_name, &pdata->qos_table[i]);
	}
#endif
}

/* MFC probe function */
static int s5p_mfc_probe(struct platform_device *pdev)
{
	struct s5p_mfc_dev *dev;
	struct video_device *vfd;
	struct resource *res;
	int ret = -ENOENT;
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	int i;
#endif

	dev_dbg(&pdev->dev, "%s()\n", __func__);
	dev = devm_kzalloc(&pdev->dev, sizeof(struct s5p_mfc_dev), GFP_KERNEL);
	if (!dev) {
		dev_err(&pdev->dev, "Not enough memory for MFC device.\n");
		return -ENOMEM;
	}

	spin_lock_init(&dev->irqlock);
	spin_lock_init(&dev->condlock);

	dev->device = &pdev->dev;
	dev->pdata = pdev->dev.platform_data;

	dev->variant = mfc_get_drv_data(pdev);

	if (dev->device->of_node)
		dev->id = of_alias_get_id(pdev->dev.of_node, "mfc");

	dev_dbg(&pdev->dev, "of alias get id : mfc-%d \n", dev->id);

	if (dev->id < 0 || dev->id >= dev->variant->num_entities) {
		dev_err(&pdev->dev, "Invalid platform device id: %d\n", dev->id);
		ret = -EINVAL;
		goto err_pm;

	}

	dev->pdata = devm_kzalloc(&pdev->dev, sizeof(struct s5p_mfc_platdata), GFP_KERNEL);
	if (!dev->pdata) {
		dev_err(&pdev->dev, "no memory for state\n");
		ret = -ENOMEM;
		goto err_pm;
	}

	mfc_parse_dt(dev->device->of_node, dev);

	atomic_set(&dev->trace_ref, 0);
	dev->mfc_trace = g_mfc_trace[dev->id];

	ret = s5p_mfc_init_pm(dev);
	if (ret < 0) {
		dev_err(&pdev->dev, "failed to setup mfc clock & power\n");
		goto err_pm;
	}

	res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get memory region resource\n");
		ret = -ENOENT;
		goto err_res_mem;
	}
	dev->mfc_mem = request_mem_region(res->start, resource_size(res),
					pdev->name);
	if (dev->mfc_mem == NULL) {
		dev_err(&pdev->dev, "failed to get memory region\n");
		ret = -ENOENT;
		goto err_req_mem;
	}
	dev->regs_base = ioremap(dev->mfc_mem->start,
				resource_size(dev->mfc_mem));
	if (dev->regs_base == NULL) {
		dev_err(&pdev->dev, "failed to ioremap address region\n");
		ret = -ENOENT;
		goto err_ioremap;
	}

	res = platform_get_resource(pdev, IORESOURCE_IRQ, 0);
	if (res == NULL) {
		dev_err(&pdev->dev, "failed to get irq resource\n");
		ret = -ENOENT;
		goto err_res_irq;
	}
	dev->irq = res->start;
	ret = request_threaded_irq(dev->irq, s5p_mfc_top_half_irq, s5p_mfc_irq,
				IRQF_ONESHOT, pdev->name, dev);
	if (ret != 0) {
		dev_err(&pdev->dev, "failed to install irq (%d)\n", ret);
		goto err_req_irq;
	}

	mutex_init(&dev->mfc_mutex);

	ret = v4l2_device_register(&pdev->dev, &dev->v4l2_dev);
	if (ret)
		goto err_v4l2_dev;

	init_waitqueue_head(&dev->queue);

	/* decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_dec;
	}
	*vfd = s5p_mfc_dec_videodev;

	vfd->ioctl_ops = get_dec_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;

	snprintf(vfd->name, sizeof(vfd->name), "%s%d", s5p_mfc_dec_videodev.name, dev->id);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, EXYNOS_VIDEONODE_MFC_DEC + 60*dev->id);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_dec;
	}
	v4l2_info(&dev->v4l2_dev, "decoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_dec = vfd;

	video_set_drvdata(vfd, dev);

	/* encoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_enc;
	}
	*vfd = s5p_mfc_enc_videodev;

	vfd->ioctl_ops = get_enc_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	snprintf(vfd->name, sizeof(vfd->name), "%s%d", s5p_mfc_enc_videodev.name, dev->id);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, EXYNOS_VIDEONODE_MFC_ENC + 60*dev->id);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_enc;
	}
	v4l2_info(&dev->v4l2_dev, "encoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_enc = vfd;

	video_set_drvdata(vfd, dev);

	/* secure decoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_dec_drm;
	}
	*vfd = s5p_mfc_dec_drm_videodev;

	vfd->ioctl_ops = get_dec_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;

	snprintf(vfd->name, sizeof(vfd->name), "%s%d", s5p_mfc_dec_drm_videodev.name, dev->id);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, EXYNOS_VIDEONODE_MFC_DEC_DRM + 60*dev->id);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_dec_drm;
	}
	v4l2_info(&dev->v4l2_dev, "secure decoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_dec_drm = vfd;

	video_set_drvdata(vfd, dev);

	/* secure encoder */
	vfd = video_device_alloc();
	if (!vfd) {
		v4l2_err(&dev->v4l2_dev, "Failed to allocate video device\n");
		ret = -ENOMEM;
		goto alloc_vdev_enc_drm;
	}
	*vfd = s5p_mfc_enc_drm_videodev;

	vfd->ioctl_ops = get_enc_v4l2_ioctl_ops();

	vfd->lock = &dev->mfc_mutex;
	vfd->v4l2_dev = &dev->v4l2_dev;
	vfd->vfl_dir = VFL_DIR_M2M;
	snprintf(vfd->name, sizeof(vfd->name), "%s%d", s5p_mfc_enc_drm_videodev.name, dev->id);

	ret = video_register_device(vfd, VFL_TYPE_GRABBER, EXYNOS_VIDEONODE_MFC_ENC_DRM + 60*dev->id);
	if (ret) {
		v4l2_err(&dev->v4l2_dev, "Failed to register video device\n");
		video_device_release(vfd);
		goto reg_vdev_enc_drm;
	}
	v4l2_info(&dev->v4l2_dev, "secure encoder registered as /dev/video%d\n",
								vfd->num);
	dev->vfd_enc_drm = vfd;

	video_set_drvdata(vfd, dev);
	/* end of node setting*/

	platform_set_drvdata(pdev, dev);

	dev->hw_lock = 0;
	dev->watchdog_wq =
		create_singlethread_workqueue("s5p_mfc/watchdog");
	if (!dev->watchdog_wq) {
		dev_err(&pdev->dev, "failed to create workqueue for watchdog\n");
		goto err_wq_watchdog;
	}
	INIT_WORK(&dev->watchdog_work, s5p_mfc_watchdog_worker);
	atomic_set(&dev->watchdog_cnt, 0);
	atomic_set(&dev->watchdog_run, 0);
	init_timer(&dev->watchdog_timer);
	dev->watchdog_timer.data = (unsigned long)dev;
	dev->watchdog_timer.function = s5p_mfc_watchdog;

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	INIT_LIST_HEAD(&dev->qos_queue);
#endif

	/* default FW alloc is added */
	dev->alloc_ctx = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			IS_MFCV6(dev) ? SZ_4K : SZ_128K,
			VB2ION_CTX_VMCONTIG |
			VB2ION_CTX_IOMMU |
			VB2ION_CTX_UNCACHED);
	if (IS_ERR(dev->alloc_ctx)) {
		mfc_err_dev("Couldn't prepare allocator ctx.\n");
		ret = PTR_ERR(dev->alloc_ctx);
		goto alloc_ctx_fail;
	}

	dev->sched_wq = alloc_workqueue("s5p_mfc/sched", WQ_UNBOUND
					| WQ_MEM_RECLAIM | WQ_HIGHPRI, 1);
	if (dev->sched_wq == NULL) {
		dev_err(&pdev->dev, "failed to create workqueue for scheduler\n");
		goto err_wq_sched;
	}
	INIT_WORK(&dev->sched_work, mfc_sched_worker);

#ifdef CONFIG_ION_EXYNOS
	dev->mfc_ion_client = ion_client_create(ion_exynos, "mfc");
	if (IS_ERR(dev->mfc_ion_client)) {
		dev_err(&pdev->dev, "failed to ion_client_create\n");
		goto err_ion_client;
	}
#endif

	dev->alloc_ctx_fw = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			IS_MFCV6(dev) ? SZ_4K : SZ_128K,
			VB2ION_CTX_DRM_MFCNFW | VB2ION_CTX_IOMMU);
	if (IS_ERR(dev->alloc_ctx_fw)) {
		mfc_err_dev("failed to prepare F/W allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_fw);
		goto alloc_ctx_fw_fail;
	}

#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	dev->alloc_ctx_drm_fw = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			IS_MFCV6(dev) ? SZ_4K : SZ_128K,
			VB2ION_CTX_DRM_MFCFW | VB2ION_CTX_IOMMU);
	if (IS_ERR(dev->alloc_ctx_drm_fw)) {
		mfc_err_dev("failed to prepare F/W allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_drm_fw);
		goto alloc_ctx_drm_fw_fail;
	}
	dev->alloc_ctx_drm = (struct vb2_alloc_ctx *)
		vb2_ion_create_context(&pdev->dev,
			SZ_4K,
			VB2ION_CTX_UNCACHED | VB2ION_CTX_DRM_VIDEO | VB2ION_CTX_IOMMU);
	if (IS_ERR(dev->alloc_ctx_drm)) {
		mfc_err_dev("failed to prepare DRM allocation context\n");
		ret = PTR_ERR(dev->alloc_ctx_drm);
		goto alloc_ctx_sh_fail;
	}
#endif

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
	atomic_set(&dev->qos_req_cur, 0);

	dev->qos_req_cnt = kzalloc(dev->pdata->num_qos_steps * sizeof(atomic_t),
					GFP_KERNEL);
	if (!dev->qos_req_cnt) {
		dev_err(&pdev->dev, "failed to allocate QoS request count\n");
		ret = -ENOMEM;
		goto err_qos_cnt;
	}
	for (i = 0; i < dev->pdata->num_qos_steps; i++)
		atomic_set(&dev->qos_req_cnt[i], 0);

	for (i = 0; i < dev->pdata->num_qos_steps; i++) {
		mfc_info_dev("QoS table[%d] int : %d, mif : %d\n",
				i,
				dev->pdata->qos_table[i].freq_int,
				dev->pdata->qos_table[i].freq_mif);
	}
#endif

	iovmm_set_fault_handler(dev->device,
		exynos_mfc_sysmmu_fault_handler, dev);

	g_mfc_dev[dev->id] = dev;

	vb2_ion_attach_iommu(dev->alloc_ctx);

	pr_debug("%s--\n", __func__);
	return 0;

/* Deinit MFC if probe had failed */
#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
err_qos_cnt:
#endif
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
alloc_ctx_sh_fail:
	vb2_ion_destroy_context(dev->alloc_ctx_drm_fw);
alloc_ctx_drm_fw_fail:
#endif
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
alloc_ctx_fw_fail:
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->mfc_ion_client);
err_ion_client:
#endif
	destroy_workqueue(dev->sched_wq);
err_wq_sched:
	vb2_ion_destroy_context(dev->alloc_ctx);
alloc_ctx_fail:
	destroy_workqueue(dev->watchdog_wq);
err_wq_watchdog:
	video_unregister_device(dev->vfd_enc_drm);
reg_vdev_enc_drm:
alloc_vdev_enc_drm:
	video_unregister_device(dev->vfd_dec_drm);
reg_vdev_dec_drm:
alloc_vdev_dec_drm:
	video_unregister_device(dev->vfd_enc);
reg_vdev_enc:
alloc_vdev_enc:
	video_unregister_device(dev->vfd_dec);
reg_vdev_dec:
alloc_vdev_dec:
	v4l2_device_unregister(&dev->v4l2_dev);
err_v4l2_dev:
	mutex_destroy(&dev->mfc_mutex);
	free_irq(dev->irq, dev);
err_req_irq:
err_res_irq:
	iounmap(dev->regs_base);
err_ioremap:
	release_mem_region(dev->mfc_mem->start, resource_size(dev->mfc_mem));
err_req_mem:
err_res_mem:
	s5p_mfc_final_pm(dev);
err_pm:
	return ret;
}

/* Remove the driver */
static int s5p_mfc_remove(struct platform_device *pdev)
{
	struct s5p_mfc_dev *dev = platform_get_drvdata(pdev);

	dev_dbg(&pdev->dev, "%s++\n", __func__);
	v4l2_info(&dev->v4l2_dev, "Removing %s\n", pdev->name);
	del_timer_sync(&dev->watchdog_timer);
	flush_workqueue(dev->watchdog_wq);
	destroy_workqueue(dev->watchdog_wq);
	flush_workqueue(dev->sched_wq);
	destroy_workqueue(dev->sched_wq);
	video_unregister_device(dev->vfd_enc);
	video_unregister_device(dev->vfd_dec);
	v4l2_device_unregister(&dev->v4l2_dev);
#ifdef CONFIG_EXYNOS_CONTENT_PATH_PROTECTION
	remove_proc_entry(MFC_PROC_FW_STATUS, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_DRM_INSTANCE_NUMBER, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_INSTANCE_NUMBER, mfc_proc_entry);
	remove_proc_entry(MFC_PROC_ROOT, NULL);
	vb2_ion_destroy_context(dev->alloc_ctx_drm);
#endif
	vb2_ion_destroy_context(dev->alloc_ctx_fw);
#ifdef CONFIG_ION_EXYNOS
	ion_client_destroy(dev->mfc_ion_client);
#endif
	mfc_debug(2, "Will now deinit HW\n");
	s5p_mfc_deinit_hw(dev);
	vb2_ion_destroy_context(dev->alloc_ctx);
	free_irq(dev->irq, dev);
	iounmap(dev->regs_base);
	release_mem_region(dev->mfc_mem->start, resource_size(dev->mfc_mem));
	s5p_mfc_final_pm(dev);
	kfree(dev);
	dev_dbg(&pdev->dev, "%s--\n", __func__);
	return 0;
}

#ifdef CONFIG_PM_SLEEP
static int s5p_mfc_suspend(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = s5p_mfc_sleep(m_dev);

	return ret;
}

static int s5p_mfc_resume(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int ret;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (m_dev->num_inst == 0)
		return 0;

	ret = s5p_mfc_wakeup(m_dev);

	return ret;
}
#endif

#ifdef CONFIG_PM_RUNTIME
static int s5p_mfc_runtime_suspend(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 0);

	return 0;
}

static int s5p_mfc_runtime_idle(struct device *dev)
{
	return 0;
}

static int s5p_mfc_runtime_resume(struct device *dev)
{
	struct s5p_mfc_dev *m_dev = platform_get_drvdata(to_platform_device(dev));
	int pre_power;

	if (!m_dev) {
		mfc_err("no mfc device to run\n");
		return -EINVAL;
	}

	if (!m_dev->alloc_ctx)
		return 0;

	pre_power = atomic_read(&m_dev->pm.power);
	atomic_set(&m_dev->pm.power, 1);

	return 0;
}
#endif

/* Power management */
static const struct dev_pm_ops s5p_mfc_pm_ops = {
	SET_SYSTEM_SLEEP_PM_OPS(s5p_mfc_suspend, s5p_mfc_resume)
	SET_RUNTIME_PM_OPS(
			s5p_mfc_runtime_suspend,
			s5p_mfc_runtime_resume,
			s5p_mfc_runtime_idle
	)
};

struct s5p_mfc_buf_size_v5 mfc_buf_size_v5 = {
	.h264_ctx_buf		= PAGE_ALIGN(0x96000),	/* 600KB */
	.non_h264_ctx_buf	= PAGE_ALIGN(0x2800),	/*  10KB */
	.desc_buf		= PAGE_ALIGN(0x20000),	/* 128KB */
	.shared_buf		= PAGE_ALIGN(0x1000),	/*   4KB */
};

struct s5p_mfc_buf_size_v6 mfc_buf_size_v6 = {
	.dev_ctx	= PAGE_ALIGN(0x7800),	/*  30KB */
	.h264_dec_ctx	= PAGE_ALIGN(0x200000),	/* 1.6MB */
	.other_dec_ctx	= PAGE_ALIGN(0x5000),	/*  20KB */
	.h264_enc_ctx	= PAGE_ALIGN(0x19000),	/* 100KB */
	.hevc_enc_ctx	= PAGE_ALIGN(0x7800),	/*  30KB */
	.other_enc_ctx	= PAGE_ALIGN(0x3C00),	/*  15KB */
	.shared_buf	= PAGE_ALIGN(0x2000),	/*   8KB */
};

struct s5p_mfc_buf_size buf_size_v5 = {
	.firmware_code	= PAGE_ALIGN(0x60000),	/* 384KB */
	.cpb_buf	= PAGE_ALIGN(0x300000),	/*   3MB */
	.buf		= &mfc_buf_size_v5,
};

struct s5p_mfc_buf_size buf_size_v6 = {
	.firmware_code	= PAGE_ALIGN(0x100000),	/* 1MB */
	.cpb_buf	= PAGE_ALIGN(0x300000),	/* 3MB */
	.buf		= &mfc_buf_size_v6,
};

struct s5p_mfc_buf_align mfc_buf_align_v5 = {
	.mfc_base_align = 17,
};

struct s5p_mfc_buf_align mfc_buf_align_v6 = {
	.mfc_base_align = 0,
};

static struct s5p_mfc_variant mfc_drvdata_v5 = {
	.buf_size = &buf_size_v5,
	.buf_align = &mfc_buf_align_v5,
};

static struct s5p_mfc_variant mfc_drvdata_v6 = {
	.buf_size = &buf_size_v6,
	.buf_align = &mfc_buf_align_v6,
	.num_entities = 2,
};

static struct platform_device_id mfc_driver_ids[] = {
	{
		.name = "s5p-mfc",
		.driver_data = (unsigned long)&mfc_drvdata_v6,
	}, {
		.name = "s5p-mfc-v5",
		.driver_data = (unsigned long)&mfc_drvdata_v5,
	}, {
		.name = "s5p-mfc-v6",
		.driver_data = (unsigned long)&mfc_drvdata_v6,
	},
	{},
};
MODULE_DEVICE_TABLE(platform, mfc_driver_ids);

static const struct of_device_id exynos_mfc_match[] = {
	{
		.compatible = "samsung,mfc-v5",
		.data = &mfc_drvdata_v5,
	}, {
		.compatible = "samsung,mfc-v6",
		.data = &mfc_drvdata_v6,
	},
	{},
};
MODULE_DEVICE_TABLE(of, exynos_mfc_match);

static void *mfc_get_drv_data(struct platform_device *pdev)
{
	struct s5p_mfc_variant *driver_data = NULL;

	if (pdev->dev.of_node) {
		const struct of_device_id *match;
		match = of_match_node(of_match_ptr(exynos_mfc_match),
				pdev->dev.of_node);
		if (match)
			driver_data = (struct s5p_mfc_variant *)match->data;
	} else {
		driver_data = (struct s5p_mfc_variant *)
			platform_get_device_id(pdev)->driver_data;
	}
	return driver_data;
}

static struct platform_driver s5p_mfc_driver = {
	.probe		= s5p_mfc_probe,
	.remove		= s5p_mfc_remove,
	.id_table	= mfc_driver_ids,
	.driver	= {
		.name	= S5P_MFC_NAME,
		.owner	= THIS_MODULE,
		.pm	= &s5p_mfc_pm_ops,
		.of_match_table = exynos_mfc_match,
		.suppress_bind_attrs = true,
	},
};


module_platform_driver(s5p_mfc_driver);

MODULE_LICENSE("GPL");
MODULE_AUTHOR("Kamil Debski <k.debski@samsung.com>");

