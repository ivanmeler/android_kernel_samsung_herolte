/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_ctrl.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_CTRL_H
#define __S5P_MFC_CTRL_H __FILE__

#include "s5p_mfc_common.h"

int s5p_mfc_init_hw(struct s5p_mfc_dev *dev);
void s5p_mfc_deinit_hw(struct s5p_mfc_dev *dev);

int s5p_mfc_sleep(struct s5p_mfc_dev *dev);
int s5p_mfc_wakeup(struct s5p_mfc_dev *dev);

#endif /* __S5P_MFC_CTRL_H */
