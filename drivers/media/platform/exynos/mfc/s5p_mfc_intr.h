/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_intr.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * It contains waiting functions declarations.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_INTR_H
#define __S5P_MFC_INTR_H __FILE__

#include "s5p_mfc_common.h"

int s5p_mfc_wait_for_done_ctx(struct s5p_mfc_ctx *ctx, int command);
int s5p_mfc_wait_for_done_dev(struct s5p_mfc_dev *dev, int command);
void s5p_mfc_cleanup_timeout(struct s5p_mfc_ctx *ctx);

int s5p_mfc_get_new_ctx(struct s5p_mfc_dev *dev);

static inline int clear_hw_bit(struct s5p_mfc_ctx *ctx)
{
	struct s5p_mfc_dev *dev = ctx->dev;
	int ret = -1;

	if (!atomic_read(&dev->watchdog_run)) {
		ret = test_and_clear_bit(ctx->num, &dev->hw_lock);
		/* Reset the timeout watchdog */
		atomic_set(&dev->watchdog_cnt, 0);
	} else {
		mfc_err_ctx("couldn't clear hw_lock\n");
	}

	return ret;
}

static inline void s5p_mfc_clean_dev_int_flags(struct s5p_mfc_dev *dev)
{
	dev->int_cond = 0;
	dev->int_type = 0;
	dev->int_err = 0;
}

static inline void s5p_mfc_clean_ctx_int_flags(struct s5p_mfc_ctx *ctx)
{
	ctx->int_cond = 0;
	ctx->int_type = 0;
	ctx->int_err = 0;
}

static inline void s5p_mfc_change_state(struct s5p_mfc_ctx *ctx, enum s5p_mfc_inst_state state)
{
	struct s5p_mfc_dev *dev = ctx->dev;

	MFC_TRACE_CTX("** state : %d\n", state);
	ctx->state = state;
}

#endif /* __S5P_MFC_INTR_H */
