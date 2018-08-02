/*
 * drivers/media/platform/exynos/mfc/s5p_mfc_qos.h
 *
 * Copyright (c) 2010 Samsung Electronics Co., Ltd.
 *		http://www.samsung.com/
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 */

#ifndef __S5P_MFC_QOS_H
#define __S5P_MFC_QOS_H __FILE__

#include "s5p_mfc_common.h"

#ifdef CONFIG_MFC_USE_BUS_DEVFREQ
void s5p_mfc_qos_on(struct s5p_mfc_ctx *ctx);
void s5p_mfc_qos_off(struct s5p_mfc_ctx *ctx);
#else
#define s5p_mfc_qos_on(ctx)	do {} while (0)
#define s5p_mfc_qos_off(ctx)	do {} while (0)
#endif

#endif /* __S5P_MFC_QOS_H */
