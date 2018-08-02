/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 * exynos5 fimc-is/mipi-csi functions
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
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/videodev2.h>
#include <linux/io.h>
#include <linux/phy/phy.h>

#include "fimc-is-config.h"
#include "fimc-is-core.h"
#include "fimc-is-regs.h"
#include "fimc-is-device-csi.h"
#include "fimc-is-device-sensor.h"

#define CSI_VALID_ENTRY_TO_CH(id) ((id) >= ENTRY_SSVC0 && (id) <= ENTRY_SSVC3)
#define CSI_ENTRY_TO_CH(id) ({BUG_ON(!CSI_VALID_ENTRY_TO_CH(id));id - ENTRY_SSVC0;}) /* range : vc0(0) ~ vc3(3) */
#define CSI_CH_TO_ENTRY(id) (id + ENTRY_SSVC0) /* range : ENTRY_SSVC0 ~ ENTRY_SSVC3 */

static void csis_flush_vc_buf_done(struct fimc_is_device_csi *csi, u32 vc,
		enum fimc_is_frame_state target,
		enum vb2_buffer_state state);

static inline void notify_fcount(struct fimc_is_device_csi *csi)
{
	if (test_bit(CSIS_JOIN_ISCHAIN, &csi->state)) {
		if (csi->instance== CSI_ID_A)
			writel(atomic_read(&csi->fcount), notify_fcount_sen0);
		else if (csi->instance == CSI_ID_B)
			writel(atomic_read(&csi->fcount), notify_fcount_sen1);
		else if (csi->instance == CSI_ID_C)
			writel(atomic_read(&csi->fcount), notify_fcount_sen2);
		else
			err("[CSI] unresolved channel(%d)", csi->instance);
	}
}

#if defined(SUPPORTED_EARLYBUF_DONE_SW)
static enum hrtimer_restart csis_early_buf_done(struct hrtimer *timer)
{
	struct fimc_is_device_sensor *device = container_of(timer, struct fimc_is_device_sensor, early_buf_timer);
	struct fimc_is_device_csi *csi;

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(device->subdev_csi);
	csi->sw_checker = EXPECT_FRAME_START;
	tasklet_schedule(&csi->tasklet_csis_end);

	return HRTIMER_NORESTART;
}
#endif

static inline void csi_frame_start_inline(struct fimc_is_device_csi *csi)
{
#ifdef DBG_CSIISR
	printk(KERN_CONT "<");
#endif
	/* frame start interrupt */
	csi->sw_checker = EXPECT_FRAME_END;
	atomic_inc(&csi->fcount);
#ifdef ENABLE_IS_CORE
	notify_fcount(csi);
#else
	{
		u32 vsync_cnt = atomic_read(&csi->fcount);
		v4l2_subdev_notify(*csi->subdev, CSI_NOTIFY_VSYNC, &vsync_cnt);
	}
#endif

	tasklet_schedule(&csi->tasklet_csis_str);
}

static inline void csi_frame_end_inline(struct fimc_is_device_csi *csi)
{
#ifdef DBG_CSIISR
	printk(KERN_CONT ">");
#endif
	/* frame end interrupt */
	csi->sw_checker = EXPECT_FRAME_START;

#ifndef ENABLE_IS_CORE
	{
		u32 vsync_cnt = atomic_read(&csi->fcount);
		atomic_set(&csi->vblank_count, vsync_cnt);
		v4l2_subdev_notify(*csi->subdev, CSI_NOTIFY_VBLANK, &vsync_cnt);
	}
#endif

	tasklet_schedule(&csi->tasklet_csis_end);
}

static inline void csi_s_config_dma(struct fimc_is_device_csi *csi, struct fimc_is_vci_config *vci_config)
{
	int i = 0;
	int entry = 0;
	struct fimc_is_image tmp_image;
	struct fimc_is_image *image;
	struct fimc_is_subdev *dma_subdev = NULL;

	memset(&tmp_image, 0x0, sizeof(struct fimc_is_image));

	for (i = CSI_VIRTUAL_CH_0; i < CSI_VIRTUAL_CH_MAX; i++) {
		dma_subdev = NULL;
		entry = CSI_CH_TO_ENTRY(i);

		if (i == CSI_VIRTUAL_CH_0)
			image = &csi->image;
		else
			dma_subdev = csi->dma_subdev[entry];

		/* dma setting for several virtual ch 1 ~ 3 specially */
		if (i > CSI_VIRTUAL_CH_0) {
			if (!dma_subdev || !test_bit(FIMC_IS_SUBDEV_OPEN, &dma_subdev->state)) {
				image = &csi->image;
			} else {
				/* cpy format from vc video context */
				memcpy(&tmp_image.format, &(GET_SUBDEV_QUEUE(dma_subdev))->framecfg.format, sizeof(struct fimc_is_fmt));
				image = &tmp_image;
			}
		}

		csi_hw_s_config_dma(csi->base_reg, i, image);
	}
}

static inline void csi_s_buf_addr(struct fimc_is_device_csi *csi, struct fimc_is_frame *frame, u32 index, u32 vc)
{
	BUG_ON(!frame);

	csi_hw_s_dma_addr(csi->base_reg, vc, index, frame->dvaddr_buffer[0]);
}

static inline void csi_s_output_dma(struct fimc_is_device_csi *csi, u32 vc, bool enable)
{
	csi_hw_s_output_dma(csi->base_reg, vc, enable);
}

#ifdef SUPPORTED_EARLYBUF_DONE_SW
static void csis_early_buf_done_start(struct v4l2_subdev *subdev)
{
	struct fimc_is_device_sensor *device;
	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		err("device is NULL");
		BUG();
	}

	if (device->early_buf_done_mode) {
		int framerate = fimc_is_sensor_g_framerate(device);
		/* timeout set : frameduration(ms) * early_buf_done_ratio(5~50%) */
		int msec_timeout = ((1000 / framerate) *
				(100 - (5 * device->early_buf_done_mode))) / 100;
		if (msec_timeout > 0)
			hrtimer_start(&device->early_buf_timer,
					ktime_set(0, msec_timeout * NSEC_PER_MSEC), HRTIMER_MODE_REL);
	}
}
#endif

static void csis_s_all_vc_dma_buf(struct fimc_is_device_csi *csi)
{
	u32 i, vc;
	int cur_dma_enable;
	struct fimc_is_subdev *dma_subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	/* dma setting for several virtual ch 0 ~ 3 */
	for (i = ENTRY_SSVC0; i <= ENTRY_SSVC3; i++) {
		vc = CSI_ENTRY_TO_CH(i);

		/* ch0 setting */
		if (vc == CSI_VIRTUAL_CH_0) {
			framemgr = csi->framemgr;
		} else {
			dma_subdev = csi->dma_subdev[i];
			if (!dma_subdev || !test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
				continue;

			framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
		}

		BUG_ON(!framemgr);

		/* If error happened, return all processing frame to HAL with error state. */
		if (test_bit((CSIS_BUF_ERR_VC0 + vc), &csi->state)) {
			csis_flush_vc_buf_done(csi, vc, FS_PROCESS, VB2_BUF_STATE_ERROR);
			err("[F%d][VC%d] frame was done with error", atomic_read(&csi->fcount), vc);
			clear_bit((CSIS_BUF_ERR_VC0 + vc), &csi->state);
		}

		cur_dma_enable = csi_hw_g_output_cur_dma_enable(csi->base_reg, vc);

		framemgr_e_barrier(framemgr, 0);

		frame = peek_frame(framemgr, FS_REQUEST);

		/*
		 * Sync DMA Setting
		 * (Enable) condition based on N frame
		 *  1) if there's no "process frame" for N + 1
		 *  2) if there's a "request frame"
		 */
		if (framemgr->queued_count[FS_PROCESS] < 2) {
			if (frame) {
				/* one buffering */
				csi_s_buf_addr(csi, frame, 0, vc);
				csi_s_output_dma(csi, vc, true);
				trans_frame(framemgr, frame, FS_PROCESS);
			} else {
				/*
				 * (Disable) condition based on N frame
				 *  1) if there's no "process frame" for N + 1
				 *  2) if there's no "request frame"
				 *  3) if dma was enabled in current frame(N)
				 */
				if (cur_dma_enable) {
					csi_s_output_dma(csi, vc, false);
				} else {
					if (framemgr->queued_count[FS_PROCESS]
						&& !csi_hw_g_output_dma_enable(csi->base_reg, vc)) {
						csi_s_output_dma(csi, vc, true);
						frame = peek_frame(framemgr, FS_PROCESS);
						warn("[VC%d][CF%d][FF%d][DF%d] Invalide DMA off [%d/%d/%d]\n",
							vc, atomic_read(&csi->fcount),
							frame->fcount, csi->dbg_fcount,
							framemgr->queued_count[FS_REQUEST],
							framemgr->queued_count[FS_PROCESS],
							framemgr->queued_count[FS_COMPLETE]);
					}
				}
			}
		} else {
			warn("[VC%d][F%d] process count is too many..(%d/%d/%d)",
					vc, atomic_read(&csi->fcount),
					framemgr->queued_count[FS_REQUEST],
					framemgr->queued_count[FS_PROCESS],
					framemgr->queued_count[FS_COMPLETE]);
		}

		/* print infomation DMA on/off */
		if (test_bit(CSIS_START_STREAM, &csi->state) &&
			csi->pre_dma_enable[vc] != cur_dma_enable) {
			info("[VC%d][F%d] DMA %s [%d/%d/%d]\n", vc, atomic_read(&csi->fcount),
					(cur_dma_enable ? "on" : "off"),
					framemgr->queued_count[FS_REQUEST],
					framemgr->queued_count[FS_PROCESS],
					framemgr->queued_count[FS_COMPLETE]);
			csi->pre_dma_enable[vc] = cur_dma_enable;
		}

		framemgr_x_barrier(framemgr, 0);
	}
}

static void csis_flush_vc_buf_done(struct fimc_is_device_csi *csi, u32 vc,
		enum fimc_is_frame_state target,
		enum vb2_buffer_state state)
{
	struct fimc_is_device_sensor *device;
	struct fimc_is_subdev *dma_subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;
	struct fimc_is_video_ctx *vctx;

	device = container_of(csi->subdev, struct fimc_is_device_sensor, subdev_csi);

	BUG_ON(!device);

	/* buffer done for several virtual ch 0 ~ 3 */
	if (vc == CSI_MODE_CH0_ONLY) {
		framemgr = csi->framemgr;
		vctx = device->vctx;
	} else {
		dma_subdev = csi->dma_subdev[CSI_CH_TO_ENTRY(vc)];
		if (!dma_subdev || !test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
			return;

		framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);
		vctx = dma_subdev->vctx;
	}

	BUG_ON(!framemgr);
	framemgr_e_barrier(framemgr, 0);

	frame = peek_frame(framemgr, target);
	while (frame) {
		CALL_VOPS(vctx, done, frame->index, state);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, target);
	}

	framemgr_x_barrier(framemgr, 0);
}


static void csis_flush_all_vc_buf_done(struct fimc_is_device_csi *csi, u32 state)
{
	u32 i;

	/* buffer done for several virtual ch 0 ~ 3 */
	for (i = CSI_VIRTUAL_CH_0; i < CSI_VIRTUAL_CH_MAX; i++) {
		csis_flush_vc_buf_done(csi, i, FS_REQUEST, state);
		csis_flush_vc_buf_done(csi, i, FS_PROCESS, state);
	}
}

/*
 * Tasklet for handling of start/end interrupt bottom-half
 *
 * tasklet_csis_str_otf : In case of OTF mode (csi->)
 * tasklet_csis_str_m2m : In case of M2M mode (csi->)
 * tasklet_csis_end     : In case of OTF or M2M mode (csi->)
 */
static u32 g_print_cnt;
void tasklet_csis_str_otf(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct fimc_is_device_csi *csi;
	struct fimc_is_device_sensor *device;
	struct fimc_is_device_ischain *ischain;
	struct fimc_is_groupmgr *groupmgr;
	struct fimc_is_group *group_3aa, *group_isp;
	u32 fcount, group_3aa_id, group_isp_id;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		BUG();
	}

	device = v4l2_get_subdev_hostdata(subdev);
	if (!device) {
		err("device is NULL");
		BUG();
	}

	fcount = atomic_read(&csi->fcount);
	ischain = device->ischain;

#ifdef TASKLET_MSG
	pr_info("S%d\n", fcount);
#endif

	groupmgr = ischain->groupmgr;
	group_3aa = &ischain->group_3aa;
	group_isp = &ischain->group_isp;

	group_3aa_id = group_3aa->id;
	group_isp_id = group_isp->id;

	if (group_3aa_id >= GROUP_ID_MAX) {
		merr("group 3aa id is invalid(%d)", csi, group_3aa_id);
		goto trigger_skip;
	}

	if (group_isp_id >= GROUP_ID_MAX) {
		merr("group isp id is invalid(%d)", csi, group_isp_id);
		goto trigger_skip;
	}

	if (unlikely(list_empty(&group_3aa->smp_trigger.wait_list))) {
		atomic_set(&group_3aa->sensor_fcount, fcount + group_3aa->skip_shots);

		/*
		 * pcount : program count
		 * current program count(location) in kthread
		 */
		if (((g_print_cnt % LOG_INTERVAL_OF_DROPS) == 0) ||
			(g_print_cnt < LOG_INTERVAL_OF_DROPS)) {
			info("[CSI] GP%d(res %d, rcnt %d, scnt %d), "
				"grp2(res %d, rcnt %d, scnt %d), "
				"fcount %d pcount %d\n",
				group_3aa_id,
				groupmgr->gtask[group_3aa_id].smp_resource.count,
				atomic_read(&group_3aa->rcount),
				atomic_read(&group_3aa->scount),
				groupmgr->gtask[group_isp_id].smp_resource.count,
				atomic_read(&group_isp->rcount),
				atomic_read(&group_isp->scount),
				fcount + group_3aa->skip_shots,
				group_3aa->pcount);
		}
		g_print_cnt++;
	} else {
		g_print_cnt = 0;
		atomic_set(&group_3aa->sensor_fcount, fcount + group_3aa->skip_shots);
		up(&group_3aa->smp_trigger);
	}

trigger_skip:
	/* set all virtual channel's dma */
	csis_s_all_vc_dma_buf(csi);

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	{
		struct fimc_is_framemgr *framemgr;
		struct fimc_is_frame *frame;

		framemgr = GET_SUBDEV_FRAMEMGR(&group_3aa->leader);

		frame = peek_frame(framemgr, FS_PROCESS);

		if (frame && frame->fcount == fcount)
			monitor_point(group_3aa, frame, TMM_SHOT2);
	}
#endif
#endif
#if defined(SUPPORTED_EARLYBUF_DONE_SW)
	csis_early_buf_done_start(subdev);
#endif
	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FSTART, &fcount);
}

void tasklet_csis_str_m2m(unsigned long data)
{
	struct v4l2_subdev *subdev;
	struct fimc_is_device_csi *csi;
	u32 fcount;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	fcount = atomic_read(&csi->fcount);

#ifdef TASKLET_MSG
	pr_info("S%d\n", fcount);
#endif
	/* set other virtual channel's dma */
	csis_s_all_vc_dma_buf(csi);

#if defined(SUPPORTED_EARLYBUF_DONE_SW)
	csis_early_buf_done_start(subdev);
#endif
	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FSTART, &fcount);
}

static void csi_dma_tag(struct v4l2_subdev *subdev,
	struct fimc_is_device_csi *csi,
	struct fimc_is_framemgr *framemgr, u32 vc)
{
	struct fimc_is_frame *frame = NULL;
	struct fimc_is_frame *frame_done = NULL;

	framemgr_e_barrier(framemgr, 0);

	if (csi_hw_g_output_dma_enable(csi->base_reg, vc)) {
		if (framemgr->queued_count[FS_PROCESS] == 2) {
			frame = peek_frame(framemgr, FS_PROCESS);
			if (frame) {
				frame_done = frame;
				trans_frame(framemgr, frame, FS_COMPLETE);
				csi->dbg_fcount = frame->fcount;
			} else {
				merr("[CSI][VC%d] sensor process is empty", csi, vc);
				frame_manager_print_queues(framemgr);
				BUG();
			}
		}
	} else {
		frame = peek_frame(framemgr, FS_PROCESS);
		if (frame) {
			frame_done = frame;
			trans_frame(framemgr, frame, FS_COMPLETE);
		}
	}

	framemgr_x_barrier(framemgr, 0);

	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FEND, frame_done);
}

static void tasklet_csis_dma_vc0(unsigned long data)
{
	struct fimc_is_device_csi *csi;
	struct fimc_is_framemgr *framemgr;
	struct v4l2_subdev *subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	framemgr = csi->framemgr;

	csi_dma_tag(subdev, csi, framemgr, CSI_VIRTUAL_CH_0);
}

static void tasklet_csis_dma_vc1(unsigned long data)
{
	struct fimc_is_device_csi *csi;
	struct fimc_is_framemgr *framemgr;
	struct v4l2_subdev *subdev;
	struct fimc_is_subdev *dma_subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	dma_subdev = csi->dma_subdev[ENTRY_SSVC1];
	if (!dma_subdev ||
		!test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
		return;

	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);

	csi_dma_tag(subdev, csi, framemgr, CSI_VIRTUAL_CH_1);
}

static void tasklet_csis_dma_vc2(unsigned long data)
{
	struct fimc_is_device_csi *csi;
	struct fimc_is_framemgr *framemgr;
	struct v4l2_subdev *subdev;
	struct fimc_is_subdev *dma_subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	dma_subdev = csi->dma_subdev[ENTRY_SSVC2];
	if (!dma_subdev ||
		!test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
		return;

	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);

	csi_dma_tag(subdev, csi, framemgr, CSI_VIRTUAL_CH_2);
}

static void tasklet_csis_dma_vc3(unsigned long data)
{
	struct fimc_is_device_csi *csi;
	struct fimc_is_framemgr *framemgr;
	struct v4l2_subdev *subdev;
	struct fimc_is_subdev *dma_subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

	dma_subdev = csi->dma_subdev[ENTRY_SSVC3];
	if (!dma_subdev ||
		!test_bit(FIMC_IS_SUBDEV_START, &dma_subdev->state))
		return;

	framemgr = GET_SUBDEV_FRAMEMGR(dma_subdev);

	csi_dma_tag(subdev, csi, framemgr, CSI_VIRTUAL_CH_3);
}

static void tasklet_csis_end(unsigned long data)
{
	struct fimc_is_device_csi *csi;
	struct v4l2_subdev *subdev;

	subdev = (struct v4l2_subdev *)data;
	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("[CSI] csi is NULL");
		BUG();
	}

#ifdef TASKLET_MSG
	pr_info("E%d\n", atomic_read(&csi->fcount));
#endif
	v4l2_subdev_notify(subdev, CSIS_NOTIFY_FEND, NULL);
}

static u32 get_vci_channel(struct fimc_is_vci *vci,
	const u32 vcis, u32 pixelformat)
{
	u32 i;
	u32 index = vcis;

	BUG_ON(!vci);

	for (i = 0; i < vcis; i++) {
		if (vci[i].pixelformat == pixelformat) {
			index = i;
			break;
		}
	}

	if (index == vcis) {
		err("invalid vc setting(foramt : %d)", pixelformat);
		BUG();
	}

	return index;
}

static void csi_err_handler(struct fimc_is_device_csi *csi, u32 *err_id)
{
	const char* err_str = NULL;
	int i, j;
	bool dma_abort_flag = false;
	bool buf_flush_flag = false;

	for (i = 0; i < CSI_VIRTUAL_CH_MAX; i++) {
		/* skip error handling if there's no error in this virtual ch. */
		if (!err_id[i])
			continue;

		/* If any error happened, set the error bit to return buffer with err. */
		set_bit((CSIS_BUF_ERR_VC0 + i), &csi->state);

		/* flag to flush processing frame right now */
		buf_flush_flag = false;

		for (j = 0; j < CSIS_ERR_END; j++) {

			if (!((1 << j) & err_id[i]))
				continue;

			/* If error happened, flush all dma fifo to prevent other side effect like sysmmu fault etc. */
			if (!dma_abort_flag) {
				csi_hw_s_control(csi->base_reg, CSIS_CTRL_DMA_ABORT_REQ, true);
				dma_abort_flag = true;
				merr("dma abort req!!", csi);
			}

			switch (j) {
			case CSIS_ERR_ID:
				err_str = GET_STR(CSIS_ERR_ID);
				break;
			case CSIS_ERR_CRC:
				err_str = GET_STR(CSIS_ERR_CRC);
				break;
			case CSIS_ERR_ECC:
				err_str = GET_STR(CSIS_ERR_ECC);
				break;
			case CSIS_ERR_WRONG_CFG:
				err_str = GET_STR(CSIS_ERR_WRONG_CFG);
				break;
			case CSIS_ERR_OVERFLOW_VC:
				err_str = GET_STR(CSIS_ERR_OVERFLOW_VC);
				break;
			case CSIS_ERR_LOST_FE_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FE_VC);
				buf_flush_flag = true;
				break;
			case CSIS_ERR_LOST_FS_VC:
				err_str = GET_STR(CSIS_ERR_LOST_FS_VC);
				break;
			case CSIS_ERR_SOT_VC:
				err_str = GET_STR(CSIS_ERR_SOT_VC);
				break;
			case CSIS_ERR_OTF_OVERLAP:
				err_str = GET_STR(CSIS_ERR_OTF_OVERLAP);
				break;
			case CSIS_ERR_DMA_ERR_DMAFIFO_FULL:
				err_str = GET_STR(CSIS_ERR_DMA_ERR_DMAFIFO_FULL);
#ifdef OVERFLOW_PANIC_ENABLE
				panic("CSIS error!! %s", err_str);
#endif
				break;
			case CSIS_ERR_DMA_ERR_TRXFIFO_FULL:
				err_str = GET_STR(CSIS_ERR_DMA_ERR_TRXFIFO_FULL);
#ifdef OVERFLOW_PANIC_ENABLE
				panic("CSIS error!! %s", err_str);
#endif
				break;
			case CSIS_ERR_DMA_ERR_BRESP_ERR:
				err_str = GET_STR(CSIS_ERR_DMA_ERR_BRESP_ERR);
#ifdef OVERFLOW_PANIC_ENABLE
				panic("CSIS error!! %s", err_str);
#endif
				break;
			}

			merr("[VC%d][F%d] Occured the %s(%d)", csi, i, atomic_read(&csi->fcount), err_str, j);
		}

		/*
		 * If lost fe was happened, return all processing frame to HAL with error state.
		 * In case of other error case, returning with error should be processed in DMA end tasklet.
		 */
		if (unlikely(buf_flush_flag)) {
			csis_flush_vc_buf_done(csi, i, FS_PROCESS, VB2_BUF_STATE_ERROR);
			err("[F%d][VC%d] frame was done with error due to lost fe", atomic_read(&csi->fcount), i);
			clear_bit((CSIS_BUF_ERR_VC0 + i), &csi->state);
		}
	}
}

static irqreturn_t csi_isr(int irq, void *data)
{
	struct fimc_is_device_csi *csi;
	int frame_start, frame_end;
	int dma_frame_end;
	struct csis_irq_src irq_src;
	int ret;

	csi = data;
	memset(&irq_src, 0x0, sizeof(struct csis_irq_src));
	ret = csi_hw_g_irq_src(csi->base_reg, &irq_src, true);

	/* Get Frame Start Status */
	frame_start = irq_src.otf_start & (1 << CSI_VIRTUAL_CH_0);

	/* Get Frame End Status */
	frame_end = (irq_src.otf_end | irq_src.line_end) & (1 << CSI_VIRTUAL_CH_0);

	/* Get DMA END Status */
#if defined(SUPPORTED_EARLYBUF_DONE_HW)
	dma_frame_end = irq_src.line_end;
#else
	dma_frame_end = irq_src.dma_end;
#endif
	/* DMA End */
	if (dma_frame_end) {
		/* VC0 */
		if (dma_frame_end & (1 << CSI_VIRTUAL_CH_0))
			tasklet_schedule(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_0]);
		/* VC1 */
		if (csi->dma_subdev[ENTRY_SSVC1] && (dma_frame_end & (1 << CSI_VIRTUAL_CH_1)))
			tasklet_schedule(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_1]);
		/* VC2 */
		if (csi->dma_subdev[ENTRY_SSVC2] && (dma_frame_end & (1 << CSI_VIRTUAL_CH_2)))
			tasklet_schedule(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_2]);
		/* VC3 */
		if (csi->dma_subdev[ENTRY_SSVC3] && (dma_frame_end & (1 << CSI_VIRTUAL_CH_3)))
			tasklet_schedule(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_3]);
	}

	/* Frame Start and End */
	if (frame_start && frame_end) {
#ifdef DBG_CSIISR
		printk(KERN_CONT "*");
#endif
		/* frame both interrupt since latency */
		if (csi->sw_checker == EXPECT_FRAME_START) {
			csi_frame_start_inline(csi);
			csi_frame_end_inline(csi);
		} else {
			csi_frame_end_inline(csi);
			csi_frame_start_inline(csi);
		}
	/* Frame Start */
	} else if (frame_start) {
		/* W/A: Skip start tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_START) {
			warn("[CSIS%d] Lost end interupt\n",
					csi->instance);
			goto clear_status;
		}
		csi_frame_start_inline(csi);
	/* Frame End */
	} else if (frame_end) {
		/* W/A: Skip end tasklet at interrupt lost case */
		if (csi->sw_checker != EXPECT_FRAME_END) {
			warn("[CSIS%d] Lost start interupt\n",
					csi->instance);
			goto clear_status;
		}
#ifdef DBG_CSIISR
		printk(KERN_CONT ">");
#endif
		csi_frame_end_inline(csi);
	}

	/* Error Occured */
	if (irq_src.err_flag)
		csi_err_handler(csi, (u32 *)irq_src.err_id);

clear_status:
	return IRQ_HANDLED;
}

int fimc_is_csi_open(struct v4l2_subdev *subdev,
	struct fimc_is_framemgr *framemgr)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct fimc_is_device_sensor *device;

	BUG_ON(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	csi->sensor_cfgs = 0;
	csi->sensor_cfg = NULL;
	memset(&csi->image, 0, sizeof(struct fimc_is_image));

	device = container_of(csi->subdev, struct fimc_is_device_sensor, subdev_csi);

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_err;

	minfo("[CSI] registered isr handler(%d) state(%d)\n", csi,
				csi->instance, test_bit(CSIS_DMA_ENABLE, &csi->state));
	csi->framemgr = framemgr;
	atomic_set(&csi->fcount, 0);

#ifndef ENABLE_IS_CORE
	atomic_set(&csi->vblank_count, 0);
	atomic_set(&csi->vvalid, 0);
#endif
p_err:
	return ret;
}

int fimc_is_csi_close(struct v4l2_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct fimc_is_device_sensor *device;

	BUG_ON(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = container_of(csi->subdev, struct fimc_is_device_sensor, subdev_csi);

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_err;

p_err:
	return ret;
}

/* value : module enum */
static int csi_init(struct v4l2_subdev *subdev, u32 value)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct fimc_is_module_enum *module;
	struct fimc_is_device_sensor *device;

	BUG_ON(!subdev);

	csi = v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	device = container_of(csi->subdev, struct fimc_is_device_sensor, subdev_csi);
	module = &device->module_enum[value];

	csi->sensor_cfgs = module->cfgs;
	csi->sensor_cfg = module->cfg;
	csi->vcis = module->vcis;
	csi->vci = module->vci;
	csi->image.framerate = SENSOR_DEFAULT_FRAMERATE; /* default frame rate */
	csi->mode = module->mode;
	/* default value */
	csi->lanes = module->lanes;
	csi->mipi_speed = 0;
	csi->image.format.bitwidth = module->bitwidth;

p_err:
	return ret;
}

static int csi_s_power(struct v4l2_subdev *subdev,
	int on)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	if (on)
		ret = phy_power_on(csi->phy);
	else
		ret = phy_power_off(csi->phy);

	if (ret) {
		err("fail to csi%d power on/off(%d)", csi->instance, on);
		goto p_err;
	}

p_err:
	mdbgd_front("%s(%d, %d)\n", csi, __func__, on, ret);
	return ret;
}

static const struct v4l2_subdev_core_ops core_ops = {
	.init = csi_init,
	.s_power = csi_s_power
};

static int csi_stream_on(struct v4l2_subdev *subdev,
	struct fimc_is_device_csi *csi)
{
	int ret = 0;
	u32 settle, index;
	u32 __iomem *base_reg;
	struct fimc_is_device_sensor *device = v4l2_get_subdev_hostdata(subdev);

	BUG_ON(!csi);
	BUG_ON(!csi->sensor_cfg);
	BUG_ON(!device);

	if (test_bit(CSIS_START_STREAM, &csi->state)) {
		merr("[CSI] already start", csi);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(CSIS_DMA_ENABLE, &csi->state)) {
		ret = request_irq(csi->irq,
				csi_isr,
				FIMC_IS_HW_IRQ_FLAG,
				"mipi-csi",
				csi);
		if (ret) {
			merr("request_irq(IRQ_MIPICSI %d) is fail(%d)", csi, csi->irq, ret);
			goto p_err;
		}
	}

	base_reg = csi->base_reg;

	if (!device->cfg) {
		merr("[CSI] cfg is NULL", csi);
		ret = -EINVAL;
		goto p_err;
	}
	csi->lanes = device->cfg->lanes;
	csi->mipi_speed = device->cfg->mipi_speed;
	settle = device->cfg->settle;

	minfo("[CSI] settle(%dx%d@%d) = %d\n", csi,
		csi->image.window.width,
		csi->image.window.height,
		csi->image.framerate,
		settle);

	index = get_vci_channel(csi->vci, csi->vcis, csi->image.format.pixelformat);
	minfo("[CSI] vci(0x%X) = %d\n", csi, csi->image.format.pixelformat, index);

	if (device->ischain)
		set_bit(CSIS_JOIN_ISCHAIN, &csi->state);
	else
		clear_bit(CSIS_JOIN_ISCHAIN, &csi->state);

	csi_hw_reset(base_reg);
	csi_hw_phy_otp_config(base_reg, csi->instance);
	csi_hw_s_settle(base_reg, settle);

	csi_hw_s_lane(base_reg, &csi->image, csi->lanes, csi->mipi_speed);
	csi_hw_s_control(base_reg, CSIS_CTRL_INTERLEAVE_MODE, csi->mode);

	if (csi->mode == CSI_MODE_CH0_ONLY) {
		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_0,
			&csi->vci[index].config[CSI_VIRTUAL_CH_0],
			csi->image.window.width,
			csi->image.window.height);
	} else {
		u32 i = 0;
		u32 entry = 0;
		u32 vc_width[CSI_VIRTUAL_CH_MAX];
		u32 vc_height[CSI_VIRTUAL_CH_MAX];
		struct fimc_is_subdev *dma_subdev;

		vc_width[CSI_VIRTUAL_CH_0] = csi->image.window.width;
		vc_height[CSI_VIRTUAL_CH_0] = csi->image.window.height;

		vc_width[CSI_VIRTUAL_CH_3] = vc_width[CSI_VIRTUAL_CH_2] = vc_width[CSI_VIRTUAL_CH_1] = vc_width[CSI_VIRTUAL_CH_0];
		vc_height[CSI_VIRTUAL_CH_3] = vc_height[CSI_VIRTUAL_CH_2] = vc_height[CSI_VIRTUAL_CH_1] = vc_height[CSI_VIRTUAL_CH_0];

		for (i = CSI_VIRTUAL_CH_1; i < CSI_VIRTUAL_CH_MAX; i++) {
			entry = CSI_CH_TO_ENTRY(i);
			dma_subdev = csi->dma_subdev[entry];
			if (dma_subdev &&
					test_bit(FIMC_IS_SUBDEV_OPEN, &dma_subdev->state)) {
				if (dma_subdev->output.width != 0 && dma_subdev->output.height != 0) {
					vc_width[i] = dma_subdev->output.width;
					vc_height[i] = dma_subdev->output.height;
				} else {
					mwarn("[CSI][VC%d] format size(%d/%d) is wrong\n",
						csi, i, dma_subdev->output.width, dma_subdev->output.height);
				}
			}

			minfo("[CSI] VC%d: size(%dx%d)\n", csi, i, vc_width[i], vc_height[i]);
		}

		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_0,
			&csi->vci[index].config[CSI_VIRTUAL_CH_0],
			vc_width[ENTRY_SSVC0], vc_height[ENTRY_SSVC0]);
		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_1,
			&csi->vci[index].config[CSI_VIRTUAL_CH_1],
			vc_width[ENTRY_SSVC1], vc_height[ENTRY_SSVC1]);
		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_2,
			&csi->vci[index].config[CSI_VIRTUAL_CH_2],
			vc_width[ENTRY_SSVC2], vc_height[ENTRY_SSVC2]);
		csi_hw_s_config(base_reg,
			CSI_VIRTUAL_CH_3,
			&csi->vci[index].config[CSI_VIRTUAL_CH_3],
			vc_width[ENTRY_SSVC3], vc_height[ENTRY_SSVC3]);
	}

	csi_hw_s_irq_msk(base_reg, true);

	if (test_bit(CSIS_DMA_ENABLE, &csi->state)) {
		/* runtime buffer done state for error */
		clear_bit(CSIS_BUF_ERR_VC0, &csi->state);
		clear_bit(CSIS_BUF_ERR_VC1, &csi->state);
		clear_bit(CSIS_BUF_ERR_VC2, &csi->state);
		clear_bit(CSIS_BUF_ERR_VC3, &csi->state);

		csi->sw_checker = EXPECT_FRAME_START;
		csi->overflow_cnt = 0;
		csi_s_config_dma(csi, csi->vci[index].config);
		memset(csi->pre_dma_enable, -1, ARRAY_SIZE(csi->pre_dma_enable));

		/* DMA enabled for all virtual ch before stream-on */
		csis_s_all_vc_dma_buf(csi);

		/* Tasklet Setting */
		if (device->ischain &&
			test_bit(FIMC_IS_GROUP_OTF_INPUT, &device->ischain->group_3aa.state)) {
			/* OTF */
			tasklet_init(&csi->tasklet_csis_str, tasklet_csis_str_otf, (unsigned long)subdev);
			tasklet_init(&csi->tasklet_csis_end, tasklet_csis_end, (unsigned long)subdev);
		} else {
			/* M2M */
			tasklet_init(&csi->tasklet_csis_str, tasklet_csis_str_m2m, (unsigned long)subdev);
			tasklet_init(&csi->tasklet_csis_end, tasklet_csis_end, (unsigned long)subdev);
#if defined(SUPPORTED_EARLYBUF_DONE_SW) || defined(SUPPORTED_EARLYBUF_DONE_HW)
			if (device->early_buf_done_mode) {
#if defined(SUPPORTED_EARLYBUF_DONE_SW)
				device->early_buf_timer.function = csis_early_buf_done;
#elif defined(SUPPORTED_EARLYBUF_DONE_HW)
				csi_hw_s_control(base_reg, CSIS_CTRL_LINE_RATIO, device->early_buf_done_mode);
#endif
			}
#endif
		}

		/* DMA Tasklet Setting */
		tasklet_init(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_0], tasklet_csis_dma_vc0, (unsigned long)subdev);
		if (csi->dma_subdev[ENTRY_SSVC1])
			tasklet_init(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_1], tasklet_csis_dma_vc1, (unsigned long)subdev);
		if (csi->dma_subdev[ENTRY_SSVC2])
			tasklet_init(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_2], tasklet_csis_dma_vc2, (unsigned long)subdev);
		if (csi->dma_subdev[ENTRY_SSVC3])
			tasklet_init(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_3], tasklet_csis_dma_vc3, (unsigned long)subdev);
	}

	csi_hw_enable(base_reg);

#ifdef DBG_DUMPREG
	csi_hw_dump(base_reg);
#endif
	set_bit(CSIS_START_STREAM, &csi->state);
p_err:
	return ret;
}

static int csi_stream_off(struct v4l2_subdev *subdev,
	struct fimc_is_device_csi *csi)
{
	int ret = 0;
	u32 __iomem *base_reg;

	BUG_ON(!csi);

	if (!test_bit(CSIS_START_STREAM, &csi->state)) {
		merr("[CSI] already stop", csi);
		ret = -EINVAL;
		goto p_err;
	}

	base_reg = csi->base_reg;

	csi_hw_s_irq_msk(base_reg, false);

	csi_hw_disable(base_reg);

	tasklet_kill(&csi->tasklet_csis_str);
	tasklet_kill(&csi->tasklet_csis_end);

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_dma_skip;

	tasklet_kill(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_0]);
	if (csi->dma_subdev[ENTRY_SSVC1])
		tasklet_kill(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_1]);
	if (csi->dma_subdev[ENTRY_SSVC2])
		tasklet_kill(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_2]);
	if (csi->dma_subdev[ENTRY_SSVC3])
		tasklet_kill(&csi->tasklet_csis_dma[CSI_VIRTUAL_CH_3]);

	csis_flush_all_vc_buf_done(csi, VB2_BUF_STATE_ERROR);

#ifndef ENABLE_IS_CORE
	atomic_set(&csi->vvalid, 0);
#endif

	free_irq(csi->irq, csi);

p_dma_skip:
	clear_bit(CSIS_START_STREAM, &csi->state);
p_err:
	return ret;
}

static int csi_s_stream(struct v4l2_subdev *subdev, int enable)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	/* H/W setting skip */
	if (test_bit(CSIS_DUMMY, &csi->state)) {
		mdbgd_front("%s(dummy)\n", csi, __func__);
		goto p_err;
	}

	if (enable) {
		ret = csi_stream_on(subdev, csi);
		if (ret) {
			merr("[CSI] csi_stream_on is fail(%d)", csi, ret);
			goto p_err;
		}
	} else {
		ret = csi_stream_off(subdev, csi);
		if (ret) {
			merr("[CSI] csi_stream_off is fail(%d)", csi, ret);
			goto p_err;
		}
	}

p_err:
	return 0;
}

static int csi_s_param(struct v4l2_subdev *subdev, struct v4l2_streamparm *param)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;
	struct v4l2_captureparm *cp;
	struct v4l2_fract *tpf;

	BUG_ON(!subdev);
	BUG_ON(!param);

	cp = &param->parm.capture;
	tpf = &cp->timeperframe;

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	csi->image.framerate = tpf->denominator / tpf->numerator;

	mdbgd_front("%s(%d, %d)\n", csi, __func__, csi->image.framerate, ret);
	return ret;
}

static int csi_s_format(struct v4l2_subdev *subdev, struct v4l2_mbus_framefmt *fmt)
{
	int ret = 0;
	struct fimc_is_device_csi *csi;

	BUG_ON(!subdev);
	BUG_ON(!fmt);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (!csi) {
		err("csi is NULL");
		return -EINVAL;
	}

	csi->image.window.offs_h = 0;
	csi->image.window.offs_v = 0;
	csi->image.window.width = fmt->width;
	csi->image.window.height = fmt->height;
	csi->image.window.o_width = fmt->width;
	csi->image.window.o_height = fmt->height;
	csi->image.format.pixelformat = fmt->code;
	csi->image.format.field = fmt->field;

	mdbgd_front("%s(%dx%d, %X)\n", csi, __func__, fmt->width, fmt->height, fmt->code);
	return ret;
}

static int csi_s_buffer(struct v4l2_subdev *subdev, void *buf, unsigned int *size)
{
	int ret = 0;
	u32 vc = 0;
	unsigned long flags;
	struct fimc_is_device_csi *csi;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);

	csi = (struct fimc_is_device_csi *)v4l2_get_subdevdata(subdev);
	if (unlikely(csi == NULL)) {
		err("csi is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(CSIS_DMA_ENABLE, &csi->state))
		goto p_err;

	if (!test_bit(CSIS_START_STREAM, &csi->state))
		goto p_err;

	/* sensor bayer buf */
	if (!buf) {
		framemgr = csi->framemgr;
		if (unlikely(framemgr == NULL)) {
			merr("framemgr is NULL", csi);
			ret = -EINVAL;
			goto p_err;
		}

		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_15, flags);
		frame = peek_frame(framemgr, FS_REQUEST);
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_15, flags);

		vc = CSI_VIRTUAL_CH_0;
	} else {
		/* for virtual channels */
		struct fimc_is_subdev *subdev = NULL;

		frame = (struct fimc_is_frame *)buf;
		subdev = frame->subdev;
		framemgr = GET_SUBDEV_FRAMEMGR(subdev);

		vc = CSI_ENTRY_TO_CH(subdev->id);
	}

	if (frame) {
		framemgr_e_barrier_irqs(framemgr, FMGR_IDX_15, flags);

		/*
		 * ASync DMA Setting
		 *  (Enable) condition based on N frame
		 *   1) if there's no "process frame" for N + 1
		 *   2) if there's a "request frame"
		 */
		if (!csi_hw_g_output_dma_enable(csi->base_reg, vc) &&
			framemgr->queued_count[FS_PROCESS] < 2) {
			csi_s_buf_addr(csi, frame, 0, vc);
			csi_s_output_dma(csi, vc, true);

			trans_frame(framemgr, frame, FS_PROCESS);
		}

		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_15, flags);
	}

p_err:
	return ret;
}

static const struct v4l2_subdev_video_ops video_ops = {
	.s_stream = csi_s_stream,
	.s_parm = csi_s_param,
	.s_mbus_fmt = csi_s_format,
	.s_rx_buffer = csi_s_buffer
};

static const struct v4l2_subdev_ops subdev_ops = {
	.core = &core_ops,
	.video = &video_ops
};

int fimc_is_csi_probe(void *parent, u32 instance)
{
	int i = 0;
	int ret = 0;
	struct v4l2_subdev *subdev_csi;
	struct fimc_is_device_csi *csi;
	struct fimc_is_device_sensor *device = parent;
	struct resource *mem_res;
	struct platform_device *pdev;

	BUG_ON(!device);

	subdev_csi = kzalloc(sizeof(struct v4l2_subdev), GFP_KERNEL);
	if (!subdev_csi) {
		merr("subdev_csi is NULL", device);
		ret = -ENOMEM;
		goto p_err;
	}
	device->subdev_csi = subdev_csi;

	csi = kzalloc(sizeof(struct fimc_is_device_csi), GFP_KERNEL);
	if (!csi) {
		merr("csi is NULL", device);
		ret = -ENOMEM;
		goto p_err_free1;
	}

	/* Get SFR base register */
	pdev = device->pdev;
	mem_res = platform_get_resource(pdev, IORESOURCE_MEM, 0);
	if (!mem_res) {
		probe_err("Failed to get io memory region(%p)", mem_res);
		ret = -EBUSY;
		goto err_get_resource;
	}

	csi->base_reg =  devm_ioremap_nocache(&pdev->dev, mem_res->start, resource_size(mem_res));
	if (!csi->base_reg) {
		probe_err("Failed to remap io region(%p)", csi->base_reg);
		ret = -ENOMEM;
		goto err_get_resource;
	}

	/* Get IRQ SPI number */
	csi->irq = platform_get_irq(pdev, 0);
	if (csi->irq < 0) {
		probe_err("Failed to get csi_irq(%d)", csi->irq);
		ret = -EBUSY;
		goto err_get_irq;
	}

	csi->phy = devm_phy_get(&pdev->dev, "csis_dphy");
	if (IS_ERR(csi->phy))
		return PTR_ERR(csi->phy);

	/* pointer to me from device sensor */
	csi->subdev = &device->subdev_csi;

	csi->instance = instance;

	/* default state setting */
	clear_bit(CSIS_DUMMY, &csi->state);
	set_bit(CSIS_DMA_ENABLE, &csi->state);

	/* init dma subdev slots */
	for (i = 0; i < ENTRY_SEN_END; i++)
		csi->dma_subdev[i] = NULL;

	v4l2_subdev_init(subdev_csi, &subdev_ops);
	v4l2_set_subdevdata(subdev_csi, csi);
	v4l2_set_subdev_hostdata(subdev_csi, device);
	snprintf(subdev_csi->name, V4L2_SUBDEV_NAME_SIZE, "csi-subdev.%d", instance);
	ret = v4l2_device_register_subdev(&device->v4l2_dev, subdev_csi);
	if (ret) {
		merr("v4l2_device_register_subdev is fail(%d)", device, ret);
		goto err_reg_v4l2_subdev;
	}

	info("[%d][FRT:D] %s(%d)\n", instance, __func__, ret);
	return 0;

err_reg_v4l2_subdev:
err_get_irq:
	iounmap(csi->base_reg);

err_get_resource:
	kfree(csi);

p_err_free1:
	kfree(subdev_csi);
	device->subdev_csi = NULL;

p_err:
	err("[%d][FRT:D] %s(%d)\n", instance, __func__, ret);
	return ret;
}
