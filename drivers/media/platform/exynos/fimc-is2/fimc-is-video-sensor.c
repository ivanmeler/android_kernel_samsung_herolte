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
#include <linux/videodev2_exynos_camera.h>
#include <linux/v4l2-mediabus.h>
#include <linux/bug.h>

#include "fimc-is-device-sensor.h"
#include "fimc-is-cmd.h"
#include "fimc-is-regs.h"
#include "fimc-is-err.h"
#include "fimc-is-video.h"
#include "fimc-is-resourcemgr.h"

#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
extern int s2mpb02_ir_led_current(uint32_t current_value);
extern int s2mpb02_ir_led_pulse_width(uint32_t width);
extern int s2mpb02_ir_led_pulse_delay(uint32_t delay);
extern int s2mpb02_ir_led_max_time(uint32_t max_time);
#endif

const struct v4l2_file_operations fimc_is_ssx_video_fops;
const struct v4l2_ioctl_ops fimc_is_ssx_video_ioctl_ops;
const struct vb2_ops fimc_is_ssx_qops;

int fimc_is_ssx_video_probe(void *data)
{
	int ret = 0;
	struct fimc_is_device_sensor *device;
	struct fimc_is_video *video;
	char name[30];
	u32 instance;

	BUG_ON(!data);

	device = (struct fimc_is_device_sensor *)data;
	instance = device->instance;
	video = &device->video;
	video->resourcemgr = device->resourcemgr;
	snprintf(name, sizeof(name), "%s%d", FIMC_IS_VIDEO_SSX_NAME, instance);

	if (!device->pdev) {
		err("pdev is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_video_probe(video,
		name,
		FIMC_IS_VIDEO_SS0_NUM + instance,
		VFL_DIR_RX,
		&device->mem,
		&device->v4l2_dev,
		&fimc_is_ssx_video_fops,
		&fimc_is_ssx_video_ioctl_ops);
	if (ret)
		dev_err(&device->pdev->dev, "%s is fail(%d)\n", __func__, ret);

p_err:
	return ret;
}

/*
 * =============================================================================
 * Video File Opertation
 * =============================================================================
 */

static int fimc_is_ssx_video_open(struct file *file)
{
	int ret = 0;
	struct fimc_is_video *video;
	struct fimc_is_video_ctx *vctx;
	struct fimc_is_device_sensor *device;
	struct fimc_is_resourcemgr *resourcemgr;

	vctx = NULL;
	video = video_drvdata(file);
	device = container_of(video, struct fimc_is_device_sensor, video);
	resourcemgr = video->resourcemgr;
	if (!resourcemgr) {
		err("resourcemgr is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_resource_open(resourcemgr, video->id - FIMC_IS_VIDEO_SS0_NUM, NULL);
	if (ret) {
		err("fimc_is_resource_open is fail(%d)", ret);
		goto p_err;
	}

	minfo("[SS%d:V] %s\n", device, video->id, __func__);

	ret = open_vctx(file, video, &vctx, device->instance, FRAMEMGR_ID_SSX);
	if (ret) {
		merr("open_vctx is fail(%d)", device, ret);
		goto p_err;
	}

	ret = fimc_is_video_open(vctx,
		device,
		VIDEO_SSX_READY_BUFFERS,
		video,
		&fimc_is_ssx_qops,
		&fimc_is_sensor_ops);
	if (ret) {
		merr("fimc_is_video_open is fail(%d)", device, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

	ret = fimc_is_sensor_open(device, vctx);
	if (ret) {
		merr("fimc_is_ssx_open is fail(%d)", device, ret);
		close_vctx(file, video, vctx);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ssx_video_close(struct file *file)
{
	int ret = 0;
	int refcount;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_video *video;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_VIDEO(vctx));
	BUG_ON(!GET_DEVICE(vctx));

	video = GET_VIDEO(vctx);
	device = GET_DEVICE(vctx);

	ret = fimc_is_sensor_close(device);
	if (ret)
		merr("fimc_is_sensor_close is fail(%d)", device, ret);

	ret = fimc_is_video_close(vctx);
	if (ret)
		merr("fimc_is_video_close is fail(%d)", device, ret);

	refcount = close_vctx(file, video, vctx);
	if (refcount < 0)
		merr("close_vctx is fail(%d)", device, refcount);

	minfo("[SS%d:V] %s(%d):%d\n", device, GET_SSX_ID(video), __func__, refcount, ret);

	return ret;
}

static unsigned int fimc_is_ssx_video_poll(struct file *file,
	struct poll_table_struct *wait)
{
	u32 ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_poll(file, vctx, wait);

	return ret;
}

static int fimc_is_ssx_video_mmap(struct file *file,
	struct vm_area_struct *vma)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	ret = fimc_is_video_mmap(file, vctx, vma);
	if (ret)
		merr("fimc_is_video_mmap is fail(%d)", vctx, ret);

	return ret;
}

const struct v4l2_file_operations fimc_is_ssx_video_fops = {
	.owner		= THIS_MODULE,
	.open		= fimc_is_ssx_video_open,
	.release	= fimc_is_ssx_video_close,
	.poll		= fimc_is_ssx_video_poll,
	.unlocked_ioctl	= video_ioctl2,
	.mmap		= fimc_is_ssx_video_mmap,
};

/*
 * =============================================================================
 * Video Ioctl Opertation
 * =============================================================================
 */

static int fimc_is_ssx_video_querycap(struct file *file, void *fh,
					struct v4l2_capability *cap)
{
	/* Todo : add to query capability code */
	return 0;
}

static int fimc_is_ssx_video_enum_fmt_mplane(struct file *file, void *priv,
				    struct v4l2_fmtdesc *f)
{
	/* Todo : add to enumerate format code */
	return 0;
}

static int fimc_is_ssx_video_get_format_mplane(struct file *file, void *fh,
						struct v4l2_format *format)
{
	/* Todo : add to get format code */
	return 0;
}

static int fimc_is_ssx_video_set_format_mplane(struct file *file, void *fh,
	struct v4l2_format *format)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);
	BUG_ON(!format);

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_set_format_mplane(file, vctx, format);
	if (ret) {
		merr("fimc_is_video_set_format_mplane is fail(%d)", vctx, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ssx_video_cropcap(struct file *file, void *fh,
	struct v4l2_cropcap *cropcap)
{
	/* Todo : add to crop capability code */
	return 0;
}

static int fimc_is_ssx_video_get_crop(struct file *file, void *fh,
	struct v4l2_crop *crop)
{
	/* Todo : add to get crop control code */
	return 0;
}

static int fimc_is_ssx_video_set_crop(struct file *file, void *fh,
	const struct v4l2_crop *crop)
{
	/* Todo : add to set crop control code */
	return 0;
}

static int fimc_is_ssx_video_reqbufs(struct file *file, void *priv,
	struct v4l2_requestbuffers *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	BUG_ON(!vctx);

	mdbgv_sensor("%s(buffers : %d)\n", vctx, __func__, buf->count);

	ret = fimc_is_video_reqbufs(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_reqbufs is fail(error %d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_video_querybuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_querybuf(file, vctx, buf);
	if (ret)
		merr("fimc_is_video_querybuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_video_qbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

#ifdef DBG_STREAMING
	/*dbg_sensor("%s\n", __func__);*/
#endif

	ret = CALL_VOPS(vctx, qbuf, buf);
	if (ret)
		merr("qbuf is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_video_dqbuf(struct file *file, void *priv,
	struct v4l2_buffer *buf)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	bool blocking = file->f_flags & O_NONBLOCK;

#ifdef DBG_STREAMING
	mdbgv_sensor("%s\n", vctx, __func__);
#endif

	ret = CALL_VOPS(vctx, dqbuf, buf, blocking);
	if (ret) {
		if (!blocking || (ret != -EAGAIN))
			merr("dqbuf is fail(%d)", vctx, ret);
	}

	return ret;
}

static int fimc_is_ssx_video_streamon(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_streamon(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamon is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_video_streamoff(struct file *file, void *priv,
	enum v4l2_buf_type type)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;

	mdbgv_sensor("%s\n", vctx, __func__);

	ret = fimc_is_video_streamoff(file, vctx, type);
	if (ret)
		merr("fimc_is_video_streamoff is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_video_enum_input(struct file *file, void *priv,
	struct v4l2_input *input)
{
	/* Todo: add to enumerate input code */
	info("%s is calld\n", __func__);
	return 0;
}

static int fimc_is_ssx_video_g_input(struct file *file, void *priv,
	unsigned int *input)
{
	/* Todo: add to get input control code */
	return 0;
}

static int fimc_is_ssx_video_s_input(struct file *file, void *priv,
	unsigned int input)
{
	int ret = 0;
	u32 scenario;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;
	struct fimc_is_framemgr *framemgr;

	BUG_ON(!vctx);

	mdbgv_sensor("%s(input : %08X)\n", vctx, __func__, input);

	device = GET_DEVICE(vctx);
	framemgr = GET_FRAMEMGR(vctx);
	scenario = (input & SENSOR_SCENARIO_MASK) >> SENSOR_SCENARIO_SHIFT;
	input = (input & SENSOR_MODULE_MASK) >> SENSOR_MODULE_SHIFT;

	ret = fimc_is_video_s_input(file, vctx);
	if (ret) {
		merr("fimc_is_video_s_input is fail(%d)", vctx, ret);
		goto p_err;
	}

	ret = fimc_is_sensor_s_input(device, input, scenario);
	if (ret) {
		merr("fimc_is_sensor_s_input is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static int fimc_is_ssx_video_s_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;
	struct v4l2_subdev *subdev_flite;

	BUG_ON(!ctrl);
	BUG_ON(!vctx);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	subdev_flite = device->subdev_flite;
	if (!subdev_flite) {
		err("subdev_flite is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_S_STREAM:
		{
			u32 sstream, instant, noblock;

			sstream = (ctrl->value & SENSOR_SSTREAM_MASK) >> SENSOR_SSTREAM_SHIFT;
			instant = (ctrl->value & SENSOR_INSTANT_MASK) >> SENSOR_INSTANT_SHIFT;
			noblock = (ctrl->value & SENSOR_NOBLOCK_MASK) >> SENSOR_NOBLOCK_SHIFT;
			/*
			 * nonblock(0) : blocking command
			 * nonblock(1) : non-blocking command
			 */

			if (sstream == IS_ENABLE_STREAM) {
				ret = fimc_is_sensor_front_start(device, instant, noblock);
				if (ret) {
					merr("fimc_is_sensor_front_start is fail(%d)", device, ret);
					goto p_err;
				}
			} else {
				ret = fimc_is_sensor_front_stop(device);
				if (ret) {
					merr("fimc_is_sensor_front_stop is fail(%d)", device, ret);
					goto p_err;
				}
			}
		}
		break;
	case V4L2_CID_IS_S_BNS:
		if (device->pdata->is_bns == false) {
			mwarn("Could not support BNS", device);
			goto p_err;
		}

		ret = fimc_is_sensor_s_bns(device, ctrl->value);
		if (ret) {
			merr("fimc_is_sensor_s_bns is fail(%d)", device, ret);
			goto p_err;
		}

		ret = v4l2_subdev_call(subdev_flite, core, s_ctrl, ctrl);
		if (ret) {
			merr("v4l2_flite_call(s_ctrl) is fail(%d)", device, ret);
			goto p_err;
		}
		break;
	/*
	 * gain boost: min_target_fps,  max_target_fps, scene_mode
	 */
	case V4L2_CID_IS_MIN_TARGET_FPS:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set min_target_fps: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->min_target_fps = ctrl->value;
		}
		break;
	case V4L2_CID_IS_MAX_TARGET_FPS:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set max_target_fps: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->max_target_fps = ctrl->value;
		}
		break;
	case V4L2_CID_SCENEMODE:
		if (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state)) {
			err("failed to set scene_mode: %d - sensor stream on already\n",
					ctrl->value);
			ret = -EINVAL;
		} else {
			device->scene_mode = ctrl->value;
		}
		break;
	case V4L2_CID_SENSOR_SET_FRAME_RATE:
		if (fimc_is_sensor_s_frame_duration(device, ctrl->value)) {
			err("failed to set frame duration : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_AE_TARGET:
		if (fimc_is_sensor_s_exposure_time(device, ctrl->value)) {
			err("failed to set exposure time : %d\n - %d",
					ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_GAIN:
		if (fimc_is_sensor_s_again(device, ctrl->value)) {
			err("failed to set gain : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_SENSOR_SET_SHUTTER:
		if (fimc_is_sensor_s_shutterspeed(device, ctrl->value)) {
			err("failed to set shutter speed : %d\n - %d",
				ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#ifdef CONFIG_LEDS_IRIS_IRLED_SUPPORT
	case V4L2_CID_IRLED_SET_WIDTH:
		ret = s2mpb02_ir_led_pulse_width(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse width : %d\n - %d",ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_DELAY:
		ret = s2mpb02_ir_led_pulse_delay(ctrl->value);
		if (ret < 0) {
			err("failed to set irled pulse delay : %d\n - %d",ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_CURRENT:
		ret = s2mpb02_ir_led_current(ctrl->value);
		if (ret < 0) {
			err("failed to set irled current : %d\n - %d",ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
	case V4L2_CID_IRLED_SET_ONTIME:
		ret = s2mpb02_ir_led_max_time(ctrl->value);
		if (ret < 0) {
			err("failed to set irled max time : %d\n - %d",ctrl->value, ret);
			ret = -EINVAL;
		}
		break;
#endif
	default:
		ret = fimc_is_sensor_s_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int fimc_is_ssx_video_g_ctrl(struct file *file, void *priv,
	struct v4l2_control *ctrl)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!ctrl);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	switch (ctrl->id) {
	case V4L2_CID_IS_G_STREAM:
		if (device->instant_ret)
			ctrl->value = device->instant_ret;
		else
			ctrl->value = (test_bit(FIMC_IS_SENSOR_FRONT_START, &device->state) ?
				IS_ENABLE_STREAM : IS_DISABLE_STREAM);
		break;
	case V4L2_CID_IS_G_BNS_SIZE:
		{
			u32 width, height;

			width = fimc_is_sensor_g_bns_width(device);
			height = fimc_is_sensor_g_bns_height(device);

			ctrl->value = (width << 16) | height;
		}
		break;
	case V4L2_CID_IS_G_DTPSTATUS:
		if (test_bit(FIMC_IS_SENSOR_FRONT_DTP_STOP, &device->state))
			ctrl->value = 1;
		else
			ctrl->value = 0;
		break;
	default:
		ret = fimc_is_sensor_g_ctrl(device, ctrl);
		if (ret) {
			err("invalid ioctl(0x%08X) is requested", ctrl->id);
			ret = -EINVAL;
			goto p_err;
		}
		break;
	}

p_err:
	return ret;
}

static int fimc_is_ssx_video_g_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device = vctx->device;
	struct v4l2_captureparm *cp = &parm->parm.capture;
	struct v4l2_fract *tfp = &cp->timeperframe;

	if (parm->type != V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE)
		return -EINVAL;

	cp->capability |= V4L2_CAP_TIMEPERFRAME;
	tfp->numerator = 1;
	tfp->denominator = device->image.framerate;

	return 0;
}

static int fimc_is_ssx_video_s_parm(struct file *file, void *priv,
	struct v4l2_streamparm *parm)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = file->private_data;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!parm);

	mdbgv_sensor("%s\n", vctx, __func__);

	device = vctx->device;
	if (!device) {
		err("device is NULL");
		ret = -EINVAL;
		goto p_err;
	}

	ret = fimc_is_sensor_s_framerate(device, parm);
	if (ret) {
		merr("fimc_is_ssx_s_framerate is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

const struct v4l2_ioctl_ops fimc_is_ssx_video_ioctl_ops = {
	.vidioc_querycap		= fimc_is_ssx_video_querycap,
	.vidioc_enum_fmt_vid_cap_mplane	= fimc_is_ssx_video_enum_fmt_mplane,
	.vidioc_g_fmt_vid_cap_mplane	= fimc_is_ssx_video_get_format_mplane,
	.vidioc_s_fmt_vid_cap_mplane	= fimc_is_ssx_video_set_format_mplane,
	.vidioc_cropcap			= fimc_is_ssx_video_cropcap,
	.vidioc_g_crop			= fimc_is_ssx_video_get_crop,
	.vidioc_s_crop			= fimc_is_ssx_video_set_crop,
	.vidioc_reqbufs			= fimc_is_ssx_video_reqbufs,
	.vidioc_querybuf		= fimc_is_ssx_video_querybuf,
	.vidioc_qbuf			= fimc_is_ssx_video_qbuf,
	.vidioc_dqbuf			= fimc_is_ssx_video_dqbuf,
	.vidioc_streamon		= fimc_is_ssx_video_streamon,
	.vidioc_streamoff		= fimc_is_ssx_video_streamoff,
	.vidioc_enum_input		= fimc_is_ssx_video_enum_input,
	.vidioc_g_input			= fimc_is_ssx_video_g_input,
	.vidioc_s_input			= fimc_is_ssx_video_s_input,
	.vidioc_s_ctrl			= fimc_is_ssx_video_s_ctrl,
	.vidioc_g_ctrl			= fimc_is_ssx_video_g_ctrl,
	.vidioc_g_parm			= fimc_is_ssx_video_g_parm,
	.vidioc_s_parm			= fimc_is_ssx_video_s_parm,
};

static int fimc_is_ssx_queue_setup(struct vb2_queue *vbq,
	const struct v4l2_format *fmt,
	unsigned int *num_buffers, unsigned int *num_planes,
	unsigned int sizes[],
	void *allocators[])
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_video *video;
	struct fimc_is_queue *queue;

	BUG_ON(!vctx);
	BUG_ON(!vctx->video);

	mdbgv_sensor("%s\n", vctx, __func__);

	video = GET_VIDEO(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_setup(queue,
		video->alloc_ctx,
		num_planes,
		sizes,
		allocators);
	if (ret)
		merr("fimc_is_queue_setup is fail(%d)", vctx, ret);

	return ret;
}

static int fimc_is_ssx_buffer_prepare(struct vb2_buffer *vb)
{
	return fimc_is_queue_prepare(vb);
}

static inline void fimc_is_ssx_wait_prepare(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_prepare(vbq);
}

static inline void fimc_is_ssx_wait_finish(struct vb2_queue *vbq)
{
	fimc_is_queue_wait_finish(vbq);
}

static int fimc_is_ssx_start_streaming(struct vb2_queue *vbq,
	unsigned int count)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

	mdbgv_sensor("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_start_streaming(queue, device);
	if (ret) {
		merr("fimc_is_queue_stop_streaming is fail(%d)", device, ret);
		goto p_err;
	}

p_err:
	return ret;
}

static void fimc_is_ssx_stop_streaming(struct vb2_queue *vbq)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vbq->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

	mdbgv_sensor("%s\n", vctx, __func__);

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_stop_streaming(queue, device);
	if (ret) {
		merr("fimc_is_queue_stop_streaming is fail(%d)", device, ret);
		return;
	}

}

static void fimc_is_ssx_buffer_queue(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_queue *queue;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

#ifdef DBG_STREAMING
	mdbgv_sensor("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif

	device = GET_DEVICE(vctx);
	queue = GET_QUEUE(vctx);

	ret = fimc_is_queue_buffer_queue(queue, vb);
	if (ret) {
		merr("fimc_is_queue_buffer_queue is fail(%d)", device, ret);
		return;
	}

	ret = fimc_is_sensor_buffer_queue(device, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_sensor_buffer_queue is fail(%d)", device, ret);
		return;
	}
}

static void fimc_is_ssx_buffer_finish(struct vb2_buffer *vb)
{
	int ret = 0;
	struct fimc_is_video_ctx *vctx = vb->vb2_queue->drv_priv;
	struct fimc_is_device_sensor *device;

	BUG_ON(!vctx);
	BUG_ON(!GET_DEVICE(vctx));

#ifdef DBG_STREAMING
	mdbgv_sensor("%s(%d)\n", vctx, __func__, vb->v4l2_buf.index);
#endif

	device = GET_DEVICE(vctx);

	ret = fimc_is_sensor_buffer_finish(device, vb->v4l2_buf.index);
	if (ret) {
		merr("fimc_is_sensor_buffer_finish is fail(%d)", device, ret);
		return;
	}
}

const struct vb2_ops fimc_is_ssx_qops = {
	.queue_setup		= fimc_is_ssx_queue_setup,
	.buf_prepare		= fimc_is_ssx_buffer_prepare,
	.buf_queue		= fimc_is_ssx_buffer_queue,
	.buf_finish		= fimc_is_ssx_buffer_finish,
	.wait_prepare		= fimc_is_ssx_wait_prepare,
	.wait_finish		= fimc_is_ssx_wait_finish,
	.start_streaming	= fimc_is_ssx_start_streaming,
	.stop_streaming		= fimc_is_ssx_stop_streaming,
};
