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

#ifndef FIMC_IS_SUBDEV_H
#define FIMC_IS_SUBDEV_H

#include "fimc-is-video.h"

struct fimc_is_device_sensor;
struct fimc_is_device_ischain;
struct fimc_is_groupmgr;
struct fimc_is_group;

enum fimc_is_subdev_device_type {
	FIMC_IS_SENSOR_SUBDEV,
	FIMC_IS_ISCHAIN_SUBDEV,
};

enum fimc_is_subdev_state {
	FIMC_IS_SUBDEV_OPEN,
	FIMC_IS_SUBDEV_START,
	FIMC_IS_SUBDEV_RUN,
	FIMC_IS_SUBDEV_FORCE_SET,
	FIMC_IS_SUBDEV_PARAM_ERR
};

struct fimc_is_subdev_path {
	u32					width;
	u32					height;
	struct fimc_is_crop			canv;
	struct fimc_is_crop			crop;
};

enum fimc_is_sensor_subdev_id {
	ENTRY_SSVC0,
	ENTRY_SSVC1,
	ENTRY_SSVC2,
	ENTRY_SSVC3,
	ENTRY_SEN_END
};

enum fimc_is_ischain_subdev_id {
	ENTRY_3AA,
	ENTRY_3AC,
	ENTRY_3AP,
	ENTRY_ISP,
	ENTRY_IXC,
	ENTRY_IXP,
	ENTRY_DRC,
	ENTRY_DIS,
	ENTRY_ODC,
	ENTRY_DNR,
	ENTRY_SCC,
	ENTRY_SCP,
	ENTRY_MCS,
	ENTRY_M0P,
	ENTRY_M1P,
	ENTRY_M2P,
	ENTRY_M3P,
	ENTRY_M4P,
	ENTRY_VRA,
	ENTRY_ISCHAIN_END
};

struct fimc_is_subdev_ops {
	int (*bypass)(struct fimc_is_subdev *subdev,
		void *device_data,
		struct fimc_is_frame *frame,
		bool bypass);
	int (*cfg)(struct fimc_is_subdev *subdev,
		void *device_data,
		struct fimc_is_frame *frame,
		struct fimc_is_crop *incrop,
		struct fimc_is_crop *otcrop,
		u32 *lindex,
		u32 *hindex,
		u32 *indexes);
	int (*tag)(struct fimc_is_subdev *subdev,
		void *device_data,
		struct fimc_is_frame *frame,
		struct camera2_node *node);
};

struct fimc_is_subdev {
	u32					id;
	u32					vid; /* video id */
	u32					cid; /* capture node id */
	char					name[4];
	u32					instance;
	unsigned long				state;

	u32					constraints_width; /* spec in width */
	u32					constraints_height; /* spec in height */

	u32					param_otf_in;
	u32					param_dma_in;
	u32					param_otf_ot;
	u32					param_dma_ot;

	struct fimc_is_subdev_path		input;
	struct fimc_is_subdev_path		output;

	struct fimc_is_video_ctx		*vctx;
	struct fimc_is_subdev			*leader;
	const struct fimc_is_subdev_ops		*ops;
};

int fimc_is_sensor_subdev_open(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_sensor_subdev_close(struct fimc_is_device_sensor *device,
	struct fimc_is_video_ctx *vctx);

int fimc_is_ischain_subdev_open(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);
int fimc_is_ischain_subdev_close(struct fimc_is_device_ischain *device,
	struct fimc_is_video_ctx *vctx);

/*common subdev*/
int fimc_is_subdev_probe(struct fimc_is_subdev *subdev,
	u32 instance,
	u32 id,
	char *name,
	const struct fimc_is_subdev_ops *sops);
int fimc_is_subdev_open(struct fimc_is_subdev *subdev,
	struct fimc_is_video_ctx *vctx,
	void *ctl_data);
int fimc_is_subdev_close(struct fimc_is_subdev *subdev);
int fimc_is_subdev_reqbuf(struct fimc_is_subdev *subdev);
int fimc_is_subdev_buffer_queue(struct fimc_is_subdev *subdev, u32 index);
int fimc_is_subdev_buffer_finish(struct fimc_is_subdev *subdev, u32 index);

void fimc_is_subdev_dis_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dis_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_dnr_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_dnr_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_drc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_drc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
void fimc_is_subdev_odc_start(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_odc_stop(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes);
void fimc_is_subdev_odc_bypass(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame, u32 *lindex, u32 *hindex, u32 *indexes, bool bypass);
int fimc_is_vra_trigger(struct fimc_is_device_ischain *device,
	struct fimc_is_subdev *subdev,
	struct fimc_is_frame *frame);

int fimc_is_sensor_subdev_reqbuf(void *qdevice,
	struct fimc_is_queue *queue, u32 count);
struct fimc_is_subdev * video2subdev(enum fimc_is_subdev_device_type device_type,
	void *device, u32 vid);

#define GET_SUBDEV_FRAMEMGR(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue.framemgr) : NULL)
#define GET_SUBDEV_QUEUE(subdev) \
	(((subdev) && (subdev)->vctx) ? (&(subdev)->vctx->queue) : NULL)
#define CALL_SOPS(s, op, args...)	(((s) && (s)->ops && (s)->ops->op) ? ((s)->ops->op(s, args)) : 0)

#endif
