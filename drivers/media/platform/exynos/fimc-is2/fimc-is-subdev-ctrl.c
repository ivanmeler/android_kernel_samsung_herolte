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

#include "fimc-is-core.h"
#include "fimc-is-param.h"
#include "fimc-is-device-ischain.h"
#include "fimc-is-debug.h"

struct fimc_is_subdev * video2subdev(enum fimc_is_subdev_device_type device_type,
	void *device, u32 vid)
{
	struct fimc_is_subdev *subdev = NULL;
	struct fimc_is_device_sensor *sensor = NULL;
	struct fimc_is_device_ischain *ischain = NULL;

	if (device_type == FIMC_IS_SENSOR_SUBDEV)
		sensor = (struct fimc_is_device_sensor *)device;
	else
		ischain = (struct fimc_is_device_ischain *)device;

	switch (vid) {
	case FIMC_IS_VIDEO_SS0VC0_NUM:
	case FIMC_IS_VIDEO_SS1VC0_NUM:
	case FIMC_IS_VIDEO_SS2VC0_NUM:
	case FIMC_IS_VIDEO_SS3VC0_NUM:
	case FIMC_IS_VIDEO_SS4VC0_NUM:
	case FIMC_IS_VIDEO_SS5VC0_NUM:
		subdev = &sensor->ssvc0;
		break;
	case FIMC_IS_VIDEO_SS0VC1_NUM:
	case FIMC_IS_VIDEO_SS1VC1_NUM:
	case FIMC_IS_VIDEO_SS2VC1_NUM:
	case FIMC_IS_VIDEO_SS3VC1_NUM:
	case FIMC_IS_VIDEO_SS4VC1_NUM:
	case FIMC_IS_VIDEO_SS5VC1_NUM:
		subdev = &sensor->ssvc1;
		break;
	case FIMC_IS_VIDEO_SS0VC2_NUM:
	case FIMC_IS_VIDEO_SS1VC2_NUM:
	case FIMC_IS_VIDEO_SS2VC2_NUM:
	case FIMC_IS_VIDEO_SS3VC2_NUM:
	case FIMC_IS_VIDEO_SS4VC2_NUM:
	case FIMC_IS_VIDEO_SS5VC2_NUM:
		subdev = &sensor->ssvc2;
		break;
	case FIMC_IS_VIDEO_SS0VC3_NUM:
	case FIMC_IS_VIDEO_SS1VC3_NUM:
	case FIMC_IS_VIDEO_SS2VC3_NUM:
	case FIMC_IS_VIDEO_SS3VC3_NUM:
	case FIMC_IS_VIDEO_SS4VC3_NUM:
	case FIMC_IS_VIDEO_SS5VC3_NUM:
		subdev = &sensor->ssvc3;
		break;
	case FIMC_IS_VIDEO_30S_NUM:
	case FIMC_IS_VIDEO_31S_NUM:
		subdev = &ischain->group_3aa.leader;
		break;
	case FIMC_IS_VIDEO_30C_NUM:
	case FIMC_IS_VIDEO_31C_NUM:
		subdev = &ischain->txc;
		break;
	case FIMC_IS_VIDEO_30P_NUM:
	case FIMC_IS_VIDEO_31P_NUM:
		subdev = &ischain->txp;
		break;
	case FIMC_IS_VIDEO_I0S_NUM:
	case FIMC_IS_VIDEO_I1S_NUM:
		subdev = &ischain->group_isp.leader;
		break;
	case FIMC_IS_VIDEO_I0C_NUM:
	case FIMC_IS_VIDEO_I1C_NUM:
		subdev = &ischain->ixc;
		break;
	case FIMC_IS_VIDEO_I0P_NUM:
	case FIMC_IS_VIDEO_I1P_NUM:
		subdev = &ischain->ixp;
		break;
	case FIMC_IS_VIDEO_DIS_NUM:
		subdev = &ischain->group_dis.leader;
		break;
	case FIMC_IS_VIDEO_SCC_NUM:
		subdev = &ischain->scc;
		break;
	case FIMC_IS_VIDEO_SCP_NUM:
		subdev = &ischain->scp;
		break;
	case FIMC_IS_VIDEO_M0S_NUM:
	case FIMC_IS_VIDEO_M1S_NUM:
		subdev = &ischain->group_mcs.leader;
		break;
	case FIMC_IS_VIDEO_M0P_NUM:
		subdev = &ischain->m0p;
		break;
	case FIMC_IS_VIDEO_M1P_NUM:
		subdev = &ischain->m1p;
		break;
	case FIMC_IS_VIDEO_M2P_NUM:
		subdev = &ischain->m2p;
		break;
	case FIMC_IS_VIDEO_M3P_NUM:
		subdev = &ischain->m3p;
		break;
	case FIMC_IS_VIDEO_M4P_NUM:
		subdev = &ischain->m4p;
		break;
	case FIMC_IS_VIDEO_VRA_NUM:
		subdev = &ischain->group_vra.leader;
		break;
	default:
		err("[%d] vid %d is NOT found", (ischain ? ischain->instance : sensor->instance), vid);
		break;
	}

	return subdev;
}

int fimc_is_subdev_probe(struct fimc_is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name,
	const struct fimc_is_subdev_ops *sops)
{
	BUG_ON(!subdev);
	BUG_ON(!name);

	subdev->id = id;
	subdev->instance = instance;
	subdev->ops = sops;
	memset(subdev->name, 0x0, sizeof(subdev->name));
	strncpy(subdev->name, name, sizeof(char[3]));
	clear_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

	return 0;
}

int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	void *ctl_data)
{
	int ret = 0;
	const struct param_control *init_ctl = (const struct param_control *)ctl_data;

	BUG_ON(!subdev);

	if (test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("already open", subdev, subdev);
		ret = -EPERM;
		goto p_err;
	}

	subdev->vctx = vctx;
	subdev->vid = (GET_VIDEO(vctx)) ? GET_VIDEO(vctx)->id : 0;
	subdev->cid = CAPTURE_NODE_MAX;
	subdev->input.width = 0;
	subdev->input.height = 0;
	subdev->input.crop.x = 0;
	subdev->input.crop.y = 0;
	subdev->input.crop.w = 0;
	subdev->input.crop.h = 0;
	subdev->output.width = 0;
	subdev->output.height = 0;
	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = 0;
	subdev->output.crop.h = 0;

	if (init_ctl) {
		set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

		if (subdev->id == ENTRY_VRA) {
			/* vra only control by command for enabling or disabling */
			if (init_ctl->cmd == CONTROL_COMMAND_STOP)
				clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
		} else {
			if (init_ctl->bypass == CONTROL_BYPASS_ENABLE)
				clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
			else
				set_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
		}
	} else {
		clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);
		clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	}

	set_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);

p_err:
	return ret;
}

int fimc_is_sensor_subdev_open(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	subdev = video2subdev(FIMC_IS_SENSOR_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = subdev;

	ret = fimc_is_subdev_open(subdev, vctx, NULL);
	if (ret) {
		merr("fimc_is_subdev_open is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_subdev_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));

	subdev = video2subdev(FIMC_IS_ISCHAIN_SUBDEV, (void *)device, GET_VIDEO(vctx)->id);
	if (!subdev) {
		merr("video2subdev is fail", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = subdev;

	ret = fimc_is_subdev_open(subdev, vctx, NULL);
	if (ret) {
		merr("fimc_is_subdev_open is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_open_wrap(device, false);
	if (ret) {
		merr("fimc_is_ischain_open_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_subdev_close(struct fimc_is_subdev *subdev)
{
	int ret = 0;

	if (!test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state)) {
		mserr("subdev is already close", subdev, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	subdev->leader = NULL;
	subdev->vctx = NULL;
	subdev->vid = 0;

	clear_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_FORCE_SET, &subdev->state);

p_err:
	return 0;
}

int fimc_is_sensor_subdev_close(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = fimc_is_subdev_close(subdev);
	if (ret) {
		merr("fimc_is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_ischain_subdev_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx)
{
	int ret = 0;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!vctx);

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	vctx->subdev = NULL;

	ret = fimc_is_subdev_close(subdev);
	if (ret) {
		merr("fimc_is_subdev_close is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_ischain_close_wrap(device);
	if (ret) {
		merr("fimc_is_ischain_close_wrap is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_start(struct fimc_is_subdev *subdev)
{
	int ret = 0;
	struct fimc_is_subdev *leader;

	BUG_ON(!subdev);
	BUG_ON(!subdev->leader);

	leader = subdev->leader;

	if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("already start", subdev, subdev);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		mserr("leader%d is ALREADY started", subdev, subdev, leader->id);
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_sensor_subdev_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_SENSOR_S_INPUT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		mserr("already start", subdev, subdev);
		goto p_err;
	}

	set_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_start(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_ISCHAIN_INIT, &device->state)) {
		mserr("device is not yet init", device, subdev);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_start(subdev);
	if (ret) {
		mserr("fimc_is_subdev_start is fail(%d)", device, subdev, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_stop(struct fimc_is_subdev *subdev)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_subdev *leader;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!subdev->leader);
	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));

	leader = subdev->leader;
	framemgr = GET_SUBDEV_FRAMEMGR(subdev);

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", subdev);
		goto p_err;
	}

	if (test_bit(FIMC_IS_SUBDEV_START, &leader->state)) {
		merr("leader%d is NOT stopped", subdev, leader->id);
		goto p_err;
	}

	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	if (framemgr->queued_count[FS_PROCESS] > 0) {
		framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);
		merr("being processed, can't stop", subdev);
		ret = -EINVAL;
		goto p_err;
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_sensor_subdev_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_device_sensor *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	if (!test_bit(FIMC_IS_SUBDEV_START, &subdev->state)) {
		merr("already stop", subdev);
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_16, flags);

	frame = peek_frame(framemgr, FS_PROCESS);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_PROCESS);
	}

	frame = peek_frame(framemgr, FS_REQUEST);
	while (frame) {
		CALL_VOPS(subdev->vctx, done, frame->index, VB2_BUF_STATE_ERROR);
		trans_frame(framemgr, frame, FS_COMPLETE);
		frame = peek_frame(framemgr, FS_REQUEST);
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_16, flags);

	clear_bit(FIMC_IS_SUBDEV_RUN, &subdev->state);
	clear_bit(FIMC_IS_SUBDEV_START, &subdev->state);

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_stop(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_stop(subdev);
	if (ret) {
		merr("fimc_is_subdev_stop is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_subdev_s_format(struct fimc_is_subdev *subdev,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	u32 pixelformat = 0, width, height;

	BUG_ON(!subdev);
	BUG_ON(!queue);
	BUG_ON(!queue->framecfg.format);

	pixelformat = queue->framecfg.format->pixelformat;

	width = queue->framecfg.width;
	height = queue->framecfg.height;

	switch (subdev->id) {
	case ENTRY_SCC:
	case ENTRY_SCP:
		switch(pixelformat) {
		/*
		 * YUV422 1P, YUV422 2P : x8
		 * YUV422 3P : x16
		 */
		case V4L2_PIX_FMT_YUV422P:
			if (width % 8) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		/*
		 * YUV420 2P : x8
		 * YUV420 3P : x16
		 */
		case V4L2_PIX_FMT_NV12M:
		case V4L2_PIX_FMT_NV21M:
			if (width % 8) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		case V4L2_PIX_FMT_YUV420M:
		case V4L2_PIX_FMT_YVU420M:
			if (width % 16) {
				merr("width(%d) of format(%d) is not supported size",
					subdev, width, pixelformat);
				ret = -EINVAL;
				goto p_err;
			}
			break;
		default:
			merr("format(%d) is not supported", subdev, pixelformat);
			ret = -EINVAL;
			goto p_err;
			break;
		}
		break;
	default:
		break;
	}

	subdev->output.width = width;
	subdev->output.height = height;

	subdev->output.crop.x = 0;
	subdev->output.crop.y = 0;
	subdev->output.crop.w = subdev->output.width;
	subdev->output.crop.h = subdev->output.height;

p_err:
	return ret;
}

static int fimc_is_sensor_subdev_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_sensor *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_s_format(subdev, queue);
	if (ret) {
		merr("fimc_is_subdev_s_format is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ischain_subdev_s_format(void *qdevice,
	struct fimc_is_queue *queue)
{
	int ret = 0;
	struct fimc_is_device_ischain *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;

	BUG_ON(!device);
	BUG_ON(!queue);

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_subdev_s_format(subdev, queue);
	if (ret) {
		merr("fimc_is_subdev_s_format is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

int fimc_is_sensor_subdev_reqbuf(void *qdevice,
	struct fimc_is_queue *queue, u32 count)
{
	int i = 0;
	int ret = 0;
	struct fimc_is_device_sensor *device = qdevice;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_subdev *subdev;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	BUG_ON(!device);
	BUG_ON(!queue);

	if (!count)
		goto p_err;

	vctx = container_of(queue, struct fimc_is_video_ctx, queue);
	if (!vctx) {
		merr("vctx is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	subdev = vctx->subdev;
	if (!subdev) {
		merr("subdev is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	if (!framemgr) {
		merr("framemgr is NULL", device);
		ret = -EINVAL;
		goto p_err;
	}

	for (i = 0; i < count; i++) {
		frame = &framemgr->frames[i];
		frame->subdev = subdev;
	}

p_err:
	return ret;
}

int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	unsigned long flags;
	struct fimc_is_framemgr *framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	struct fimc_is_frame *frame;

	BUG_ON(!subdev);
	BUG_ON(!GET_SUBDEV_FRAMEMGR(subdev));
	BUG_ON(index >= framemgr->num_frames);

	/* 1. check frame validation */
	frame = &framemgr->frames[index];
	if (!frame) {
		mserr("frame is null\n", subdev, subdev);
		ret = EINVAL;
		goto p_err;
	}

	if (index >= framemgr->num_frames) {
		mserr("index(%d) is invalid", subdev, subdev, index);
		ret = -EINVAL;
		goto p_err;
	}

	if (unlikely(!test_bit(FRAME_MEM_INIT, &frame->mem_state))) {
		mserr("frame %d is NOT init", subdev, subdev, index);
		ret = EINVAL;
		goto p_err;
	}

	/* 2. update frame manager */
	framemgr_e_barrier_irqs(framemgr, FMGR_IDX_17, flags);

	if (frame->state == FS_FREE) {
		trans_frame(framemgr, frame, FS_REQUEST);
	} else {
		mserr("frame %d is invalid state(%d)\n", subdev, subdev, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irqr(framemgr, FMGR_IDX_17, flags);

p_err:
	return ret;
}

int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev,
	u32 index)
{
	int ret = 0;
	struct fimc_is_framemgr *framemgr;
	struct fimc_is_frame *frame;

	if (!subdev) {
		warn("subdev is NULL(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(!test_bit(FIMC_IS_SUBDEV_OPEN, &subdev->state))) {
		warn("subdev was closed..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	if (unlikely(!GET_SUBDEV_FRAMEMGR(subdev))) {
		warn("subdev's framemgr is null..(%d)", index);
		ret = -EINVAL;
		return ret;
	}

	framemgr = GET_SUBDEV_FRAMEMGR(subdev);
	BUG_ON(index >= framemgr->num_frames);

	frame = &framemgr->frames[index];
	framemgr_e_barrier_irq(framemgr, FMGR_IDX_18);

	if (frame->state == FS_COMPLETE) {
		trans_frame(framemgr, frame, FS_FREE);
	} else {
		merr("frame is empty from complete", subdev);
		merr("frame(%d) is not com state(%d)", subdev, index, frame->state);
		frame_manager_print_queues(framemgr);
		ret = -EINVAL;
	}

	framemgr_x_barrier_irq(framemgr, FMGR_IDX_18);

	return ret;
}

const struct fimc_is_queue_ops fimc_is_sensor_subdev_ops = {
	.start_streaming	= fimc_is_sensor_subdev_start,
	.stop_streaming		= fimc_is_sensor_subdev_stop,
	.s_format		= fimc_is_sensor_subdev_s_format,
	.request_bufs		= fimc_is_sensor_subdev_reqbuf,
};

const struct fimc_is_queue_ops fimc_is_ischain_subdev_ops = {
	.start_streaming	= fimc_is_ischain_subdev_start,
	.stop_streaming		= fimc_is_ischain_subdev_stop,
	.s_format		= fimc_is_ischain_subdev_s_format
};

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dis start */
}

void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dis stop */
}

void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->dis_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr start */
}

void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for dnr stop */
}

void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->tdnr_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
#ifdef SOC_DRC
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
#endif
}

void fimc_is_subdev_drc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
#ifdef SOC_DRC
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_STOP;
	ctl_param->bypass = CONTROL_BYPASS_ENABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
#endif
}

void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
#ifdef SOC_DRC
	struct param_control *ctl_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	ctl_param = fimc_is_itf_g_param(device, frame, PARAM_DRC_CONTROL);
	ctl_param->cmd = CONTROL_COMMAND_START;
	ctl_param->bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_DRC_CONTROL);
	*hindex |= HIGHBIT_OF(PARAM_DRC_CONTROL);
	(*indexes)++;
#endif
}

void fimc_is_subdev_odc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for odc start */
}

void fimc_is_subdev_odc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes)
{
	/* this function is for odc stop */
}

void fimc_is_subdev_odc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass)
{
	struct param_tpu_config *config_param;

	BUG_ON(!device);
	BUG_ON(!lindex);
	BUG_ON(!hindex);
	BUG_ON(!indexes);

	config_param = fimc_is_itf_g_param(device, frame, PARAM_TPU_CONFIG);
	config_param->odc_bypass = bypass ? CONTROL_BYPASS_ENABLE : CONTROL_BYPASS_DISABLE;
	*lindex |= LOWBIT_OF(PARAM_TPU_CONFIG);
	*hindex |= HIGHBIT_OF(PARAM_TPU_CONFIG);
	(*indexes)++;
}

#ifdef ENABLE_FD_SW
int fimc_is_vra_trigger(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame)
{
	int ret = 0;
	struct fimc_is_lib_fd *lib_data;
	struct camera2_fd_udm *fd_dm;
	struct camera2_stats_dm *fdstats;
	u32 detect_num = 0;

	BUG_ON(!device);
	BUG_ON(!subdev);
	BUG_ON(!frame);

	/* Parameter memset */
	fdstats = &frame->shot->dm.stats;
	memset(fdstats->faceRectangles, 0, sizeof(fdstats->faceRectangles));
	memset(fdstats->faceScores, 0, sizeof(fdstats->faceScores));
	memset(fdstats->faceIds, 0, sizeof(fdstats->faceIds));

	/* vra bypass check */
	if (!test_bit(FIMC_IS_SUBDEV_RUN, &subdev->state))
		goto vra_pass;

	if (unlikely(!device->fd_lib)) {
		merr("fimc_is_fd_lib is NULL", device);
		goto vra_pass;
	}

	if (unlikely(!device->fd_lib->lib_data)) {
		merr("fimc_is_lib_data is NULL", device);
		goto vra_pass;
	}

	lib_data = device->fd_lib->lib_data;
	if (!test_bit(FD_LIB_CONFIG, &device->fd_lib->state)) {
		merr("didn't become FD lib config", device);
		goto vra_pass;
	}

	/* ToDo: Support face detection mode by setfile */
	fdstats->faceDetectMode = FACEDETECT_MODE_FULL;

	if(!test_bit(FD_LIB_RUN, &device->fd_lib->state)) {

		/* Parameter copy for Host FD */
		fd_dm = &frame->shot->udm.fd;
		lib_data->fd_uctl.faceDetectMode = fdstats->faceDetectMode;
		lib_data->data_fd_lib->sat = fd_dm->vendorSpecific[0];

		if (!lib_data->num_detect) {
			memset(&lib_data->prev_fd_uctl, 0, sizeof(struct camera2_fd_uctl));
			lib_data->prev_num_detect = 0;
		} else {
			for (detect_num = 0; detect_num < lib_data->num_detect; detect_num++) {
				/* Parameter copy for HAL */
				fdstats->faceRectangles[detect_num][0] =
					lib_data->fd_uctl.faceRectangles[detect_num][0];
				fdstats->faceRectangles[detect_num][1] =
					lib_data->fd_uctl.faceRectangles[detect_num][1];
				fdstats->faceRectangles[detect_num][2] =
					lib_data->fd_uctl.faceRectangles[detect_num][2];
				fdstats->faceRectangles[detect_num][3] =
					lib_data->fd_uctl.faceRectangles[detect_num][3];
				fdstats->faceScores[detect_num] =
					lib_data->fd_uctl.faceScores[detect_num];
				fdstats->faceIds[detect_num] =
					lib_data->fd_uctl.faceIds[detect_num];
			}

			/* FD information backup */
			memcpy(&lib_data->prev_fd_uctl, &lib_data->fd_uctl,
				sizeof(struct camera2_fd_uctl));
			lib_data->prev_num_detect = lib_data->num_detect;

		}

		/* host fd thread run */
		fimc_is_lib_fd_trigger(device->fd_lib);

	} else {
		for (detect_num = 0; detect_num < lib_data->prev_num_detect; detect_num++) {
			/* Previous parameter copy for HAL */
			fdstats->faceRectangles[detect_num][0] =
				lib_data->prev_fd_uctl.faceRectangles[detect_num][0];
			fdstats->faceRectangles[detect_num][1] =
				lib_data->prev_fd_uctl.faceRectangles[detect_num][1];
			fdstats->faceRectangles[detect_num][2] =
				lib_data->prev_fd_uctl.faceRectangles[detect_num][2];
			fdstats->faceRectangles[detect_num][3] =
				lib_data->prev_fd_uctl.faceRectangles[detect_num][3];
			fdstats->faceScores[detect_num] =
				lib_data->prev_fd_uctl.faceScores[detect_num];
			fdstats->faceIds[detect_num] =
				lib_data->prev_fd_uctl.faceIds[detect_num];
		}
	}

vra_pass:
	return ret;
}
#endif
