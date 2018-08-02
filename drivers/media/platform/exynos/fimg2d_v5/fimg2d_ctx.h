/* linux/drivers/media/video/exynos/fimg2d_v5/fimg2d_ctx.h
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

static inline void fimg2d_enqueue(struct fimg2d_bltcmd *cmd,
						struct fimg2d_control *ctrl)
{
	if (cmd->blt.use_fence) {
		list_add_tail(&cmd->node, &ctrl->waiting_job_q);
		atomic_inc(&ctrl->n_waiting_q);
	} else {
		list_add_tail(&cmd->node, &ctrl->running_job_q);
		atomic_inc(&ctrl->n_running_q);
	}
}

static inline void fimg2d_dequeue(struct fimg2d_bltcmd *cmd,
						struct fimg2d_control *ctrl)
{
	list_del(&cmd->node);
	atomic_dec(&ctrl->n_running_q);
}

static inline int fimg2d_queue_is_empty(struct list_head *q)
{
	return list_empty(q);
}

static inline struct fimg2d_bltcmd
	*fimg2d_get_first_command(struct fimg2d_control *ctrl, int is_wait_q)
{
	struct list_head *job_q;

	job_q = is_wait_q ? &ctrl->waiting_job_q : &ctrl->running_job_q;

	if (list_empty(job_q))
		return NULL;

	return list_first_entry(job_q, struct fimg2d_bltcmd, node);
}

void fimg2d_add_context(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx);
void fimg2d_del_context(struct fimg2d_control *ctrl,
		struct fimg2d_context *ctx);
struct fimg2d_bltcmd *fimg2d_add_command(struct fimg2d_control *ctrl,
	struct fimg2d_context *ctx, struct fimg2d_blit __user *buf, int *info);
void fimg2d_del_command(struct fimg2d_control *ctrl, struct fimg2d_bltcmd *cmd);
struct fimg2d_bltcmd *fimg2d_get_command(struct fimg2d_control *ctrl,
								int is_wait_q);
