/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_opr_v10.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * Header file for Samsung MFC (Multi Function Codec - FIMV) driver
 * Contains declarations of hw related functions.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __S5P_MFC_OPR_V10_H
#define __S5P_MFC_OPR_V10_H __FILE__

#include "s5p_mfc_common.h"

void s5p_mfc_try_run(struct s5p_mfc_dev *dev);
void s5p_mfc_cleanup_timeout_and_try_run(struct s5p_mfc_ctx *ctx);

#endif /* __S5P_MFC_OPR_V10_H */
