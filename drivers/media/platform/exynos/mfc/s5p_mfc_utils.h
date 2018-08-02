/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_utils.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_UTILS_H
#define __S5P_MFC_UTILS_H __FILE__

#include <linux/time.h>

#include "s5p_mfc_common.h"

static inline unsigned long timeval_diff(struct timeval *to,
					struct timeval *from)
{
	return (to->tv_sec * USEC_PER_SEC + to->tv_usec)
		- (from->tv_sec * USEC_PER_SEC + from->tv_usec);
}

int get_framerate_by_interval(int interval);
int get_framerate(struct timeval *to, struct timeval *from);

void s5p_mfc_cleanup_queue(struct list_head *lh);
int check_vb_with_fmt(struct s5p_mfc_fmt *fmt, struct vb2_buffer *vb);

int s5p_mfc_stream_buf_prot(struct s5p_mfc_ctx *ctx,
				struct s5p_mfc_buf *buf, bool en);
int s5p_mfc_raw_buf_prot(struct s5p_mfc_ctx *ctx,
				struct s5p_mfc_buf *buf, bool en);
#endif /* __S5P_MFC_UTILS_H */
