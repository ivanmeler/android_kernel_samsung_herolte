/*
 * Samsung Exynos5 SoC series FIMC-IS driver
 *
 *
 * Copyright (c) 2011 Samsung Electronics Co., Ltd
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef FIMC_IS_GROUP_MGR_H
#define FIMC_IS_GROUP_MGR_H

#include "fimc-is-config.h"
#include "fimc-is-time.h"
#include "fimc-is-subdev-ctrl.h"
#include "fimc-is-video.h"
#include "fimc-is-cmd.h"

/* #define DEBUG_AA */
/* #define DEBUG_FLASH */

#define TRACE_GROUP
#define GROUP_ID_3AA0		0
#define GROUP_ID_3AA1		1
#define GROUP_ID_ISP0		2
#define GROUP_ID_ISP1		3
#define GROUP_ID_DIS0		4
#define GROUP_ID_MCS0		5
#define GROUP_ID_MCS1		6
#define GROUP_ID_VRA0		7
#define GROUP_ID_MAX		8
#define GROUP_ID_INVALID	0xFFFFFFFF
#define GROUP_ID_PARM_MASK	(0x3F)
#define GROUP_ID_SHIFT		(16)
#define GROUP_ID_MASK		(0xFFFF)
#define GROUP_ID(id)		(1 << (id))

#define GROUP_SLOT_3AA		0
#define GROUP_SLOT_ISP		1
#define GROUP_SLOT_DIS		2
#define GROUP_SLOT_MCS		3
#define GROUP_SLOT_VRA		4
#define GROUP_SLOT_MAX		5

#define FIMC_IS_MAX_GFRAME	(VIDEO_MAX_FRAME * 2) /* max shot buffer of F/W : 32, MAX 2 groups */
#define MIN_OF_ASYNC_SHOTS	1
#define MIN_OF_SYNC_SHOTS	2
#define MIN_OF_SHOT_RSC		(MIN_OF_ASYNC_SHOTS + MIN_OF_SYNC_SHOTS)

enum fimc_is_group_state {
	FIMC_IS_GROUP_OPEN,
	FIMC_IS_GROUP_INIT,
	FIMC_IS_GROUP_START,
	FIMC_IS_GROUP_SHOT,
	FIMC_IS_GROUP_REQUEST_FSTOP,
	FIMC_IS_GROUP_FORCE_STOP,
	FIMC_IS_GROUP_OTF_INPUT,
	FIMC_IS_GROUP_OTF_OUTPUT,
	FIMC_IS_GROUP_PIPE_INPUT,
	FIMC_IS_GROUP_PIPE_OUTPUT,
	FIMC_IS_GROUP_SEMI_PIPE_INPUT,
	FIMC_IS_GROUP_SEMI_PIPE_OUTPUT,
	FIMC_IS_GROUP_UNMAP
};

enum fimc_is_group_input_type {
	GROUP_INPUT_MEMORY,
	GROUP_INPUT_OTF,
	GROUP_INPUT_PIPE,
	GROUP_INPUT_SEMI_PIPE
};

struct fimc_is_frame;
struct fimc_is_device_ischain;
typedef int (*fimc_is_shot_callback)(struct fimc_is_device_ischain *device,
	struct fimc_is_frame *frame);
typedef int (*fimc_is_pipe_shot_callback)(struct fimc_is_device_ischain *device,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame);

struct fimc_is_group_frame {
	struct list_head		list;
	u32				fcount;
	struct fimc_is_crop		canv;
	struct camera2_node_group	group_cfg[GROUP_SLOT_MAX];
};

struct fimc_is_group_framemgr {
	struct fimc_is_group_frame	gframe[FIMC_IS_MAX_GFRAME];
	spinlock_t			gframe_slock;
	struct list_head		gframe_head;
	u32				gframe_cnt;
};

struct fimc_is_group {
	struct fimc_is_group		*next;
	struct fimc_is_group		*prev;
	struct fimc_is_group		*gnext;
	struct fimc_is_group		*gprev;
	struct fimc_is_group		*parent;
	struct fimc_is_group		*child;
	struct fimc_is_group		*tail;

	struct fimc_is_subdev		leader;
	struct fimc_is_subdev		*junction;
	struct fimc_is_subdev		*subdev[ENTRY_ISCHAIN_END];

	/* for otf interface */
	atomic_t			sensor_fcount;
	atomic_t			backup_fcount;
	struct semaphore		smp_trigger;
	struct semaphore		smp_shot;
	atomic_t			smp_shot_count;
	u32				init_shots;
	u32				asyn_shots;
	u32				sync_shots;
	u32				skip_shots;
#ifdef ENABLE_FAST_SHOT
	struct camera2_ctl		fast_ctl;
#endif
	struct camera2_aa_ctl		intent_ctl;

	u32				id; /* group id */
	u32				slot; /* group slot */
	u32				instance; /* device instance */
	u32				source_vid; /* source video id */
	u32				pcount; /* program count */
	u32				fcount; /* frame count */
	atomic_t			scount; /* shot count */
	atomic_t			rcount; /* request count */
	unsigned long			state;

	struct list_head		gframe_head;
	u32				gframe_cnt;

	fimc_is_shot_callback		shot_callback;
	fimc_is_pipe_shot_callback	pipe_shot_callback;
	struct fimc_is_device_ischain	*device;

#ifdef DEBUG_AA
#ifdef DEBUG_FLASH
	enum aa_ae_flashmode		flashmode;
	struct camera2_flash_dm		flash;
#endif
#endif

#ifdef MEASURE_TIME
#ifdef MONITOR_TIME
	struct fimc_is_time		time;
#endif
#endif
	u32				aeflashMode; /* Flash Mode Control */
};

enum fimc_is_group_task_state {
	FIMC_IS_GTASK_START,
	FIMC_IS_GTASK_REQUEST_STOP
};

struct fimc_is_group_task {
	u32				id;
	struct task_struct		*task;
	struct kthread_worker		worker;
	struct semaphore		smp_resource;
	unsigned long			state;
	atomic_t			refcount;
};

struct fimc_is_groupmgr {
	struct fimc_is_group_framemgr	gframemgr[FIMC_IS_STREAM_COUNT];
	struct fimc_is_group		*leader[FIMC_IS_STREAM_COUNT];
	struct fimc_is_group		*group[FIMC_IS_STREAM_COUNT][GROUP_SLOT_MAX];
	struct fimc_is_group_task	gtask[GROUP_ID_MAX];
};

int fimc_is_groupmgr_probe(struct fimc_is_groupmgr *groupmgr);
int fimc_is_groupmgr_init(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device);
int fimc_is_groupmgr_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device);
int fimc_is_groupmgr_stop(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_device_ischain *device);

int fimc_is_group_probe(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_device_ischain *device,
	fimc_is_shot_callback shot_callback,
	u32 slot,
	u32 id,
	char *name,
	const struct fimc_is_subdev_ops *sops);
int fimc_is_group_open(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 id,
	struct fimc_is_video_ctx *vctx);
int fimc_is_group_close(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group);
int fimc_is_group_init(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	u32 input_type,
	u32 video_id,
	u32 stream_leader);
int fimc_is_group_start(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group);
int fimc_is_group_stop(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group);
int fimc_is_group_buffer_queue(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_queue *queue,
	u32 index);
int fimc_is_group_buffer_finish(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 index);
int fimc_is_group_shot(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame);
int fimc_is_group_done(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group,
	struct fimc_is_frame *frame,
	u32 done_state);

int fimc_is_gframe_cancel(struct fimc_is_groupmgr *groupmgr,
	struct fimc_is_group *group, u32 target_fcount);

#endif
